#include "sensesp_n2k_gateway/twai_transmitter.h"

#include "esp_log.h"

namespace sensesp {

namespace {
constexpr const char* kTag = "twai_tx";
}

TwaiTransmitter::TwaiTransmitter(size_t tx_queue_depth) {
  tx_queue_ = xQueueCreate(tx_queue_depth, sizeof(TwaiMessage));
}

TwaiTransmitter::~TwaiTransmitter() {
  stop();
  if (tx_queue_) vQueueDelete(tx_queue_);
}

void TwaiTransmitter::start() {
  if (running_.exchange(true)) return;
  xTaskCreate(&TwaiTransmitter::tx_task, "twai_tx", 4096, this, 4, &tx_task_);
  ESP_LOGI(kTag, "TWAI transmitter started");
}

void TwaiTransmitter::stop() {
  if (!running_.exchange(false)) return;
  if (tx_task_) {
    vTaskDelay(pdMS_TO_TICKS(100));
    tx_task_ = nullptr;
  }
}

void TwaiTransmitter::set(const TwaiMessage& msg) {
  if (!tx_queue_) return;
  // Non-blocking enqueue — drop if queue is full.
  if (xQueueSend(tx_queue_, &msg, 0) != pdTRUE) {
    tx_fail_count_.fetch_add(1, std::memory_order_relaxed);
  }
}

void TwaiTransmitter::tx_task(void* arg) {
  auto* self = static_cast<TwaiTransmitter*>(arg);

  while (self->running_.load()) {
    TwaiMessage msg;
    if (xQueueReceive(self->tx_queue_, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
      esp_err_t err = twai_transmit(&msg.frame, pdMS_TO_TICKS(50));
      if (err == ESP_OK) {
        self->tx_count_.fetch_add(1, std::memory_order_relaxed);
      } else {
        self->tx_fail_count_.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGD(kTag, "TX failed: %s", esp_err_to_name(err));
      }
    }
  }

  vTaskDelete(nullptr);
}

}  // namespace sensesp
