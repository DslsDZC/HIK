#ifndef HIK_RSA_H
#define HIK_RSA_H

#include "../efi/types.h"
#include "sha384.h"

#define RSA_MAX_BITS          4096
#define RSA_MAX_BYTES         (RSA_MAX_BITS / 8)
#define RSA_MAX_DIGITS        (RSA_MAX_BITS / 64)

typedef struct {
    UINT64  N[RSA_MAX_DIGITS];
    UINT64  E[RSA_MAX_DIGITS];
    UINT64  D[RSA_MAX_DIGITS];
    UINT64  P[RSA_MAX_DIGITS / 2];
    UINT64  Q[RSA_MAX_DIGITS / 2];
    UINT64  DP[RSA_MAX_DIGITS / 2];
    UINT64  DQ[RSA_MAX_DIGITS / 2];
    UINT64  QINV[RSA_MAX_DIGITS / 2];
    UINT32  Bits;
} RSA_KEY;

typedef struct {
    UINT8   Modulus[RSA_MAX_BYTES];
    UINT8   Exponent[RSA_MAX_BYTES];
    UINT32  ModulusSize;
    UINT32  ExponentSize;
} RSA_PUBLIC_KEY;

typedef struct {
    UINT8   Signature[RSA_MAX_BYTES];
    UINT32  SignatureSize;
} RSA_SIGNATURE;

EFI_STATUS RsaVerifySignature(
    const RSA_PUBLIC_KEY* PublicKey,
    const UINT8* Hash,
    UINTN HashSize,
    const RSA_SIGNATURE* Signature
);

EFI_STATUS RsaVerifySha384(
    const RSA_PUBLIC_KEY* PublicKey,
    const UINT8 Digest[SHA384_DIGEST_SIZE],
    const RSA_SIGNATURE* Signature
);

#endif