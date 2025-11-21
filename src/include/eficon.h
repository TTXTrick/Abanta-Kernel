#ifndef EFICON_H
#define EFICON_H

typedef struct {
    EFI_STATUS (*Reset)(void *Self, BOOLEAN ExtendedVerification);
    EFI_STATUS (*OutputString)(void *Self, CHAR16 *String);
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// You can add more later if needed

#endif
