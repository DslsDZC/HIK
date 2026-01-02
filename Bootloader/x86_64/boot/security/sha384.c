/*
 * HIK BIOS Bootloader - SHA-384 Implementation
 */

#include "sha384.h"
#include "../util/util.h"

/* SHA-384 constants */
static const uint64_t K[80] = {
    0x428A2F98D728AE22, 0x7137449123EF65CD, 0xB5C0FBCFEC4D3B2F, 0xE9B5DBA58189DBBC,
    0x3956C25BF348B538, 0x59F111F1B605D019, 0x923F82A4AF194F9B, 0xAB1C5ED5DA6D8118,
    0xD807AA98A3030242, 0x12835B0145706FBE, 0x243185BE4EE4B28C, 0x550C7DC3D5FFB4E2,
    0x72BE5D74F27B896F, 0x80DEB1FE3B1696B1, 0x9BDC06A725C71235, 0xC19BF174CF692694,
    0xE49B69C19EF14AD2, 0xEFBE4786384F25E3, 0x0FC19DC68B8CD5B5, 0x240CA1CC77AC9C65,
    0x2DE92C6F592B0275, 0x4A7484AA6EA6E483, 0x5CB0A9DCBD41FBD4, 0x76F988DA831153B5,
    0x983E5152EE66DFAB, 0xA831C66D2DB43210, 0xB00327C898FB213F, 0xBF597FC7BEEF0EE4,
    0xC6E00BF33DA88FC2, 0xD5A79147930AA725, 0x06CA6351E003826F, 0x142929670A0E6E70,
    0x27B70A8546D22FFC, 0x2E1B21385C26C926, 0x4D2C6DFC5AC42AED, 0x53380D139D95B3DF,
    0x650A73548BAF63DE, 0x766A0ABB3C77B2A8, 0x81C2C92E47EDAEE6, 0x92722C851482353B,
    0xA2BFE8A14CF10364, 0xA81A664BBC423001, 0xC24B8B70D0F89791, 0xC76C51A30654BE30,
    0xD192E819D6EF5218, 0xD69906245565A910, 0xF40E35855771202A, 0x106AA07032BBD1B8,
    0x19A4C116B8D2D0C8, 0x1E376C085141AB53, 0x2748774CDF8EEB99, 0x34B0BCB5E19B48A8,
    0x391C0CB3C5C95A63, 0x4ED8AA4AE3418ACB, 0x5B9CCA4F7763E373, 0x682E6FF3D6B2B8A3,
    0x748F82EE5DEFB2FC, 0x78A5636F43172F60, 0x84C87814A1F0AB72, 0x8CC702081A6439EC,
    0x90BEFFFA23631E28, 0xA4506CEBDE82BDE9, 0xBEF9A3F7B2C67915, 0xC67178F2E372532B,
    0xCA273ECEEA26619C, 0xD186B8C721C0C207, 0xEADA7DD6CDE0EB1E, 0xF57D4F7FEE6ED178,
    0x06F067AA72176FBA, 0x0A637DC5A2C898A6, 0x113F9804BEF90DAE, 0x1B710B35131C471B,
    0x28DB77F523047D84, 0x32CAAB7B40C72493, 0x3C9EBE0A15C9BEBC, 0x431D67C49C100D4C,
    0x4CC5D4BECB3E42B6, 0x597F299CFC657E2A, 0x5FCB6FAB3AD6FAEC, 0x6C44198C4A475817
};

/* SHA-384 initial hash values */
static const uint64_t H0[6] = {
    0xCBBB9D5DC1059ED8, 0x629A292A367CD507, 0x9159015A3070DD17,
    0x152FECD8F70E5939, 0x67332667FFC00B31, 0x8EB44A8768581511
};

/* SHA-384 functions */
#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define SHR64(x, n) ((x) >> (n))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define SIGMA1(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define sigma0(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ SHR64(x, 7))
#define sigma1(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ SHR64(x, 6))

/*
 * Initialize SHA-384 context
 */
void sha384_init(sha384_context_t *ctx) {
    ctx->count[0] = 0;
    ctx->count[1] = 0;
    
    for (int i = 0; i < 6; i++) {
        ctx->state[i] = H0[i];
    }
    
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

/*
 * Update SHA-384 with data
 */
void sha384_update(sha384_context_t *ctx, const uint8_t *data, uint64_t len) {
    uint64_t i, index, part_len;
    
    index = (ctx->count[0] >> 3) & 0x7F;
    
    if ((ctx->count[0] += (len << 3)) < (len << 3)) {
        ctx->count[1]++;
    }
    
    ctx->count[1] += (len >> 29);
    
    part_len = 128 - index;
    
    if (len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        sha384_transform(ctx->state, ctx->buffer);
        
        for (i = part_len; i + 127 < len; i += 128) {
            sha384_transform(ctx->state, &data[i]);
        }
        
        index = 0;
    } else {
        i = 0;
    }
    
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

/*
 * Finalize SHA-384 and get hash
 */
void sha384_final(sha384_context_t *ctx, uint8_t hash[48]) {
    uint8_t bits[16];
    uint64_t index, pad_len;
    
    /* Save number of bits */
    for (int i = 0; i < 8; i++) {
        bits[i] = (ctx->count[1] >> (56 - i * 8)) & 0xFF;
    }
    for (int i = 0; i < 8; i++) {
        bits[8 + i] = (ctx->count[0] >> (56 - i * 8)) & 0xFF;
    }
    
    /* Pad out to 112 mod 128 */
    index = (ctx->count[0] >> 3) & 0x7F;
    pad_len = (index < 112) ? (112 - index) : (240 - index);
    
    sha384_update(ctx, (const uint8_t*)"\x80", 1);
    while (pad_len--) {
        sha384_update(ctx, (const uint8_t*)"\x00", 1);
    }
    
    /* Append length */
    sha384_update(ctx, bits, 16);
    
    /* Output state */
    for (int i = 0; i < 48; i++) {
        hash[i] = (ctx->state[i / 8] >> (56 - (i % 8) * 8)) & 0xFF;
    }
}

/*
 * SHA-384 transform
 */
static void sha384_transform(uint64_t state[6], const uint8_t block[128]) {
    uint64_t W[80];
    uint64_t a, b, c, d, e, f, g, h;
    uint64_t t1, t2;
    
    /* Prepare message schedule */
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint64_t)block[i * 8] << 56) |
               ((uint64_t)block[i * 8 + 1] << 48) |
               ((uint64_t)block[i * 8 + 2] << 40) |
               ((uint64_t)block[i * 8 + 3] << 32) |
               ((uint64_t)block[i * 8 + 4] << 24) |
               ((uint64_t)block[i * 8 + 5] << 16) |
               ((uint64_t)block[i * 8 + 6] << 8) |
               ((uint64_t)block[i * 8 + 7]);
    }
    
    for (int i = 16; i < 80; i++) {
        W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
    }
    
    /* Initialize working variables */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[5];  /* Reuse for SHA-384 */
    h = state[5];  /* Reuse for SHA-384 */
    
    /* Main loop */
    for (int i = 0; i < 80; i++) {
        t1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + W[i];
        t2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    /* Update state */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
}