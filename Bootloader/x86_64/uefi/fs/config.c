#include "config.h"
#include "volume.h"
#include "../efi/string.h"

static CHAR16* ConfigSkipWhitespace(CHAR16* Str) {
    while (*Str == L' ' || *Str == L'\t') {
        Str++;
    }
    return Str;
}

static CHAR16* ConfigSkipLine(CHAR16* Str) {
    while (*Str != L'\0' && *Str != L'\n') {
        Str++;
    }
    if (*Str == L'\n') {
        Str++;
    }
    return Str;
}

static CHAR16* ConfigParseValue(CHAR16* Str, CHAR16* Value, UINTN MaxLen) {
    UINTN i = 0;
    
    Str = ConfigSkipWhitespace(Str);
    
    if (*Str == L'"') {
        Str++;
        while (*Str != L'\"' && *Str != L'\0' && i < MaxLen - 1) {
            Value[i++] = *Str++;
        }
        if (*Str == L'\"') {
            Str++;
        }
    } else {
        while (*Str != L'\n' && *Str != L'\0' && *Str != L' ' && *Str != L'\t' && i < MaxLen - 1) {
            Value[i++] = *Str++;
        }
    }
    
    Value[i] = L'\0';
    return Str;
}

static CHAR16* ConfigFindKey(CHAR16* Str, CHAR16* Key, CHAR16** Value) {
    CHAR16 Line[512];
    CHAR16* LineStart;
    CHAR16* EqualSign;
    
    while (*Str != L'\0') {
        LineStart = Str;
        Str = ConfigSkipLine(Str);
        
        UINTN Len = Str - LineStart;
        if (Len > sizeof(Line) / sizeof(CHAR16) - 1) {
            Len = sizeof(Line) / sizeof(CHAR16) - 1;
        }
        
        MemCpy(Line, LineStart, Len * sizeof(CHAR16));
        Line[Len] = L'\0';
        
        CHAR16* KeyStart = ConfigSkipWhitespace(Line);
        if (*KeyStart == L'#' || *KeyStart == L'\0') {
            continue;
        }
        
        EqualSign = KeyStart;
        while (*EqualSign != L'=' && *EqualSign != L'\0') {
            EqualSign++;
        }
        
        if (*EqualSign != L'=') {
            continue;
        }
        
        *EqualSign = L'\0';
        EqualSign++;
        
        if (StrCmp(KeyStart, Key) == 0) {
            *Value = EqualSign;
            return Str;
        }
    }
    
    return NULL;
}

EFI_STATUS ConfigLoad(CHAR16* Path, BOOT_CONFIG* Config) {
    EFI_STATUS Status;
    VOLUME Volume;
    EFI_FILE_PROTOCOL* File;
    CHAR16* Buffer;
    UINT64 FileSize;
    UINTN ReadSize;
    CHAR16* Str;
    CHAR16* Value;
    UINT32 CurrentEntry = 0;
    
    MemSet(Config, 0, sizeof(BOOT_CONFIG));
    Config->Timeout = 5;
    Config->DefaultEntry = 0;
    
    Status = FsOpenVolume(gImageHandle, &Volume);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to open volume\n");
        return Status;
    }
    
    Status = FsOpenFile(&Volume, Path, &File);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to open config file\n");
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
    
    Status = gBS->AllocatePool(EfiLoaderData, FileSize + sizeof(CHAR16), (VOID**)&Buffer);
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to allocate buffer\n");
        FsCloseFile(File);
        FsCloseVolume(&Volume);
        return Status;
    }
    
    ReadSize = FileSize;
    Status = FsReadFile(File, Buffer, ReadSize, &ReadSize);
    FsCloseFile(File);
    FsCloseVolume(&Volume);
    
    if (EFI_ERROR(Status)) {
        EfiPrintError(L"Failed to read config file\n");
        gBS->FreePool(Buffer);
        return Status;
    }
    
    Buffer[ReadSize / sizeof(CHAR16)] = L'\0';
    Str = Buffer;
    
    while (*Str != L'\0') {
        Value = NULL;
        Str = ConfigFindKey(Str, L"title", &Value);
        if (Value) {
            ConfigParseValue(Value, Config->Title, sizeof(Config->Title) / sizeof(CHAR16));
            continue;
        }
        
        Value = NULL;
        Str = ConfigFindKey(Str, L"timeout", &Value);
        if (Value) {
            CHAR16 TimeoutStr[16];
            ConfigParseValue(Value, TimeoutStr, sizeof(TimeoutStr) / sizeof(CHAR16));
            Config->Timeout = (UINT32)AsciiStrLen((CHAR8*)TimeoutStr);
            continue;
        }
        
        Value = NULL;
        Str = ConfigFindKey(Str, L"default", &Value);
        if (Value) {
            CHAR16 DefaultStr[16];
            ConfigParseValue(Value, DefaultStr, sizeof(DefaultStr) / sizeof(CHAR16));
            Config->DefaultEntry = (UINT32)AsciiStrLen((CHAR8*)DefaultStr);
            continue;
        }
        
        Value = NULL;
        Str = ConfigFindKey(Str, L"entry", &Value);
        if (Value && CurrentEntry < MAX_BOOT_ENTRIES) {
            BOOT_ENTRY* Entry = &Config->Entries[CurrentEntry];
            
            ConfigParseValue(Value, Entry->Name, sizeof(Entry->Name) / sizeof(CHAR16));
            Entry->Enabled = TRUE;
            
            Value = NULL;
            Str = ConfigFindKey(Str, L"kernel", &Value);
            if (Value) {
                ConfigParseValue(Value, Entry->KernelPath, sizeof(Entry->KernelPath) / sizeof(CHAR16));
            }
            
            Value = NULL;
            Str = ConfigFindKey(Str, L"initrd", &Value);
            if (Value) {
                ConfigParseValue(Value, Entry->InitrdPath, sizeof(Entry->InitrdPath) / sizeof(CHAR16));
            }
            
            Value = NULL;
            Str = ConfigFindKey(Str, L"args", &Value);
            if (Value) {
                ConfigParseValue(Value, Entry->Args, sizeof(Entry->Args) / sizeof(CHAR16));
            }
            
            Value = NULL;
            Str = ConfigFindKey(Str, L"default", &Value);
            if (Value) {
                CHAR16 DefaultStr[16];
                ConfigParseValue(Value, DefaultStr, sizeof(DefaultStr) / sizeof(CHAR16));
                Entry->Default = (AsciiStrCmp((CHAR8*)DefaultStr, "true") == 0);
            }
            
            CurrentEntry++;
        }
    }
    
    Config->EntryCount = CurrentEntry;
    gBS->FreePool(Buffer);
    
    return EFI_SUCCESS;
}

EFI_STATUS ConfigSave(CHAR16* Path, BOOT_CONFIG* Config) {
    return EFI_UNSUPPORTED;
}

EFI_STATUS ConfigGetDefaultEntry(BOOT_CONFIG* Config, BOOT_ENTRY** Entry) {
    UINT32 i;
    
    for (i = 0; i < Config->EntryCount; i++) {
        if (Config->Entries[i].Default && Config->Entries[i].Enabled) {
            *Entry = &Config->Entries[i];
            return EFI_SUCCESS;
        }
    }
    
    if (Config->EntryCount > 0 && Config->Entries[0].Enabled) {
        *Entry = &Config->Entries[0];
        return EFI_SUCCESS;
    }
    
    return EFI_NOT_FOUND;
}

EFI_STATUS ConfigAddEntry(BOOT_CONFIG* Config, BOOT_ENTRY* Entry) {
    if (Config->EntryCount >= MAX_BOOT_ENTRIES) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    MemCpy(&Config->Entries[Config->EntryCount], Entry, sizeof(BOOT_ENTRY));
    Config->EntryCount++;
    
    return EFI_SUCCESS;
}

EFI_STATUS ConfigRemoveEntry(BOOT_CONFIG* Config, UINT32 Index) {
    UINT32 i;
    
    if (Index >= Config->EntryCount) {
        return EFI_NOT_FOUND;
    }
    
    for (i = Index; i < Config->EntryCount - 1; i++) {
        MemCpy(&Config->Entries[i], &Config->Entries[i + 1], sizeof(BOOT_ENTRY));
    }
    
    Config->EntryCount--;
    
    return EFI_SUCCESS;
}