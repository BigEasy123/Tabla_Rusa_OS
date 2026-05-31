#include <stdint.h>
#include "console.h"
#include "fb.h"
#include "fs.h"
#include "jobs.h"

#define FB_RASTER_W 96
#define FB_RASTER_H 54

struct fb_state {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t surfaces;
    uint32_t pixels;
    uint32_t rects;
    uint32_t glyphs;
    uint32_t frames;
    int mouse_x;
    int mouse_y;
};

static struct fb_state fb = {640, 480, 32, 3, 0, 0, 0, 0, 32, 24};
static uint32_t raster[FB_RASTER_H][FB_RASTER_W];
static char saver_name[16] = "none";

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

static uint8_t glyph_pixel(char c, uint32_t gx, uint32_t gy){
    uint32_t seed = (uint32_t)c;
    if(c == ' ') return 0;
    if(gx == 0 || gx == 4 || gy == 0 || gy == 6)
        return 1;
    return ((seed + gx * 3 + gy * 5) & 0x5) == 0;
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
    fb.mouse_x = (int)x;
    fb.mouse_y = (int)y;
    (void)buttons;
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

static uint32_t wave_value(uint32_t x, uint32_t y, uint32_t frame){
    uint32_t a = (x * 7 + frame * 11) & 63;
    uint32_t b = (y * 9 + frame * 5) & 63;
    return ((a ^ b) * 4) & 255;
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
    console_puts("screensaver ");
    console_puts(saver_name);
    console_puts(" frames=");
    console_write_dec(fb.frames);
    console_puts(" checksum=");
    console_write_dec(fb_checksum());
    console_putc('\n');
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
        "mode=640x480x32\n"
        "raster=96x54\n"
        "surfaces=3\n"
        "font=5x7-soft\n"
        "screensavers=lava,rain,stars,waves\n"
        "mouse=32,24\n");
    fs_append_line("/var/log/system.log", "fb: raster framebuffer and font renderer online");
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
        console_puts("usage: fb status | mode W H BPP | surface | clear C | mouse X Y | pixel X Y C | rect X Y W H [C] | text X Y C TEXT | demo | saver lava|rain|stars|waves [N] | dump [W H] | font | blit\n");
    }
}
