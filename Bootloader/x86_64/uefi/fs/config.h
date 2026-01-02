#ifndef HIK_CONFIG_H
#define HIK_CONFIG_H

#include "../efi/types.h"
#include "../efi/efi.h"

#define MAX_BOOT_ENTRIES    10
#define MAX_PATH_LENGTH     256
#define MAX_ARGS_LENGTH     512

typedef struct {
    CHAR16                          Name[64];
    CHAR16                          KernelPath[MAX_PATH_LENGTH];
    CHAR16                          InitrdPath[MAX_PATH_LENGTH];
    CHAR16                          Args[MAX_ARGS_LENGTH];
    UINT32                          Timeout;
    BOOLEAN                         Default;
    BOOLEAN                         Enabled;
} BOOT_ENTRY;

typedef struct {
    BOOT_ENTRY                      Entries[MAX_BOOT_ENTRIES];
    UINT32                          EntryCount;
    UINT32                          DefaultEntry;
    UINT32                          Timeout;
    CHAR16                          Title[64];
} BOOT_CONFIG;

EFI_STATUS ConfigLoad(CHAR16* Path, BOOT_CONFIG* Config);
EFI_STATUS ConfigSave(CHAR16* Path, BOOT_CONFIG* Config);
EFI_STATUS ConfigGetDefaultEntry(BOOT_CONFIG* Config, BOOT_ENTRY** Entry);
EFI_STATUS ConfigAddEntry(BOOT_CONFIG* Config, BOOT_ENTRY* Entry);
EFI_STATUS ConfigRemoveEntry(BOOT_CONFIG* Config, UINT32 Index);

#endif