#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_clear_output(void);
void console_scroll(int delta);
void console_scroll_cmd(char* arg);
uint32_t console_scrollback_lines(void);
uint32_t console_scroll_offset(void);
void console_framebuffer_terminal(int enabled);
void console_set_pointer(uint32_t x, uint32_t y, uint32_t buttons);
void console_putc(char c);
void console_puts(const char* s);
void console_write_hex(uint32_t value);
void console_write_dec(uint32_t value);
void console_input_clear(void);
void console_input_write(const char* s);
void console_input_write_at(const char* s, size_t cursor_col);

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char* s);

#endif
