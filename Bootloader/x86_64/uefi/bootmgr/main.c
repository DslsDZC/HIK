#include "bootmgr.h"
#include "../efi/efi.h"
#include "../efi/string.h"
#include "../fs/config.h"

#define CONFIG_PATH   L"\\EFI\\HIK\\boot.conf"

EFI_HANDLE gImageHandle = NULL;

EFI_STATUS EFIAPI UefiMain(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE* SystemTable
) {
    EFI_STATUS Status;
    BOOT_CONFIG Config;
    BOOT_ENTRY* SelectedEntry;
    KERNEL_IMAGE KernelImage;
    RSA_PUBLIC_KEY PublicKey;
    
    gImageHandle = ImageHandle;
    EfiInitialize(ImageHandle, SystemTable);
    
    Status = BootMgrInitialize();
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Boot manager initialization failed\n");
        gBS->Stall(5000000);
        return Status;
    }
    
    Status = ConfigLoad(CONFIG_PATH, &Config);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to load boot configuration\n");
        EfiPrintString(L"Using default configuration...\n");
        
        MemSet(&Config, 0, sizeof(BOOT_CONFIG));
        StrCpy(Config.Entries[0].Name, L"HIK Kernel");
        StrCpy(Config.Entries[0].KernelPath, KERNEL_PATH);
        Config.Entries[0].Enabled = TRUE;
        Config.Entries[0].Default = TRUE;
        Config.EntryCount = 1;
        Config.Timeout = 5;
    }
    
    Status = BootMgrShowMenu(&Config, &SelectedEntry);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to select boot entry\n");
        gBS->Stall(5000000);
        return Status;
    }
    
    EfiPrintString(L"\n");
    EfiPrintString(L"Loading kernel: ");
    EfiPrintString(SelectedEntry->Name);
    EfiPrintString(L"\n");
    
    Status = BootMgrLoadKernel(SelectedEntry, &KernelImage);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to load kernel\n");
        gBS->Stall(5000000);
        return Status;
    }
    
    MemSet(&PublicKey, 0, sizeof(RSA_PUBLIC_KEY));
    
    Status = BootMgrVerifyKernel(&KernelImage, &PublicKey);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Kernel verification failed\n");
        BootMgrCleanup(&KernelImage);
        gBS->Stall(5000000);
        return Status;
    }
    
    Status = BootMgrBootKernel(&KernelImage, SelectedEntry);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to boot kernel\n");
        BootMgrCleanup(&KernelImage);
        gBS->Stall(5000000);
        return Status;
    }
    
    BootMgrCleanup(&KernelImage);
    
    return EFI_SUCCESS;
}