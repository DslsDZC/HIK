/*
 * HIK BIOS Bootloader - SHA-384 Hash
 */

#ifndef SHA384_H
#define SHA384_H

#include <stdint.h>

/* SHA-384 context */
typedef struct {
    uint64_t state[6];      /* State (6 x 64-bit) */
    uint64_t count[2];      /* Bit count */
    uint8_t  buffer[128];   /* Input buffer */
} sha384_context_t;

/* Initialize SHA-384 context */
void sha384_init(sha384_context_t *ctx);

/* Update SHA-384 with data */
void sha384_update(sha384_context_t *ctx, const uint8_t *data, uint64_t len);

/* Finalize SHA-384 and get hash */
void sha384_final(sha384_context_t *ctx, uint8_t hash[48]);

#endif /* SHA384_H */