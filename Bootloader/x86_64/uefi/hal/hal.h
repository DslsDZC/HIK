#ifndef HIK_HAL_H
#define HIK_HAL_H

#include "../efi/types.h"
#include "../efi/efi.h"

typedef struct {
    UINT64  Rax;
    UINT64  Rbx;
    UINT64  Rcx;
    UINT64  Rdx;
    UINT64  Rsi;
    UINT64  Rdi;
    UINT64  Rbp;
    UINT64  R8;
    UINT64  R9;
    UINT64  R10;
    UINT64  R11;
    UINT64  R12;
    UINT64  R13;
    UINT64  R14;
    UINT64  R15;
    UINT64  Rip;
    UINT64  Rflags;
    UINT64  Cs;
    UINT64  Ss;
    UINT64  Ds;
    UINT64  Es;
    UINT64  Fs;
    UINT64  Gs;
} REGISTERS;

typedef struct {
    UINT64  Limit;
    UINT64  Base;
} GDTR;

typedef struct {
    UINT64  Limit;
    UINT64  Base;
} IDTR;

typedef struct {
    UINT16  LimitLow;
    UINT16  BaseLow;
    UINT8   BaseMiddle;
    UINT8   Access;
    UINT8   Granularity;
    UINT8   BaseHigh;
} __attribute__((packed)) GDT_ENTRY;

typedef struct {
    UINT16  OffsetLow;
    UINT16  Selector;
    UINT8   Ist;
    UINT8   TypeAttr;
    UINT16  OffsetMiddle;
    UINT32  OffsetHigh;
    UINT32  Reserved;
} __attribute__((packed)) IDT_ENTRY;

typedef struct {
    UINT64  Base;
    UINT64  Length;
    UINT64  Type;
} MEMORY_MAP_ENTRY;

typedef struct {
    UINT64  EntryCount;
    MEMORY_MAP_ENTRY* Entries;
} MEMORY_MAP;

typedef struct {
    UINT64  Base;
    UINT64  Size;
    UINT64  Type;
} ACPI_TABLE;

typedef struct {
    UINT64  RsdpAddress;
    ACPI_TABLE* Tables;
    UINT64  TableCount;
} ACPI_INFO;

typedef struct {
    UINT64  EntryPoint;
    UINT64  StackTop;
    HIK_BOOT_INFO* BootInfo;
} JUMP_CONTEXT;

VOID HalInitialize(VOID);
VOID HalDisableInterrupts(VOID);
VOID HalEnableInterrupts(VOID);
VOID HalHalt(VOID);

VOID HalSetupGdt(VOID);
VOID HalSetupIdt(VOID);
VOID HalSetup Paging(VOID);

VOID HalGetMemoryMap(MEMORY_MAP* Map);
VOID HalGetAcpiInfo(ACPI_INFO* Info);

VOID HalJumpToKernel(JUMP_CONTEXT* Context);

VOID HalReadMsr(UINT32 Msr, UINT32* Low, UINT32* High);
VOID HalWriteMsr(UINT32 Msr, UINT32 Low, UINT32 High);

UINT64 HalReadCr0(VOID);
VOID HalWriteCr0(UINT64 Value);
UINT64 HalReadCr2(VOID);
VOID HalWriteCr2(UINT64 Value);
UINT64 HalReadCr3(VOID);
VOID HalWriteCr3(UINT64 Value);
UINT64 HalReadCr4(VOID);
VOID HalWriteCr4(UINT64 Value);

UINT64 HalReadMsr64(UINT32 Msr);
VOID HalWriteMsr64(UINT32 Msr, UINT64 Value);

VOID HalInvalidateTlb(VOID);
VOID HalInvalidatePage(UINT64 Address);

VOID HalCpuid(UINT32 Leaf, UINT32* Eax, UINT32* Ebx, UINT32* Ecx, UINT32* Edx);

#endif