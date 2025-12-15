#include "mdp_framing.h"

size_t cobsEncode(const uint8_t* input, size_t length, uint8_t* output) {
  size_t read_index = 0;
  size_t write_index = 1;
  size_t code_index = 0;
  uint8_t code = 1;

  while (read_index < length) {
    if (input[read_index] == 0) {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      output[write_index++] = input[read_index++];
      code++;
      if (code == 0xFF) {
        output[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }

  output[code_index] = code;
  return write_index;
}

bool cobsDecode(const uint8_t* input, size_t length, uint8_t* output, size_t* outLen) {
  size_t read_index = 0;
  size_t write_index = 0;

  while (read_index < length) {
    uint8_t code = input[read_index];
    if (code == 0) return false;
    if (read_index + code > length + 1) return false;

    read_index++;
    for (uint8_t i = 1; i < code; i++) {
      output[write_index++] = input[read_index++];
    }
    if (code != 0xFF && read_index < length) {
      output[write_index++] = 0;
    }
  }

  *outLen = write_index;
  return true;
}

uint16_t crc16_ccitt_false(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}