#ifndef SENSESP_N2K_GATEWAY_CANDUMP_FORMAT_H_
#define SENSESP_N2K_GATEWAY_CANDUMP_FORMAT_H_

#include "sensesp_n2k_gateway/twai_message.h"

namespace sensesp {

/// Encode a TwaiMessage to candump ASCII format:
///   (1234567890.123456) vcan0 09F10203#FF00FF00FF00FF00\n
/// Returns number of bytes written (excluding null terminator),
/// or -1 if buf is too small.
int candump_encode(const TwaiMessage& msg, const char* iface,
                   char* buf, size_t buf_len);

/// Decode a candump ASCII line into a TwaiMessage.
/// Returns true on success, false on parse error.
bool candump_decode(const char* line, TwaiMessage* out);

}  // namespace sensesp

#endif  // SENSESP_N2K_GATEWAY_CANDUMP_FORMAT_H_
