#include "util.h"
#include "../efi/string.h"
#include "../hal/hal.h"

VOID UtilPrintHexDump(const UINT8* Data, UINTN Size) {
    UINTN i, j;
    
    for (i = 0; i < Size; i += 16) {
        EfiPrintHex(i);
        EfiPrintString(L": ");
        
        for (j = 0; j < 16; j++) {
            if (i + j < Size) {
                if (Data[i + j] < 0x10) {
                    EfiPrintString(L"0");
                }
                EfiPrintHex(Data[i + j]);
                EfiPrintString(L" ");
            } else {
                EfiPrintString(L"   ");
            }
        }
        
        EfiPrintString(L" ");
        
        for (j = 0; j < 16 && i + j < Size; j++) {
            CHAR8 c = Data[i + j];
            if (c >= 32 && c < 127) {
                CHAR16 str[2] = {c, 0};
                EfiPrintString(str);
            } else {
                EfiPrintString(L".");
            }
        }
        
        EfiPrintString(L"\n");
    }
}

VOID UtilPrintMemoryMap(VOID) {
    EFI_STATUS Status;
    UINTN MapSize = 0;
    UINTN MapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
    EFI_MEMORY_DESCRIPTOR* Descriptor;
    UINTN EntryCount;
    UINTN i;
    
    EfiPrintString(L"Memory Map:\n");
    EfiPrintString(L"Type            Start            End              Size             Attributes\n");
    EfiPrintString(L"--------------------------------------------------------------------------------\n");
    
    Status = gBS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        EfiPrintError(L"Failed to get memory map size\n");
        return;
    }
    
    MapSize += 2 * DescriptorSize;
    Status = gBS->AllocatePool(EfiLoaderData, MapSize, (VOID**)&MemoryMap);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate memory map\n");
        return;
    }
    
    Status = gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to get memory map\n");
        gBS->FreePool(MemoryMap);
        return;
    }
    
    EntryCount = MapSize / DescriptorSize;
    Descriptor = MemoryMap;
    
    for (i = 0; i < EntryCount; i++) {
        const CHAR8* TypeName = "Unknown";
        
        switch (Descriptor->Type) {
            case EfiReservedMemoryType:
                TypeName = "Reserved";
                break;
            case EfiLoaderCode:
                TypeName = "LoaderCode";
                break;
            case EfiLoaderData:
                TypeName = "LoaderData";
                break;
            case EfiBootServicesCode:
                TypeName = "BootCode";
                break;
            case EfiBootServicesData:
                TypeName = "BootData";
                break;
            case EfiRuntimeServicesCode:
                TypeName = "RuntimeCode";
                break;
            case EfiRuntimeServicesData:
                TypeName = "RuntimeData";
                break;
            case EfiConventionalMemory:
                TypeName = "Conventional";
                break;
            case EfiUnusableMemory:
                TypeName = "Unusable";
                break;
            case EfiACPIReclaimMemory:
                TypeName = "ACPIReclaim";
                break;
            case EfiACPIMemoryNVS:
                TypeName = "ACPINVS";
                break;
            case EfiMemoryMappedIO:
                TypeName = "MMIO";
                break;
            case EfiMemoryMappedIOPortSpace:
                TypeName = "IOPort";
                break;
            case EfiPalCode:
                TypeName = "PALCode";
                break;
            case EfiPersistentMemory:
                TypeName = "Persistent";
                break;
            default:
                TypeName = "Unknown";
                break;
        }
        
        EfiPrintString((CHAR16*)TypeName);
        EfiPrintString(L"  ");
        EfiPrintHex(Descriptor->PhysicalStart);
        EfiPrintString(L"  ");
        EfiPrintHex(Descriptor->PhysicalStart + Descriptor->NumberOfPages * 4096 - 1);
        EfiPrintString(L"  ");
        EfiPrintHex(Descriptor->NumberOfPages * 4096);
        EfiPrintString(L"  ");
        EfiPrintHex(Descriptor->Attribute);
        EfiPrintString(L"\n");
        
        Descriptor = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Descriptor + DescriptorSize);
    }
    
    gBS->FreePool(MemoryMap);
}

VOID UtilPrintCpuInfo(VOID) {
    UINT32 Eax, Ebx, Ecx, Edx;
    
    EfiPrintString(L"CPU Information:\n");
    
    HalCpuid(0, &Eax, &Ebx, &Ecx, &Edx);
    
    EfiPrintString(L"  Vendor ID: ");
    EfiPrintString((CHAR16*)&Ebx);
    EfiPrintString((CHAR16*)&Edx);
    EfiPrintString((CHAR16*)&Ecx);
    EfiPrintString(L"\n");
    
    HalCpuid(1, &Eax, &Ebx, &Ecx, &Edx);
    
    EfiPrintString(L"  Family: ");
    EfiPrintHex((Eax >> 8) & 0xF);
    EfiPrintString(L"\n");
    
    EfiPrintString(L"  Model: ");
    EfiPrintHex((Eax >> 4) & 0xF);
    EfiPrintString(L"\n");
    
    EfiPrintString(L"  Stepping: ");
    EfiPrintHex(Eax & 0xF);
    EfiPrintString(L"\n");
    
    if (Edx & (1 << 28)) {
        EfiPrintString(L"  Hyper-Threading: Enabled\n");
    }
    
    if (Edx & (1 << 29)) {
        EfiPrintString(L"  Thermal Monitor: Enabled\n");
    }
}

UINT64 UtilAlignUp(UINT64 Value, UINT64 Alignment) {
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

UINT64 UtilAlignDown(UINT64 Value, UINT64 Alignment) {
    return Value & ~(Alignment - 1);
}

BOOLEAN UtilIsAligned(UINT64 Value, UINT64 Alignment) {
    return (Value & (Alignment - 1)) == 0;
}

UINTN UtilCountBits(UINT64 Value) {
    UINTN Count = 0;
    
    while (Value) {
        Count += Value & 1;
        Value >>= 1;
    }
    
    return Count;
}

UINTN UtilFindFirstSet(UINT64 Value) {
    UINTN Index = 0;
    
    if (Value == 0) {
        return 64;
    }
    
    while ((Value & 1) == 0) {
        Value >>= 1;
        Index++;
    }
    
    return Index;
}

UINTN UtilFindLastSet(UINT64 Value) {
    UINTN Index = 0;
    
    if (Value == 0) {
        return 64;
    }
    
    while (Value != 0) {
        Value >>= 1;
        Index++;
    }
    
    return Index - 1;
}

VOID UtilDelay(UINTN Microseconds) {
    gBS->Stall(Microseconds);
}

VOID UtilBusyWait(UINTN Count) {
    volatile UINTN i;
    
    for (i = 0; i < Count; i++) {
        __asm__ volatile("nop");
    }
}