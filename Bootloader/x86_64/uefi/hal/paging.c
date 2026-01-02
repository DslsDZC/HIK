#include "hal.h"

#define PAGE_SIZE      4096
#define PAGE_ENTRIES   512

typedef struct {
    UINT64  Present : 1;
    UINT64  Writable : 1;
    UINT64  User : 1;
    UINT64  PWT : 1;
    UINT64  PCD : 1;
    UINT64  Accessed : 1;
    UINT64  Dirty : 1;
    UINT64  PAT : 1;
    UINT64  Global : 1;
    UINT64  Available : 3;
    UINT64  Frame : 40;
    UINT64  Reserved : 11;
    UINT64  NX : 1;
} __attribute__((packed)) PAGE_ENTRY;

typedef struct {
    PAGE_ENTRY Entries[PAGE_ENTRIES];
} __attribute__((aligned(PAGE_SIZE))) PAGE_TABLE;

static PAGE_TABLE* gPml4;
static PAGE_TABLE* gPdpt;
static PAGE_TABLE* gPd;
static PAGE_TABLE* gPt;

static PAGE_ENTRY HalMakePageEntry(UINT64 PhysicalAddress, BOOLEAN Writable, BOOLEAN User) {
    PAGE_ENTRY Entry;
    
    Entry.Present = 1;
    Entry.Writable = Writable ? 1 : 0;
    Entry.User = User ? 1 : 0;
    Entry.PWT = 0;
    Entry.PCD = 0;
    Entry.Accessed = 0;
    Entry.Dirty = 0;
    Entry.PAT = 0;
    Entry.Global = 0;
    Entry.Available = 0;
    Entry.Frame = PhysicalAddress >> 12;
    Entry.Reserved = 0;
    Entry.NX = 0;
    
    return Entry;
}

VOID HalSetupPaging(VOID) {
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS Pml4Addr;
    EFI_PHYSICAL_ADDRESS PdptAddr;
    EFI_PHYSICAL_ADDRESS PdAddr;
    EFI_PHYSICAL_ADDRESS PtAddr;
    UINT64 i;
    
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &Pml4Addr);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate PML4\n");
        return;
    }
    gPml4 = (PAGE_TABLE*)Pml4Addr;
    MemSet(gPml4, 0, sizeof(PAGE_TABLE));
    
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PdptAddr);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate PDPT\n");
        return;
    }
    gPdpt = (PAGE_TABLE*)PdptAddr;
    MemSet(gPdpt, 0, sizeof(PAGE_TABLE));
    gPml4->Entries[0] = HalMakePageEntry((UINT64)gPdpt, TRUE, FALSE);
    
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PdAddr);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate PD\n");
        return;
    }
    gPd = (PAGE_TABLE*)PdAddr;
    MemSet(gPd, 0, sizeof(PAGE_TABLE));
    gPdpt->Entries[0] = HalMakePageEntry((UINT64)gPd, TRUE, FALSE);
    
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &PtAddr);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate PT\n");
        return;
    }
    gPt = (PAGE_TABLE*)PtAddr;
    MemSet(gPt, 0, sizeof(PAGE_TABLE));
    gPd->Entries[0] = HalMakePageEntry((UINT64)gPt, TRUE, FALSE);
    
    for (i = 0; i < PAGE_ENTRIES; i++) {
        gPt->Entries[i] = HalMakePageEntry(i * PAGE_SIZE, TRUE, FALSE);
    }
    
    __asm__ volatile("mov %0, %%cr3" : : "r"((UINT64)gPml4));
    
    UINT64 Cr0 = HalReadCr0();
    Cr0 |= (1 << 31);
    HalWriteCr0(Cr0);
    
    UINT64 Cr4 = HalReadCr4();
    Cr4 |= (1 << 5);
    HalWriteCr4(Cr4);
}

VOID HalInvalidateTlb(VOID) {
    __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax");
}

VOID HalInvalidatePage(UINT64 Address) {
    __asm__ volatile("invlpg (%0)" : : "r"(Address) : "memory");
}