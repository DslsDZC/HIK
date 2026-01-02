#include "efi.h"
#include "string.h"

EFI_HANDLE gImageHandle = NULL;
EFI_SYSTEM_TABLE* gST = NULL;
EFI_BOOT_SERVICES* gBS = NULL;
EFI_RUNTIME_SERVICES* gRT = NULL;

void EfiInitialize(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;
    
    gBS->SetWatchdogTimer(0, 0, 0, NULL);
}

void EfiPrintString(const CHAR16* String) {
    if (gST && gST->ConOut) {
        gST->ConOut->OutputString(gST->ConOut, (CHAR16*)String);
    }
}

void EfiPrintError(const CHAR16* String) {
    if (gST && gST->ConOut) {
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_RED | EFI_BACKGROUND_BLACK);
        gST->ConOut->OutputString(gST->ConOut, (CHAR16*)String);
        gST->ConOut->SetAttribute(gST->ConOut, EFI_TEXT_WHITE | EFI_BACKGROUND_BLACK);
    }
}

void EfiPrintHex(UINT64 Value) {
    CHAR16 Buffer[17];
    CHAR16 Digits[] = L"0123456789ABCDEF";
    int i;
    
    for (i = 15; i >= 0; i--) {
        Buffer[i] = Digits[Value & 0xF];
        Value >>= 4;
    }
    Buffer[16] = L'\0';
    
    EfiPrintString(Buffer);
}