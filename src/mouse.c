#include <stdint.h>
#include "console.h"
#include "events.h"
#include "fb.h"
#include "fs.h"
#include "gui.h"
#include "jobs.h"
#include "mouse.h"
#include "window.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

struct mouse_state {
    int x;
    int y;
    int wheel;
    uint32_t buttons;
    uint32_t last_buttons;
    uint32_t events;
    uint32_t scrolls;
    int packet_index;
    int packet_size;
    int wheel_mode;
    uint8_t packet[4];
};

static struct mouse_state mouse = {32, 24, 0, 0, 0, 0, 0, 0, 3, 0, {0, 0, 0, 0}};

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

static void io_wait_input_clear(void){
    for(int i=0; i<100000 && (inb(PS2_STATUS) & 0x02); i++) {}
}

static int ps2_read_byte(uint8_t* out){
    for(int i=0; i<100000; i++){
        if(inb(PS2_STATUS) & 0x01){
            *out = inb(PS2_DATA);
            return 1;
        }
    }
    return 0;
}

static int mouse_aux_write(uint8_t value){
    uint8_t ack = 0;
    io_wait_input_clear();
    outb(PS2_COMMAND, 0xD4);
    io_wait_input_clear();
    outb(PS2_DATA, value);
    if(!ps2_read_byte(&ack))
        return 0;
    return ack == 0xFA;
}

static uint8_t ps2_controller_read_config(void){
    uint8_t config = 0;
    io_wait_input_clear();
    outb(PS2_COMMAND, 0x20);
    (void)ps2_read_byte(&config);
    return config;
}

static void ps2_controller_write_config(uint8_t config){
    io_wait_input_clear();
    outb(PS2_COMMAND, 0x60);
    io_wait_input_clear();
    outb(PS2_DATA, config);
}

static void mouse_try_wheel_mode(void){
    uint8_t id = 0;
    io_wait_input_clear();
    outb(PS2_COMMAND, 0xA8);
    while(inb(PS2_STATUS) & 0x01)
        (void)inb(PS2_DATA);
    if(!mouse_aux_write(0xF6)) return;
    if(!mouse_aux_write(0xF3)) return;
    if(!mouse_aux_write(200)) return;
    if(!mouse_aux_write(0xF3)) return;
    if(!mouse_aux_write(100)) return;
    if(!mouse_aux_write(0xF3)) return;
    if(!mouse_aux_write(80)) return;
    if(!mouse_aux_write(0xF2)) return;
    if(!ps2_read_byte(&id)) return;
    if(id == 3){
        mouse.wheel_mode = 1;
        mouse.packet_size = 4;
    }
    (void)mouse_aux_write(0xF4);
}

static void mouse_clamp(void){
    if(mouse.x < 0) mouse.x = 0;
    if(mouse.y < 0) mouse.y = 0;
    if(mouse.x > 1023) mouse.x = 1023;
    if(mouse.y > 767) mouse.y = 767;
}

static void mouse_sync_fb(void){
    fb_set_mouse((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
    console_set_pointer((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
}

static void mouse_focus_at(void){
    if(mouse.x < 512 && mouse.y < 360)
        window_focus("shell");
    else if(mouse.x >= 512 && mouse.y < 360)
        window_focus("inspector");
    else if(mouse.x < 512)
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
    mouse.wheel = 0;
    mouse.buttons = 0;
    mouse.last_buttons = 0;
    mouse.events = 0;
    mouse.scrolls = 0;
    mouse.packet_index = 0;
    mouse.packet_size = 3;
    mouse.wheel_mode = 0;
    fb_set_mouse((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
    fs_write("/dev/mouse", "device=ps2-mouse\nstate=ready\nbuttons=3\nscroll=gesture+wheel\n");
    fs_append_line("/var/log/system.log", "mouse: input router online");
}

void mouse_install(void){
    uint8_t mask;
    uint8_t config;
    io_wait_input_clear();
    outb(PS2_COMMAND, 0xA8);
    config = ps2_controller_read_config();
    config |= 0x02;
    config &= (uint8_t)~0x20;
    ps2_controller_write_config(config);
    mouse_try_wheel_mode();
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
    gui_handle_drag((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
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
    uint32_t old_buttons = mouse.buttons;
    if(down) mouse.buttons |= mask;
    else mouse.buttons &= ~mask;
    mouse_sync_fb();
    if(button == 0 && down && !(old_buttons & 1)){
        mouse_focus_at();
        gui_handle_click((uint32_t)mouse.x, (uint32_t)mouse.y);
    }
    if(button == 0 && !down)
        gui_handle_drag((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
    mouse.last_buttons = mouse.buttons;
    mouse_event(down ? "mouse: button down" : "mouse: button up", 1);
}

void mouse_scroll(int amount){
    if(amount == 0)
        return;
    mouse.wheel += amount;
    mouse.scrolls++;
    if(!gui_handle_scroll(amount))
        console_scroll(amount > 0 ? amount * 3 : amount * 3);
    mouse_event(amount > 0 ? "mouse: scroll up" : "mouse: scroll down", 1);
}

void mouse_handler(void){
    uint8_t data = inb(PS2_DATA);
    if(mouse.packet_index == 0 && !(data & 0x08))
        return;
    mouse.packet[mouse.packet_index++] = data;
    if(mouse.packet_index < mouse.packet_size)
        return;
    mouse.packet_index = 0;
    int dx = (int)(int8_t)mouse.packet[1];
    int dy = -(int)(int8_t)mouse.packet[2];
    int wheel = 0;
    uint32_t old_buttons = mouse.buttons;
    mouse.buttons = mouse.packet[0] & 0x07;
    if(mouse.wheel_mode){
        wheel = mouse.packet[3] & 0x0F;
        if(wheel & 0x08)
            wheel |= ~0x0F;
        mouse.buttons |= (mouse.packet[3] & 0x30) >> 1;
    }
    mouse.x += dx;
    mouse.y += dy;
    mouse_clamp();
    gui_handle_drag((uint32_t)mouse.x, (uint32_t)mouse.y, mouse.buttons);
    if((mouse.buttons & 1) && !(old_buttons & 1))
        gui_handle_click((uint32_t)mouse.x, (uint32_t)mouse.y);
    if(wheel)
        mouse_scroll(wheel);
    else if((mouse.buttons & 4) && dy)
        mouse_scroll(dy > 0 ? 1 : -1);
    mouse_sync_fb();
    mouse.last_buttons = mouse.buttons;
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

int32_t mouse_wheel(void){
    return mouse.wheel;
}

uint32_t mouse_scroll_count(void){
    return mouse.scrolls;
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
        console_puts(" wheel=");
        if(mouse.wheel < 0){
            console_putc('-');
            console_write_dec((uint32_t)(-mouse.wheel));
        } else {
            console_write_dec((uint32_t)mouse.wheel);
        }
        console_puts(" scrolls=");
        console_write_dec(mouse.scrolls);
        console_puts(" wheel_mode=");
        console_puts(mouse.wheel_mode ? "on" : "gesture");
        console_puts(" events=");
        console_write_dec(mouse.events);
        console_putc('\n');
    } else if(str_eq(action, "scroll") || str_eq(action, "wheel")){
        const char* dir = first_arg(rest, &rest);
        int lines = parse_i32(rest);
        if(lines == 0)
            lines = 1;
        if(str_eq(dir, "up"))
            mouse_scroll(lines);
        else if(str_eq(dir, "down"))
            mouse_scroll(-lines);
        else
            mouse_scroll(parse_i32(dir));
        console_puts("mouse: scrolled terminal\n");
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
        console_puts("usage: mouse status | scroll up|down [N] | wheel N | move DX DY | set X Y | click | down B | up B\n");
    }
}
