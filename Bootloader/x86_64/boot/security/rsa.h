/*
 * HIK BIOS Bootloader - RSA Signature Verification
 */

#ifndef RSA_H
#define RSA_H

#include <stdint.h>

/* RSA-3072 key size */
#define RSA_KEY_SIZE 384  /* 3072 bits = 384 bytes */

/* Verify RSA-PSS signature with SHA-384 */
int rsa_verify_sha384(const uint8_t hash[48], const uint8_t signature[384], 
                      const uint8_t public_key[384]);

/* Initialize RSA module */
int rsa_init(void);

#endif /* RSA_H */