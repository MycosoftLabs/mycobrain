#pragma once

#include <Arduino.h>
#include <stdint.h>

// MDP v1 constants
static const uint16_t MDP_MAGIC = 0xA15A;
static const uint8_t MDP_VERSION = 0x01;

enum MdpMsgType : uint8_t {
  MDP_TELEMETRY = 0x01,
  MDP_COMMAND = 0x02,
  MDP_ACK = 0x03,
  MDP_EVENT = 0x05,
  MDP_HELLO = 0x06
};

enum MdpEndpoint : uint8_t {
  EP_SIDE_A = 0xA1,
  EP_SIDE_B = 0xB1,
  EP_GATEWAY = 0xC0,
  EP_BCAST = 0xFF
};

enum MdpFlags : uint8_t {
  ACK_REQUESTED = 0x01,
  IS_ACK = 0x02,
  IS_NACK = 0x04
};

#pragma pack(push, 1)
struct MdpHeader {
  uint16_t magic;
  uint8_t version;
  uint8_t msg_type;
  uint32_t seq;
  uint32_t ack;
  uint8_t flags;
  uint8_t src;
  uint8_t dst;
  uint8_t rsv;
};
#pragma pack(pop)

static inline uint16_t mdp_crc16_ccitt_false(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000) crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
      else crc = static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

// Returns encoded length, 0 on overflow.
static inline size_t mdp_cobs_encode(const uint8_t* input, size_t len, uint8_t* out, size_t out_cap) {
  if (out_cap < len + 2) return 0;
  size_t read_index = 0;
  size_t write_index = 1;
  size_t code_index = 0;
  uint8_t code = 1;

  while (read_index < len) {
    if (input[read_index] == 0) {
      out[code_index] = code;
      code = 1;
      code_index = write_index++;
      if (write_index >= out_cap) return 0;
      ++read_index;
    } else {
      out[write_index++] = input[read_index++];
      ++code;
      if (code == 0xFF) {
        out[code_index] = code;
        code = 1;
        code_index = write_index++;
        if (write_index >= out_cap) return 0;
      }
      if (write_index > out_cap) return 0;
    }
  }

  out[code_index] = code;
  return write_index;
}

// Returns decoded length, 0 on malformed frame.
static inline size_t mdp_cobs_decode(const uint8_t* input, size_t len, uint8_t* out, size_t out_cap) {
  if (len == 0) return 0;
  size_t read_index = 0;
  size_t write_index = 0;

  while (read_index < len) {
    uint8_t code = input[read_index];
    if (code == 0) return 0;
    ++read_index;

    for (uint8_t i = 1; i < code; ++i) {
      if (read_index >= len || write_index >= out_cap) return 0;
      out[write_index++] = input[read_index++];
    }

    if (code != 0xFF && read_index < len) {
      if (write_index >= out_cap) return 0;
      out[write_index++] = 0;
    }
  }

  return write_index;
}
