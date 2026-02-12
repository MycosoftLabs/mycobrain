#include "myco_envelope.h"
#include <stdio.h>
#include <string.h>

// --- CRYPTO HOOKS (Monocypher example) ---
// Uncomment if you integrate Monocypher.
//
// #include "monocypher.h"
// #include "optional/monocypher-ed25519.h"
//
// void myco_hash256(uint8_t out32[32], const uint8_t *msg, size_t msg_len) {
//     crypto_blake2b(out32, 32, msg, msg_len);
// }
// void myco_ed25519_sign(uint8_t sig64[64], const uint8_t sk64[64], const uint8_t *msg, size_t msg_len) {
//     crypto_ed25519_sign(sig64, sk64, msg, msg_len);
// }
// int myco_ed25519_verify(const uint8_t pk32[32], const uint8_t *msg, size_t msg_len, const uint8_t sig64[64]) {
//     return crypto_ed25519_check(sig64, pk32, msg, msg_len) == 0 ? 1 : 0;
// }

// Dummy stubs so this compiles as-is.
// Replace with real crypto.
void myco_hash256(uint8_t out32[32], const uint8_t *msg, size_t msg_len) {
    (void)msg; (void)msg_len;
    memset(out32, 0xAA, 32);
}
void myco_ed25519_sign(uint8_t sig64[64], const uint8_t sk64[64], const uint8_t *msg, size_t msg_len) {
    (void)sk64; (void)msg; (void)msg_len;
    memset(sig64, 0xBB, 64);
}
int myco_ed25519_verify(const uint8_t pk32[32], const uint8_t *msg, size_t msg_len, const uint8_t sig64[64]) {
    (void)pk32; (void)msg; (void)msg_len; (void)sig64;
    return 1;
}

int main(void) {
    uint8_t msg_id[16] = {0};
    for (int i=0;i<16;i++) msg_id[i] = (uint8_t)i;

    myco_reading_t readings[2] = {
        {.sid=1, .vi=217, .vs=1, .unit=1, .quality=0}, // 21.7 C
        {.sid=4, .vi=12,  .vs=2, .unit=3, .quality=0}, // 0.12 ppm
    };

    myco_geo_t geo = {.has_fix=1, .lat_e7=327157000, .lon_e7=-1171611000, .acc_m=5};

    uint8_t sk64[64] = {0}; // replace with provisioned key
    uint8_t out[256];
    size_t out_len = 0;

    int rc = myco_build_envelope_cbor(out, sizeof(out), &out_len,
                                      "myco-node-001",
                                      MYCO_PROTO_MQTT,
                                      msg_id,
                                      1760000000000LL,
                                      42,
                                      123456,
                                      &geo,
                                      readings, 2,
                                      sk64);
    if (rc) {
        printf("build failed: %d\n", rc);
        return 1;
    }

    printf("CBOR bytes (%zu):\n", out_len);
    for (size_t i=0;i<out_len;i++) printf("%02X", out[i]);
    printf("\n");
    return 0;
}
