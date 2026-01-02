#include "string.h"

UINTN StrLen(const CHAR16* String) {
    UINTN Len = 0;
    while (String[Len] != L'\0') {
        Len++;
    }
    return Len;
}

INTN StrCmp(const CHAR16* String1, const CHAR16* String2) {
    while (*String1 && (*String1 == *String2)) {
        String1++;
        String2++;
    }
    return *String1 - *String2;
}

VOID StrCpy(CHAR16* Dest, const CHAR16* Src) {
    while (*Src) {
        *Dest++ = *Src++;
    }
    *Dest = L'\0';
}

VOID StrCat(CHAR16* Dest, const CHAR16* Src) {
    StrCpy(Dest + StrLen(Dest), Src);
}

UINTN StrSize(const CHAR16* String) {
    return (StrLen(String) + 1) * sizeof(CHAR16);
}

UINTN AsciiStrLen(const CHAR8* String) {
    UINTN Len = 0;
    while (String[Len] != '\0') {
        Len++;
    }
    return Len;
}

INTN AsciiStrCmp(const CHAR8* String1, const CHAR8* String2) {
    while (*String1 && (*String1 == *String2)) {
        String1++;
        String2++;
    }
    return *String1 - *String2;
}

VOID AsciiStrCpy(CHAR8* Dest, const CHAR8* Src) {
    while (*Src) {
        *Dest++ = *Src++;
    }
    *Dest = '\0';
}

VOID AsciiStrCat(CHAR8* Dest, const CHAR8* Src) {
    AsciiStrCpy(Dest + AsciiStrLen(Dest), Src);
}

VOID* MemCpy(VOID* Dest, const VOID* Src, UINTN Size) {
    UINT8* D = (UINT8*)Dest;
    const UINT8* S = (const UINT8*)Src;
    
    while (Size--) {
        *D++ = *S++;
    }
    return Dest;
}

VOID* MemSet(VOID* Dest, UINT8 Value, UINTN Size) {
    UINT8* D = (UINT8*)Dest;
    
    while (Size--) {
        *D++ = Value;
    }
    return Dest;
}

INTN MemCmp(const VOID* Buf1, const VOID* Buf2, UINTN Size) {
    const UINT8* B1 = (const UINT8*)Buf1;
    const UINT8* B2 = (const UINT8*)Buf2;
    
    while (Size--) {
        if (*B1 != *B2) {
            return *B1 - *B2;
        }
        B1++;
        B2++;
    }
    return 0;
}