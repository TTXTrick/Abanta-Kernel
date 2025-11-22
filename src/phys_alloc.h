#ifndef PHYS_ALLOC_H
#define PHYS_ALLOC_H

#include <efi.h>
#include <efilib.h>

/* Initialize physical allocator with a base physical address and length (both page-aligned).
   This must be called after ExitBootServices (with memory obtained from the memory map).
*/
void phys_init(uintptr_t base, size_t length);

/* allocate/free single pages (4KiB) */
void *phys_alloc_page(void);
void phys_free_page(void *page);

/* simple kmalloc using pages (very tiny) */
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
