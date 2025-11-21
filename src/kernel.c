/* src/kernel.c */
#include <stdint.h>
#include "vga.h"

void kernel_main64(void) {
    vga_clear();
    vga_write("Abanta x86_64 kernel booted!\n\n");
    vga_write("Welcome to Abanta 64-bit — minimal kernel v0.1\n\n");
    vga_write("Halting in a loop...\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
