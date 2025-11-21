#ifndef EFIDEF_H
#define EFIDEF_H

#include <stdint.h>

/*
 * Basic UEFI type definitions
 */

typedef uint8_t   BOOLEAN;
typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;

typedef UINT64 EFI_STATUS;
typedef void   *EFI_HANDLE;
typedef UINT16 CHAR16;

/*
 * EFI GUID structure
 */
typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8  Data4[8];
} EFI_GUID;

/*
 * Table header common to all UEFI tables
 */
typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

/*
 * Common status codes
 */
#define EFI_SUCCESS               0
#define EFI_LOAD_ERROR            (1 | (1ULL<<63))
#define EFI_INVALID_PARAMETER     (2 | (1ULL<<63))
#define EFI_UNSUPPORTED           (3 | (1ULL<<63))
#define EFI_BAD_BUFFER_SIZE       (4 | (1ULL<<63))
#define EFI_BUFFER_TOO_SMALL      (5 | (1ULL<<63))
#define EFI_NOT_READY             (6 | (1ULL<<63))
#define EFI_DEVICE_ERROR          (7 | (1ULL<<63))
#define EFI_WRITE_PROTECTED       (8 | (1ULL<<63))
#define EFI_OUT_OF_RESOURCES      (9 | (1ULL<<63))
#define EFI_VOLUME_CORRUPTED      (10 | (1ULL<<63))
#define EFI_VOLUME_FULL           (11 | (1ULL<<63))
#define EFI_NO_MEDIA              (12 | (1ULL<<63))
#define EFI_MEDIA_CHANGED         (13 | (1ULL<<63))
#define EFI_NOT_FOUND             (14 | (1ULL<<63))
#define EFI_ACCESS_DENIED         (15 | (1ULL<<63))
#define EFI_TIMEOUT               (18 | (1ULL<<63))

/*
 * Memory Type Constants (minimal subset)
 */
#define EfiReservedMemoryType        0
#define EfiLoaderCode                1
#define EfiLoaderData                2
#define EfiBootServicesCode          3
#define EfiBootServicesData          4
#define EfiRuntimeServicesCode       5
#define EfiRuntimeServicesData       6
#define EfiConventionalMemory        7
#define EfiUnusableMemory            8
#define EfiACPIReclaimMemory         9
#define EfiACPIMemoryNVS             10
#define EfiMemoryMappedIO            11
#define EfiMemoryMappedIOPortSpace   12
#define EfiPalCode                   13
#define EfiPersistentMemory          14
#define EfiMaxMemoryType             15

#endif
