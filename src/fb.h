#ifndef FB_H
#define FB_H

#include <stdint.h>

void fb_init(void);
void fb_clear(uint32_t color);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color);
void fb_set_mouse(uint32_t x, uint32_t y, uint32_t buttons);
uint32_t fb_checksum(void);
void fb_cmd(char* arg);

#endif
