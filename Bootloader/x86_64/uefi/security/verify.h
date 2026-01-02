#ifndef HIK_VERIFY_H
#define HIK_VERIFY_H

#include "../efi/types.h"
#include "../efi/efi.h"
#include "sha384.h"
#include "rsa.h"

#define HIK_SIGNATURE_ALGORITHM_RSA_SHA384    0x01

typedef struct {
    UINT32  Algorithm;
    UINT32  SignatureSize;
    UINT8   Signature[RSA_MAX_BYTES];
} HIK_SIGNATURE;

EFI_STATUS VerifyKernelHeader(
    const HIK_KERNEL_HEADER* Header,
    const HIK_SIGNATURE* Signature,
    const RSA_PUBLIC_KEY* PublicKey
);

EFI_STATUS VerifyKernelImage(
    const UINT8* Image,
    UINTN ImageSize,
    const HIK_SIGNATURE* Signature,
    const RSA_PUBLIC_KEY* PublicKey
);

EFI_STATUS VerifySecureBoot(VOID);
BOOLEAN IsSecureBootEnabled(VOID);

#endif