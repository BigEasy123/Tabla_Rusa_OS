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
    int mouse_x;
    int mouse_y;
};

static struct fb_state fb = {640, 480, 32, 3, 32, 24};

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
    } else if(str_eq(action, "blit")){
        jobs_account("framebuffer", 8);
        console_puts("fb: composited surfaces -> primary buffer\n");
    } else {
        console_puts("usage: fb status | mode W H BPP | surface | mouse X Y | font | blit\n");
    }
}
