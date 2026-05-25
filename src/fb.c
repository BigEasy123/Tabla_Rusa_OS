#include <stdint.h>
#include "console.h"
#include "fb.h"
#include "fs.h"
#include "jobs.h"

struct fb_state {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t surfaces;
    uint32_t pixels;
    uint32_t rects;
    int mouse_x;
    int mouse_y;
};

static struct fb_state fb = {640, 480, 32, 3, 0, 0, 32, 24};
#define FB_STORE_MAX 64
static uint32_t pixel_store[FB_STORE_MAX];
static uint32_t pixel_x[FB_STORE_MAX];
static uint32_t pixel_y[FB_STORE_MAX];
static uint32_t pixel_store_used = 0;

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

void fb_init(void){
    for(uint32_t i=0; i<FB_STORE_MAX; i++)
        pixel_store[i] = 0;
    for(uint32_t i=0; i<FB_STORE_MAX; i++){
        pixel_x[i] = 0;
        pixel_y[i] = 0;
    }
    pixel_store_used = 0;
    fs_mkdir("/system/gui");
    fs_write("/system/gui/framebuffer.txt", "mode=640x480x32\nsurfaces=3\nmouse=32,24\n");
    fs_append_line("/var/log/system.log", "fb: framebuffer descriptor online");
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
        console_puts(" surfaces=");
        console_write_dec(fb.surfaces);
        console_puts(" pixels=");
        console_write_dec(fb.pixels);
        console_puts(" rects=");
        console_write_dec(fb.rects);
        console_puts(" mouse=");
        console_write_dec((uint32_t)fb.mouse_x);
        console_putc(',');
        console_write_dec((uint32_t)fb.mouse_y);
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
    } else if(str_eq(action, "mouse")){
        fb.mouse_x = (int)parse_u32(first_arg(rest, &rest));
        fb.mouse_y = (int)parse_u32(first_arg(rest, &rest));
        console_puts("fb: mouse moved\n");
    } else if(str_eq(action, "font")){
        console_puts("font: 8x16 monospace bitmap planned, text blit API reserved\n");
    } else if(str_eq(action, "pixel")){
        uint32_t x = parse_u32(first_arg(rest, &rest));
        uint32_t y = parse_u32(first_arg(rest, &rest));
        uint32_t color = parse_u32(first_arg(rest, &rest));
        fb.pixels++;
        if(pixel_store_used < FB_STORE_MAX){
            pixel_x[pixel_store_used] = x;
            pixel_y[pixel_store_used] = y;
            pixel_store[pixel_store_used++] = (x << 20) ^ (y << 8) ^ color;
        }
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
        fb.rects++;
        fb.pixels += w * h;
        if(pixel_store_used < FB_STORE_MAX){
            pixel_x[pixel_store_used] = x;
            pixel_y[pixel_store_used] = y;
            pixel_store[pixel_store_used++] = (x << 20) ^ (y << 8) ^ (w * h);
        }
        console_puts("fb: rect ");
        console_write_dec(x);
        console_putc(',');
        console_write_dec(y);
        console_puts(" ");
        console_write_dec(w);
        console_putc('x');
        console_write_dec(h);
        console_putc('\n');
    } else if(str_eq(action, "blit")){
        jobs_account("framebuffer", 8);
        uint32_t checksum = 0;
        for(uint32_t i=0; i<pixel_store_used; i++)
            checksum ^= pixel_store[i];
        console_puts("fb: composited surfaces -> primary buffer checksum=");
        console_write_dec(checksum);
        console_putc('\n');
    } else if(str_eq(action, "dump")){
        for(uint32_t y=0; y<8; y++){
            for(uint32_t x=0; x<16; x++){
                int lit = 0;
                for(uint32_t i=0; i<pixel_store_used; i++)
                    if((pixel_x[i] % 16) == x && (pixel_y[i] % 8) == y)
                        lit = 1;
                console_putc(lit ? '#' : '.');
            }
            console_putc('\n');
        }
    } else {
        console_puts("usage: fb status | mode W H BPP | surface | mouse X Y | pixel X Y C | rect X Y W H | dump | font | blit\n");
    }
}
