#ifndef EFIDEF_H
#define EFIDEF_H

#include <stdint.h>

typedef uint8_t  BOOLEAN;
typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef UINT64 EFI_STATUS;
typedef void* EFI_HANDLE;
typedef uint16_t CHAR16;

#define EFI_SUCCESS 0

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

#endif
