#ifndef SENSESP_N2K_GATEWAY_TWAI_RECEIVER_H_
#define SENSESP_N2K_GATEWAY_TWAI_RECEIVER_H_

#include <atomic>

#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sensesp/system/valueproducer.h"
#include "sensesp_n2k_gateway/twai_message.h"

namespace sensesp {

struct TwaiReceiverConfig {
  gpio_num_t tx_pin = GPIO_NUM_4;
  gpio_num_t rx_pin = GPIO_NUM_5;
  uint32_t bitrate = 250000;  // NMEA 2000 standard
  size_t rx_queue_depth = 64;
};

/// Reads CAN frames from the TWAI peripheral and emits them as
/// TwaiMessage values. Runs a dedicated FreeRTOS task for RX.
class TwaiReceiver : public ValueProducer<TwaiMessage> {
 public:
  explicit TwaiReceiver(const TwaiReceiverConfig& config = {});
  ~TwaiReceiver();

  void start();
  void stop();

  uint32_t rx_count() const { return rx_count_; }
  uint32_t bus_off_count() const { return bus_off_count_; }

 private:
  static void rx_task(void* arg);

  TwaiReceiverConfig config_;
  TaskHandle_t rx_task_ = nullptr;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> rx_count_{0};
  std::atomic<uint32_t> bus_off_count_{0};
};

}  // namespace sensesp

#endif  // SENSESP_N2K_GATEWAY_TWAI_RECEIVER_H_
