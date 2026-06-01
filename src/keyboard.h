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
#define KB_KEY_F1 0x110
#define KB_KEY_F2 0x111
#define KB_KEY_F3 0x112
#define KB_KEY_F4 0x113
#define KB_KEY_F5 0x114
#define KB_KEY_F6 0x115
#define KB_KEY_F7 0x116
#define KB_KEY_F8 0x117
#define KB_KEY_F9 0x118
#define KB_KEY_F10 0x119
#define KB_KEY_F11 0x11A
#define KB_KEY_F12 0x11B

void keyboard_handler(void);
void keyboard_install(void);
int kb_read_key(void);
char kb_read_char(void);
int keyboard_ctrl_down(void);
int keyboard_alt_down(void);
int keyboard_caps_on(void);
int keyboard_num_on(void);
int keyboard_scroll_on(void);
const char* keyboard_type(void);
const char* keyboard_layout(void);
void keyboard_cmd(char* arg);

#endif
