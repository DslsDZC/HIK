#include "hal.h"

#define IDT_ENTRIES    256

static IDT_ENTRY gIdt[IDT_ENTRIES];
static IDTR gIdtr;

static VOID HalSetIdtEntry(UINT32 Index, UINT64 Handler, UINT16 Selector, UINT8 TypeAttr) {
    gIdt[Index].OffsetLow = Handler & 0xFFFF;
    gIdt[Index].Selector = Selector;
    gIdt[Index].Ist = 0;
    gIdt[Index].TypeAttr = TypeAttr;
    gIdt[Index].OffsetMiddle = (Handler >> 16) & 0xFFFF;
    gIdt[Index].OffsetHigh = (Handler >> 32) & 0xFFFFFFFF;
    gIdt[Index].Reserved = 0;
}

static VOID HalDefaultExceptionHandler(VOID) {
    EfiPrintString(L"Exception occurred!\n");
    while (1) {
        __asm__ volatile("hlt");
    }
}

VOID HalSetupIdt(VOID) {
    UINT32 i;
    
    MemSet(&gIdt, 0, sizeof(gIdt));
    
    for (i = 0; i < IDT_ENTRIES; i++) {
        HalSetIdtEntry(i, (UINT64)HalDefaultExceptionHandler, 0x08, 0x8E);
    }
    
    gIdtr.Limit = sizeof(gIdt) - 1;
    gIdtr.Base = (UINT64)&gIdt;
    
    __asm__ volatile("lidt %0" : : "m"(gIdtr));
}