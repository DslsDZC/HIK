#include "verify.h"
#include "../efi/string.h"

EFI_STATUS VerifyKernelHeader(
    const HIK_KERNEL_HEADER* Header,
    const HIK_SIGNATURE* Signature,
    const RSA_PUBLIC_KEY* PublicKey
) {
    UINT8 Digest[SHA384_DIGEST_SIZE];
    RSA_SIGNATURE RsaSig;
    
    if (Header->Signature != HIK_KERNEL_MAGIC) {
        EfiPrintError(L"Invalid kernel magic\n");
        return EFI_INVALID_PARAMETER;
    }
    
    if (Header->Version > HIK_KERNEL_VERSION) {
        EfiPrintError(L"Unsupported kernel version\n");
        return EFI_UNSUPPORTED;
    }
    
    if (Signature->Algorithm != HIK_SIGNATURE_ALGORITHM_RSA_SHA384) {
        EfiPrintError(L"Unsupported signature algorithm\n");
        return EFI_UNSUPPORTED;
    }
    
    Sha384((const UINT8*)Header, sizeof(HIK_KERNEL_HEADER), Digest);
    
    MemCpy(RsaSig.Signature, Signature->Signature, Signature->SignatureSize);
    RsaSig.SignatureSize = Signature->SignatureSize;
    
    return RsaVerifySha384(PublicKey, Digest, &RsaSig);
}

EFI_STATUS VerifyKernelImage(
    const UINT8* Image,
    UINTN ImageSize,
    const HIK_SIGNATURE* Signature,
    const RSA_PUBLIC_KEY* PublicKey
) {
    UINT8 Digest[SHA384_DIGEST_SIZE];
    RSA_SIGNATURE RsaSig;
    
    if (ImageSize < sizeof(HIK_KERNEL_HEADER)) {
        EfiPrintError(L"Invalid image size\n");
        return EFI_INVALID_PARAMETER;
    }
    
    if (Signature->Algorithm != HIK_SIGNATURE_ALGORITHM_RSA_SHA384) {
        EfiPrintError(L"Unsupported signature algorithm\n");
        return EFI_UNSUPPORTED;
    }
    
    Sha384(Image, ImageSize, Digest);
    
    MemCpy(RsaSig.Signature, Signature->Signature, Signature->SignatureSize);
    RsaSig.SignatureSize = Signature->SignatureSize;
    
    return RsaVerifySha384(PublicKey, Digest, &RsaSig);
}

BOOLEAN IsSecureBootEnabled(VOID) {
    EFI_STATUS Status;
    UINT8* Data;
    UINTN DataSize;
    
    DataSize = 0;
    Status = gRT->GetVariable(
        L"SecureBoot",
        &gEfiGlobalVariableGuid,
        NULL,
        &DataSize,
        NULL
    );
    
    if (Status == EFI_BUFFER_TOO_SMALL && DataSize == 1) {
        Status = gBS->AllocatePool(EfiLoaderData, DataSize, (VOID**)&Data);
        if (EFI_ERROR(Status)) {
            return FALSE;
        }
        
        Status = gRT->GetVariable(
            L"SecureBoot",
            &gEfiGlobalVariableGuid,
            NULL,
            &DataSize,
            Data
        );
        
        if (!EFI_ERROR(Status) && Data[0] == 1) {
            gBS->FreePool(Data);
            return TRUE;
        }
        
        gBS->FreePool(Data);
    }
    
    return FALSE;
}

EFI_STATUS VerifySecureBoot(VOID) {
    if (IsSecureBootEnabled()) {
        EfiPrintString(L"Secure Boot is enabled\n");
        return EFI_SUCCESS;
    } else {
        EfiPrintString(L"Secure Boot is disabled\n");
        return EFI_SUCCESS;
    }
}