#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void mouse_init(void);
void mouse_install(void);
void mouse_handler(void);
void mouse_move(int dx, int dy);
void mouse_set(int x, int y);
void mouse_button(uint32_t button, int down);
uint32_t mouse_x(void);
uint32_t mouse_y(void);
uint32_t mouse_buttons(void);
uint32_t mouse_event_count(void);
void mouse_cmd(char* arg);

#endif
