#ifndef FB_H
#define FB_H

#include <stdint.h>

void fb_init(void);
void fb_bootstrap(uint32_t mb_info_addr);
void fb_map_hardware(void);
int fb_hardware_ready(void);
uint32_t fb_width(void);
uint32_t fb_height(void);
uint32_t fb_bpp(void);
uint32_t fb_pitch(void);
uint32_t fb_type(void);
void fb_clear(uint32_t color);
void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color);
void fb_set_mouse(uint32_t x, uint32_t y, uint32_t buttons);
void fb_set_cursor_style(const char* style);
const char* fb_cursor_style(void);
uint32_t fb_checksum(void);
void fb_draw_wallpaper(const char* name, int animate);
void fb_draw_saver_backdrop(const char* name);
void fb_run_saver(const char* name, uint32_t frames);
void fb_cmd(char* arg);

#endif
