#include "console.h"
#include "fb.h"
#include <stddef.h>

#define VGA_MEM ((uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25
#define VGA_INPUT_ROW (VGA_H - 2)
#define VGA_OUTPUT_H VGA_INPUT_ROW
#define SCROLLBACK_LINES 256
#define COM1 0x3F8

static uint8_t vga_color = 0x07;
static uint8_t input_color = 0x0F;
static size_t cx = 0;
static size_t cy = 0;
static char scrollback[SCROLLBACK_LINES][VGA_W + 1];
static uint32_t scrollback_count = 0;
static char current_line[VGA_W + 1];
static size_t current_len = 0;
static uint32_t view_offset = 0;
static uint32_t pointer_x = 32;
static uint32_t pointer_y = 24;
static uint32_t pointer_buttons = 0;
static int pointer_visible = 0;
static int fb_terminal = 0;
static char* capture_buf = 0;
static size_t capture_max = 0;
static size_t capture_len = 0;

static inline void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port){
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void cursor_set(size_t row, size_t col){
    uint16_t pos = (uint16_t)(row * VGA_W + col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void serial_init(void){
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putc(char c){
    if(c == '\n')
        serial_putc('\r');
    for(int i=0; i<100000 && !(inb(COM1 + 5) & 0x20); i++) {}
    outb(COM1, (uint8_t)c);
}

void serial_puts(const char* s){
    while(*s) serial_putc(*s++);
}

static size_t str_len(const char* s){
    size_t len = 0;
    while(s && s[len]) len++;
    return len;
}

static uint32_t total_output_lines(void){
    return scrollback_count + (current_len > 0 ? 1U : 0U);
}

static uint32_t max_view_offset(void){
    uint32_t total = total_output_lines();
    if(total <= VGA_OUTPUT_H)
        return 0;
    return total - VGA_OUTPUT_H;
}

static const char* history_line(uint32_t idx){
    if(idx < scrollback_count)
        return scrollback[idx % SCROLLBACK_LINES];
    return current_line;
}

static void draw_output_line(size_t row, const char* text){
    size_t col = 0;
    for(; col<VGA_W && text && text[col]; col++)
        VGA_MEM[row*VGA_W+col] = ((uint16_t)vga_color<<8) | (uint8_t)text[col];
    for(; col<VGA_W; col++)
        VGA_MEM[row*VGA_W+col] = ((uint16_t)vga_color<<8) | ' ';
}

static void render_fb_terminal_input(void){
    if(!fb_terminal || !fb_hardware_ready())
        return;
    fb_fill_rect(0, 728, 1024, 40, 0x071018);
    fb_draw_text(16, 744, "> ", 0x8EE8A0);
}

static void render_fb_terminal(void){
    if(!fb_terminal || !fb_hardware_ready())
        return;
    uint32_t total = total_output_lines();
    uint32_t max_offset = max_view_offset();
    uint32_t top = 0;
    if(view_offset > max_offset)
        view_offset = max_offset;
    if(total > VGA_OUTPUT_H)
        top = total - VGA_OUTPUT_H - view_offset;
    fb_clear(0x071018);
    fb_fill_rect(0, 0, 1024, 30, 0x111A24);
    fb_draw_text(16, 14, "Tabla Rusa Terminal", 0xFFFFFF);
    fb_draw_text(820, 14, "Esc returns to GUI", 0xCFE8FF);
    for(size_t row=0; row<VGA_OUTPUT_H; row++){
        uint32_t idx = top + (uint32_t)row;
        const char* line = idx < total ? history_line(idx) : "";
        fb_draw_text(16, 46 + (uint32_t)row * 26, line, 0xE8F4FF);
    }
    render_fb_terminal_input();
}

static void render_output(void){
    uint32_t total = total_output_lines();
    uint32_t max_offset = max_view_offset();
    uint32_t top = 0;
    if(view_offset > max_offset)
        view_offset = max_offset;
    if(total > VGA_OUTPUT_H)
        top = total - VGA_OUTPUT_H - view_offset;
    for(size_t row=0; row<VGA_OUTPUT_H; row++){
        uint32_t idx = top + (uint32_t)row;
        if(idx < total)
            draw_output_line(row, history_line(idx));
        else
            draw_output_line(row, "");
    }
    cy = current_len ? (size_t)((total > VGA_OUTPUT_H ? VGA_OUTPUT_H : total) - 1) : 0;
    cx = current_len < VGA_W ? current_len : VGA_W - 1;
    if(pointer_visible){
        size_t prow = (size_t)((pointer_y * VGA_OUTPUT_H) / 480);
        size_t pcol = (size_t)((pointer_x * VGA_W) / 640);
        uint8_t color = pointer_buttons ? 0x4F : 0x0E;
        if(prow >= VGA_OUTPUT_H) prow = VGA_OUTPUT_H - 1;
        if(pcol >= VGA_W) pcol = VGA_W - 1;
        VGA_MEM[prow*VGA_W+pcol] = ((uint16_t)color<<8) | '+';
    }
    render_fb_terminal();
}

static void history_push(const char* line){
    size_t len = str_len(line);
    uint32_t slot = scrollback_count;
    if(len > VGA_W)
        len = VGA_W;
    if(scrollback_count >= SCROLLBACK_LINES){
        for(uint32_t i=1; i<SCROLLBACK_LINES; i++){
            for(size_t j=0; j<=VGA_W; j++)
                scrollback[i-1][j] = scrollback[i][j];
        }
        scrollback_count = SCROLLBACK_LINES - 1;
        slot = SCROLLBACK_LINES - 1;
    }
    for(size_t i=0; i<len; i++)
        scrollback[slot][i] = line[i];
    scrollback[slot][len] = 0;
    scrollback_count++;
}

static void newline_current(void){
    current_line[current_len] = 0;
    history_push(current_line);
    current_len = 0;
    current_line[0] = 0;
    view_offset = 0;
    render_output();
}

static void page_break(void){
    current_len = 0;
    current_line[0] = 0;
    for(size_t i=0; i<VGA_OUTPUT_H; i++)
        history_push("");
    view_offset = 0;
    render_output();
}

void console_clear(void){
    for(size_t i=0; i<VGA_W*VGA_H; i++)
        VGA_MEM[i] = ((uint16_t)vga_color<<8) | ' ';
    cx = 0;
    cy = 0;
    scrollback_count = 0;
    current_len = 0;
    current_line[0] = 0;
    view_offset = 0;
    pointer_visible = 0;
    cursor_set(VGA_INPUT_ROW, 2);
}

void console_init(void){
    serial_init();
    console_clear();
}

void console_clear_output(void){
    page_break();
}

void console_input_clear(void){
    for(size_t col=0; col<VGA_W; col++)
        VGA_MEM[VGA_INPUT_ROW*VGA_W+col] = ((uint16_t)input_color<<8) | ' ';
    for(size_t col=0; col<VGA_W; col++)
        VGA_MEM[(VGA_H-1)*VGA_W+col] = ((uint16_t)vga_color<<8) | ' ';
    cursor_set(VGA_INPUT_ROW, 0);
    render_fb_terminal_input();
}

void console_input_write(const char* s){
    size_t col = 0;
    while(col < VGA_W && s[col])
        col++;
    console_input_write_at(s, col);
}

void console_input_write_at(const char* s, size_t cursor_col){
    console_input_clear();
    size_t col = 0;
    while(col < VGA_W && s[col]){
        VGA_MEM[VGA_INPUT_ROW*VGA_W+col] = ((uint16_t)input_color<<8) | (uint8_t)s[col];
        col++;
    }
    cursor_set(VGA_INPUT_ROW, cursor_col < VGA_W ? cursor_col : VGA_W - 1);
    if(fb_terminal && fb_hardware_ready()){
        fb_fill_rect(0, 728, 1024, 40, 0x071018);
        fb_draw_text(16, 744, s, 0xFFFFFF);
        uint32_t cursor_x = 16 + (uint32_t)(cursor_col < 160 ? cursor_col : 159) * 6;
        fb_fill_rect(cursor_x, 742, 2, 12, 0x8EE8A0);
        fb_set_mouse(pointer_x, pointer_y, pointer_buttons);
    }
}

void console_framebuffer_terminal(int enabled){
    fb_terminal = enabled ? 1 : 0;
    if(fb_terminal)
        render_fb_terminal();
}

void console_capture_begin(char* buffer, size_t max){
    capture_buf = buffer;
    capture_max = max;
    capture_len = 0;
    if(capture_buf && capture_max)
        capture_buf[0] = 0;
}

void console_capture_end(void){
    if(capture_buf && capture_max){
        if(capture_len >= capture_max)
            capture_len = capture_max - 1;
        capture_buf[capture_len] = 0;
    }
    capture_buf = 0;
    capture_max = 0;
    capture_len = 0;
}

void console_putc(char c){
    serial_putc(c);
    if(capture_buf && capture_max && capture_len + 1 < capture_max){
        capture_buf[capture_len++] = c;
        capture_buf[capture_len] = 0;
    }
    if(c == '\r'){
        cx = 0;
        return;
    }
    if(c == '\n'){
        newline_current();
        return;
    }
    if(c == '\b'){
        if(current_len > 0){
            current_len--;
            current_line[current_len] = 0;
            view_offset = 0;
            render_output();
        }
        return;
    }
    if(current_len >= VGA_W)
        newline_current();
    current_line[current_len++] = c;
    current_line[current_len] = 0;
    view_offset = 0;
}

void console_scroll(int delta){
    uint32_t max_offset = max_view_offset();
    if(delta > 0){
        uint32_t add = (uint32_t)delta;
        view_offset = view_offset + add > max_offset ? max_offset : view_offset + add;
    } else if(delta < 0){
        uint32_t sub = (uint32_t)(-delta);
        view_offset = sub > view_offset ? 0 : view_offset - sub;
    }
    render_output();
}

uint32_t console_scrollback_lines(void){
    return total_output_lines();
}

uint32_t console_scroll_offset(void){
    return view_offset;
}

void console_set_pointer(uint32_t x, uint32_t y, uint32_t buttons){
    pointer_x = x;
    pointer_y = y;
    pointer_buttons = buttons;
    pointer_visible = 1;
    render_output();
}

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(*s == ' ' || *s == '\t') s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static int starts_with(const char* s, const char* prefix){
    while(*prefix){
        if(*s++ != *prefix++) return 0;
    }
    return 1;
}

void console_scroll_cmd(char* arg){
    char* value;
    uint32_t lines;
    while(*arg == ' ' || *arg == '\t') arg++;
    value = arg;
    while(*value && *value != ' ' && *value != '\t') value++;
    lines = parse_u32(value);
    if(lines == 0)
        lines = VGA_OUTPUT_H / 2;
    if(starts_with(arg, "up")){
        console_scroll((int)lines);
    } else if(starts_with(arg, "down")){
        console_scroll(-(int)lines);
    } else if(starts_with(arg, "top")){
        console_scroll((int)max_view_offset());
    } else if(starts_with(arg, "bottom")){
        view_offset = 0;
        render_output();
    } else if(starts_with(arg, "status")){
        console_puts("scrollback lines=");
        console_write_dec(console_scrollback_lines());
        console_puts(" offset=");
        console_write_dec(view_offset);
        console_puts(" max=");
        console_write_dec(max_view_offset());
        console_putc('\n');
    } else {
        console_puts("usage: scroll up|down|top|bottom|status [LINES]\n");
        console_puts("keys: PageUp and PageDown also move through terminal history\n");
    }
}

void console_puts(const char* s){
    while(*s) console_putc(*s++);
}

void console_write_hex(uint32_t value){
    static const char hex[] = "0123456789ABCDEF";
    console_puts("0x");
    for(int shift=28; shift>=0; shift-=4)
        console_putc(hex[(value >> shift) & 0xF]);
}

void console_write_dec(uint32_t value){
    char buf[11];
    int i = 0;
    if(value == 0){
        console_putc('0');
        return;
    }
    while(value > 0 && i < 10){
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while(i > 0)
        console_putc(buf[--i]);
}
