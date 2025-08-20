#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define LINE_BUFFER_SIZE 128

void keyboard_init(void);
void keyboard_handler(void);

// Optional: get current line buffer
char* keyboard_get_line(void);
uint32_t keyboard_get_length(void);
void keyboard_clear_line(void);
int keyboard_line_ready(void);
void keyboard_clear_line_ready(void);

#endif
