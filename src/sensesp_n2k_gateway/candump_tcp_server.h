#ifndef SENSESP_N2K_GATEWAY_CANDUMP_TCP_SERVER_H_
#define SENSESP_N2K_GATEWAY_CANDUMP_TCP_SERVER_H_

#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "sensesp_n2k_gateway/twai_receiver.h"
#include "sensesp_n2k_gateway/twai_transmitter.h"

namespace sensesp {

struct CandumpTcpServerConfig {
  uint16_t port = 2599;
  uint8_t max_clients = 3;
  const char* interface_name = "can0";
};

/// TCP server that streams candump ASCII to connected clients and
/// accepts inbound candump lines for transmission on the CAN bus.
class CandumpTcpServer {
 public:
  CandumpTcpServer(TwaiReceiver* receiver,
                   TwaiTransmitter* transmitter,
                   const CandumpTcpServerConfig& config = {});
  ~CandumpTcpServer();

  void start();
  void stop();

  uint32_t connected_clients() const { return connected_clients_; }

 private:
  static void server_task(void* arg);
  static void client_task(void* arg);

  // Called by TwaiReceiver's emit() — fans out to per-client queues.
  void on_frame(const TwaiMessage& msg);

  TwaiReceiver* receiver_;
  TwaiTransmitter* transmitter_;
  CandumpTcpServerConfig config_;

  TaskHandle_t server_task_ = nullptr;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> connected_clients_{0};

  // Per-client queues for fan-out.
  static constexpr int kMaxClients = 8;
  QueueHandle_t client_queues_[kMaxClients] = {};
  SemaphoreHandle_t clients_mutex_ = nullptr;
};

}  // namespace sensesp

#endif  // SENSESP_N2K_GATEWAY_CANDUMP_TCP_SERVER_H_
