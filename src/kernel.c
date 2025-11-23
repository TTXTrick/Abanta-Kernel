// src/kernel.c
#include <stdint.h>

static volatile uint16_t* vga = (uint16_t*)0xB8000;
static uint8_t row = 0, col = 0;

static void putchar(char c) {
    if (c == '\n') { row++; col = 0; return; }
    uint16_t v = (0x07 << 8) | c;
    vga[row * 80 + col] = v;
    col++;
}

static void print(const char* s) {
    while (*s) putchar(*s++);
}

void kernel_main() {
    print("Abanta kernel loaded.\n");
    print("abanta> ");
    while (1) __asm__("hlt");
}
