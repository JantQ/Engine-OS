#pragma once

#include <cstdint>


#define EFI_SUCCESS 0


typedef void* EFI_HANDLE;
typedef uint64_t UINTN;
typedef uint16_t CHAR16;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint8_t  UINT8;
typedef uint64_t EFI_STATUS;
typedef uint64_t UINT64;

typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;

typedef struct{
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

typedef struct{
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

// EFI Memory
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;


typedef struct{
  UINT32 Type;
  EFI_PHYSICAL_ADDRESS PhysicalStart;
  EFI_VIRTUAL_ADDRESS VirtualStart;
  UINT64 NumberOfPages;
  UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

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




// EFI Boot Services
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_LOCATE_PROTOCOL) (EFI_GUID* Protocol, void* Registration, void** Interface);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_GET_MEMORY_MAP) (UINTN *MemoryMapSize, EFI_MEMORY_DESCRIPTOR *MemoryMap, UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_FREE_POOL) (void *Buffer);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_ALLOCATE_POOL)(EFI_MEMORY_TYPE PoolType, UINTN Size, void** Buffer);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_EXIT_BOOT_SERVICES) (EFI_HANDLE ImageHandle, UINTN MapKey);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_STALL) (UINTN Microseconds);


typedef struct{
    EFI_TABLE_HEADER Hdr;
    void *pad[4];
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    void *pad1[20];
    EFI_EXIT_BOOT_SERVICES ExitBootServices;
    void *pad3[1];
    EFI_STALL Stall;
    void *pad2[8];
    EFI_LOCATE_PROTOCOL LocateProtocol;
} EFI_BOOT_SERVICES;




// Simple Text Input Protocotl

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef struct{
  UINT16 ScanCode;
  CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_INPUT_READ_KEY) (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key);

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
  void *Reset;
  EFI_INPUT_READ_KEY ReadKeyStroke;
  void *WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;





// Simple Text Output Proctol
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_RESET) (struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const bool ExtendedVerification);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_STRING) (struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const CHAR16 *String);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_SET_CURSOR_POSITION) (struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, UINTN Column, UINTN Row);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_TEXT_ENABLE_CURSOR) (struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, bool Visible);



typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL{
  EFI_TEXT_RESET Reset;
  EFI_TEXT_STRING OutputString;
  void *pad[5];
  EFI_TEXT_SET_CURSOR_POSITION SetCursorPosition;
  EFI_TEXT_ENABLE_CURSOR EnableCursor;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// Storage

typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_GET_VARIABLE) (CHAR16 *VariableName, EFI_GUID *VendorGuid, UINT32 *Attributes, UINTN *DataSize, void *Data);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_SET_VARIABLE) (CHAR16 *VariableName, EFI_GUID *VendorGuid, UINT32 Attrbiutes, UINTN DataSize, void *Data);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_RESET_SYSTEM) (UINT32 ResetType, EFI_STATUS ResetStatus, UINTN DataSize, void *ResetData);

typedef struct{
    EFI_TABLE_HEADER Hdr;
    void *pad[6];
    EFI_GET_VARIABLE getVariable;
    void *GetNextVariableName;
    EFI_SET_VARIABLE SetVariable;
    void *GetNextHighMonotonicCount;
    EFI_RESET_SYSTEM ResetSystem;
} EFI_RUNTIME_SERVICES;


// System Table
typedef struct{
    EFI_TABLE_HEADER Hdr;
    void *FirmwareVendor;
    UINT32 FirmawareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    void *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    UINT32 RedMask, GreenMask, BlueMask, ReservedMask;
} EFI_PIXEL_BITMASK;

typedef struct{
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelFormat;
    EFI_PIXEL_BITMASK PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct{
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    UINTN FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE) (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber, UINTN *SizeOfInfo, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info);
typedef EFI_STATUS (__attribute__((ms_abi)) *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE) (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 ModeNumber);

struct EFI_GRAPHICS_OUTPUT_PROTOCOL{
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE SetMode;
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

