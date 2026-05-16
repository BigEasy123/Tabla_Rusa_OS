#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "heap.h"
#include "idt.h"
#include "keyboard.h"
#include "memory.h"
#include "paging.h"
#include "timer.h"

#define INBUF_MAX 128
#define HISTORY_MAX 8
static char inbuf[INBUF_MAX];
static char history[HISTORY_MAX][INBUF_MAX];
static size_t history_count = 0;
static int history_view = -1;

static int streq(const char* a, const char* b){
    while(*a && *b){ if(*a!=*b) return 0; a++; b++; }
    return *a==0 && *b==0;
}

static int starts_with(const char* s, const char* prefix){
    while(*prefix){
        if(*s++ != *prefix++) return 0;
    }
    return 1;
}

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(*s == ' ') s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static void str_copy(char* dst, const char* src, size_t max){
    size_t i = 0;
    if(max == 0) return;
    while(i + 1 < max && src[i]){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static size_t str_len(const char* s){
    size_t len = 0;
    while(s[len]) len++;
    return len;
}

static void history_add(const char* line){
    if(line[0] == 0)
        return;
    if(history_count < HISTORY_MAX){
        str_copy(history[history_count++], line, INBUF_MAX);
        return;
    }
    for(size_t i=1; i<HISTORY_MAX; i++)
        str_copy(history[i-1], history[i], INBUF_MAX);
    str_copy(history[HISTORY_MAX-1], line, INBUF_MAX);
}

static void prompt(void){
    console_puts("> ");
}

static void redraw_input(size_t* len){
    console_putc('\r');
    console_puts("> ");
    console_puts(inbuf);
    console_puts("                                                                                ");
    console_putc('\r');
    console_puts("> ");
    console_puts(inbuf);
    *len = str_len(inbuf);
}

static void cmd_help(void){
    console_puts("Commands:\n");
    console_puts("  help        - show this help\n");
    console_puts("  echo ARG    - print ARG\n");
    console_puts("  cls         - clear screen\n");
    console_puts("  about       - kernel info\n");
    console_puts("  fault       - test exception handler\n");
    console_puts("  ticks       - show PIT ticks\n");
    console_puts("  uptime      - show uptime seconds\n");
    console_puts("  sleep N     - sleep N ticks\n");
    console_puts("  mem         - show memory summary\n");
    console_puts("  mmap        - print memory map\n");
    console_puts("  heap        - show heap state\n");
    console_puts("  alloc N     - bump-allocate N bytes\n");
    console_puts("  paging      - show paging state\n");
    console_puts("  regs        - show basic CPU flags\n");
    console_puts("  halt        - stop CPU\n");
    console_puts("  reboot      - keyboard-controller reboot\n");
}

static void cmd_about(void){
    console_puts("Tabla Rusa OS 0.0.2 (i386, multiboot2)\n");
}

static void cmd_fault(void){
    __asm__ __volatile__("ud2");
}

static void cmd_regs(void){
    uint32_t eflags, cr0, cr3;
    __asm__ __volatile__("pushf; pop %0" : "=r"(eflags));
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    console_puts("EFLAGS=");
    console_write_hex(eflags);
    console_puts(" CR0=");
    console_write_hex(cr0);
    console_puts(" CR3=");
    console_write_hex(cr3);
    console_putc('\n');
}

static inline void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port){
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void cmd_reboot(void){
    console_puts("Rebooting...\n");
    while(inb(0x64) & 0x02) {}
    outb(0x64, 0xFE);
    for(;;) __asm__ __volatile__("hlt");
}

static void shell_eval(char* line){
    char* p=line;
    while(*p==' ') p++;
    char* cmd=p;
    while(*p && *p!=' ') p++;
    char saved=*p; *p=0;
    char* arg=(saved==0)?p:p+1;

    if(streq(cmd,"help")) cmd_help();
    else if(streq(cmd,"about")) cmd_about();
    else if(streq(cmd,"cls")) console_clear();
    else if(streq(cmd,"fault")) cmd_fault();
    else if(streq(cmd,"echo")) { console_puts(arg); console_putc('\n'); }
    else if(streq(cmd,"ticks")) { console_puts("ticks="); console_write_dec(timer_ticks()); console_putc('\n'); }
    else if(streq(cmd,"uptime")) { console_puts("uptime="); console_write_dec(timer_uptime_seconds()); console_puts("s\n"); }
    else if(streq(cmd,"sleep")) { sleep_ticks(parse_u32(arg)); console_puts("awake\n"); }
    else if(streq(cmd,"mem")) {
        console_puts("total_kib="); console_write_dec(memory_total_kib());
        console_puts(" usable_kib="); console_write_dec(memory_usable_kib());
        console_puts(" entries="); console_write_dec(memory_map_entries());
        console_putc('\n');
    }
    else if(streq(cmd,"mmap")) memory_print_map();
    else if(streq(cmd,"heap")) {
        console_puts("heap_start="); console_write_hex(heap_start());
        console_puts(" heap_next="); console_write_hex(heap_current());
        console_puts(" used="); console_write_dec(heap_bytes_used());
        console_puts(" bytes\n");
    }
    else if(streq(cmd,"alloc") || starts_with(cmd, "alloc")) {
        uint32_t size = parse_u32(arg);
        void* ptr = kmalloc(size);
        console_puts("allocated ");
        console_write_dec(size);
        console_puts(" bytes at ");
        console_write_hex((uint32_t)ptr);
        console_putc('\n');
    }
    else if(streq(cmd,"paging")) {
        console_puts("paging=");
        console_puts(paging_is_enabled() ? "on" : "off");
        console_puts(" directory=");
        console_write_hex(paging_directory_addr());
        console_putc('\n');
    }
    else if(streq(cmd,"regs")) cmd_regs();
    else if(streq(cmd,"halt")) { console_puts("Halting.\n"); for(;;) __asm__ __volatile__("cli; hlt"); }
    else if(streq(cmd,"reboot")) cmd_reboot();
    else if(*cmd==0) {}
    else { console_puts("Unknown command: "); console_puts(cmd); console_putc('\n'); }
}

void kmain(uint32_t mb_magic, uint32_t mb_info_addr){
    console_init();
    console_puts("Tabla Rusa OS booting...\n");

    memory_init(mb_magic, mb_info_addr);
    serial_puts("memory ok\n");
    heap_init();
    serial_puts("heap ok\n");
    paging_init();
    serial_puts("paging ok\n");
    idt_init();
    serial_puts("idt ok\n");
    pic_remap();
    serial_puts("pic ok\n");
    timer_init(100);
    serial_puts("timer ok\n");
    keyboard_install();
    serial_puts("keyboard ok\n");

    __asm__ __volatile__("sti");

    console_puts("Booted! Type 'help' to begin.\n");
    prompt();

    size_t len=0;
    for(;;){
        int key = kb_read_key();
        if(key==0){ __asm__ __volatile__("hlt"); continue; }

        if(key == KB_KEY_UP && history_count > 0){
            if(history_view < 0) history_view = (int)history_count - 1;
            else if(history_view > 0) history_view--;
            str_copy(inbuf, history[history_view], INBUF_MAX);
            redraw_input(&len);
            continue;
        }
        if(key == KB_KEY_DOWN){
            if(history_view >= 0 && history_view + 1 < (int)history_count){
                history_view++;
                str_copy(inbuf, history[history_view], INBUF_MAX);
            } else {
                history_view = -1;
                inbuf[0] = 0;
            }
            redraw_input(&len);
            continue;
        }
        if(key > 0xFF)
            continue;

        char c = (char)key;
        if(c=='\n'){
            console_putc('\n');
            inbuf[len]=0;
            history_add(inbuf);
            history_view = -1;
            shell_eval(inbuf);
            len=0;
            inbuf[0]=0;
            prompt();
            continue;
        }
        if(c=='\b'){
            if(len>0){ len--; inbuf[len]=0; console_putc('\b'); }
            continue;
        }
        if(len<INBUF_MAX-1){
            inbuf[len++]=c;
            inbuf[len]=0;
            console_putc(c);
        }
    }
}
