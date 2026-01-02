#ifndef HIK_UTIL_H
#define HIK_UTIL_H

#include "../efi/types.h"
#include "../efi/efi.h"

VOID UtilPrintHexDump(const UINT8* Data, UINTN Size);
VOID UtilPrintMemoryMap(VOID);
VOID UtilPrintCpuInfo(VOID);

UINT64 UtilAlignUp(UINT64 Value, UINT64 Alignment);
UINT64 UtilAlignDown(UINT64 Value, UINT64 Alignment);
BOOLEAN UtilIsAligned(UINT64 Value, UINT64 Alignment);

UINTN UtilCountBits(UINT64 Value);
UINTN UtilFindFirstSet(UINT64 Value);
UINTN UtilFindLastSet(UINT64 Value);

VOID UtilDelay(UINTN Microseconds);
VOID UtilBusyWait(UINTN Count);

#endif