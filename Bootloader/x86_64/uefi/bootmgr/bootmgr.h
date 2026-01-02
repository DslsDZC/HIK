#ifndef HIK_BOOTMGR_H
#define HIK_BOOTMGR_H

#include "../efi/types.h"
#include "../efi/efi.h"
#include "../fs/config.h"
#include "../security/verify.h"

typedef struct {
    HIK_KERNEL_HEADER*   Header;
    UINT8*               Code;
    UINT8*               Data;
    UINT8*               Config;
    UINT8*               Signature;
    UINT64               CodeSize;
    UINT64               DataSize;
    UINT64               ConfigSize;
    UINT64               SignatureSize;
    UINT64               ImageSize;
} KERNEL_IMAGE;

EFI_STATUS BootMgrInitialize(VOID);
EFI_STATUS BootMgrLoadKernel(BOOT_ENTRY* Entry, KERNEL_IMAGE* Image);
EFI_STATUS BootMgrVerifyKernel(KERNEL_IMAGE* Image, const RSA_PUBLIC_KEY* PublicKey);
EFI_STATUS BootMgrBootKernel(KERNEL_IMAGE* Image, BOOT_ENTRY* Entry);
EFI_STATUS BootMgrShowMenu(BOOT_CONFIG* Config, BOOT_ENTRY** SelectedEntry);

VOID BootMgrCleanup(KERNEL_IMAGE* Image);

#endif