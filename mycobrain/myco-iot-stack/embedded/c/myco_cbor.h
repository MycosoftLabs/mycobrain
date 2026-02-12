#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t len;
    int err; // 0=ok, <0 error
} myco_cbor_t;

void myco_cbor_init(myco_cbor_t *w, uint8_t *buf, size_t cap);

int myco_cbor_put_uint(myco_cbor_t *w, uint64_t v);
int myco_cbor_put_int(myco_cbor_t *w, int64_t v);
int myco_cbor_put_bstr(myco_cbor_t *w, const uint8_t *p, size_t n);
int myco_cbor_put_tstr(myco_cbor_t *w, const char *s);

// definite-length containers
int myco_cbor_put_array(myco_cbor_t *w, size_t n);
int myco_cbor_put_map(myco_cbor_t *w, size_t n);

// utility
static inline size_t myco_cbor_len(const myco_cbor_t *w) { return w->len; }
static inline int myco_cbor_err(const myco_cbor_t *w) { return w->err; }

#ifdef __cplusplus
}
#endif
