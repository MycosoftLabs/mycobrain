#include "myco_cbor.h"
#include <string.h>

static int put_u8(myco_cbor_t *w, uint8_t v) {
    if (w->err) return w->err;
    if (w->len + 1 > w->cap) return (w->err = -1);
    w->buf[w->len++] = v;
    return 0;
}

static int put_bytes(myco_cbor_t *w, const uint8_t *p, size_t n) {
    if (w->err) return w->err;
    if (w->len + n > w->cap) return (w->err = -1);
    memcpy(w->buf + w->len, p, n);
    w->len += n;
    return 0;
}

static int put_type_val(myco_cbor_t *w, uint8_t major, uint64_t val) {
    // major: 0..7
    if (val <= 23) {
        return put_u8(w, (uint8_t)((major << 5) | (uint8_t)val));
    } else if (val <= 0xFF) {
        int rc = put_u8(w, (uint8_t)((major << 5) | 24));
        if (rc) return rc;
        return put_u8(w, (uint8_t)val);
    } else if (val <= 0xFFFF) {
        int rc = put_u8(w, (uint8_t)((major << 5) | 25));
        if (rc) return rc;
        uint8_t b[2] = { (uint8_t)(val >> 8), (uint8_t)(val) };
        return put_bytes(w, b, 2);
    } else if (val <= 0xFFFFFFFFu) {
        int rc = put_u8(w, (uint8_t)((major << 5) | 26));
        if (rc) return rc;
        uint8_t b[4] = {
            (uint8_t)(val >> 24), (uint8_t)(val >> 16),
            (uint8_t)(val >> 8), (uint8_t)(val)
        };
        return put_bytes(w, b, 4);
    } else {
        int rc = put_u8(w, (uint8_t)((major << 5) | 27));
        if (rc) return rc;
        uint8_t b[8] = {
            (uint8_t)(val >> 56), (uint8_t)(val >> 48),
            (uint8_t)(val >> 40), (uint8_t)(val >> 32),
            (uint8_t)(val >> 24), (uint8_t)(val >> 16),
            (uint8_t)(val >> 8), (uint8_t)(val)
        };
        return put_bytes(w, b, 8);
    }
}

void myco_cbor_init(myco_cbor_t *w, uint8_t *buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
    w->err = 0;
}

int myco_cbor_put_uint(myco_cbor_t *w, uint64_t v) { return put_type_val(w, 0, v); }

int myco_cbor_put_int(myco_cbor_t *w, int64_t v) {
    if (v >= 0) return put_type_val(w, 0, (uint64_t)v);
    // CBOR negative integer N is encoded as -1 - n
    uint64_t n = (uint64_t)(-1 - v);
    return put_type_val(w, 1, n);
}

int myco_cbor_put_bstr(myco_cbor_t *w, const uint8_t *p, size_t n) {
    int rc = put_type_val(w, 2, n);
    if (rc) return rc;
    return put_bytes(w, p, n);
}

int myco_cbor_put_tstr(myco_cbor_t *w, const char *s) {
    size_t n = strlen(s);
    int rc = put_type_val(w, 3, n);
    if (rc) return rc;
    return put_bytes(w, (const uint8_t*)s, n);
}

int myco_cbor_put_array(myco_cbor_t *w, size_t n) { return put_type_val(w, 4, n); }
int myco_cbor_put_map(myco_cbor_t *w, size_t n) { return put_type_val(w, 5, n); }
