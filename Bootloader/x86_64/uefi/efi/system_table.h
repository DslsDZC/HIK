#ifndef HIK_UEFI_SYSTEM_TABLE_H
#define HIK_UEFI_SYSTEM_TABLE_H

#include "types.h"
#include "protocol.h"

typedef struct {
    CHAR16*                         Signature;
    UINT32                          Revision;
    UINT32                          HeaderSize;
    UINT32                          CRC32;
    UINT32                          Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    UINT64                          Signature;
    UINT32                          Revision;
    UINT32                          HeaderSize;
    UINT32                          CRC32;
    UINT32                          Reserved;
} EFI_RUNTIME_SERVICES;

typedef struct {
    EFI_STATUS                      (*RaiseTPL)(UINTN NewTPL);
    void                            (*RestoreTPL)(UINTN OldTPL);
    EFI_STATUS                      (*AllocatePages)(
        EFI_ALLOCATE_TYPE            Type,
        EFI_MEMORY_TYPE              MemoryType,
        UINTN                        NumberOfPages,
        EFI_PHYSICAL_ADDRESS*        Memory
    );
    EFI_STATUS                      (*FreePages)(
        EFI_PHYSICAL_ADDRESS         Memory,
        UINTN                        NumberOfPages
    );
    EFI_STATUS                      (*GetMemoryMap)(
        UINTN*                       MemoryMapSize,
        EFI_MEMORY_DESCRIPTOR*       MemoryMap,
        UINTN*                       MapKey,
        UINTN*                       DescriptorSize,
        UINT32*                      DescriptorVersion
    );
    EFI_STATUS                      (*AllocatePool)(
        EFI_MEMORY_TYPE              PoolType,
        UINTN                        Size,
        void**                       Buffer
    );
    EFI_STATUS                      (*FreePool)(void* Buffer);
    EFI_STATUS                      (*CreateEvent)(
        UINT32                       Type,
        UINTN                       NotifyTpl,
        void                        (*NotifyFunction)(EFI_EVENT Event, void* Context),
        void*                        Context,
        EFI_EVENT*                   Event
    );
    EFI_STATUS                      (*SetTimer)(
        EFI_EVENT                    Event,
        EFI_TIMER_DELAY              Type,
        UINT64                       TriggerTime
    );
    EFI_STATUS                      (*WaitForEvent)(
        UINTN                        NumberOfEvents,
        EFI_EVENT*                   Event,
        UINTN*                       Index
    );
    EFI_STATUS                      (*SignalEvent)(EFI_EVENT Event);
    EFI_STATUS                      (*CloseEvent)(EFI_EVENT Event);
    EFI_STATUS                      (*CheckEvent)(EFI_EVENT Event);
} EFI_BOOT_SERVICES_32;

typedef struct {
    UINT64                          RaiseTPL;
    UINT64                          RestoreTPL;
    UINT64                          AllocatePages;
    UINT64                          FreePages;
    UINT64                          GetMemoryMap;
    UINT64                          AllocatePool;
    UINT64                          FreePool;
    UINT64                          CreateEvent;
    UINT64                          SetTimer;
    UINT64                          WaitForEvent;
    UINT64                          SignalEvent;
    UINT64                          CloseEvent;
    UINT64                          CheckEvent;
} EFI_BOOT_SERVICES_64;

typedef struct {
    EFI_TABLE_HEADER                Hdr;
    EFI_STATUS                      (*RaiseTPL)(UINTN NewTPL);
    void                            (*RestoreTPL)(UINTN OldTPL);
    EFI_STATUS                      (*AllocatePages)(
        EFI_ALLOCATE_TYPE            Type,
        EFI_MEMORY_TYPE              MemoryType,
        UINTN                        NumberOfPages,
        EFI_PHYSICAL_ADDRESS*        Memory
    );
    EFI_STATUS                      (*FreePages)(
        EFI_PHYSICAL_ADDRESS         Memory,
        UINTN                        NumberOfPages
    );
    EFI_STATUS                      (*GetMemoryMap)(
        UINTN*                       MemoryMapSize,
        EFI_MEMORY_DESCRIPTOR*       MemoryMap,
        UINTN*                       MapKey,
        UINTN*                       DescriptorSize,
        UINT32*                      DescriptorVersion
    );
    EFI_STATUS                      (*AllocatePool)(
        EFI_MEMORY_TYPE              PoolType,
        UINTN                        Size,
        void**                       Buffer
    );
    EFI_STATUS                      (*FreePool)(void* Buffer);
    EFI_STATUS                      (*CreateEvent)(
        UINT32                       Type,
        UINTN                       NotifyTpl,
        void                        (*NotifyFunction)(EFI_EVENT Event, void* Context),
        void*                        Context,
        EFI_EVENT*                   Event
    );
    EFI_STATUS                      (*SetTimer)(
        EFI_EVENT                    Event,
        EFI_TIMER_DELAY              Type,
        UINT64                       TriggerTime
    );
    EFI_STATUS                      (*WaitForEvent)(
        UINTN                        NumberOfEvents,
        EFI_EVENT*                   Event,
        UINTN*                       Index
    );
    EFI_STATUS                      (*SignalEvent)(EFI_EVENT Event);
    EFI_STATUS                      (*CloseEvent)(EFI_EVENT Event);
    EFI_STATUS                      (*CheckEvent)(EFI_EVENT Event);
    EFI_STATUS                      (*InstallProtocolInterface)(
        EFI_HANDLE*                  Handle,
        EFI_GUID*                    Protocol,
        EFI_INTERFACE_TYPE           InterfaceType,
        void*                        Interface
    );
    EFI_STATUS                      (*ReinstallProtocolInterface)(
        EFI_HANDLE                   Handle,
        EFI_GUID*                    Protocol,
        void*                        OldInterface,
        void*                        NewInterface
    );
    EFI_STATUS                      (*UninstallProtocolInterface)(
        EFI_HANDLE                   Handle,
        EFI_GUID*                    Protocol,
        void*                        Interface
    );
    EFI_STATUS                      (*HandleProtocol)(
        EFI_HANDLE                   Handle,
        EFI_GUID*                    Protocol,
        void**                       Interface
    );
    void*                           Reserved;
    EFI_STATUS                      (*RegisterProtocolNotify)(
        EFI_GUID*                    Protocol,
        EFI_EVENT                    Event,
        void**                       Registration
    );
    EFI_STATUS                      (*LocateHandle)(
        EFI_LOCATE_SEARCH_TYPE       SearchType,
        EFI_GUID*                    Protocol,
        void*                        SearchKey,
        UINTN*                       BufferSize,
        EFI_HANDLE*                  Buffer
    );
    EFI_STATUS                      (*LocateDevicePath)(
        EFI_GUID*                    Protocol,
        EFI_DEVICE_PATH_PROTOCOL**   DevicePath,
        EFI_HANDLE*                  Device
    );
    EFI_STATUS                      (*InstallConfigurationTable)(
        EFI_GUID*                    Guid,
        void*                        Table
    );
    EFI_STATUS                      (*LoadImage)(
        BOOLEAN                      BootPolicy,
        EFI_HANDLE                   ParentImageHandle,
        EFI_DEVICE_PATH_PROTOCOL*    DevicePath,
        void*                        SourceBuffer,
        UINTN                        SourceSize,
        EFI_HANDLE*                  ImageHandle
    );
    EFI_STATUS                      (*StartImage)(
        EFI_HANDLE                   ImageHandle,
        UINTN*                       ExitDataSize,
        CHAR16**                     ExitData
    );
    EFI_STATUS                      (*Exit)(
        EFI_HANDLE                   ImageHandle,
        EFI_STATUS                   ExitStatus,
        UINTN                        ExitDataSize,
        CHAR16*                      ExitData
    );
    EFI_STATUS                      (*UnloadImage)(EFI_HANDLE ImageHandle);
    EFI_STATUS                      (*ExitBootServices)(
        EFI_HANDLE                   ImageHandle,
        UINTN                        MapKey
    );
    EFI_STATUS                      (*GetNextMonotonicCount)(UINT64* Count);
    EFI_STATUS                      (*Stall)(UINTN Microseconds);
    EFI_STATUS                      (*SetWatchdogTimer)(
        UINTN                        Timeout,
        UINT64                       WatchdogCode,
        UINTN                        DataSize,
        CHAR16*                      WatchdogData
    );
    EFI_STATUS                      (*ConnectController)(
        EFI_HANDLE                   ControllerHandle,
        EFI_HANDLE*                  DriverImageHandle,
        EFI_DEVICE_PATH_PROTOCOL*    RemainingDevicePath,
        BOOLEAN                      Recursive
    );
    EFI_STATUS                      (*DisconnectController)(
        EFI_HANDLE                   ControllerHandle,
        EFI_HANDLE                   DriverImageHandle,
        EFI_HANDLE                   ChildHandle
    );
    EFI_STATUS                      (*OpenProtocol)(
        EFI_HANDLE                   Handle,
        EFI_GUID*                    Protocol,
        void**                       Interface,
        EFI_HANDLE                   AgentHandle,
        EFI_HANDLE                   ControllerHandle,
        UINT32                       Attributes
    );
    EFI_STATUS                      (*CloseProtocol)(
        EFI_HANDLE                   Handle,
        EFI_GUID*                    Protocol,
        EFI_HANDLE                   AgentHandle,
        EFI_HANDLE                   ControllerHandle
    );
    EFI_STATUS                      (*OpenProtocolInformation)(
        EFI_HANDLE                   Handle,
        EFI_GUID*                    Protocol,
        struct _EFI_OPEN_PROTOCOL_INFORMATION_ENTRY**  EntryBuffer,
        UINTN*                       EntryCount
    );
    EFI_STATUS                      (*ProtocolsPerHandle)(
        EFI_HANDLE                   Handle,
        EFI_GUID***                  ProtocolBuffer,
        UINTN*                       ProtocolBufferCount
    );
    EFI_STATUS                      (*LocateHandleBuffer)(
        EFI_LOCATE_SEARCH_TYPE       SearchType,
        EFI_GUID*                    Protocol,
        void*                        SearchKey,
        UINTN*                       NoHandles,
        EFI_HANDLE**                 Buffer
    );
    EFI_STATUS                      (*LocateProtocol)(
        EFI_GUID*                    Protocol,
        void*                        Registration,
        void**                       Interface
    );
    EFI_STATUS                      (*InstallMultipleProtocolInterfaces)(
        EFI_HANDLE*                  Handle,
        ...
    );
    EFI_STATUS                      (*UninstallMultipleProtocolInterfaces)(
        EFI_HANDLE                   Handle,
        ...
    );
    EFI_STATUS                      (*CalculateCrc32)(
        void*                        Data,
        UINTN                        DataSize,
        UINT32*                      Crc32
    );
    void                            (*CopyMem)(void* Destination, void* Source, UINTN Length);
    void                            (*SetMem)(void* Buffer, UINTN Size, UINT8 Value);
    EFI_STATUS                      (*CreateEventEx)(
        UINT32                       Type,
        UINTN                        NotifyTpl,
        void                        (*NotifyFunction)(EFI_EVENT Event, void* Context),
        void*                        Context,
        EFI_GUID*                    EventGroup,
        EFI_EVENT*                   Event
    );
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER                Hdr;
    EFI_STATUS                      (*GetTime)(
        struct _EFI_TIME*            Time,
        struct _EFI_TIME_CAPABILITIES* Capabilities
    );
    EFI_STATUS                      (*SetTime)(struct _EFI_TIME* Time);
    EFI_STATUS                      (*GetWakeupTime)(
        BOOLEAN*                    Enabled,
        BOOLEAN*                    Pending,
        struct _EFI_TIME*            Time
    );
    EFI_STATUS                      (*SetWakeupTime)(
        BOOLEAN                      Enabled,
        struct _EFI_TIME*            Time
    );
    EFI_STATUS                      (*SetVirtualAddressMap)(
        UINTN                        MemoryMapSize,
        UINTN                        DescriptorSize,
        UINT32                       DescriptorVersion,
        EFI_MEMORY_DESCRIPTOR*       VirtualMap
    );
    EFI_STATUS                      (*ConvertPointer)(
        UINTN                        DebugDisposition,
        void**                       Address
    );
    EFI_STATUS                      (*GetVariable)(
        CHAR16*                      VariableName,
        EFI_GUID*                    VendorGuid,
        UINT32*                      Attributes,
        UINTN*                       DataSize,
        void*                        Data
    );
    EFI_STATUS                      (*GetNextVariableName)(
        UINTN*                       VariableNameSize,
        CHAR16*                      VariableName,
        EFI_GUID*                    VendorGuid
    );
    EFI_STATUS                      (*SetVariable)(
        CHAR16*                      VariableName,
        EFI_GUID*                    VendorGuid,
        UINT32                       Attributes,
        UINTN                        DataSize,
        void*                        Data
    );
    EFI_STATUS                      (*GetNextHighMonotonicCount)(UINT32* HighCount);
    EFI_STATUS                      (*ResetSystem)(
        EFI_RESET_TYPE               ResetType,
        EFI_STATUS                   ResetStatus,
        UINTN                        DataSize,
        void*                        Data
    );
    EFI_STATUS                      (*UpdateCapsule)(
        struct _EFI_CAPSULE_HEADER** CapsuleHeaderArray,
        UINTN                        CapsuleCount,
        EFI_PHYSICAL_ADDRESS         ScatterGatherList
    );
    EFI_STATUS                      (*QueryCapsuleCapabilities)(
        struct _EFI_CAPSULE_HEADER** CapsuleHeaderArray,
        UINTN                        CapsuleCount,
        UINT64*                       MaximumCapsuleSize,
        EFI_RESET_TYPE*              ResetType
    );
    EFI_STATUS                      (*QueryVariableInfo)(
        UINT32                       Attributes,
        UINT64*                       MaximumVariableStorageSize,
        UINT64*                       RemainingVariableStorageSize,
        UINT64*                       MaximumVariableSize
    );
} EFI_RUNTIME_SERVICES;

typedef struct {
    CHAR16*                         Signature;
    UINT32                          Revision;
    UINT32                          HeaderSize;
    UINT32                          CRC32;
    UINT32                          Reserved;
    EFI_HANDLE                      FirmwareVendor;
    UINT32                          FirmwareRevision;
    EFI_HANDLE                      ConsoleInHandle;
    struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL*   ConIn;
    EFI_HANDLE                      ConsoleOutHandle;
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*  ConOut;
    EFI_HANDLE                      StandardErrorHandle;
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*  StdErr;
    EFI_RUNTIME_SERVICES*           RuntimeServices;
    EFI_BOOT_SERVICES*              BootServices;
    UINTN                           NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE*        ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    UINT32                          Attributes;
    UINT64                          VendorGuid;
    void*                           VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    UINT16                          Year;
    UINT8                           Month;
    UINT8                           Day;
    UINT8                           Hour;
    UINT8                           Minute;
    UINT8                           Second;
    UINT8                           Pad1;
    UINT32                          Nanosecond;
    INT16                           TimeZone;
    UINT8                           Daylight;
    UINT8                           Pad2;
} EFI_TIME;

typedef struct {
    UINT32                          Resolution;
    UINT32                          Accuracy;
    BOOLEAN                         SetsToZero;
} EFI_TIME_CAPABILITIES;

typedef struct {
    CHAR16                          *ResetString;
    CHAR16                          *ResetString2;
} EFI_RESET_NOTIFICATION;

typedef enum {
    EfiResetCold,
    EfiResetWarm,
    EfiResetShutdown,
    EfiResetPlatformSpecific
} EFI_RESET_TYPE;

typedef struct {
    CHAR16                          *String;
    UINT32                          Attributes;
} EFI_STRING_ID;

typedef struct {
    EFI_TABLE_HEADER                Hdr;
    EFI_STATUS                      (*Reset)(
        struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL* This,
        BOOLEAN                      ExtendedVerification
    );
    EFI_STATUS                      (*ReadKeyStroke)(
        struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL* This,
        struct _EFI_INPUT_KEY*       Key
    );
    EFI_EVENT                       WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef struct {
    UINT16                          ScanCode;
    CHAR16                          UnicodeChar;
} EFI_INPUT_KEY;

typedef struct {
    EFI_TABLE_HEADER                Hdr;
    EFI_STATUS                      (*Reset)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        BOOLEAN                      ExtendedVerification
    );
    EFI_STATUS                      (*OutputString)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        CHAR16*                      String
    );
    EFI_STATUS                      (*TestString)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        CHAR16*                      String
    );
    EFI_STATUS                      (*QueryMode)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN                        ModeNumber,
        UINTN*                       Columns,
        UINTN*                       Rows
    );
    EFI_STATUS                      (*SetMode)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN                        ModeNumber
    );
    EFI_STATUS                      (*SetAttribute)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN                        Attribute
    );
    EFI_STATUS                      (*ClearScreen)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This
    );
    EFI_STATUS                      (*SetCursorPosition)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        UINTN                        Column,
        UINTN                        Row
    );
    EFI_STATUS                      (*EnableCursor)(
        struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* This,
        BOOLEAN                      Enable
    );
    EFI_SIMPLE_TEXT_OUTPUT_MODE*    Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    INTN                            MaxMode;
    INTN                            Mode;
    INTN                            Attribute;
    INTN                            CursorColumn;
    INTN                            CursorRow;
    BOOLEAN                         CursorVisible;
} EFI_SIMPLE_TEXT_OUTPUT_MODE;

#define EFI_TEXT_BLACK              0x00
#define EFI_TEXT_BLUE               0x01
#define EFI_TEXT_GREEN              0x02
#define EFI_TEXT_CYAN               0x03
#define EFI_TEXT_RED                0x04
#define EFI_TEXT_MAGENTA            0x05
#define EFI_TEXT_BROWN              0x06
#define EFI_TEXT_LIGHTGRAY          0x07
#define EFI_TEXT_BRIGHT             0x08
#define EFI_TEXT_DARKGRAY           0x08
#define EFI_TEXT_LIGHTBLUE          0x09
#define EFI_TEXT_LIGHTGREEN         0x0A
#define EFI_TEXT_LIGHTCYAN          0x0B
#define EFI_TEXT_LIGHTRED           0x0C
#define EFI_TEXT_LIGHTMAGENTA       0x0D
#define EFI_TEXT_YELLOW             0x0E
#define EFI_TEXT_WHITE              0x0F

#define EFI_BACKGROUND_BLACK        0x00
#define EFI_BACKGROUND_BLUE         0x10
#define EFI_BACKGROUND_GREEN        0x20
#define EFI_BACKGROUND_CYAN         0x30
#define EFI_BACKGROUND_RED          0x40
#define EFI_BACKGROUND_MAGENTA      0x50
#define EFI_BACKGROUND_BROWN        0x60
#define EFI_BACKGROUND_LIGHTGRAY    0x70

typedef enum {
    EFI_LOCATE_BY_HANDLE,
    EFI_LOCATE_BY_PROTOCOL,
    EFI_LOCATE_BY_REGISTER_NOTIFY
} EFI_LOCATE_SEARCH_TYPE;

typedef enum {
    EfiAnyPages,
    EfiMaxAddress,
    EfiAllocateAddress,
    EfiAllocateMaxAddress
} EFI_ALLOCATE_TYPE;

typedef enum {
    EfiTimerCancel,
    EfiTimerPeriodic,
    EfiTimerRelative
} EFI_TIMER_DELAY;

typedef enum {
    EfiOpenProtocolByHandleProtocol,
    EfiOpenProtocolByDriver,
    EfiOpenProtocolByChildController
} EFI_OPEN_PROTOCOL_ATTRIBUTE;

typedef struct {
    EFI_HANDLE                      AgentHandle;
    EFI_HANDLE                      ControllerHandle;
    UINT32                          Attributes;
    UINT32                          OpenCount;
} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;

#endif