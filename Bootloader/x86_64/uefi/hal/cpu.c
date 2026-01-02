#include "hal.h"

VOID HalDisableInterrupts(VOID) {
    __asm__ volatile("cli");
}

VOID HalEnableInterrupts(VOID) {
    __asm__ volatile("sti");
}

VOID HalHalt(VOID) {
    __asm__ volatile("hlt");
}

UINT64 HalReadCr0(VOID) {
    UINT64 Value;
    __asm__ volatile("mov %%cr0, %0" : "=r"(Value));
    return Value;
}

VOID HalWriteCr0(UINT64 Value) {
    __asm__ volatile("mov %0, %%cr0" : : "r"(Value));
}

UINT64 HalReadCr2(VOID) {
    UINT64 Value;
    __asm__ volatile("mov %%cr2, %0" : "=r"(Value));
    return Value;
}

VOID HalWriteCr2(UINT64 Value) {
    __asm__ volatile("mov %0, %%cr2" : : "r"(Value));
}

UINT64 HalReadCr3(VOID) {
    UINT64 Value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(Value));
    return Value;
}

VOID HalWriteCr3(UINT64 Value) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(Value));
}

UINT64 HalReadCr4(VOID) {
    UINT64 Value;
    __asm__ volatile("mov %%cr4, %0" : "=r"(Value));
    return Value;
}

VOID HalWriteCr4(UINT64 Value) {
    __asm__ volatile("mov %0, %%cr4" : : "r"(Value));
}

VOID HalReadMsr(UINT32 Msr, UINT32* Low, UINT32* High) {
    __asm__ volatile("rdmsr" : "=a"(*Low), "=d"(*High) : "c"(Msr));
}

VOID HalWriteMsr(UINT32 Msr, UINT32 Low, UINT32 High) {
    __asm__ volatile("wrmsr" : : "c"(Msr), "a"(Low), "d"(High));
}

UINT64 HalReadMsr64(UINT32 Msr) {
    UINT32 Low, High;
    HalReadMsr(Msr, &Low, &High);
    return ((UINT64)High << 32) | Low;
}

VOID HalWriteMsr64(UINT32 Msr, UINT64 Value) {
    HalWriteMsr(Msr, (UINT32)(Value & 0xFFFFFFFF), (UINT32)(Value >> 32));
}

VOID HalCpuid(UINT32 Leaf, UINT32* Eax, UINT32* Ebx, UINT32* Ecx, UINT32* Edx) {
    __asm__ volatile("cpuid"
        : "=a"(*Eax), "=b"(*Ebx), "=c"(*Ecx), "=d"(*Edx)
        : "a"(Leaf)
    );
}

VOID HalInitialize(VOID) {
    HalDisableInterrupts();
    HalSetupGdt();
    HalSetupIdt();
}