#ifndef MDP_FRAMING_H
#define MDP_FRAMING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// COBS encode/decode
size_t cobsEncode(const uint8_t* input, size_t length, uint8_t* output);
bool cobsDecode(const uint8_t* input, size_t length, uint8_t* output, size_t* outLen);

// CRC16-CCITT-FALSE
uint16_t crc16_ccitt_false(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif