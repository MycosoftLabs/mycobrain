#include "mdp_utils.h"
#include "mdp_framing.h"
#include <string.h>

size_t mdp_build_frame(const uint8_t* payload, uint16_t payload_len,
                       uint8_t* frame_buf, size_t frame_buf_size) {
  if (!payload || !frame_buf || payload_len == 0) return 0;

  size_t max_cobs_len = (size_t)payload_len + 2 + (((size_t)payload_len + 2 + 253) / 254) + 1;
  if (max_cobs_len > frame_buf_size) return 0;

  uint8_t raw[1024];
  if ((size_t)payload_len + 2 > sizeof(raw)) return 0;

  memcpy(raw, payload, payload_len);
  uint16_t crc = crc16_ccitt_false(payload, payload_len);
  raw[payload_len] = (uint8_t)(crc & 0xFF);
  raw[payload_len + 1] = (uint8_t)((crc >> 8) & 0xFF);

  size_t enc_len = cobsEncode(raw, (size_t)payload_len + 2, frame_buf);
  if (enc_len + 1 > frame_buf_size) return 0;

  frame_buf[enc_len] = 0x00;
  return enc_len + 1;
}

size_t mdp_decode_frame(const uint8_t* frame, size_t frame_len,
                        uint8_t* payload_buf, size_t payload_buf_size) {
  if (!frame || !payload_buf || frame_len == 0) return 0;

  size_t data_len = frame_len;
  if (frame[frame_len - 1] == 0x00) data_len = frame_len - 1;
  if (data_len == 0) return 0;

  uint8_t decoded[1536];
  if (data_len > sizeof(decoded)) return 0;

  size_t decoded_len = 0;
  if (!cobsDecode(frame, data_len, decoded, &decoded_len)) return 0;
  if (decoded_len < 2) return 0;

  uint16_t recv_crc = (uint16_t)decoded[decoded_len - 2] | ((uint16_t)decoded[decoded_len - 1] << 8);
  uint16_t calc_crc = crc16_ccitt_false(decoded, decoded_len - 2);
  if (recv_crc != calc_crc) return 0;

  size_t payload_len = decoded_len - 2;
  if (payload_len > payload_buf_size) return 0;

  memcpy(payload_buf, decoded, payload_len);
  return payload_len;
}