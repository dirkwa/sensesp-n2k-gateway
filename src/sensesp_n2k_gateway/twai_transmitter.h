#ifndef SENSESP_N2K_GATEWAY_TWAI_TRANSMITTER_H_
#define SENSESP_N2K_GATEWAY_TWAI_TRANSMITTER_H_

#include <atomic>

#include "driver/twai.h"
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

  uint32_t tx_count() const { return tx_count_; }
  uint32_t tx_fail_count() const { return tx_fail_count_; }

 private:
  static void tx_task(void* arg);

  QueueHandle_t tx_queue_ = nullptr;
  TaskHandle_t tx_task_ = nullptr;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> tx_count_{0};
  std::atomic<uint32_t> tx_fail_count_{0};
};

}  // namespace sensesp

#endif  // SENSESP_N2K_GATEWAY_TWAI_TRANSMITTER_H_
