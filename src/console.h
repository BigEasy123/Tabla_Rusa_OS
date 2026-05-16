#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

void console_init(void);
void console_clear(void);
void console_clear_output(void);
void console_putc(char c);
void console_puts(const char* s);
void console_write_hex(uint32_t value);
void console_write_dec(uint32_t value);
void console_input_clear(void);
void console_input_write(const char* s);

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char* s);

#endif
