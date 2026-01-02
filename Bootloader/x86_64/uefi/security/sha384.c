#include "sha384.h"
#include "../efi/string.h"

#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (64-(b))))
#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,28) ^ ROTRIGHT(x,34) ^ ROTRIGHT(x,39))
#define EP1(x) (ROTRIGHT(x,14) ^ ROTRIGHT(x,18) ^ ROTRIGHT(x,41))
#define SIG0(x) (ROTRIGHT(x,1) ^ ROTRIGHT(x,8) ^ ((x) >> 7))
#define SIG1(x) (ROTRIGHT(x,19) ^ ROTRIGHT(x,61) ^ ((x) >> 6))

static const UINT64 K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

VOID Sha384Init(SHA384_CTX* Ctx) {
    Ctx->State[0] = 0xcbbb9d5dc1059ed8ULL;
    Ctx->State[1] = 0x629a292a367cd507ULL;
    Ctx->State[2] = 0x9159015a3070dd17ULL;
    Ctx->State[3] = 0x152fecd8f70e5939ULL;
    Ctx->State[4] = 0x67332667ffc00b31ULL;
    Ctx->State[5] = 0x8eb44a8768581511ULL;
    Ctx->State[6] = 0xdb0c2e0d64f98fa7ULL;
    Ctx->State[7] = 0x47b5481dbefa4fa4ULL;
    Ctx->Count[0] = 0;
    Ctx->Count[1] = 0;
}

static VOID Sha384Transform(SHA384_CTX* Ctx, const UINT8 Buffer[128]) {
    UINT64 a, b, c, d, e, f, g, h;
    UINT64 t1, t2;
    UINT64 W[80];
    UINTN i;
    
    for (i = 0; i < 16; i++) {
        W[i] = ((UINT64)Buffer[i * 8] << 56) |
               ((UINT64)Buffer[i * 8 + 1] << 48) |
               ((UINT64)Buffer[i * 8 + 2] << 40) |
               ((UINT64)Buffer[i * 8 + 3] << 32) |
               ((UINT64)Buffer[i * 8 + 4] << 24) |
               ((UINT64)Buffer[i * 8 + 5] << 16) |
               ((UINT64)Buffer[i * 8 + 6] << 8) |
               ((UINT64)Buffer[i * 8 + 7]);
    }
    
    for (i = 16; i < 80; i++) {
        W[i] = SIG1(W[i - 2]) + W[i - 7] + SIG0(W[i - 15]) + W[i - 16];
    }
    
    a = Ctx->State[0];
    b = Ctx->State[1];
    c = Ctx->State[2];
    d = Ctx->State[3];
    e = Ctx->State[4];
    f = Ctx->State[5];
    g = Ctx->State[6];
    h = Ctx->State[7];
    
    for (i = 0; i < 80; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + W[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    Ctx->State[0] += a;
    Ctx->State[1] += b;
    Ctx->State[2] += c;
    Ctx->State[3] += d;
    Ctx->State[4] += e;
    Ctx->State[5] += f;
    Ctx->State[6] += g;
    Ctx->State[7] += h;
}

VOID Sha384Update(SHA384_CTX* Ctx, const UINT8* Data, UINTN Len) {
    UINTN i, j;
    
    j = (Ctx->Count[0] >> 3) & 127;
    
    if ((Ctx->Count[0] += (Len << 3)) < (Len << 3)) {
        Ctx->Count[1]++;
    }
    
    Ctx->Count[1] += (Len >> 29);
    
    if ((j + Len) > 127) {
        i = 128 - j;
        MemCpy(&Ctx->Buffer[j], Data, i);
        Sha384Transform(Ctx, Ctx->Buffer);
        
        for (; i + 127 < Len; i += 128) {
            Sha384Transform(Ctx, &Data[i]);
        }
        
        j = 0;
    } else {
        i = 0;
    }
    
    MemCpy(&Ctx->Buffer[j], &Data[i], Len - i);
}

VOID Sha384Final(SHA384_CTX* Ctx, UINT8 Digest[SHA384_DIGEST_SIZE]) {
    UINT8 Bits[16];
    UINTN i, index;
    
    for (i = 0; i < 8; i++) {
        Bits[i] = (Ctx->Count[1] >> (56 - i * 8)) & 0xFF;
        Bits[i + 8] = (Ctx->Count[0] >> (56 - i * 8)) & 0xFF;
    }
    
    index = (Ctx->Count[0] >> 3) & 127;
    
    if (index < 112) {
        Ctx->Buffer[index++] = 0x80;
        while (index < 112) {
            Ctx->Buffer[index++] = 0;
        }
    } else {
        Ctx->Buffer[index++] = 0x80;
        while (index < 128) {
            Ctx->Buffer[index++] = 0;
        }
        Sha384Transform(Ctx, Ctx->Buffer);
        MemSet(Ctx->Buffer, 0, 112);
    }
    
    MemCpy(&Ctx->Buffer[112], Bits, 16);
    Sha384Transform(Ctx, Ctx->Buffer);
    
    for (i = 0; i < 6; i++) {
        Digest[i * 8] = (Ctx->State[i] >> 56) & 0xFF;
        Digest[i * 8 + 1] = (Ctx->State[i] >> 48) & 0xFF;
        Digest[i * 8 + 2] = (Ctx->State[i] >> 40) & 0xFF;
        Digest[i * 8 + 3] = (Ctx->State[i] >> 32) & 0xFF;
        Digest[i * 8 + 4] = (Ctx->State[i] >> 24) & 0xFF;
        Digest[i * 8 + 5] = (Ctx->State[i] >> 16) & 0xFF;
        Digest[i * 8 + 6] = (Ctx->State[i] >> 8) & 0xFF;
        Digest[i * 8 + 7] = Ctx->State[i] & 0xFF;
    }
}

VOID Sha384(const UINT8* Data, UINTN Len, UINT8 Digest[SHA384_DIGEST_SIZE]) {
    SHA384_CTX Ctx;
    
    Sha384Init(&Ctx);
    Sha384Update(&Ctx, Data, Len);
    Sha384Final(&Ctx, Digest);
}