#ifndef HIK_VOLUME_H
#define HIK_VOLUME_H

#include "../efi/types.h"
#include "../efi/protocol.h"
#include "../efi/efi.h"

typedef struct {
    EFI_HANDLE                      DeviceHandle;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
    EFI_FILE_PROTOCOL*              Root;
} VOLUME;

typedef struct {
    CHAR16*                         Name;
    UINT64                          Size;
    BOOLEAN                         IsDirectory;
} FILE_INFO;

EFI_STATUS FsOpenVolume(EFI_HANDLE DeviceHandle, VOLUME* Volume);
EFI_STATUS FsCloseVolume(VOLUME* Volume);
EFI_STATUS FsOpenFile(VOLUME* Volume, CHAR16* Path, EFI_FILE_PROTOCOL** File);
EFI_STATUS FsCloseFile(EFI_FILE_PROTOCOL* File);
EFI_STATUS FsReadFile(EFI_FILE_PROTOCOL* File, VOID* Buffer, UINTN Size, UINTN* ReadSize);
EFI_STATUS FsWriteFile(EFI_FILE_PROTOCOL* File, VOID* Buffer, UINTN Size, UINTN* WriteSize);
EFI_STATUS FsGetFileSize(EFI_FILE_PROTOCOL* File, UINT64* Size);
EFI_STATUS FsSetFilePosition(EFI_FILE_PROTOCOL* File, UINT64 Position);
EFI_STATUS FsListDirectory(VOLUME* Volume, CHAR16* Path, FILE_INFO** Files, UINTN* FileCount);

#endif