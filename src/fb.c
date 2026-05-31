#include <stdint.h>
#include "console.h"
#include "fb.h"
#include "fs.h"
#include "jobs.h"
#include "paging.h"

#define FB_RASTER_W 96
#define FB_RASTER_H 54
#define MB2_TAG_FRAMEBUFFER 8
#define CURSOR_BACK_MAX 96

struct cursor_back_pixel {
    uint32_t x;
    uint32_t y;
    uint32_t color;
};

struct fb_state {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t surfaces;
    uint32_t pixels;
    uint32_t rects;
    uint32_t glyphs;
    uint32_t frames;
    uint32_t hw_addr;
    uint32_t hw_pitch;
    uint32_t hw_type;
    int hw_ready;
    int mouse_x;
    int mouse_y;
    int old_mouse_x;
    int old_mouse_y;
};

static struct fb_state fb = {640, 480, 32, 3, 0, 0, 0, 0, 0, 0, 0, 0, 32, 24, -1, -1};
static uint32_t raster[FB_RASTER_H][FB_RASTER_W];
static char saver_name[16] = "none";
static char cursor_style[16] = "dot";
static struct cursor_back_pixel cursor_back[CURSOR_BACK_MAX];
static uint32_t cursor_back_count = 0;

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_framebuffer_tag {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t fb_type;
    uint16_t reserved;
} __attribute__((packed));

static char lower_char(char c){
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static int str_eq(const char* a, const char* b){
    while(*a && *b){
        if(lower_char(*a) != lower_char(*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char* first_arg(char* arg, char** rest){
    while(is_space(*arg)) arg++;
    char* start = arg;
    while(*arg && !is_space(*arg)) arg++;
    if(*arg){
        *arg = 0;
        arg++;
    }
    while(is_space(*arg)) arg++;
    *rest = arg;
    return start;
}

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static uint32_t align8(uint32_t value){
    return (value + 7U) & ~7U;
}

static uint32_t color32(uint32_t color){
    uint32_t v = color & 0xFF;
    if(color > 0xFF)
        return color;
    return (v << 16) | (v << 8) | v;
}

static void hw_put_pixel(uint32_t x, uint32_t y, uint32_t color){
    if(!fb.hw_ready || fb.bpp != 32 || x >= fb.width || y >= fb.height)
        return;
    uint32_t* px = (uint32_t*)(fb.hw_addr + y * fb.hw_pitch + x * 4);
    *px = color32(color);
}

static uint32_t hw_get_pixel(uint32_t x, uint32_t y){
    if(!fb.hw_ready || fb.bpp != 32 || x >= fb.width || y >= fb.height)
        return 0;
    uint32_t* px = (uint32_t*)(fb.hw_addr + y * fb.hw_pitch + x * 4);
    return *px;
}

static void cursor_restore(void){
    for(uint32_t i=0; i<cursor_back_count; i++)
        hw_put_pixel(cursor_back[i].x, cursor_back[i].y, cursor_back[i].color);
    cursor_back_count = 0;
}

static void cursor_save_pixel(uint32_t x, uint32_t y){
    if(cursor_back_count >= CURSOR_BACK_MAX || x >= fb.width || y >= fb.height)
        return;
    for(uint32_t i=0; i<cursor_back_count; i++)
        if(cursor_back[i].x == x && cursor_back[i].y == y)
            return;
    cursor_back[cursor_back_count].x = x;
    cursor_back[cursor_back_count].y = y;
    cursor_back[cursor_back_count].color = hw_get_pixel(x, y);
    cursor_back_count++;
}

static void cursor_save_region(uint32_t x, uint32_t y){
    if(!fb.hw_ready || fb.bpp != 32)
        return;
    for(int d=-12; d<=12; d++){
        int px = (int)x + d;
        int py = (int)y + d;
        if(px >= 0 && px < (int)fb.width)
            cursor_save_pixel((uint32_t)px, y);
        if(py >= 0 && py < (int)fb.height)
            cursor_save_pixel(x, (uint32_t)py);
    }
    for(int oy=-3; oy<=3; oy++){
        for(int ox=-3; ox<=3; ox++){
            int px = (int)x + ox;
            int py = (int)y + oy;
            if(px >= 0 && px < (int)fb.width && py >= 0 && py < (int)fb.height)
                cursor_save_pixel((uint32_t)px, (uint32_t)py);
        }
    }
}

static void hw_draw_cursor(uint32_t x, uint32_t y, uint32_t buttons){
    uint32_t cross = buttons ? 0xFF4040 : 0xFFFFFF;
    uint32_t center = 0x000000;
    for(int d=-12; d<=12; d++){
        int px = (int)x + d;
        int py = (int)y + d;
        if(px >= 0 && px < (int)fb.width)
            hw_put_pixel((uint32_t)px, y, cross);
        if(py >= 0 && py < (int)fb.height)
            hw_put_pixel(x, (uint32_t)py, cross);
    }
    if(str_eq(cursor_style, "dot") || str_eq(cursor_style, "target")){
        for(int oy=-3; oy<=3; oy++){
            for(int ox=-3; ox<=3; ox++){
                if(ox * ox + oy * oy > 10)
                    continue;
                int px = (int)x + ox;
                int py = (int)y + oy;
                if(px >= 0 && px < (int)fb.width && py >= 0 && py < (int)fb.height)
                    hw_put_pixel((uint32_t)px, (uint32_t)py, center);
            }
        }
    }
}

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(max == 0) return;
    while(src && src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void fb_clear(uint32_t color){
    for(uint32_t y=0; y<FB_RASTER_H; y++)
        for(uint32_t x=0; x<FB_RASTER_W; x++)
            raster[y][x] = color;
    if(fb.hw_ready && fb.bpp == 32){
        uint32_t c = color32(color);
        for(uint32_t y=0; y<fb.height; y++){
            uint32_t* row = (uint32_t*)(fb.hw_addr + y * fb.hw_pitch);
            for(uint32_t x=0; x<fb.width; x++)
                row[x] = c;
        }
    }
    fb.old_mouse_x = -1;
    fb.old_mouse_y = -1;
    cursor_back_count = 0;
}

void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color){
    uint32_t rx;
    uint32_t ry;
    if(fb.width == 0 || fb.height == 0) return;
    rx = (x * FB_RASTER_W) / fb.width;
    ry = (y * FB_RASTER_H) / fb.height;
    if(rx >= FB_RASTER_W) rx = FB_RASTER_W - 1;
    if(ry >= FB_RASTER_H) ry = FB_RASTER_H - 1;
    raster[ry][rx] = color;
    hw_put_pixel(x, y, color);
    fb.pixels++;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color){
    uint32_t x2 = x + w;
    uint32_t y2 = y + h;
    if(w == 0 || h == 0) return;
    for(uint32_t py=y; py<y2; py++)
        for(uint32_t px=x; px<x2; px++)
            fb_put_pixel(px, py, color);
    fb.rects++;
}

static uint8_t glyph_row(char c, uint32_t row){
    static const uint8_t digits[10][7] = {
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
        {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
        {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
        {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
        {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}
    };
    static const uint8_t letters[26][7] = {
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
        {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
        {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
        {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
        {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
        {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
        {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},
        {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
        {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}
    };
    if(row >= 7) return 0;
    if(c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if(c >= '0' && c <= '9') return digits[c - '0'][row];
    if(c >= 'A' && c <= 'Z') return letters[c - 'A'][row];
    switch(c){
        case ' ': return 0;
        case '.': return row == 6 ? 0x04 : 0;
        case ',': return row == 5 ? 0x04 : (row == 6 ? 0x08 : 0);
        case ':': return row == 2 || row == 5 ? 0x04 : 0;
        case ';': return row == 2 ? 0x04 : (row == 5 ? 0x04 : (row == 6 ? 0x08 : 0));
        case '-': return row == 3 ? 0x1F : 0;
        case '_': return row == 6 ? 0x1F : 0;
        case '/': return 0x01 << (4 - row > 4 ? 0 : (4 - row));
        case '\\': return row < 5 ? (0x10 >> row) : 0x01;
        case '|': return 0x04;
        case '+': return row == 3 ? 0x1F : (row >= 1 && row <= 5 ? 0x04 : 0);
        case '=': return row == 2 || row == 4 ? 0x1F : 0;
        case '$': return row == 0 ? 0x04 : (row == 1 ? 0x0F : (row == 2 ? 0x14 : (row == 3 ? 0x0E : (row == 4 ? 0x05 : (row == 5 ? 0x1E : 0x04)))));
        case '[': return row == 0 || row == 6 ? 0x0E : 0x08;
        case ']': return row == 0 || row == 6 ? 0x0E : 0x02;
        case '(': return row == 0 || row == 6 ? 0x02 : 0x04;
        case ')': return row == 0 || row == 6 ? 0x08 : 0x04;
        case '<': return row < 3 ? (0x02 << row) : (row <= 5 ? (0x10 >> (row - 3)) : 0);
        case '>': return row < 3 ? (0x08 >> row) : (row <= 5 ? (0x02 << (row - 3)) : 0);
        default: return row == 0 || row == 6 ? 0x1F : 0x11;
    }
}

static uint8_t glyph_pixel(char c, uint32_t gx, uint32_t gy){
    uint8_t row = glyph_row(c, gy);
    return (row & (uint8_t)(0x10 >> gx)) != 0;
}

void fb_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color){
    uint32_t cursor = x;
    for(uint32_t i=0; text && text[i]; i++){
        if(text[i] == '\n'){
            cursor = x;
            y += 8;
            continue;
        }
        for(uint32_t gy=0; gy<7; gy++)
            for(uint32_t gx=0; gx<5; gx++)
                if(glyph_pixel(text[i], gx, gy))
                    fb_put_pixel(cursor + gx, y + gy, color);
        cursor += 6;
        fb.glyphs++;
    }
}

void fb_set_mouse(uint32_t x, uint32_t y, uint32_t buttons){
    if(x >= fb.width) x = fb.width ? fb.width - 1 : 0;
    if(y >= fb.height) y = fb.height ? fb.height - 1 : 0;
    cursor_restore();
    fb.mouse_x = (int)x;
    fb.mouse_y = (int)y;
    fb.old_mouse_x = (int)x;
    fb.old_mouse_y = (int)y;
    cursor_save_region(x, y);
    hw_draw_cursor(x, y, buttons);
}

void fb_set_cursor_style(const char* style){
    if(style && (str_eq(style, "cross") || str_eq(style, "dot") || str_eq(style, "target")))
        copy_text(cursor_style, style, sizeof(cursor_style));
    fb_set_mouse((uint32_t)fb.mouse_x, (uint32_t)fb.mouse_y, 0);
}

const char* fb_cursor_style(void){
    return cursor_style;
}

uint32_t fb_checksum(void){
    uint32_t checksum = 0;
    for(uint32_t y=0; y<FB_RASTER_H; y++)
        for(uint32_t x=0; x<FB_RASTER_W; x++)
            checksum = (checksum << 5) ^ (checksum >> 2) ^ raster[y][x] ^ (x * 17) ^ (y * 31);
    return checksum;
}

static char shade_for(uint32_t color){
    uint32_t v = color & 0xFF;
    if(v < 16) return ' ';
    if(v < 48) return '.';
    if(v < 96) return ':';
    if(v < 144) return '*';
    if(v < 192) return 'o';
    if(v < 224) return 'O';
    return '#';
}

static int cursor_at(uint32_t x, uint32_t y){
    uint32_t cx = ((uint32_t)fb.mouse_x * FB_RASTER_W) / (fb.width ? fb.width : 1);
    uint32_t cy = ((uint32_t)fb.mouse_y * FB_RASTER_H) / (fb.height ? fb.height : 1);
    int dx;
    int dy;
    if(cx >= FB_RASTER_W) cx = FB_RASTER_W - 1;
    if(cy >= FB_RASTER_H) cy = FB_RASTER_H - 1;
    dx = (int)x - (int)cx;
    dy = (int)y - (int)cy;
    return (dx == 0 && dy >= -2 && dy <= 2) || (dy == 0 && dx >= -2 && dx <= 2);
}

static void fb_dump_region(uint32_t max_w, uint32_t max_h){
    if(max_w == 0 || max_w > FB_RASTER_W) max_w = 48;
    if(max_h == 0 || max_h > FB_RASTER_H) max_h = 18;
    for(uint32_t y=0; y<max_h; y++){
        for(uint32_t x=0; x<max_w; x++)
            console_putc(cursor_at(x, y) ? '+' : shade_for(raster[y][x]));
        console_putc('\n');
    }
}

static void hw_blit_raster(void){
    if(!fb.hw_ready || fb.bpp != 32)
        return;
    for(uint32_t y=0; y<fb.height; y++){
        uint32_t ry = (y * FB_RASTER_H) / fb.height;
        for(uint32_t x=0; x<fb.width; x++){
            uint32_t rx = (x * FB_RASTER_W) / fb.width;
            hw_put_pixel(x, y, raster[ry][rx]);
        }
    }
    cursor_back_count = 0;
}

static uint32_t wave_value(uint32_t x, uint32_t y, uint32_t frame){
    uint32_t a = (x * 7 + frame * 11) & 63;
    uint32_t b = (y * 9 + frame * 5) & 63;
    return ((a ^ b) * 4) & 255;
}

static uint32_t mix_channel(uint32_t a, uint32_t b, uint32_t t, uint32_t max){
    if(max == 0) return a & 0xFF;
    return (((a & 0xFF) * (max - t)) + ((b & 0xFF) * t)) / max;
}

static uint32_t mix_color(uint32_t a, uint32_t b, uint32_t t, uint32_t max){
    uint32_t ar = (a >> 16) & 0xFF;
    uint32_t ag = (a >> 8) & 0xFF;
    uint32_t ab = a & 0xFF;
    uint32_t br = (b >> 16) & 0xFF;
    uint32_t bg = (b >> 8) & 0xFF;
    uint32_t bb = b & 0xFF;
    return (mix_channel(ar, br, t, max) << 16) |
           (mix_channel(ag, bg, t, max) << 8) |
           mix_channel(ab, bb, t, max);
}

static void wallpaper_gradient(uint32_t top, uint32_t bottom){
    uint32_t h = fb.height ? fb.height : 1;
    for(uint32_t y=0; y<h; y+=4){
        uint32_t color = mix_color(top, bottom, y, h - 1);
        fb_fill_rect(0, y, fb.width, 4, color);
    }
}

static void wallpaper_disk(int cx, int cy, int radius, uint32_t inner, uint32_t outer){
    int r2 = radius * radius;
    if(radius <= 0) return;
    for(int y=cy-radius; y<=cy+radius; y++){
        if(y < 0 || y >= (int)fb.height) continue;
        for(int x=cx-radius; x<=cx+radius; x++){
            if(x < 0 || x >= (int)fb.width) continue;
            int dx = x - cx;
            int dy = y - cy;
            int d2 = dx * dx + dy * dy;
            if(d2 > r2) continue;
            uint32_t color = mix_color(inner, outer, (uint32_t)d2, (uint32_t)r2);
            fb_put_pixel((uint32_t)x, (uint32_t)y, color);
        }
    }
}

static void wallpaper_line(uint32_t y, uint32_t color){
    if(y < fb.height)
        fb_fill_rect(0, y, fb.width, 2, color);
}

void fb_draw_wallpaper(const char* name, int animate){
    uint32_t frame = animate ? fb.frames++ : fb.frames;
    if(!name || !name[0])
        name = "calm";
    copy_text(saver_name, name, sizeof(saver_name));
    if(str_eq(name, "rain")){
        wallpaper_gradient(0x1F3442, 0x0D141C);
        for(uint32_t i=0; i<42; i++){
            uint32_t x = (i * 47 + (animate ? frame * 3 : 0)) % (fb.width ? fb.width : 1);
            uint32_t y = (i * 71 + (animate ? frame * 5 : 0)) % (fb.height ? fb.height : 1);
            fb_fill_rect(x, y, 2, 18, 0x8AB8C8);
        }
    } else if(str_eq(name, "stars")){
        wallpaper_gradient(0x10172A, 0x05070D);
        for(uint32_t i=0; i<120; i++){
            uint32_t x = (i * 53 + (animate ? frame : 0)) % (fb.width ? fb.width : 1);
            uint32_t y = (i * 31 + (i % 7) * 19) % (fb.height ? fb.height : 1);
            uint32_t color = (i % 5 == 0) ? 0xD8E8FF : 0x7F98B8;
            fb_fill_rect(x, y, 2, 2, color);
        }
    } else if(str_eq(name, "waves")){
        wallpaper_gradient(0x143C50, 0x071820);
        for(uint32_t y=120; y<fb.height; y+=44){
            uint32_t drift = animate ? (frame + y) % 28 : y % 28;
            wallpaper_line(y + drift, 0x3F8794);
            wallpaper_line(y + 14 + drift / 2, 0x73B2B8);
        }
    } else {
        wallpaper_gradient(0x263040, 0x0D1218);
        uint32_t shift = animate ? (frame % 80) : 0;
        wallpaper_disk(270 + (int)shift / 4, 230, 118, 0xB85A42, 0x273040);
        wallpaper_disk(502, 390 + (int)shift / 6, 156, 0x7D486E, 0x17202A);
        wallpaper_disk(760 - (int)shift / 5, 230, 104, 0xD18A46, 0x202838);
        fb_fill_rect(0, fb.height > 170 ? fb.height - 170 : 0, fb.width, 170, 0x101820);
        for(uint32_t y=fb.height > 170 ? fb.height - 170 : 0; y<fb.height; y+=14)
            wallpaper_line(y, 0x182431);
    }
    cursor_back_count = 0;
}

static void saver_lava(uint32_t frame){
    fb_clear(8);
    for(uint32_t y=0; y<FB_RASTER_H; y++){
        for(uint32_t x=0; x<FB_RASTER_W; x++){
            uint32_t blob1 = ((x + frame * 2) % 31) + ((y + frame) % 17);
            uint32_t blob2 = ((x * 2 + 19 - frame) % 43) + ((y * 3 + frame) % 29);
            uint32_t heat = 255 - ((blob1 * blob1 + blob2) & 255);
            raster[y][x] = heat;
        }
    }
}

static void saver_rain(uint32_t frame){
    fb_clear(8);
    for(uint32_t x=0; x<FB_RASTER_W; x+=3){
        uint32_t drop = (x * 7 + frame * 3) % FB_RASTER_H;
        for(uint32_t tail=0; tail<7; tail++){
            uint32_t y = (drop + FB_RASTER_H - tail) % FB_RASTER_H;
            raster[y][x] = 220 - tail * 24;
        }
    }
}

static void saver_stars(uint32_t frame){
    fb_clear(0);
    for(uint32_t i=0; i<160; i++){
        uint32_t x = (i * 37 + frame * (1 + (i % 5))) % FB_RASTER_W;
        uint32_t y = (i * 19 + frame * (1 + (i % 3))) % FB_RASTER_H;
        raster[y][x] = 80 + ((i * 29 + frame * 13) & 127);
    }
}

static void saver_waves(uint32_t frame){
    for(uint32_t y=0; y<FB_RASTER_H; y++)
        for(uint32_t x=0; x<FB_RASTER_W; x++)
            raster[y][x] = wave_value(x, y, frame);
}

static void fb_saver(const char* name, uint32_t frames){
    if(frames == 0) frames = 1;
    copy_text(saver_name, name, sizeof(saver_name));
    for(uint32_t frame=0; frame<frames; frame++){
        if(str_eq(name, "lava")) saver_lava(fb.frames + frame);
        else if(str_eq(name, "rain")) saver_rain(fb.frames + frame);
        else if(str_eq(name, "stars")) saver_stars(fb.frames + frame);
        else if(str_eq(name, "waves")) saver_waves(fb.frames + frame);
        else saver_lava(fb.frames + frame);
        jobs_account("screensaver", 4);
    }
    fb.frames += frames;
    hw_blit_raster();
    console_puts("screensaver ");
    console_puts(saver_name);
    console_puts(" frames=");
    console_write_dec(fb.frames);
    console_puts(" checksum=");
    console_write_dec(fb_checksum());
    console_putc('\n');
}

void fb_run_saver(const char* name, uint32_t frames){
    fb_saver(name && name[0] ? name : "lava", frames);
}

void fb_draw_saver_backdrop(const char* name){
    fb_draw_wallpaper(name, 1);
}

static void fb_draw_demo(void){
    fb_clear(12);
    fb_fill_rect(20, 20, 600, 40, 180);
    fb_fill_rect(32, 88, 260, 180, 70);
    fb_fill_rect(320, 88, 280, 180, 110);
    fb_fill_rect(32, 300, 568, 110, 40);
    fb_draw_text(36, 32, "Tabla Rusa GUI", 245);
    fb_draw_text(52, 112, "shell", 210);
    fb_draw_text(340, 112, "inspector", 230);
    fb_draw_text(52, 324, "framebuffer surfaces online", 190);
}

void fb_init(void){
    fb_clear(0);
    fs_mkdir("/system/gui");
    fs_write("/system/gui/framebuffer.txt",
        "mode=requested-1024x768x32\n"
        "raster=96x54\n"
        "hardware=multiboot2-linear-framebuffer\n"
        "surfaces=3\n"
        "font=5x7-soft\n"
        "screensavers=lava,rain,stars,waves\n"
        "mouse=32,24\n");
    fs_append_line("/var/log/system.log", "fb: raster framebuffer and font renderer online");
}

void fb_bootstrap(uint32_t mb_info_addr){
    if(mb_info_addr == 0)
        return;
    uint32_t total_size = *(uint32_t*)mb_info_addr;
    uint32_t ptr = mb_info_addr + 8;
    uint32_t end = mb_info_addr + total_size;
    while(ptr + sizeof(struct mb2_tag) <= end){
        struct mb2_tag* tag = (struct mb2_tag*)ptr;
        if(tag->type == 0)
            break;
        if(tag->type == MB2_TAG_FRAMEBUFFER){
            struct mb2_framebuffer_tag* fbt = (struct mb2_framebuffer_tag*)ptr;
            if((fbt->addr >> 32) == 0 && fbt->width && fbt->height && fbt->pitch){
                fb.hw_addr = (uint32_t)fbt->addr;
                fb.hw_pitch = fbt->pitch;
                fb.width = fbt->width;
                fb.height = fbt->height;
                fb.bpp = fbt->bpp;
                fb.hw_type = fbt->fb_type;
            }
            return;
        }
        ptr += align8(tag->size);
    }
}

void fb_map_hardware(void){
    if(fb.hw_addr == 0 || fb.hw_pitch == 0 || fb.height == 0)
        return;
    paging_identity_map_range(fb.hw_addr, fb.hw_pitch * fb.height);
    fb.hw_ready = 1;
}

int fb_hardware_ready(void){
    return fb.hw_ready;
}

void fb_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    jobs_account("framebuffer", 1);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("fb mode=");
        console_write_dec(fb.width);
        console_putc('x');
        console_write_dec(fb.height);
        console_putc('x');
        console_write_dec(fb.bpp);
        console_puts(" hardware=");
        console_puts(fb.hw_ready ? "on" : "off");
        if(fb.hw_ready){
            console_puts(" addr=");
            console_write_hex(fb.hw_addr);
            console_puts(" pitch=");
            console_write_dec(fb.hw_pitch);
        }
        console_puts(" raster=");
        console_write_dec(FB_RASTER_W);
        console_putc('x');
        console_write_dec(FB_RASTER_H);
        console_puts(" surfaces=");
        console_write_dec(fb.surfaces);
        console_puts(" pixels=");
        console_write_dec(fb.pixels);
        console_puts(" rects=");
        console_write_dec(fb.rects);
        console_puts(" glyphs=");
        console_write_dec(fb.glyphs);
        console_puts(" saver=");
        console_puts(saver_name);
        console_puts(" cursor=");
        console_puts(cursor_style);
        console_puts(" mouse=");
        console_write_dec((uint32_t)fb.mouse_x);
        console_putc(',');
        console_write_dec((uint32_t)fb.mouse_y);
        console_puts(" checksum=");
        console_write_dec(fb_checksum());
        console_putc('\n');
    } else if(str_eq(action, "mode")){
        fb.width = parse_u32(first_arg(rest, &rest));
        fb.height = parse_u32(first_arg(rest, &rest));
        fb.bpp = parse_u32(first_arg(rest, &rest));
        console_puts("fb: mode updated\n");
    } else if(str_eq(action, "surface")){
        fb.surfaces++;
        console_puts("fb: surface allocated id=");
        console_write_dec(fb.surfaces - 1);
        console_putc('\n');
    } else if(str_eq(action, "clear")){
        fb_clear(parse_u32(rest));
        console_puts("fb: cleared\n");
    } else if(str_eq(action, "mouse")){
        fb_set_mouse(parse_u32(first_arg(rest, &rest)), parse_u32(first_arg(rest, &rest)), 0);
        console_puts("fb: crosshair moved\n");
    } else if(str_eq(action, "cursor")){
        const char* style = first_arg(rest, &rest);
        if(style[0] == 0){
            console_puts("cursor=");
            console_puts(cursor_style);
            console_puts(" styles=dot,cross,target\n");
        } else if(str_eq(style, "dot") || str_eq(style, "cross") || str_eq(style, "target")){
            fb_set_cursor_style(style);
            console_puts("fb: cursor style=");
            console_puts(cursor_style);
            console_putc('\n');
        } else {
            console_puts("usage: fb cursor dot|cross|target\n");
        }
    } else if(str_eq(action, "font")){
        console_puts("font: soft 5x7 raster glyphs, command: fb text X Y COLOR WORDS\n");
    } else if(str_eq(action, "text")){
        uint32_t x = parse_u32(first_arg(rest, &rest));
        uint32_t y = parse_u32(first_arg(rest, &rest));
        uint32_t color = parse_u32(first_arg(rest, &rest));
        fb_draw_text(x, y, rest, color);
        console_puts("fb: text drawn\n");
    } else if(str_eq(action, "pixel")){
        uint32_t x = parse_u32(first_arg(rest, &rest));
        uint32_t y = parse_u32(first_arg(rest, &rest));
        uint32_t color = parse_u32(first_arg(rest, &rest));
        fb_put_pixel(x, y, color);
        console_puts("fb: pixel ");
        console_write_dec(x);
        console_putc(',');
        console_write_dec(y);
        console_puts(" color=");
        console_write_dec(color);
        console_putc('\n');
    } else if(str_eq(action, "rect")){
        uint32_t x = parse_u32(first_arg(rest, &rest));
        uint32_t y = parse_u32(first_arg(rest, &rest));
        uint32_t w = parse_u32(first_arg(rest, &rest));
        uint32_t h = parse_u32(first_arg(rest, &rest));
        uint32_t color = parse_u32(first_arg(rest, &rest));
        if(color == 0) color = 160;
        fb_fill_rect(x, y, w, h, color);
        console_puts("fb: rect ");
        console_write_dec(x);
        console_putc(',');
        console_write_dec(y);
        console_puts(" ");
        console_write_dec(w);
        console_putc('x');
        console_write_dec(h);
        console_putc('\n');
    } else if(str_eq(action, "demo")){
        fb_draw_demo();
        console_puts("fb: demo desktop rasterized\n");
    } else if(str_eq(action, "saver")){
        const char* name = first_arg(rest, &rest);
        uint32_t frames = parse_u32(rest);
        fb_saver(name[0] ? name : "lava", frames);
    } else if(str_eq(action, "blit")){
        jobs_account("framebuffer", 8);
        console_puts("fb: composited surfaces -> primary buffer checksum=");
        console_write_dec(fb_checksum());
        console_putc('\n');
    } else if(str_eq(action, "dump")){
        uint32_t w = parse_u32(first_arg(rest, &rest));
        uint32_t h = parse_u32(first_arg(rest, &rest));
        fb_dump_region(w, h);
    } else {
        console_puts("usage: fb status | mode W H BPP | surface | clear C | mouse X Y | cursor dot|cross|target | pixel X Y C | rect X Y W H [C] | text X Y C TEXT | demo | saver lava|rain|stars|waves [N] | dump [W H] | font | blit\n");
    }
}
