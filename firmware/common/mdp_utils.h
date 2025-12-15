#ifndef MDP_UTILS_H
#define MDP_UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Build a frame: COBS(payload + CRC16_LE) + 0x00
// Returns total frame length (including delimiter) or 0 on error.
size_t mdp_build_frame(const uint8_t* payload, uint16_t payload_len,
                       uint8_t* frame_buf, size_t frame_buf_size);

// Decode and validate a frame. Accepts either (encoded + 0x00) or (encoded only).
// Returns payload length (without CRC) or 0 on error.
size_t mdp_decode_frame(const uint8_t* frame, size_t frame_len,
                        uint8_t* payload_buf, size_t payload_buf_size);

#ifdef __cplusplus
}
#endif

#endif