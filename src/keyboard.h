#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KB_DATA 0x60
#define KB_KEY_UP 0x100
#define KB_KEY_DOWN 0x101
#define KB_KEY_LEFT 0x102
#define KB_KEY_RIGHT 0x103
#define KB_KEY_PAGE_UP 0x104
#define KB_KEY_PAGE_DOWN 0x105

void keyboard_handler(void);
void keyboard_install(void);
int kb_read_key(void);
char kb_read_char(void);

#endif
