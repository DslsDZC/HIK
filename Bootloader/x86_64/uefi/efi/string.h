#ifndef HIK_STRING_H
#define HIK_STRING_H

#include "types.h"

UINTN StrLen(const CHAR16* String);
INTN StrCmp(const CHAR16* String1, const CHAR16* String2);
VOID StrCpy(CHAR16* Dest, const CHAR16* Src);
VOID StrCat(CHAR16* Dest, const CHAR16* Src);
UINTN StrSize(const CHAR16* String);

UINTN AsciiStrLen(const CHAR8* String);
INTN AsciiStrCmp(const CHAR8* String1, const CHAR8* String2);
VOID AsciiStrCpy(CHAR8* Dest, const CHAR8* Src);
VOID AsciiStrCat(CHAR8* Dest, const CHAR8* Src);

VOID* MemCpy(VOID* Dest, const VOID* Src, UINTN Size);
VOID* MemSet(VOID* Dest, UINT8 Value, UINTN Size);
INTN MemCmp(const VOID* Buf1, const VOID* Buf2, UINTN Size);

#endif