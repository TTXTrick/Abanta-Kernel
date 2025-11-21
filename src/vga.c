/* src/vga.c */
#include "vga.h"
#include <stddef.h>
static uint16_t *vga_buffer = (uint16_t*)0xB8000;
static int vga_row = 0;
static int vga_col = 0;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;
static uint8_t default_attr = 0x07;

static void vga_put_entry_at(char c, uint8_t attr, int row, int col) {
    const int idx = row * VGA_WIDTH + col;
    vga_buffer[idx] = ((uint16_t)attr << 8) | (uint8_t)c;
}

void vga_clear(void) {
    for (int r=0;r<VGA_HEIGHT;r++) for (int c=0;c<VGA_WIDTH;c++) vga_put_entry_at(' ', default_attr, r, c);
    vga_row = vga_col = 0;
}

void vga_putc(char c) {
    if (c == '\n') { vga_col = 0; vga_row++; if (vga_row >= VGA_HEIGHT) vga_row = VGA_HEIGHT-1; return; }
    if (c == '\r') { vga_col = 0; return; }
    vga_put_entry_at(c, default_attr, vga_row, vga_col);
    vga_col++;
    if (vga_col >= VGA_WIDTH) { vga_col = 0; vga_row++; if (vga_row >= VGA_HEIGHT) vga_row = VGA_HEIGHT-1; }
}

void vga_write(const char *s) { for (const char *p = s; *p; ++p) vga_putc(*p); }

void vga_write_hex(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    char buf[17]; buf[16]=0;
    for (int i=15;i>=0;--i) { buf[i] = hex[val & 0xF]; val >>= 4; }
    vga_write(buf);
}
