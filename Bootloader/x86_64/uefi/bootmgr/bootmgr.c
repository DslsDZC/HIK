#include "bootmgr.h"
#include "../efi/string.h"
#include "../fs/volume.h"
#include "../hal/hal.h"

#define KERNEL_PATH   L"\\EFI\\HIK\\kernel.hik"
#define STACK_SIZE    (64 * 1024)

EFI_STATUS BootMgrLoadKernel(BOOT_ENTRY* Entry, KERNEL_IMAGE* Image) {
    EFI_STATUS Status;
    VOLUME Volume;
    EFI_FILE_PROTOCOL* File;
    UINT64 FileSize;
    UINTN ReadSize;
    UINT8* Buffer;
    HIK_KERNEL_HEADER* Header;
    
    MemSet(Image, 0, sizeof(KERNEL_IMAGE));
    
    Status = FsOpenVolume(gImageHandle, &Volume);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to open volume\n");
        return Status;
    }
    
    Status = FsOpenFile(&Volume, Entry->KernelPath, &File);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to open kernel file\n");
        FsCloseVolume(&Volume);
        return Status;
    }
    
    Status = FsGetFileSize(File, &FileSize);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to get file size\n");
        FsCloseFile(File);
        FsCloseVolume(&Volume);
        return Status;
    }
    
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 
                                (FileSize + 0xFFF) / 0x1000, 
                                (EFI_PHYSICAL_ADDRESS*)&Buffer);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate memory for kernel\n");
        FsCloseFile(File);
        FsCloseVolume(&Volume);
        return Status;
    }
    
    ReadSize = FileSize;
    Status = FsReadFile(File, Buffer, ReadSize, &ReadSize);
    FsCloseFile(File);
    FsCloseVolume(&Volume);
    
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to read kernel file\n");
        gBS->FreePages((EFI_PHYSICAL_ADDRESS)Buffer, (FileSize + 0xFFF) / 0x1000);
        return Status;
    }
    
    Header = (HIK_KERNEL_HEADER*)Buffer;
    
    if (Header->Signature != HIK_KERNEL_MAGIC) {
        EfiPrintError(L"Invalid kernel magic\n");
        gBS->FreePages((EFI_PHYSICAL_ADDRESS)Buffer, (FileSize + 0xFFF) / 0x1000);
        return EFI_INVALID_PARAMETER;
    }
    
    Image->Header = Header;
    Image->Code = Buffer + Header->CodeOffset;
    Image->Data = Buffer + Header->DataOffset;
    Image->Config = Buffer + Header->ConfigOffset;
    Image->Signature = Buffer + Header->SignatureOffset;
    Image->CodeSize = Header->CodeSize;
    Image->DataSize = Header->DataSize;
    Image->ConfigSize = Header->ConfigSize;
    Image->SignatureSize = Header->SignatureSize;
    Image->ImageSize = FileSize;
    
    EfiPrintString(L"Kernel loaded successfully\n");
    EfiPrintString(L"  Entry Point: 0x");
    EfiPrintHex(Header->EntryPoint);
    EfiPrintString(L"\n");
    EfiPrintString(L"  Code Size: ");
    EfiPrintHex(Header->CodeSize);
    EfiPrintString(L" bytes\n");
    EfiPrintString(L"  Data Size: ");
    EfiPrintHex(Header->DataSize);
    EfiPrintString(L" bytes\n");
    
    return EFI_SUCCESS;
}

EFI_STATUS BootMgrVerifyKernel(KERNEL_IMAGE* Image, const RSA_PUBLIC_KEY* PublicKey) {
    EFI_STATUS Status;
    HIK_SIGNATURE* Signature;
    
    if (Image->Header->Flags & HIK_FLAG_SIGNED) {
        if (PublicKey == NULL) {
            EfiPrintError(L"No public key provided for verification\n");
            return EFI_SECURITY_VIOLATION;
        }
        
        Signature = (HIK_SIGNATURE*)Image->Signature;
        Status = VerifyKernelImage((UINT8*)Image->Header, Image->ImageSize, Signature, PublicKey);
        
        if (EFI_ERROR(Status)) {
            EfiPrintError(L"Kernel signature verification failed\n");
            return Status;
        }
        
        EfiPrintString(L"Kernel signature verified successfully\n");
    } else {
        EfiPrintString(L"Warning: Kernel is not signed\n");
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS BootMgrBootKernel(KERNEL_IMAGE* Image, BOOT_ENTRY* Entry) {
    EFI_STATUS Status;
    UINTN MapKey;
    MEMORY_MAP MemoryMap;
    ACPI_INFO AcpiInfo;
    HIK_BOOT_INFO* BootInfo;
    EFI_PHYSICAL_ADDRESS BootInfoAddr;
    EFI_PHYSICAL_ADDRESS StackAddr;
    JUMP_CONTEXT JumpCtx;
    UINTN i;
    
    EfiPrintString(L"Preparing to boot kernel...\n");
    
    Status = gBS->GetMemoryMap(NULL, NULL, &MapKey, NULL, NULL);
    UINTN MapSize = 0;
    Status = gBS->GetMemoryMap(&MapSize, NULL, &MapKey, NULL, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        EfiPrintError(L"Failed to get memory map size\n");
        return Status;
    }
    
    EFI_MEMORY_DESCRIPTOR* MemoryMapBuffer;
    Status = gBS->AllocatePool(EfiLoaderData, MapSize, (VOID**)&MemoryMapBuffer);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate memory map\n");
        return Status;
    }
    
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    Status = gBS->GetMemoryMap(&MapSize, MemoryMapBuffer, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to get memory map\n");
        gBS->FreePool(MemoryMapBuffer);
        return Status;
    }
    
    HalGetMemoryMap(&MemoryMap);
    HalGetAcpiInfo(&AcpiInfo);
    
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &BootInfoAddr);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate boot info\n");
        gBS->FreePool(MemoryMapBuffer);
        return Status;
    }
    
    BootInfo = (HIK_BOOT_INFO*)BootInfoAddr;
    MemSet(BootInfo, 0, sizeof(HIK_BOOT_INFO));
    
    BootInfo->MemoryMapBase = (UINT64)MemoryMapBuffer;
    BootInfo->MemoryMapSize = MapSize;
    BootInfo->MemoryMapDescriptorSize = DescriptorSize;
    BootInfo->MemoryMapDescriptorVersion = DescriptorVersion;
    BootInfo->AcpiTable = AcpiInfo.RsdpAddress;
    BootInfo->SystemTable = (UINT64)gST;
    
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 
                                (STACK_SIZE + 0xFFF) / 0x1000, &StackAddr);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate stack\n");
        gBS->FreePages(BootInfoAddr, 1);
        gBS->FreePool(MemoryMapBuffer);
        return Status;
    }
    
    StackAddr += STACK_SIZE;
    
    HalDisableInterrupts();
    
    Status = gBS->ExitBootServices(gImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to exit boot services\n");
        HalHalt();
    }
    
    JumpCtx.EntryPoint = (UINT64)Image->Header->EntryPoint;
    JumpCtx.StackTop = StackAddr;
    JumpCtx.BootInfo = BootInfo;
    
    HalJumpToKernel(&JumpCtx);
    
    return EFI_SUCCESS;
}

EFI_STATUS BootMgrShowMenu(BOOT_CONFIG* Config, BOOT_ENTRY** SelectedEntry) {
    UINT32 i;
    UINT32 Timeout = Config->Timeout;
    CHAR16 Input;
    EFI_STATUS Status;
    
    EfiPrintString(L"\n");
    EfiPrintString(L"=== HIK Boot Manager ===\n");
    EfiPrintString(L"\n");
    
    for (i = 0; i < Config->EntryCount; i++) {
        if (Config->Entries[i].Enabled) {
            EfiPrintString(L"  [");
            EfiPrintHex(i);
            EfiPrintString(L"] ");
            EfiPrintString(Config->Entries[i].Name);
            EfiPrintString(L"\n");
        }
    }
    
    EfiPrintString(L"\n");
    
    if (Timeout > 0) {
        EfiPrintString(L"Booting default entry in ");
        EfiPrintHex(Timeout);
        EfiPrintString(L" seconds... (Press any key to interrupt)\n");
        
        while (Timeout > 0) {
            Status = gST->ConIn->ReadKeyStroke(gST->ConIn, (EFI_INPUT_KEY*)&Input);
            if (!EFI_ERROR(Status)) {
                break;
            }
            
            gBS->Stall(1000000);
            Timeout--;
        }
        
        if (Timeout == 0) {
            return ConfigGetDefaultEntry(Config, SelectedEntry);
        }
    }
    
    EfiPrintString(L"Select entry: ");
    
    while (1) {
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, (EFI_INPUT_KEY*)&Input);
        if (!EFI_ERROR(Status)) {
            if (Input >= L'0' && Input <= L'9') {
                UINT32 Index = Input - L'0';
                if (Index < Config->EntryCount && Config->Entries[Index].Enabled) {
                    EfiPrintString(&Input);
                    EfiPrintString(L"\n");
                    *SelectedEntry = &Config->Entries[Index];
                    return EFI_SUCCESS;
                }
            }
        }
        
        gBS->Stall(100000);
    }
}

EFI_STATUS BootMgrInitialize(VOID) {
    EFI_STATUS Status;
    
    EfiPrintString(L"HIK Boot Manager v1.0\n");
    EfiPrintString(L"Copyright (c) 2026 HIK Project\n");
    EfiPrintString(L"\n");
    
    HalInitialize();
    
    Status = VerifySecureBoot();
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Secure boot verification failed\n");
        return Status;
    }
    
    return EFI_SUCCESS;
}

VOID BootMgrCleanup(KERNEL_IMAGE* Image) {
    if (Image->Header != NULL) {
        gBS->FreePages((EFI_PHYSICAL_ADDRESS)Image->Header, 
                      (Image->ImageSize + 0xFFF) / 0x1000);
    }
    
    MemSet(Image, 0, sizeof(KERNEL_IMAGE));
}