#include "sensesp_n2k_gateway/candump_format.h"

#include <cstdio>
#include <cstring>

namespace sensesp {

int candump_encode(const TwaiMessage& msg, const char* iface,
                   char* buf, size_t buf_len) {
  // Format: (seconds.microseconds) iface CANID#HEXDATA\n
  int64_t sec = msg.timestamp_us / 1000000;
  int64_t usec = msg.timestamp_us % 1000000;

  // Build the hex data string
  char data_hex[17];  // max 8 bytes = 16 hex chars + null
  int data_len = msg.frame.data_length_code;
  if (data_len > 8) data_len = 8;
  for (int i = 0; i < data_len; i++) {
    snprintf(data_hex + i * 2, 3, "%02X", msg.frame.data[i]);
  }
  data_hex[data_len * 2] = '\0';

  // CAN ID — always 8 hex digits for extended frames (NMEA 2000)
  int n = snprintf(buf, buf_len, "(%lld.%06lld) %s %08X#%s\n",
                   (long long)sec, (long long)usec,
                   iface, (unsigned)msg.frame.identifier, data_hex);
  if (n < 0 || (size_t)n >= buf_len) return -1;
  return n;
}

bool candump_decode(const char* line, TwaiMessage* out) {
  // Parse: (seconds.microseconds) iface CANID#HEXDATA
  if (!line || !out) return false;

  // Skip leading whitespace
  while (*line == ' ' || *line == '\t') line++;

  // Parse timestamp: (sec.usec)
  int64_t sec = 0, usec = 0;
  if (*line == '(') {
    line++;
    char* end;
    sec = strtoll(line, &end, 10);
    if (*end == '.') {
      end++;
      usec = strtoll(end, &end, 10);
    }
    if (*end == ')') end++;
    line = end;
  }
  out->timestamp_us = sec * 1000000 + usec;

  // Skip whitespace + interface name
  while (*line == ' ') line++;
  while (*line && *line != ' ') line++;  // skip iface
  while (*line == ' ') line++;

  // Parse CAN ID (hex, up to 8 digits)
  char* hash;
  unsigned long can_id = strtoul(line, &hash, 16);
  if (*hash != '#') return false;
  hash++;

  out->frame.identifier = can_id;
  out->frame.extd = 1;  // NMEA 2000 always extended
  out->frame.rtr = 0;
  out->frame.ss = 0;
  out->frame.self = 0;
  out->frame.dlc_non_comp = 0;

  // Parse hex data bytes
  int data_len = 0;
  while (*hash && *hash != '\n' && *hash != '\r' && data_len < 8) {
    unsigned int byte;
    if (sscanf(hash, "%2x", &byte) != 1) break;
    out->frame.data[data_len++] = (uint8_t)byte;
    hash += 2;
  }
  out->frame.data_length_code = data_len;

  return data_len > 0;
}

}  // namespace sensesp
