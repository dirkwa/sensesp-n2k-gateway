#include "sensesp_n2k_gateway/candump_tcp_server.h"

#include <cstring>

#include "esp_log.h"
#include "lwip/sockets.h"

#include "sensesp_n2k_gateway/candump_format.h"

namespace sensesp {

namespace {
constexpr const char* kTag = "candump_srv";

struct ClientContext {
  CandumpTcpServer* server;
  int sock;
  int slot;  // index into client_queues_
};
}  // namespace

CandumpTcpServer::CandumpTcpServer(TwaiReceiver* receiver,
                                   TwaiTransmitter* transmitter,
                                   const CandumpTcpServerConfig& config)
    : receiver_(receiver), transmitter_(transmitter), config_(config) {
  clients_mutex_ = xSemaphoreCreateMutex();
}

CandumpTcpServer::~CandumpTcpServer() {
  stop();
  if (clients_mutex_) vSemaphoreDelete(clients_mutex_);
}

void CandumpTcpServer::on_frame(const TwaiMessage& msg) {
  if (xSemaphoreTake(clients_mutex_, pdMS_TO_TICKS(5)) != pdTRUE) return;
  for (int i = 0; i < kMaxClients; i++) {
    if (client_queues_[i]) {
      // Non-blocking — drop if client can't keep up.
      xQueueSend(client_queues_[i], &msg, 0);
    }
  }
  xSemaphoreGive(clients_mutex_);
}

void CandumpTcpServer::start() {
  if (running_.exchange(true)) return;

  // Subscribe to TwaiReceiver's output.
  receiver_->attach([this]() { this->on_frame(receiver_->get()); });

  xTaskCreate(&CandumpTcpServer::server_task, "candump_srv", 4096,
              this, 3, &server_task_);
  ESP_LOGI(kTag, "Candump TCP server starting on port %u", config_.port);
}

void CandumpTcpServer::stop() {
  if (!running_.exchange(false)) return;
  if (server_task_) {
    vTaskDelay(pdMS_TO_TICKS(200));
    server_task_ = nullptr;
  }
}

void CandumpTcpServer::server_task(void* arg) {
  auto* self = static_cast<CandumpTcpServer*>(arg);

  int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_sock < 0) {
    ESP_LOGE(kTag, "socket() failed: %d", errno);
    vTaskDelete(nullptr);
    return;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(self->config_.port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    ESP_LOGE(kTag, "bind() failed: %d", errno);
    close(listen_sock);
    vTaskDelete(nullptr);
    return;
  }

  if (listen(listen_sock, self->config_.max_clients) != 0) {
    ESP_LOGE(kTag, "listen() failed: %d", errno);
    close(listen_sock);
    vTaskDelete(nullptr);
    return;
  }

  ESP_LOGI(kTag, "Listening on port %u", self->config_.port);

  while (self->running_.load()) {
    // Accept with a timeout so we can check running_.
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(listen_sock, &fds);

    int sel = select(listen_sock + 1, &fds, nullptr, nullptr, &tv);
    if (sel <= 0) continue;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_sock =
        accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock < 0) continue;

    // Find a free slot.
    int slot = -1;
    if (xSemaphoreTake(self->clients_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
      for (int i = 0; i < kMaxClients; i++) {
        if (self->client_queues_[i] == nullptr) {
          self->client_queues_[i] =
              xQueueCreate(128, sizeof(TwaiMessage));
          slot = i;
          break;
        }
      }
      xSemaphoreGive(self->clients_mutex_);
    }

    if (slot < 0) {
      ESP_LOGW(kTag, "Max clients reached, rejecting connection");
      close(client_sock);
      continue;
    }

    char ip_str[16];
    inet_ntoa_r(client_addr.sin_addr, ip_str, sizeof(ip_str));
    ESP_LOGI(kTag, "Client connected from %s (slot %d)", ip_str, slot);
    self->connected_clients_.fetch_add(1, std::memory_order_relaxed);

    auto* ctx = new ClientContext{self, client_sock, slot};
    xTaskCreate(&CandumpTcpServer::client_task, "candump_cli", 4096,
                ctx, 3, nullptr);
  }

  close(listen_sock);
  vTaskDelete(nullptr);
}

void CandumpTcpServer::client_task(void* arg) {
  auto* ctx = static_cast<ClientContext*>(arg);
  auto* self = ctx->server;
  int sock = ctx->sock;
  int slot = ctx->slot;
  QueueHandle_t queue = self->client_queues_[slot];

  // Set socket to non-blocking for the TX side so we can interleave
  // reading inbound lines (RX from client → TX to CAN bus).
  struct timeval tv = {.tv_sec = 0, .tv_usec = 50000};  // 50ms
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  char line_buf[128];
  int line_pos = 0;
  char encode_buf[128];

  while (self->running_.load()) {
    // 1. Drain queued frames → send candump lines to client.
    TwaiMessage msg;
    while (xQueueReceive(queue, &msg, 0) == pdTRUE) {
      int n = candump_encode(msg, self->config_.interface_name,
                             encode_buf, sizeof(encode_buf));
      if (n > 0) {
        int sent = send(sock, encode_buf, n, MSG_NOSIGNAL);
        if (sent < 0) goto disconnect;
      }
    }

    // 2. Read inbound data from client (non-blocking, 50ms timeout).
    char recv_buf[128];
    int recv_len = recv(sock, recv_buf, sizeof(recv_buf), 0);
    if (recv_len > 0) {
      // Accumulate into line_buf, parse complete lines.
      for (int i = 0; i < recv_len; i++) {
        if (recv_buf[i] == '\n' || line_pos >= (int)sizeof(line_buf) - 1) {
          line_buf[line_pos] = '\0';
          TwaiMessage tx_msg;
          if (candump_decode(line_buf, &tx_msg) && self->transmitter_) {
            self->transmitter_->set(tx_msg);
          }
          line_pos = 0;
        } else {
          line_buf[line_pos++] = recv_buf[i];
        }
      }
    } else if (recv_len == 0) {
      // Client disconnected cleanly.
      goto disconnect;
    }
    // recv_len < 0 with EAGAIN/EWOULDBLOCK is normal (timeout).

    // Brief yield if no data in either direction.
    vTaskDelay(pdMS_TO_TICKS(1));
  }

disconnect:
  ESP_LOGI(kTag, "Client disconnected (slot %d)", slot);
  close(sock);

  // Free the per-client queue.
  if (xSemaphoreTake(self->clients_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (self->client_queues_[slot]) {
      vQueueDelete(self->client_queues_[slot]);
      self->client_queues_[slot] = nullptr;
    }
    xSemaphoreGive(self->clients_mutex_);
  }
  self->connected_clients_.fetch_sub(1, std::memory_order_relaxed);

  delete ctx;
  vTaskDelete(nullptr);
}

}  // namespace sensesp
