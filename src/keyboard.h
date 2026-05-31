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
#define KB_KEY_HOME 0x106
#define KB_KEY_END 0x107
#define KB_KEY_INSERT 0x108
#define KB_KEY_DELETE 0x109
#define KB_KEY_CTRL_LEFT 0x10A
#define KB_KEY_CTRL_RIGHT 0x10B
#define KB_KEY_ALT_LEFT 0x10C
#define KB_KEY_ALT_RIGHT 0x10D
#define KB_KEY_SUPER_LEFT 0x10E
#define KB_KEY_SUPER_RIGHT 0x10F

void keyboard_handler(void);
void keyboard_install(void);
int kb_read_key(void);
char kb_read_char(void);
int keyboard_ctrl_down(void);
int keyboard_alt_down(void);

#endif
