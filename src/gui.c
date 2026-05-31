#include <stdint.h>
#include "console.h"
#include "fb.h"
#include "fs.h"
#include "mouse.h"
#include "process.h"
#include "service.h"
#include "window.h"
#include "gui.h"

static int running = 0;

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

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static void gui_draw_desktop(void){
    const struct window_info* focused = window_focused();
    fb_clear(10);
    fb_fill_rect(0, 0, 640, 34, 170);
    fb_fill_rect(24, 70, 280, 170, 55);
    fb_fill_rect(328, 70, 288, 170, 80);
    fb_fill_rect(24, 270, 280, 132, 65);
    fb_fill_rect(328, 270, 288, 132, 95);
    fb_draw_text(28, 12, "Tabla Rusa GUI", 240);
    fb_draw_text(44, 96, "shell", 220);
    fb_draw_text(350, 96, "inspector", 230);
    fb_draw_text(44, 296, "editor", 210);
    fb_draw_text(350, 296, "network", 210);
    console_clear_output();
    console_puts("+------------------------------------------------------------------------------+\n");
    console_puts("| Tabla Rusa GUI :: vga-text compositor                  tabs: shell editor net |\n");
    console_puts("+------------------------------------------------------------------------------+\n");
    console_puts("| focused: ");
    console_puts(focused->name);
    console_puts("  surface=");
    console_puts(focused->surface);
    console_puts("  pos=");
    console_write_dec((uint32_t)focused->x);
    console_putc(',');
    console_write_dec((uint32_t)focused->y);
    console_puts("\n");
    console_puts("|                                                                              |\n");
    console_puts("|  +---------------- shell ----------------+  +------------- inspector --------+ |\n");
    console_puts("|  | native language prompt                |  | processes, memory, events     | |\n");
    console_puts("|  | object commands live here             |  | math jobs and logs next       | |\n");
    console_puts("|  +---------------------------------------+  +------------------------------+ |\n");
    console_puts("|                                                                              |\n");
    console_puts("|  +--------------- editor ----------------+  +-------------- network --------+ |\n");
    console_puts("|  | line-number editing, arrow cursor     |  | loopback, tcp stubs, routing  | |\n");
    console_puts("|  +---------------------------------------+  +------------------------------+ |\n");
    console_puts("+------------------------------------------------------------------------------+\n");
}

void gui_init(void){
    fs_append_line("/var/log/system.log", "gui: text compositor foundation ready");
}

int gui_is_running(void){
    return running;
}

void gui_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("gui=");
        console_puts(running ? "running" : "stopped");
        console_puts(" backend=soft-framebuffer+vga-text\n");
        console_puts("objects: compositor window-manager tab-strip input-router desktop screensaver\n");
        console_puts("crosshair=");
        console_write_dec(mouse_x());
        console_putc(',');
        console_write_dec(mouse_y());
        console_puts(" buttons=");
        console_write_dec(mouse_buttons());
        console_putc('\n');
    } else if(str_eq(action, "start")){
        running = 1;
        service_set_running("gui", 1);
        process_set_running("gui", 1);
        fs_append_line("/var/log/system.log", "gui: compositor foundation started");
        console_puts("gui: compositor foundation started\n");
        gui_draw_desktop();
    } else if(str_eq(action, "stop")){
        running = 0;
        service_set_running("gui", 0);
        process_set_running("gui", 0);
        fs_append_line("/var/log/system.log", "gui: compositor foundation stopped");
        console_puts("gui: stopped\n");
    } else if(str_eq(action, "desktop") || str_eq(action, "draw")){
        gui_draw_desktop();
    } else if(str_eq(action, "windows") || str_eq(action, "tabs")){
        window_list();
    } else if(str_eq(action, "tab") || str_eq(action, "next")){
        window_focus_next();
        gui_draw_desktop();
    } else if(str_eq(action, "focus")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0){
            console_puts("usage: gui focus WINDOW\n");
            return;
        }
        window_focus(name);
        gui_draw_desktop();
    } else if(str_eq(action, "move")){
        char* yarg;
        const char* name = first_arg(rest, &rest);
        uint32_t x = parse_u32(rest);
        first_arg(rest, &yarg);
        uint32_t y = parse_u32(yarg);
        if(name[0] == 0){
            console_puts("usage: gui move WINDOW X Y\n");
            return;
        }
        window_move(name, (int)x, (int)y);
        gui_draw_desktop();
    } else if(str_eq(action, "click")){
        uint32_t x = parse_u32(first_arg(rest, &rest));
        uint32_t y = parse_u32(first_arg(rest, &rest));
        mouse_set((int)x, (int)y);
        mouse_button(0, 1);
        mouse_button(0, 0);
        gui_draw_desktop();
    } else {
        console_puts("usage: gui status | start | stop | desktop | windows | tab | focus NAME | move NAME X Y | click X Y\n");
    }
}
