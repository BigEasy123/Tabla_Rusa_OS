#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdint.h>
size_t console_get_row(void);
size_t console_get_col(void);
void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char *str);
void console_update_cursor(void);
void console_enable_cursor(uint8_t start, uint8_t end);
void console_set_cursor(size_t new_row, size_t new_col);
void console_backspace(void);

#endif
