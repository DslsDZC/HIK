#include "hal.h"

#define RSDP_SIGNATURE    "RSD PTR "
#define RSDP_LENGTH       20

typedef struct {
    CHAR8   Signature[8];
    UINT8   Checksum;
    CHAR8   OemId[6];
    UINT8   Revision;
    UINT32  RsdtAddress;
} __attribute__((packed)) RSDP_DESCRIPTOR;

typedef struct {
    CHAR8   Signature[4];
    UINT32  Length;
    UINT8   Revision;
    UINT8   Checksum;
    CHAR8   OemId[6];
    CHAR8   OemTableId[8];
    UINT32  OemRevision;
    CHAR8   AslCompilerId[4];
    UINT32  AslCompilerRevision;
} __attribute__((packed)) ACPI_TABLE_HEADER;

static BOOLEAN HalVerifyChecksum(UINT8* Table, UINT32 Length) {
    UINT8 Sum = 0;
    UINT32 i;
    
    for (i = 0; i < Length; i++) {
        Sum += Table[i];
    }
    
    return Sum == 0;
}

static RSDP_DESCRIPTOR* HalFindRsdp(VOID) {
    EFI_STATUS Status;
    UINTN i;
    EFI_CONFIGURATION_TABLE* ConfigTable;
    EFI_GUID Acpi20Guid = {0x8868E871, 0xE4F1, 0x11D3, {0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81}};
    EFI_GUID Acpi10Guid = {0xEB9D2D30, 0x2D88, 0x11D3, {0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}};
    
    for (i = 0; i < gST->NumberOfTableEntries; i++) {
        ConfigTable = &gST->ConfigurationTable[i];
        
        if (MemCmp(&ConfigTable->VendorGuid, &Acpi20Guid, sizeof(EFI_GUID)) == 0) {
            return (RSDP_DESCRIPTOR*)ConfigTable->VendorTable;
        }
        
        if (MemCmp(&ConfigTable->VendorGuid, &Acpi10Guid, sizeof(EFI_GUID)) == 0) {
            return (RSDP_DESCRIPTOR*)ConfigTable->VendorTable;
        }
    }
    
    return NULL;
}

VOID HalGetAcpiInfo(ACPI_INFO* Info) {
    RSDP_DESCRIPTOR* Rsdp;
    ACPI_TABLE_HEADER* Rsdt;
    UINT32* EntryPtr;
    UINT32 EntryCount;
    UINT32 i;
    
    MemSet(Info, 0, sizeof(ACPI_INFO));
    
    Rsdp = HalFindRsdp();
    if (Rsdp == NULL) {
        EfiPrintError(L"Failed to find RSDP\n");
        return;
    }
    
    if (!HalVerifyChecksum((UINT8*)Rsdp, RSDP_LENGTH)) {
        EfiPrintError(L"RSDP checksum failed\n");
        return;
    }
    
    Info->RsdpAddress = (UINT64)Rsdp;
    
    if (Rsdp->Revision >= 2) {
        EfiPrintString(L"ACPI 2.0+ detected\n");
    } else {
        EfiPrintString(L"ACPI 1.0 detected\n");
    }
    
    Rsdt = (ACPI_TABLE_HEADER*)(UINT64)Rsdp->RsdtAddress;
    if (Rsdt == NULL) {
        EfiPrintError(L"RSDT not found\n");
        return;
    }
    
    if (!HalVerifyChecksum((UINT8*)Rsdt, Rsdt->Length)) {
        EfiPrintError(L"RSDT checksum failed\n");
        return;
    }
    
    EntryCount = (Rsdt->Length - sizeof(ACPI_TABLE_HEADER)) / 4;
    EntryPtr = (UINT32*)(Rsdt + 1);
    
    Info->TableCount = EntryCount;
    Info->Tables = (ACPI_TABLE*)EntryPtr;
}