#include "myco_envelope.h"
#include "myco_cbor.h"
#include <string.h>

// Top-level keys (0..11)
#define K_V 0
#define K_D 1
#define K_P 2
#define K_M 3
#define K_T 4
#define K_S 5
#define K_N 6
#define K_G 7
#define K_R 8
#define K_X 9
#define K_H 10
#define K_Z 11

// Geo keys
#define G_LAT 0
#define G_LON 1
#define G_ACC 2

// Reading keys
#define R_ID 0
#define R_VI 1
#define R_VS 2
#define R_U  3
#define R_Q  4

static void encode_geo(myco_cbor_t *w, const myco_geo_t *geo) {
    // map size = 3
    myco_cbor_put_uint(w, K_G);
    myco_cbor_put_map(w, 3);
    myco_cbor_put_uint(w, G_LAT); myco_cbor_put_int(w, geo->lat_e7);
    myco_cbor_put_uint(w, G_LON); myco_cbor_put_int(w, geo->lon_e7);
    myco_cbor_put_uint(w, G_ACC); myco_cbor_put_uint(w, geo->acc_m);
}

static void encode_readings(myco_cbor_t *w, const myco_reading_t *rs, size_t n) {
    myco_cbor_put_uint(w, K_R);
    myco_cbor_put_array(w, n);
    for (size_t i = 0; i < n; i++) {
        // map size = 5
        myco_cbor_put_map(w, 5);
        myco_cbor_put_uint(w, R_ID); myco_cbor_put_uint(w, rs[i].sid);
        myco_cbor_put_uint(w, R_VI); myco_cbor_put_int(w, rs[i].vi);
        myco_cbor_put_uint(w, R_VS); myco_cbor_put_uint(w, rs[i].vs);
        myco_cbor_put_uint(w, R_U);  myco_cbor_put_uint(w, rs[i].unit);
        myco_cbor_put_uint(w, R_Q);  myco_cbor_put_uint(w, rs[i].quality);
    }
}

static int encode_unsigned(
    uint8_t *buf, size_t cap, size_t *len,
    const char *device_id, uint8_t proto, const uint8_t msg_id_16[16],
    int64_t epoch_ms, uint32_t seq, uint64_t mono_ms,
    const myco_geo_t *geo,
    const myco_reading_t *readings, size_t n_readings
) {
    myco_cbor_t w;
    myco_cbor_init(&w, buf, cap);

    // map entries:
    // v,d,p,m,t,s,n,(g?),r,(x?)
    size_t map_n = 8; // v,d,p,m,t,s,n,r
    if (geo && geo->has_fix) map_n += 1;

    myco_cbor_put_map(&w, map_n);

    // keys MUST be added in ascending order for determinism.

    myco_cbor_put_uint(&w, K_V); myco_cbor_put_uint(&w, 1);
    myco_cbor_put_uint(&w, K_D); myco_cbor_put_tstr(&w, device_id);
    myco_cbor_put_uint(&w, K_P); myco_cbor_put_uint(&w, proto);
    myco_cbor_put_uint(&w, K_M); myco_cbor_put_bstr(&w, msg_id_16, 16);
    myco_cbor_put_uint(&w, K_T); myco_cbor_put_int(&w, epoch_ms);
    myco_cbor_put_uint(&w, K_S); myco_cbor_put_uint(&w, seq);
    myco_cbor_put_uint(&w, K_N); myco_cbor_put_uint(&w, mono_ms);

    if (geo && geo->has_fix) {
        encode_geo(&w, geo);
    }

    encode_readings(&w, readings, n_readings);

    if (myco_cbor_err(&w)) return -1;
    *len = myco_cbor_len(&w);
    return 0;
}

int myco_build_envelope_cbor(
    uint8_t *out_buf, size_t out_cap, size_t *out_len,
    const char *device_id,
    uint8_t proto,
    const uint8_t msg_id_16[16],
    int64_t epoch_ms,
    uint32_t seq,
    uint64_t mono_ms,
    const myco_geo_t *geo,
    const myco_reading_t *readings,
    size_t n_readings,
    const uint8_t sk64[64]
) {
    // 1) Encode unsigned envelope into temp buffer
    //    (worst-case: use out_cap, but a temp buffer is safer for partial builds).
    if (out_cap < 128) return -2;

    // For simplicity in embedded, we reuse out_buf as temp.
    // If you need to keep out_buf clean, allocate a separate temp buffer.
    size_t unsigned_len = 0;
    int rc = encode_unsigned(out_buf, out_cap, &unsigned_len,
                             device_id, proto, msg_id_16,
                             epoch_ms, seq, mono_ms,
                             geo, readings, n_readings);
    if (rc) return rc;

    // 2) Hash unsigned bytes (BLAKE2b-256)
    uint8_t h[32];
    myco_hash256(h, out_buf, unsigned_len);

    // 3) Sign ("MYCO1" || h)
    uint8_t msg_to_sign[5 + 32];
    memcpy(msg_to_sign, "MYCO1", 5);
    memcpy(msg_to_sign + 5, h, 32);

    uint8_t sig[64];
    myco_ed25519_sign(sig, sk64, msg_to_sign, sizeof(msg_to_sign));

    // 4) Encode full signed envelope
    // Re-encode from scratch to include h and z.
    uint8_t *final_buf = out_buf;
    myco_cbor_t w;
    myco_cbor_init(&w, final_buf, out_cap);

    size_t map_n = 10; // v,d,p,m,t,s,n,r,h,z
    if (geo && geo->has_fix) map_n += 1; // g
    myco_cbor_put_map(&w, map_n);

    myco_cbor_put_uint(&w, K_V); myco_cbor_put_uint(&w, 1);
    myco_cbor_put_uint(&w, K_D); myco_cbor_put_tstr(&w, device_id);
    myco_cbor_put_uint(&w, K_P); myco_cbor_put_uint(&w, proto);
    myco_cbor_put_uint(&w, K_M); myco_cbor_put_bstr(&w, msg_id_16, 16);
    myco_cbor_put_uint(&w, K_T); myco_cbor_put_int(&w, epoch_ms);
    myco_cbor_put_uint(&w, K_S); myco_cbor_put_uint(&w, seq);
    myco_cbor_put_uint(&w, K_N); myco_cbor_put_uint(&w, mono_ms);

    if (geo && geo->has_fix) {
        encode_geo(&w, geo);
    }

    encode_readings(&w, readings, n_readings);

    myco_cbor_put_uint(&w, K_H); myco_cbor_put_bstr(&w, h, 32);
    myco_cbor_put_uint(&w, K_Z); myco_cbor_put_bstr(&w, sig, 64);

    if (myco_cbor_err(&w)) return -3;
    *out_len = myco_cbor_len(&w);
    return 0;
}
