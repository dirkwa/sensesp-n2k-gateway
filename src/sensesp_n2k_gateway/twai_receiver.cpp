#include "sensesp_n2k_gateway/twai_receiver.h"

#include "esp_log.h"
#include "esp_timer.h"

namespace sensesp {

namespace {
constexpr const char* kTag = "twai_rx";
}

TwaiReceiver::TwaiReceiver(const TwaiReceiverConfig& config)
    : config_(config) {}

TwaiReceiver::~TwaiReceiver() { stop(); }

void TwaiReceiver::start() {
  if (running_.exchange(true)) return;

  // Configure and install the TWAI driver.
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT(config_.tx_pin, config_.rx_pin,
                                  TWAI_MODE_NORMAL);
  g_config.rx_queue_len = config_.rx_queue_depth;
  g_config.tx_queue_len = 32;

  twai_timing_config_t t_config;
  if (config_.bitrate == 250000) {
    t_config = TWAI_TIMING_CONFIG_250KBITS();
  } else if (config_.bitrate == 500000) {
    t_config = TWAI_TIMING_CONFIG_500KBITS();
  } else {
    t_config = TWAI_TIMING_CONFIG_250KBITS();
  }

  // Accept all frames (no filter).
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "twai_driver_install failed: %s", esp_err_to_name(err));
    running_.store(false);
    return;
  }

  err = twai_start();
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "twai_start failed: %s", esp_err_to_name(err));
    twai_driver_uninstall();
    running_.store(false);
    return;
  }

  ESP_LOGI(kTag, "TWAI started: TX=%d RX=%d %ukbps",
           config_.tx_pin, config_.rx_pin, config_.bitrate / 1000);

  xTaskCreate(&TwaiReceiver::rx_task, "twai_rx", 4096, this, 5, &rx_task_);
}

void TwaiReceiver::stop() {
  if (!running_.exchange(false)) return;
  if (rx_task_) {
    // The task checks running_ and exits.
    vTaskDelay(pdMS_TO_TICKS(100));
    rx_task_ = nullptr;
  }
  twai_stop();
  twai_driver_uninstall();
  ESP_LOGI(kTag, "TWAI stopped");
}

void TwaiReceiver::rx_task(void* arg) {
  auto* self = static_cast<TwaiReceiver*>(arg);

  while (self->running_.load()) {
    twai_message_t frame;
    esp_err_t err = twai_receive(&frame, pdMS_TO_TICKS(100));

    if (err == ESP_OK) {
      TwaiMessage msg;
      msg.frame = frame;
      msg.timestamp_us = esp_timer_get_time();
      self->rx_count_.fetch_add(1, std::memory_order_relaxed);
      self->emit(msg);
    } else if (err == ESP_ERR_TIMEOUT) {
      // No frame received — check for bus-off state.
      twai_status_info_t status;
      if (twai_get_status_info(&status) == ESP_OK &&
          status.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGW(kTag, "Bus-off detected — initiating recovery");
        self->bus_off_count_.fetch_add(1, std::memory_order_relaxed);
        twai_initiate_recovery();
        vTaskDelay(pdMS_TO_TICKS(500));
      }
    }
  }

  vTaskDelete(nullptr);
}

}  // namespace sensesp
