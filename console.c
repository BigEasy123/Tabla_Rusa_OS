#include <stdint.h>
#include "console.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDRESS 0xB8000
#define VGA_COLOR 0x07

// --- Forward declarations for static helpers ---
static void console_scroll(void);
static inline void outb(uint16_t port, uint8_t val);
static inline uint8_t inb(uint16_t port);
static uint16_t vga_entry(char c, uint8_t color);

// --- VGA state ---
static uint16_t *vga_buffer;
static size_t row = 0;
static size_t col = 0;
static uint8_t color = VGA_COLOR;

// --- Helper functions ---
static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}
void console_backspace(void);

static void console_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
    }
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    row = VGA_HEIGHT - 1;
    col = 0;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// --- Console functions ---
void console_init(void) {
    vga_buffer = (uint16_t*) VGA_ADDRESS;
    console_clear();
    console_enable_cursor(0, 15); // make cursor visible
}

void console_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', color);
    row = 0;
    col = 0;
    console_update_cursor();
}

void console_putc(char c) {
    if (c == '\n') {
        col = 0;
        if (++row >= VGA_HEIGHT) console_scroll();
    } else {
        vga_buffer[row * VGA_WIDTH + col] = vga_entry(c, color);
        if (++col >= VGA_WIDTH) {
            col = 0;
            if (++row >= VGA_HEIGHT) console_scroll();
        }
    }
    console_update_cursor();
}


void console_write(const char *str) {
    for (size_t i = 0; str[i]; i++)
        console_putc(str[i]);
}

void console_update_cursor(void) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
void console_set_cursor(size_t new_row, size_t new_col) {
    if (new_row >= VGA_HEIGHT) new_row = VGA_HEIGHT - 1;
    if (new_col >= VGA_WIDTH)  new_col = VGA_WIDTH - 1;

    row = new_row;
    col = new_col;
    console_update_cursor();
}

void console_enable_cursor(uint8_t start, uint8_t end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}
void console_backspace(void) {
    if (col > 0) {
        col--;  // move cursor back
        vga_buffer[row * VGA_WIDTH + col] = vga_entry(' ', color); // clear char
    } else if (row > 0) {
        row--;         // move up a row
        col = VGA_WIDTH - 1;
        vga_buffer[row * VGA_WIDTH + col] = vga_entry(' ', color);
    }
    console_update_cursor();
}

size_t console_get_row(void) { return row; }
size_t console_get_col(void) { return col; }
