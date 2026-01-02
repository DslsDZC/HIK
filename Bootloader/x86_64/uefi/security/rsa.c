#include "rsa.h"
#include "../efi/string.h"

static VOID RsaModAdd(UINT64* Result, const UINT64* A, const UINT64* B, const UINT64* Mod, UINTN Size) {
    UINT64 Carry = 0;
    UINTN i;
    
    for (i = 0; i < Size; i++) {
        UINT64 Sum = A[i] + B[i] + Carry;
        Result[i] = Sum;
        Carry = (Sum < A[i]) || (Carry && Sum == A[i]) ? 1 : 0;
    }
    
    if (Carry || RsaCompare(Result, Mod, Size) >= 0) {
        RsaModSub(Result, Result, Mod, Size);
    }
}

static VOID RsaModSub(UINT64* Result, const UINT64* A, const UINT64* B, const UINT64* Mod, UINTN Size) {
    UINT64 Borrow = 0;
    UINTN i;
    
    for (i = 0; i < Size; i++) {
        UINT64 Diff = A[i] - B[i] - Borrow;
        Result[i] = Diff;
        Borrow = (Diff > A[i]) || (Borrow && Diff == A[i]) ? 1 : 0;
    }
    
    if (Borrow) {
        RsaModAdd(Result, Result, Mod, Size);
    }
}

static INTN RsaCompare(const UINT64* A, const UINT64* B, UINTN Size) {
    INTN i;
    
    for (i = Size - 1; i >= 0; i--) {
        if (A[i] > B[i]) return 1;
        if (A[i] < B[i]) return -1;
    }
    
    return 0;
}

static VOID RsaModMul(UINT64* Result, const UINT64* A, const UINT64* B, const UINT64* Mod, UINTN Size) {
    UINT64 Temp[RSA_MAX_DIGITS * 2];
    UINTN i, j;
    
    MemSet(Temp, 0, sizeof(Temp));
    
    for (i = 0; i < Size; i++) {
        UINT64 Carry = 0;
        for (j = 0; j < Size; j++) {
            UINT64 Product = (UINT64)A[i] * B[j] + Temp[i + j] + Carry;
            Temp[i + j] = Product;
            Carry = Product >> 32;
        }
        Temp[i + Size] = Carry;
    }
    
    for (i = 2 * Size - 1; i >= Size; i--) {
        UINT64 Q = Temp[i] / Mod[Size - 1];
        UINT64 Carry = 0;
        
        for (j = 0; j < Size; j++) {
            UINT64 Product = Q * Mod[j] + Temp[i - Size + j] + Carry;
            Temp[i - Size + j] = Product;
            Carry = Product >> 32;
        }
        
        if (Carry || RsaCompare(&Temp[i - Size + 1], &Mod[1], Size - 1) >= 0) {
            RsaModSub(&Temp[i - Size], &Temp[i - Size], Mod, Size);
        }
    }
    
    MemCpy(Result, Temp, Size * sizeof(UINT64));
}

static VOID RsaModExp(UINT64* Result, const UINT64* Base, const UINT64* Exponent, const UINT64* Mod, UINTN Size) {
    UINT64 Temp[RSA_MAX_DIGITS];
    UINTN i;
    
    MemCpy(Result, Base, Size * sizeof(UINT64));
    
    for (i = 0; i < Size; i++) {
        UINT32 j;
        for (j = 0; j < 64; j++) {
            if (Exponent[i] & (1ULL << j)) {
                MemCpy(Temp, Result, Size * sizeof(UINT64));
                RsaModMul(Result, Temp, Result, Mod, Size);
            }
        }
    }
}

EFI_STATUS RsaVerifySha384(
    const RSA_PUBLIC_KEY* PublicKey,
    const UINT8 Digest[SHA384_DIGEST_SIZE],
    const RSA_SIGNATURE* Signature
) {
    UINT8 Decrypted[RSA_MAX_BYTES];
    UINT8 PaddedHash[RSA_MAX_BYTES];
    UINTN i;
    
    if (PublicKey->ModulusSize == 0 || PublicKey->ExponentSize == 0) {
        return EFI_INVALID_PARAMETER;
    }
    
    MemSet(Decrypted, 0, sizeof(Decrypted));
    MemSet(PaddedHash, 0, sizeof(PaddedHash));
    
    PaddedHash[0] = 0x30;
    PaddedHash[1] = SHA384_DIGEST_SIZE + 2 + 2;
    PaddedHash[2] = 0x30;
    PaddedHash[3] = SHA384_DIGEST_SIZE + 2;
    PaddedHash[4] = 0x06;
    PaddedHash[5] = 0x0c;
    MemCpy(&PaddedHash[6], "SHA-384", 7);
    PaddedHash[13] = 0x05;
    PaddedHash[14] = 0x00;
    MemCpy(&PaddedHash[15], Digest, SHA384_DIGEST_SIZE);
    
    for (i = 0; i < PublicKey->ModulusSize; i++) {
        if (Decrypted[i] != PaddedHash[PublicKey->ModulusSize - SHA384_DIGEST_SIZE - 15 + i]) {
            return EFI_SECURITY_VIOLATION;
        }
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS RsaVerifySignature(
    const RSA_PUBLIC_KEY* PublicKey,
    const UINT8* Hash,
    UINTN HashSize,
    const RSA_SIGNATURE* Signature
) {
    if (HashSize == SHA384_DIGEST_SIZE) {
        return RsaVerifySha384(PublicKey, Hash, Signature);
    }
    
    return EFI_UNSUPPORTED;
}