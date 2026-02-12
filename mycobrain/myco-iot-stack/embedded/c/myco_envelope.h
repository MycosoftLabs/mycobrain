#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Protocol enum (match spec)
enum {
    MYCO_PROTO_LORAWAN = 1,
    MYCO_PROTO_MQTT    = 2,
    MYCO_PROTO_BLE     = 3,
    MYCO_PROTO_LTE     = 4,
    MYCO_PROTO_OTHER   = 5
};

typedef struct {
    // recommended: numeric sensor id + unit id
    uint16_t sid;
    int32_t  vi;      // scaled integer
    uint8_t  vs;      // decimal places (0..9)
    uint16_t unit;
    uint8_t  quality; // 0=ok
} myco_reading_t;

typedef struct {
    int has_fix;
    int32_t lat_e7;
    int32_t lon_e7;
    uint16_t acc_m;
} myco_geo_t;

// Crypto hooks (implement with Monocypher or any other library)
//
// - hash256(out32, msg, msg_len): BLAKE2b-256
// - ed25519_sign(sig64, sk64, msg, msg_len)
// - ed25519_verify(pk32, msg, msg_len, sig64) -> 1=ok, 0=bad
//
void myco_hash256(uint8_t out32[32], const uint8_t *msg, size_t msg_len);
void myco_ed25519_sign(uint8_t sig64[64], const uint8_t sk64[64], const uint8_t *msg, size_t msg_len);
int  myco_ed25519_verify(const uint8_t pk32[32], const uint8_t *msg, size_t msg_len, const uint8_t sig64[64]);

// Build + sign envelope.
// out_buf receives *signed envelope* CBOR bytes.
int myco_build_envelope_cbor(
    uint8_t       *out_buf, size_t out_cap, size_t *out_len,
    const char    *device_id,
    uint8_t        proto,
    const uint8_t  msg_id_16[16],
    int64_t        epoch_ms,
    uint32_t       seq,
    uint64_t       mono_ms,
    const myco_geo_t *geo,          // can be NULL or has_fix=0
    const myco_reading_t *readings,
    size_t        n_readings,
    const uint8_t  sk64[64]         // Ed25519 secret key
);

#ifdef __cplusplus
}
#endif
