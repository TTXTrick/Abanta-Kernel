#include <stdint.h>

#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static volatile uint16_t* vga = (uint16_t*)VGA_ADDR;
static int cursor_x = 0;
static int cursor_y = 0;

// VGA character helper
static void vga_putc(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        return;
    }

    if (cursor_y >= VGA_HEIGHT) {
        for (int y = 1; y < VGA_HEIGHT; y++)
            for (int x = 0; x < VGA_WIDTH; x++)
                vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];

        for (int x = 0; x < VGA_WIDTH; x++)
            vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ' ' | (color << 8);

        cursor_y = VGA_HEIGHT - 1;
    }

    vga[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)c | (color << 8);

    cursor_x++;
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
}

static void vga_print(const char* s, uint8_t color) {
    while (*s) vga_putc(*s++, color);
}

static void vga_clear() {
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga[y * VGA_WIDTH + x] = ' ' | (0x07 << 8);

    cursor_x = 0;
    cursor_y = 0;
}

// Keyboard port I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// US QWERTY scancode table (set 1)
static char scancode_table[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t', 'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x',
    'c','v','b','n','m',',','.','/', 0, '*', 0,' ', 0,
};

// Shell buffer
static char input_buf[128];
static int input_len = 0;

static void print_prompt() {
    vga_print("abanta> ", 0x0A);
}

static void handle_command() {
    if (input_len == 0) {
        print_prompt();
        return;
    }

    input_buf[input_len] = 0;

    if (!__builtin_strcmp(input_buf, "help")) {
        vga_print("Commands: help, clear, about\n", 0x07);
    }
    else if (!__builtin_strcmp(input_buf, "clear")) {
        vga_clear();
    }
    else if (!__builtin_strcmp(input_buf, "about")) {
        vga_print("Abanta Kernel v0.1\n", 0x07);
    }
    else {
        vga_print("Unknown command\n", 0x07);
    }

    input_len = 0;
    print_prompt();
}

void kernel_main() {
    vga_clear();
    vga_print("Abanta Kernel Loaded\n", 0x0F);
    print_prompt();

    // Main keyboard loop
    while (1) {
        if (inb(0x64) & 1) {  // keyboard buffer full
            uint8_t sc = inb(0x60);

            if (sc & 0x80) continue; // ignore key releases

            char c = scancode_table[sc];

            if (!c) continue;

            if (c == '\n') {
                vga_putc('\n', 0x07);
                handle_command();
            }
            else if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    if (cursor_x > 0) cursor_x--;
                    vga_putc(' ', 0x07);
                    cursor_x--;
                }
            }
            else {
                if (input_len < 127) {
                    input_buf[input_len++] = c;
                    vga_putc(c, 0x07);
                }
            }
        }
    }
}
