#ifndef _EFI_LIB_H_
#define _EFI_LIB_H_

#include "efi.h"
#include "eficon.h"

/*
 * Simple internal helpers
 */

static inline UINTN StrLen(CHAR16 *Str) {
    UINTN n = 0;
    while (Str[n]) n++;
    return n;
}

static inline EFI_STATUS Print(CHAR16 *String) {
    return ST->ConOut->OutputString(ST->ConOut, String);
}

static inline EFI_STATUS ClearScreen() {
    return ST->ConOut->ClearScreen(ST->ConOut);
}

static inline void *MemSet(void *buf, UINT8 value, UINTN size) {
    UINT8 *p = buf;
    while (size--) *p++ = value;
    return buf;
}

static inline void *MemCopy(void *dst, const void *src, UINTN size) {
    UINT8 *d = dst;
    const UINT8 *s = src;
    while (size--) *d++ = *s++;
    return dst;
}

#endif
