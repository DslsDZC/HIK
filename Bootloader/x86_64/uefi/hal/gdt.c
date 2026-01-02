#include "hal.h"

#define GDT_ENTRIES    7
#define GDT_NULL       0
#define GDT_KCODE      1
#define GDT_KDATA      2
#define GDT_UCODE      3
#define GDT_UDATA      4
#define GDT_TSS        5
#define GDT_TSS_HIGH   6

static GDT_ENTRY gGdt[GDT_ENTRIES];
static GDTR gGdtr;

static VOID HalSetGdtEntry(UINT32 Index, UINT64 Base, UINT64 Limit, UINT8 Access, UINT8 Granularity) {
    gGdt[Index].LimitLow = Limit & 0xFFFF;
    gGdt[Index].BaseLow = Base & 0xFFFF;
    gGdt[Index].BaseMiddle = (Base >> 16) & 0xFF;
    gGdt[Index].Access = Access;
    gGdt[Index].Granularity = ((Limit >> 16) & 0x0F) | (Granularity & 0xF0);
    gGdt[Index].BaseHigh = (Base >> 24) & 0xFF;
}

VOID HalSetupGdt(VOID) {
    MemSet(&gGdt, 0, sizeof(gGdt));
    
    HalSetGdtEntry(GDT_NULL, 0, 0, 0, 0);
    HalSetGdtEntry(GDT_KCODE, 0, 0xFFFFFFFF, 0x9A, 0xAF);
    HalSetGdtEntry(GDT_KDATA, 0, 0xFFFFFFFF, 0x92, 0xCF);
    HalSetGdtEntry(GDT_UCODE, 0, 0xFFFFFFFF, 0xFA, 0xAF);
    HalSetGdtEntry(GDT_UDATA, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    gGdtr.Limit = sizeof(gGdt) - 1;
    gGdtr.Base = (UINT64)&gGdt;
    
    __asm__ volatile("lgdt %0" : : "m"(gGdtr));
    
    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :
        :
    );
}