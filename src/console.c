#include "console.h"
#include <stddef.h>

#define VGA_MEM ((uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25
#define VGA_INPUT_ROW (VGA_H - 2)
#define VGA_OUTPUT_H VGA_INPUT_ROW
#define COM1 0x3F8

static uint8_t vga_color = 0x07;
static uint8_t input_color = 0x0F;
static size_t cx = 0;
static size_t cy = 0;

static inline void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port){
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void cursor_set(size_t row, size_t col){
    uint16_t pos = (uint16_t)(row * VGA_W + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void serial_init(void){
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putc(char c){
    if(c == '\n')
        serial_putc('\r');
    for(int i=0; i<100000 && !(inb(COM1 + 5) & 0x20); i++) {}
    outb(COM1, (uint8_t)c);
}

void serial_puts(const char* s){
    while(*s) serial_putc(*s++);
}

static void scroll_if_needed(void){
    if(cy < VGA_OUTPUT_H)
        return;
    for(size_t r=1; r<VGA_OUTPUT_H; r++)
        for(size_t col=0; col<VGA_W; col++)
            VGA_MEM[(r-1)*VGA_W+col] = VGA_MEM[r*VGA_W+col];
    for(size_t col=0; col<VGA_W; col++)
        VGA_MEM[(VGA_OUTPUT_H-1)*VGA_W+col] = ((uint16_t)vga_color<<8) | ' ';
    cy = VGA_OUTPUT_H - 1;
}

void console_clear(void){
    for(size_t i=0; i<VGA_W*VGA_H; i++)
        VGA_MEM[i] = ((uint16_t)vga_color<<8) | ' ';
    cx = 0;
    cy = 0;
    cursor_set(VGA_INPUT_ROW, 2);
}

void console_init(void){
    serial_init();
    console_clear();
}

void console_clear_output(void){
    for(size_t row=0; row<VGA_OUTPUT_H; row++)
        for(size_t col=0; col<VGA_W; col++)
            VGA_MEM[row*VGA_W+col] = ((uint16_t)vga_color<<8) | ' ';
    cx = 0;
    cy = 0;
}

void console_input_clear(void){
    for(size_t col=0; col<VGA_W; col++)
        VGA_MEM[VGA_INPUT_ROW*VGA_W+col] = ((uint16_t)input_color<<8) | ' ';
    for(size_t col=0; col<VGA_W; col++)
        VGA_MEM[(VGA_H-1)*VGA_W+col] = ((uint16_t)vga_color<<8) | ' ';
    cursor_set(VGA_INPUT_ROW, 0);
}

void console_input_write(const char* s){
    console_input_clear();
    size_t col = 0;
    while(col < VGA_W && s[col]){
        VGA_MEM[VGA_INPUT_ROW*VGA_W+col] = ((uint16_t)input_color<<8) | (uint8_t)s[col];
        col++;
    }
    cursor_set(VGA_INPUT_ROW, col < VGA_W ? col : VGA_W - 1);
}

void console_putc(char c){
    serial_putc(c);
    if(c == '\r'){
        cx = 0;
        return;
    }
    if(c == '\n'){
        cx = 0;
        cy++;
        scroll_if_needed();
        return;
    }
    if(c == '\b'){
        if(cx > 0){
            cx--;
            VGA_MEM[cy*VGA_W+cx] = ((uint16_t)vga_color<<8) | ' ';
        }
        return;
    }
    VGA_MEM[cy*VGA_W+cx] = ((uint16_t)vga_color<<8) | (uint8_t)c;
    if(++cx >= VGA_W){
        cx = 0;
        cy++;
        scroll_if_needed();
    }
}

void console_puts(const char* s){
    while(*s) console_putc(*s++);
}

void console_write_hex(uint32_t value){
    static const char hex[] = "0123456789ABCDEF";
    console_puts("0x");
    for(int shift=28; shift>=0; shift-=4)
        console_putc(hex[(value >> shift) & 0xF]);
}

void console_write_dec(uint32_t value){
    char buf[11];
    int i = 0;
    if(value == 0){
        console_putc('0');
        return;
    }
    while(value > 0 && i < 10){
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while(i > 0)
        console_putc(buf[--i]);
}
