#ifndef _EFI_DEF_H_
#define _EFI_DEF_H_

#include <stdint.h>

/*
 * UEFI basic types
 */

typedef uint8_t     BOOLEAN;
typedef uint8_t     UINT8;
typedef uint16_t    UINT16;
typedef uint32_t    UINT32;
typedef uint64_t    UINT64;

typedef int8_t      INT8;
typedef int16_t     INT16;
typedef int32_t     INT32;
typedef int64_t     INT64;

typedef uint64_t    UINTN;
typedef int64_t     INTN;

typedef UINT16      CHAR16;
typedef char        CHAR8;

typedef void       *EFI_HANDLE;
typedef UINT64      EFI_STATUS;

/*
 * GUID
 */
typedef struct {
    UINT32  Data1;
    UINT16  Data2;
    UINT16  Data3;
    UINT8   Data4[8];
} EFI_GUID;

/*
 * Table Header present in all major tables
 */
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/*
 * Physical & Virtual Addresses
 */
typedef UINT64 EFI_PHYSICAL_ADDRESS;
typedef UINT64 EFI_VIRTUAL_ADDRESS;

/*
 * EFI_MEMORY_TYPE Enumeration
 * (full list, per UEFI Spec)
 */
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
    EfiUnacceptedMemoryType,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/*
 * Memory Descriptor (used by GetMemoryMap)
 */
typedef struct {
    UINT32              Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS  VirtualStart;
    UINT64              NumberOfPages;
    UINT64              Attribute;
} EFI_MEMORY_DESCRIPTOR;

/*
 * EFI_STATUS common values
 */
#define EFI_SUCCESS               0
#define EFI_LOAD_ERROR            (EFI_STATUS)(1ULL << 63 | 1)
#define EFI_INVALID_PARAMETER     (EFI_STATUS)(1ULL << 63 | 2)
#define EFI_UNSUPPORTED           (EFI_STATUS)(1ULL << 63 | 3)
#define EFI_BAD_BUFFER_SIZE       (EFI_STATUS)(1ULL << 63 | 4)
#define EFI_BUFFER_TOO_SMALL      (EFI_STATUS)(1ULL << 63 | 5)
#define EFI_NOT_READY             (EFI_STATUS)(1ULL << 63 | 6)
#define EFI_DEVICE_ERROR          (EFI_STATUS)(1ULL << 63 | 7)
#define EFI_WRITE_PROTECTED       (EFI_STATUS)(1ULL << 63 | 8)
#define EFI_OUT_OF_RESOURCES      (EFI_STATUS)(1ULL << 63 | 9)
#define EFI_VOLUME_CORRUPTED      (EFI_STATUS)(1ULL << 63 | 10)
#define EFI_VOLUME_FULL           (EFI_STATUS)(1ULL << 63 | 11)
#define EFI_NO_MEDIA              (EFI_STATUS)(1ULL << 63 | 12)
#define EFI_MEDIA_CHANGED         (EFI_STATUS)(1ULL << 63 | 13)
#define EFI_NOT_FOUND             (EFI_STATUS)(1ULL << 63 | 14)
#define EFI_ACCESS_DENIED         (EFI_STATUS)(1ULL << 63 | 15)
#define EFI_NO_RESPONSE           (EFI_STATUS)(1ULL << 63 | 16)
#define EFI_NO_MAPPING            (EFI_STATUS)(1ULL << 63 | 17)
#define EFI_TIMEOUT               (EFI_STATUS)(1ULL << 63 | 18)
#define EFI_NOT_STARTED           (EFI_STATUS)(1ULL << 63 | 19)
#define EFI_ALREADY_STARTED       (EFI_STATUS)(1ULL << 63 | 20)
#define EFI_ABORTED               (EFI_STATUS)(1ULL << 63 | 21)
#define EFI_ICMP_ERROR            (EFI_STATUS)(1ULL << 63 | 22)
#define EFI_TFTP_ERROR            (EFI_STATUS)(1ULL << 63 | 23)
#define EFI_PROTOCOL_ERROR        (EFI_STATUS)(1ULL << 63 | 24)
#define EFI_INCOMPATIBLE_VERSION  (EFI_STATUS)(1ULL << 63 | 25)
#define EFI_SECURITY_VIOLATION    (EFI_STATUS)(1ULL << 63 | 26)
#define EFI_CRC_ERROR             (EFI_STATUS)(1ULL << 63 | 27)
#define EFI_END_OF_MEDIA          (EFI_STATUS)(1ULL << 63 | 28)
#define EFI_END_OF_FILE           (EFI_STATUS)(1ULL << 63 | 31)

#endif
