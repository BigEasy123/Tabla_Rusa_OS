#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "gfx.h"
#include "jobs.h"

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
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

static int32_t parse_i32(const char* s){
    int sign = 1;
    int32_t value = 0;
    while(is_space(*s)) s++;
    if(*s == '-'){
        sign = -1;
        s++;
    }
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (*s - '0');
        s++;
    }
    return value * sign;
}

static void print_i32(int32_t v){
    if(v < 0){
        console_putc('-');
        console_write_dec((uint32_t)(-v));
    } else {
        console_write_dec((uint32_t)v);
    }
}

void gfx_init(void){
    fs_mkdir("/system/gui");
    fs_write("/system/gui/vector.txt", "vector graphics: line rect circle path scene\n");
    fs_append_line("/var/log/system.log", "gfx: vector command surface online");
}

static void gfx_scene(void){
    console_clear_output();
    console_puts("+---------------- vector scene ----------------+\n");
    console_puts("| origin o--------x                             |\n");
    console_puts("|        \\       /                              |\n");
    console_puts("|         \\     /      circle(center=24,6,r=4) |\n");
    console_puts("|          \\   /          ****                 |\n");
    console_puts("|           \\ /          *    *                |\n");
    console_puts("|            *           *    *                |\n");
    console_puts("|          path          *    *                |\n");
    console_puts("|                         ****                 |\n");
    console_puts("+----------------------------------------------+\n");
}

void gfx_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    jobs_account("gfx-vector", 1);
    if(action[0] == 0 || str_eq(action, "help")){
        console_puts("gfx vector line X1 Y1 X2 Y2 | rect X Y W H | circle X Y R | scene\n");
    } else if(str_eq(action, "scene")){
        jobs_account("gfx-vector", 8);
        gfx_scene();
    } else if(str_eq(action, "line")){
        int32_t x1 = parse_i32(first_arg(rest, &rest));
        int32_t y1 = parse_i32(first_arg(rest, &rest));
        int32_t x2 = parse_i32(first_arg(rest, &rest));
        int32_t y2 = parse_i32(first_arg(rest, &rest));
        console_puts("line M ");
        print_i32(x1); console_putc(' '); print_i32(y1);
        console_puts(" L ");
        print_i32(x2); console_putc(' '); print_i32(y2);
        console_putc('\n');
    } else if(str_eq(action, "rect")){
        int32_t x = parse_i32(first_arg(rest, &rest));
        int32_t y = parse_i32(first_arg(rest, &rest));
        int32_t w = parse_i32(first_arg(rest, &rest));
        int32_t h = parse_i32(first_arg(rest, &rest));
        console_puts("rect path M ");
        print_i32(x); console_putc(' '); print_i32(y);
        console_puts(" h "); print_i32(w);
        console_puts(" v "); print_i32(h);
        console_puts(" h "); print_i32(-w);
        console_puts(" Z\n");
    } else if(str_eq(action, "circle")){
        int32_t x = parse_i32(first_arg(rest, &rest));
        int32_t y = parse_i32(first_arg(rest, &rest));
        int32_t r = parse_i32(first_arg(rest, &rest));
        console_puts("circle center=(");
        print_i32(x); console_putc(','); print_i32(y);
        console_puts(") r=");
        print_i32(r);
        console_puts(" bbox=(");
        print_i32(x - r); console_putc(','); print_i32(y - r);
        console_puts(")-(");
        print_i32(x + r); console_putc(','); print_i32(y + r);
        console_puts(")\n");
    } else {
        console_puts("gfx: unknown command\n");
    }
}
