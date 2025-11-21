#ifndef _EFI_CON_H_
#define _EFI_CON_H_

#include "efidef.h"

/*
 * Simple Text Output Protocol
 */

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS (*EFI_TEXT_RESET)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
);

typedef EFI_STATUS (*EFI_TEXT_OUTPUT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
);

typedef EFI_STATUS (*EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This
);

struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET          Reset;
    EFI_TEXT_OUTPUT_STRING  OutputString;
    void                   *TestString;
    void                   *QueryMode;
    void                   *SetMode;
    void                   *SetAttribute;
    EFI_TEXT_CLEAR_SCREEN   ClearScreen;
    void                   *SetCursorPosition;
    void                   *EnableCursor;
    void                   *Mode;
};

#endif
