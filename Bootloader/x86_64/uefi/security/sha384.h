#ifndef HIK_SHA384_H
#define HIK_SHA384_H

#include "../efi/types.h"

#define SHA384_DIGEST_SIZE    48

typedef struct {
    UINT64  State[8];
    UINT64  Count[2];
    UINT8   Buffer[128];
} SHA384_CTX;

VOID Sha384Init(SHA384_CTX* Ctx);
VOID Sha384Update(SHA384_CTX* Ctx, const UINT8* Data, UINTN Len);
VOID Sha384Final(SHA384_CTX* Ctx, UINT8 Digest[SHA384_DIGEST_SIZE]);
VOID Sha384(const UINT8* Data, UINTN Len, UINT8 Digest[SHA384_DIGEST_SIZE]);

#endif