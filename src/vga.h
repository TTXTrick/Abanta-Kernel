/* src/vga.h */
#ifndef VGA_H
#define VGA_H
#include <stdint.h>

void vga_clear(void);
void vga_putc(char c);
void vga_write(const char *s);
void vga_write_hex(uint64_t val);

#endif
