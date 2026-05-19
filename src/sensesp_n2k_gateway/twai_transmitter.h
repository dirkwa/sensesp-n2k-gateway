#ifndef SENSESP_N2K_GATEWAY_TWAI_TRANSMITTER_H_
#define SENSESP_N2K_GATEWAY_TWAI_TRANSMITTER_H_

#include <atomic>
#include <cstdint>

#include "driver/twai.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "sensesp/system/valueconsumer.h"
#include "sensesp_n2k_gateway/twai_message.h"

namespace sensesp {

/// Accepts TwaiMessage values and transmits them on the CAN bus.
/// Runs a dedicated FreeRTOS task that drains a TX queue.
class TwaiTransmitter : public ValueConsumer<TwaiMessage> {
 public:
  explicit TwaiTransmitter(size_t tx_queue_depth = 32);
  ~TwaiTransmitter();

  void start();
  void stop();

  /// ValueConsumer interface — queues a frame for transmission.
  void set(const TwaiMessage& msg) override;

  /// True if we have transmitted at least one frame since boot.
  bool ever_transmitted() const {
    return last_tx_us_.load(std::memory_order_relaxed) != 0;
  }

  /// Seconds since the last transmitted frame. INT64_MAX if none yet.
  /// Replaces the old uint32_t tx_count_ which would overflow at ~20
  /// days of busy traffic.
  int64_t seconds_since_last_tx() const {
    int64_t last = last_tx_us_.load(std::memory_order_relaxed);
    if (last == 0) return INT64_MAX;
    return (esp_timer_get_time() - last) / 1000000;
  }

  uint32_t tx_fail_count() const { return tx_fail_count_; }

 private:
  static void tx_task(void* arg);

  QueueHandle_t tx_queue_ = nullptr;
  TaskHandle_t tx_task_ = nullptr;
  std::atomic<bool> running_{false};
  // Microseconds-since-boot of last successful TX; 0 = nothing yet.
  std::atomic<int64_t> last_tx_us_{0};
  std::atomic<uint32_t> tx_fail_count_{0};
};

}  // namespace sensesp

#endif  // SENSESP_N2K_GATEWAY_TWAI_TRANSMITTER_H_
