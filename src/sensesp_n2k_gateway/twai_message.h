#ifndef SENSESP_N2K_GATEWAY_TWAI_MESSAGE_H_
#define SENSESP_N2K_GATEWAY_TWAI_MESSAGE_H_

#include "driver/twai.h"

namespace sensesp {

/// Thin wrapper around twai_message_t with a microsecond timestamp.
struct TwaiMessage {
  twai_message_t frame;
  int64_t timestamp_us;  // esp_timer_get_time()
};

}  // namespace sensesp

#endif  // SENSESP_N2K_GATEWAY_TWAI_MESSAGE_H_
