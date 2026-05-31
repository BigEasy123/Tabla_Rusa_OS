#include <stdint.h>
#include "console.h"
#include "events.h"
#include "fb.h"
#include "fs.h"
#include "jobs.h"
#include "mouse.h"
#include "window.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

struct mouse_state {
    int x;
    int y;
    uint32_t buttons;
    uint32_t events;
    int packet_index;
    uint8_t packet[3];
};

static struct mouse_state mouse = {32, 24, 0, 0, 0, {0, 0, 0}};

static inline uint8_t inb(uint16_t port){
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

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

static int32_t parse_i32(const char* s){
    int32_t value = 0;
    int sign = 1;
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

static void mouse_clamp(void){
    if(mouse.x < 0) mouse.x = 0;
    if(mouse.y < 0) mouse.y = 0;
    if(mouse.x > 639) mouse.x = 639;
    if(mouse.y > 479) mouse.y = 479;
}

static void mouse_sync_fb(void){
    fb_set_mouse((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
}

static void mouse_focus_at(void){
    if(mouse.x < 320 && mouse.y < 250)
        window_focus("shell");
    else if(mouse.x >= 320 && mouse.y < 250)
        window_focus("inspector");
    else if(mouse.x < 320)
        window_focus("editor");
    else
        window_focus("network");
}

static void mouse_event(const char* kind, int emit){
    mouse.events++;
    jobs_account("input", 1);
    if(emit){
        fs_append_line("/var/log/system.log", kind);
        events_emit("input.mouse");
    }
}

void mouse_init(void){
    mouse.x = 32;
    mouse.y = 24;
    mouse.buttons = 0;
    mouse.events = 0;
    mouse.packet_index = 0;
    fb_set_mouse((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
    fs_write("/dev/mouse", "device=ps2-mouse\nstate=ready\nbuttons=3\n");
    fs_append_line("/var/log/system.log", "mouse: input router online");
}

void mouse_install(void){
    uint8_t mask;
    mask = inb(0x21);
    mask &= ~0x04;
    outb(0x21, mask);
    mask = inb(0xA1);
    mask &= ~0x10;
    outb(0xA1, mask);
}

void mouse_move(int dx, int dy){
    mouse.x += dx;
    mouse.y += dy;
    mouse_clamp();
    mouse_sync_fb();
    mouse_event("mouse: move", 1);
}

void mouse_set(int x, int y){
    mouse.x = x;
    mouse.y = y;
    mouse_clamp();
    mouse_sync_fb();
    mouse_event("mouse: set", 1);
}

void mouse_button(uint32_t button, int down){
    uint32_t mask = 1U << button;
    if(down) mouse.buttons |= mask;
    else mouse.buttons &= ~mask;
    mouse_sync_fb();
    if(button == 0 && down)
        mouse_focus_at();
    mouse_event(down ? "mouse: button down" : "mouse: button up", 1);
}

void mouse_handler(void){
    uint8_t data = inb(PS2_DATA);
    mouse.packet[mouse.packet_index++] = data;
    if(mouse.packet_index < 3)
        return;
    mouse.packet_index = 0;
    int dx = (int)(int8_t)mouse.packet[1];
    int dy = -(int)(int8_t)mouse.packet[2];
    mouse.buttons = mouse.packet[0] & 0x07;
    mouse.x += dx;
    mouse.y += dy;
    mouse_clamp();
    if(mouse.buttons & 1)
        mouse_focus_at();
    mouse_sync_fb();
    mouse_event("mouse: irq packet", 0);
}

uint32_t mouse_x(void){
    return (uint32_t)mouse.x;
}

uint32_t mouse_y(void){
    return (uint32_t)mouse.y;
}

uint32_t mouse_buttons(void){
    return mouse.buttons;
}

uint32_t mouse_event_count(void){
    return mouse.events;
}

void mouse_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("mouse x=");
        console_write_dec((uint32_t)mouse.x);
        console_puts(" y=");
        console_write_dec((uint32_t)mouse.y);
        console_puts(" buttons=");
        console_write_dec(mouse.buttons);
        console_puts(" events=");
        console_write_dec(mouse.events);
        console_putc('\n');
    } else if(str_eq(action, "move")){
        int dx = parse_i32(first_arg(rest, &rest));
        int dy = parse_i32(first_arg(rest, &rest));
        mouse_move(dx, dy);
        console_puts("mouse: moved\n");
    } else if(str_eq(action, "set")){
        int x = parse_i32(first_arg(rest, &rest));
        int y = parse_i32(first_arg(rest, &rest));
        mouse_set(x, y);
        console_puts("mouse: positioned\n");
    } else if(str_eq(action, "click")){
        mouse_button(0, 1);
        mouse_button(0, 0);
        console_puts("mouse: clicked\n");
    } else if(str_eq(action, "down")){
        mouse_button((uint32_t)parse_i32(first_arg(rest, &rest)), 1);
        console_puts("mouse: button down\n");
    } else if(str_eq(action, "up")){
        mouse_button((uint32_t)parse_i32(first_arg(rest, &rest)), 0);
        console_puts("mouse: button up\n");
    } else {
        console_puts("usage: mouse status | move DX DY | set X Y | click | down B | up B\n");
    }
}
