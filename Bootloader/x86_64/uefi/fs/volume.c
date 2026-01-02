#include "volume.h"
#include "../efi/string.h"

extern EFI_HANDLE gImageHandle;

EFI_STATUS FsOpenVolume(EFI_HANDLE DeviceHandle, VOLUME* Volume) {
    EFI_STATUS Status;
    
    Volume->DeviceHandle = DeviceHandle;
    
    Status = gBS->OpenProtocol(
        DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&Volume->FileSystem,
        gImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    Status = Volume->FileSystem->OpenVolume(Volume->FileSystem, &Volume->Root);
    
    return Status;
}

EFI_STATUS FsCloseVolume(VOLUME* Volume) {
    EFI_STATUS Status;
    
    if (Volume->Root) {
        Status = Volume->Root->Close(Volume->Root);
        if (EFI_ERROR(Status)) {
            return Status;
        }
        Volume->Root = NULL;
    }
    
    if (Volume->FileSystem) {
        Status = gBS->CloseProtocol(
            Volume->DeviceHandle,
            &gEfiSimpleFileSystemProtocolGuid,
            gImageHandle,
            NULL
        );
        if (EFI_ERROR(Status)) {
            return Status;
        }
        Volume->FileSystem = NULL;
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS FsOpenFile(VOLUME* Volume, CHAR16* Path, EFI_FILE_PROTOCOL** File) {
    EFI_STATUS Status;
    
    Status = Volume->Root->Open(
        Volume->Root,
        File,
        Path,
        EFI_FILE_MODE_READ,
        0
    );
    
    return Status;
}

EFI_STATUS FsCloseFile(EFI_FILE_PROTOCOL* File) {
    EFI_STATUS Status;
    
    if (File) {
        Status = File->Close(File);
        return Status;
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS FsReadFile(EFI_FILE_PROTOCOL* File, VOID* Buffer, UINTN Size, UINTN* ReadSize) {
    EFI_STATUS Status;
    
    Status = File->Read(File, &Size, Buffer);
    if (ReadSize) {
        *ReadSize = Size;
    }
    
    return Status;
}

EFI_STATUS FsWriteFile(EFI_FILE_PROTOCOL* File, VOID* Buffer, UINTN Size, UINTN* WriteSize) {
    EFI_STATUS Status;
    
    Status = File->Write(File, &Size, Buffer);
    if (WriteSize) {
        *WriteSize = Size;
    }
    
    return Status;
}

EFI_STATUS FsGetFileSize(EFI_FILE_PROTOCOL* File, UINT64* Size) {
    EFI_STATUS Status;
    EFI_FILE_INFO* FileInfo;
    UINTN FileInfoSize;
    
    FileInfoSize = 0;
    Status = File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return Status;
    }
    
    Status = gBS->AllocatePool(EfiLoaderData, FileInfoSize, (VOID**)&FileInfo);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    Status = File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        gBS->FreePool(FileInfo);
        return Status;
    }
    
    *Size = FileInfo->FileSize;
    gBS->FreePool(FileInfo);
    
    return EFI_SUCCESS;
}

EFI_STATUS FsSetFilePosition(EFI_FILE_PROTOCOL* File, UINT64 Position) {
    EFI_STATUS Status;
    
    Status = File->SetPosition(File, Position);
    
    return Status;
}

EFI_STATUS FsListDirectory(VOLUME* Volume, CHAR16* Path, FILE_INFO** Files, UINTN* FileCount) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL* Dir;
    EFI_FILE_INFO* FileInfo;
    UINTN FileInfoSize;
    UINTN Count;
    UINTN Index;
    CHAR16 Buffer[512];
    
    Status = Volume->Root->Open(
        Volume->Root,
        &Dir,
        Path,
        EFI_FILE_MODE_READ,
        0
    );
    
    if (EFI_ERROR(Status)) {
        return Status;
    }
    
    Count = 0;
    FileInfoSize = sizeof(Buffer);
    
    while (TRUE) {
        Status = Dir->Read(Dir, &FileInfoSize, Buffer);
        if (EFI_ERROR(Status) || FileInfoSize == 0) {
            break;
        }
        
        FileInfo = (EFI_FILE_INFO*)Buffer;
        if (StrCmp(FileInfo->FileName, L".") != 0 && StrCmp(FileInfo->FileName, L"..") != 0) {
            Count++;
        }
    }
    
    if (Count == 0) {
        Dir->Close(Dir);
        *Files = NULL;
        *FileCount = 0;
        return EFI_SUCCESS;
    }
    
    Status = gBS->AllocatePool(EfiLoaderData, Count * sizeof(FILE_INFO), (VOID**)Files);
    if (EFI_ERROR(Status)) {
        Dir->Close(Dir);
        return Status;
    }
    
    Index = 0;
    Dir->SetPosition(Dir, 0);
    
    while (TRUE) {
        Status = Dir->Read(Dir, &FileInfoSize, Buffer);
        if (EFI_ERROR(Status) || FileInfoSize == 0) {
            break;
        }
        
        FileInfo = (EFI_FILE_INFO*)Buffer;
        if (StrCmp(FileInfo->FileName, L".") != 0 && StrCmp(FileInfo->FileName, L"..") != 0) {
            (*Files)[Index].Name = (CHAR16*)FileInfo->FileName;
            (*Files)[Index].Size = FileInfo->FileSize;
            (*Files)[Index].IsDirectory = (FileInfo->Attribute & EFI_FILE_DIRECTORY) != 0;
            Index++;
        }
    }
    
    Dir->Close(Dir);
    *FileCount = Count;
    
    return EFI_SUCCESS;
}