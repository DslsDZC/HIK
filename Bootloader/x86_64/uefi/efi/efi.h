#ifndef HIK_UEFI_H
#define HIK_UEFI_H

#include "types.h"
#include "system_table.h"
#include "protocol.h"

#define HIK_KERNEL_MAGIC             0x48494B00ULL
#define HIK_KERNEL_VERSION           0x00010000

typedef struct {
    UINT64                          Signature;
    UINT32                          Version;
    UINT32                          Flags;
    UINT64                          EntryPoint;
    UINT64                          CodeOffset;
    UINT64                          CodeSize;
    UINT64                          DataOffset;
    UINT64                          DataSize;
    UINT64                          ConfigOffset;
    UINT64                          ConfigSize;
    UINT64                          SignatureOffset;
    UINT64                          SignatureSize;
    UINT8                           Reserved[32];
} HIK_KERNEL_HEADER;

#define HIK_FLAG_SIGNED              0x00000001

typedef struct {
    UINT64                          MemoryMapBase;
    UINT64                          MemoryMapSize;
    UINT64                          MemoryMapDescriptorSize;
    UINT32                          MemoryMapDescriptorVersion;
    UINT64                          AcpiTable;
    UINT64                          SmbiosTable;
    UINT64                          SystemTable;
    UINT64                          FrameBufferBase;
    UINT64                          FrameBufferSize;
    UINT32                          HorizontalResolution;
    UINT32                          VerticalResolution;
    UINT32                          PixelFormat;
    UINT32                          Reserved;
} HIK_BOOT_INFO;

extern EFI_SYSTEM_TABLE*   gST;
extern EFI_BOOT_SERVICES*  gBS;
extern EFI_RUNTIME_SERVICES* gRT;

void EfiInitialize(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable);
void EfiPrintString(const CHAR16* String);
void EfiPrintError(const CHAR16* String);
void EfiPrintHex(UINT64 Value);

#endif