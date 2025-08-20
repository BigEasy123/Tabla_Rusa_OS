#include "keyboard.h"
#include "console.h"
#include <stdint.h>

#define DATA_PORT 0x60
#define STATUS_PORT 0x64

static const char keymap[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0
};

// Last key pressed (for debounce)
static uint8_t last_scancode = 0;

// Line buffer
static char line_buffer[LINE_BUFFER_SIZE];
static uint32_t line_length = 0;
static int line_ready = 0;  // set when Enter is pressed

// Read from port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Accessors
char* keyboard_get_line(void) { return line_buffer; }
uint32_t keyboard_get_length(void) { return line_length; }
void keyboard_clear_line(void) { line_length = 0; line_buffer[0] = 0; }
int keyboard_line_ready(void) { return line_ready; }
void keyboard_clear_line_ready(void) { line_ready = 0; }

void keyboard_handler(void) {
    uint8_t scancode = inb(DATA_PORT);

    // Key release
    if (scancode & 0x80) {
        if ((scancode & 0x7F) == last_scancode) last_scancode = 0;
        return;
    }

    // Debounce
    if (scancode == last_scancode) return;
    last_scancode = scancode;

    // Arrow keys (cursor only)
    switch (scancode) {
        case 0x48:  // Up
            console_set_cursor(console_get_row() > 0 ? console_get_row() - 1 : 0,
                               console_get_col());
            return;
        case 0x50:  // Down
            console_set_cursor(console_get_row() < 24 ? console_get_row() + 1 : 24,
                               console_get_col());
            return;
        case 0x4B:  // Left
            console_set_cursor(console_get_row(),
                               console_get_col() > 0 ? console_get_col() - 1 : 0);
            return;
        case 0x4D:  // Right
            console_set_cursor(console_get_row(),
                               console_get_col() < 79 ? console_get_col() + 1 : 79);
            return;
    }

    // Backspace
    if (scancode == 0x0E) {
        if (line_length > 0) {
            line_length--;
            line_buffer[line_length] = 0;
            console_backspace();
        }
        return;
    }

    // Enter key
    if (scancode == 0x1C) {
        console_putc('\n');
        line_buffer[line_length] = 0; // terminate string
        line_ready = 1;              // signal line ready
        line_length = 0;             // reset buffer
        return;
    }

    // Printable characters
    char c = keymap[scancode];
    if (c && line_length < LINE_BUFFER_SIZE - 1) {
        line_buffer[line_length++] = c;
        line_buffer[line_length] = 0; // keep null-terminated
        console_putc(c);
    }
}

void keyboard_init(void) {
    last_scancode = 0;
    line_length = 0;
    line_buffer[0] = 0;
    line_ready = 0;
}
