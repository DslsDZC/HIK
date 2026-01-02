#include "hal.h"

VOID HalGetMemoryMap(MEMORY_MAP* Map) {
    EFI_STATUS Status;
    UINTN MapSize = 0;
    UINTN MapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
    EFI_MEMORY_DESCRIPTOR* Descriptor;
    UINTN EntryCount = 0;
    UINTN i;
    
    Status = gBS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        EfiPrintError(L"Failed to get memory map size\n");
        return;
    }
    
    MapSize += 2 * DescriptorSize;
    Status = gBS->AllocatePool(EfiLoaderData, MapSize, (VOID**)&MemoryMap);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate memory for memory map\n");
        return;
    }
    
    Status = gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to get memory map\n");
        gBS->FreePool(MemoryMap);
        return;
    }
    
    EntryCount = MapSize / DescriptorSize;
    Map->Entries = (MEMORY_MAP_ENTRY*)MemoryMap;
    Map->EntryCount = EntryCount;
    
    Descriptor = MemoryMap;
    for (i = 0; i < EntryCount; i++) {
        Map->Entries[i].Base = Descriptor->PhysicalStart;
        Map->Entries[i].Length = Descriptor->NumberOfPages * 4096;
        Map->Entries[i].Type = Descriptor->Type;
        Descriptor = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Descriptor + DescriptorSize);
    }
}