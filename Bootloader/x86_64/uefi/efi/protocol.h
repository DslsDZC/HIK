#ifndef HIK_UEFI_PROTOCOL_H
#define HIK_UEFI_PROTOCOL_H

#include "types.h"

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    {0x5B1B31A1, 0x9562, 0x11D2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

#define EFI_DEVICE_PATH_PROTOCOL_GUID \
    {0x09576E91, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define EFI_BLOCK_IO_PROTOCOL_GUID \
    {0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}}

#define EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID \
    {0x8b843e20, 0x8132, 0x4852, {0x90, 0xcc, 0x55, 0x1a, 0x4e, 0x4a, 0x7f, 0x1c}}

#define EFI_GLOBAL_VARIABLE_GUID \
    {0x8BE4DF61, 0x93CA, 0x11D2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C}}

#define EFI_FILE_INFO_GUID \
    {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct _EFI_GUID {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

extern EFI_GUID gEfiGlobalVariableGuid;
extern EFI_GUID gEfiFileInfoGuid;
extern EFI_GUID gEfiSimpleFileSystemProtocolGuid;
extern EFI_GUID gEfiFileInfoGuid;

typedef struct _EFI_DEVICE_PATH {
    UINT8   Type;
    UINT8   SubType;
    UINT8   Length[2];
} EFI_DEVICE_PATH;

#define END_DEVICE_PATH_TYPE                 0x7f
#define END_ENTIRE_DEVICE_PATH_SUBTYPE       0xff
#define END_INSTANCE_DEVICE_PATH_SUBTYPE     0x01

#define MEDIA_DEVICE_PATH                    0x04
#define MEDIA_HARDDRIVE_DP                   0x01
#define MEDIA_FILEPATH_DP                    0x04

typedef struct _EFI_LOADED_IMAGE_PROTOCOL {
    UINT32                          Revision;
    EFI_HANDLE                      ParentHandle;
    EFI_SYSTEM_TABLE*               SystemTable;
    EFI_HANDLE                      DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL*       FilePath;
    void*                           Reserved;
    UINT32                          LoadOptionsSize;
    void*                           LoadOptions;
    void*                           ImageBase;
    UINT64                          ImageSize;
    EFI_MEMORY_TYPE                 ImageCodeType;
    EFI_MEMORY_TYPE                 ImageDataType;
    EFI_STATUS                      (*Unload)(EFI_HANDLE ImageHandle);
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64                          Revision;
    EFI_STATUS                      (*OpenVolume)(
        struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* This,
        struct _EFI_FILE_PROTOCOL**              Root
    );
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct _EFI_FILE_PROTOCOL {
    UINT64                          Revision;
    EFI_STATUS                      (*Open)(
        struct _EFI_FILE_PROTOCOL*  This,
        struct _EFI_FILE_PROTOCOL** NewHandle,
        CHAR16*                     FileName,
        UINT64                      OpenMode,
        UINT64                      Attributes
    );
    EFI_STATUS                      (*Close)(struct _EFI_FILE_PROTOCOL* This);
    EFI_STATUS                      (*Delete)(struct _EFI_FILE_PROTOCOL* This);
    EFI_STATUS                      (*Read)(
        struct _EFI_FILE_PROTOCOL*  This,
        UINTN*                      BufferSize,
        void*                       Buffer
    );
    EFI_STATUS                      (*Write)(
        struct _EFI_FILE_PROTOCOL*  This,
        UINTN*                      BufferSize,
        void*                       Buffer
    );
    EFI_STATUS                      (*GetPosition)(
        struct _EFI_FILE_PROTOCOL*  This,
        UINT64*                     Position
    );
    EFI_STATUS                      (*SetPosition)(
        struct _EFI_FILE_PROTOCOL*  This,
        UINT64                      Position
    );
    EFI_STATUS                      (*GetInfo)(
        struct _EFI_FILE_PROTOCOL*  This,
        EFI_GUID*                   InformationType,
        UINTN*                      BufferSize,
        void*                       Buffer
    );
    EFI_STATUS                      (*SetInfo)(
        struct _EFI_FILE_PROTOCOL*  This,
        EFI_GUID*                   InformationType,
        UINTN                       BufferSize,
        void*                       Buffer
    );
    EFI_STATUS                      (*Flush)(struct _EFI_FILE_PROTOCOL* This);
} EFI_FILE_PROTOCOL;

#define EFI_FILE_MODE_READ          0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE         0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE        0x8000000000000000ULL

typedef struct _EFI_BLOCK_IO_PROTOCOL {
    UINT64                          Revision;
    EFI_HANDLE                      Media;
    EFI_STATUS                      (*Reset)(
        struct _EFI_BLOCK_IO_PROTOCOL* This,
        BOOLEAN                      ExtendedVerification
    );
    EFI_STATUS                      (*ReadBlocks)(
        struct _EFI_BLOCK_IO_PROTOCOL* This,
        UINT32                        MediaId,
        EFI_LBA                       LBA,
        UINTN                         BufferSize,
        void*                         Buffer
    );
    EFI_STATUS                      (*WriteBlocks)(
        struct _EFI_BLOCK_IO_PROTOCOL* This,
        UINT32                        MediaId,
        EFI_LBA                       LBA,
        UINTN                         BufferSize,
        void*                         Buffer
    );
    EFI_STATUS                      (*FlushBlocks)(struct _EFI_BLOCK_IO_PROTOCOL* This);
} EFI_BLOCK_IO_PROTOCOL;

typedef struct {
    UINT32                          MediaId;
    BOOLEAN                         RemovableMedia;
    BOOLEAN                         MediaPresent;
    BOOLEAN                         LogicalPartition;
    BOOLEAN                         ReadOnly;
    BOOLEAN                         WriteCaching;
    UINT32                          BlockSize;
    UINT32                          IoAlign;
    EFI_LBA                         LastBlock;
} EFI_BLOCK_IO_MEDIA;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    UINT64                          Revision;
    EFI_STATUS                      (*QueryMode)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL* This,
        UINT32                        ModeNumber,
        UINTN*                        SizeOfInfo,
        struct _EFI_GRAPHICS_OUTPUT_MODE_INFORMATION** Info
    );
    EFI_STATUS                      (*SetMode)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL* This,
        UINT32                        ModeNumber
    );
    EFI_STATUS                      (*Blt)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL* This,
        struct _EFI_GRAPHICS_OUTPUT_BLT_PIXEL*   BltBuffer,
        EFI_GRAPHICS_OUTPUT_BLT_OPERATION        BltOperation,
        UINTN                                     SourceX,
        UINTN                                     SourceY,
        UINTN                                     DestinationX,
        UINTN                                     DestinationY,
        UINTN                                     Width,
        UINTN                                     Height,
        UINTN                                     Delta
    );
    struct _EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    UINT32                          MaxMode;
    UINT32                          Mode;
    struct _EFI_GRAPHICS_OUTPUT_MODE_INFORMATION*  Info;
    UINTN                           SizeOfInfo;
    EFI_PHYSICAL_ADDRESS            FrameBufferBase;
    UINTN                           FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    UINT32                          Version;
    UINT32                          HorizontalResolution;
    UINT32                          VerticalResolution;
    enum {
        PixelRedGreenBlueReserved8BitPerColor,
        PixelBlueGreenRedReserved8BitPerColor,
        PixelBitMask,
        PixelBltOnly,
        PixelFormatMax
    }                               PixelFormat;
    struct {
        UINT32                      RedMask;
        UINT32                      GreenMask;
        UINT32                      BlueMask;
        UINT32                      ReservedMask;
    }                               PixelInformation;
    UINT32                          PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

typedef struct {
    UINT8   Blue;
    UINT8   Green;
    UINT8   Red;
    UINT8   Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef struct {
    CHAR8* (*ConvertDeviceNodeToText)(
        const EFI_DEVICE_PATH_PROTOCOL* DeviceNode,
        BOOLEAN                         DisplayOnly,
        BOOLEAN                         AllowShortcuts
    );
    CHAR8* (*ConvertDevicePathToText)(
        const EFI_DEVICE_PATH_PROTOCOL* DevicePath,
        BOOLEAN                         DisplayOnly,
        BOOLEAN                         AllowShortcuts
    );
} EFI_DEVICE_PATH_TO_TEXT_PROTOCOL;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef struct {
    UINT32                          Type;
    UINT32                          Pad;
    EFI_PHYSICAL_ADDRESS            PhysicalStart;
    EFI_VIRTUAL_ADDRESS             VirtualStart;
    UINT64                          NumberOfPages;
    UINT64                          Attribute;
} EFI_MEMORY_DESCRIPTOR;

#endif