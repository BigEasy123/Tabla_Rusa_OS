#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "fs.h"
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
static int editor_active = 0;
static char editor_path[64];

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z')
        return (char)(c - 'A' + 'a');
    return c;
}

static int cmd_is(const char* cmd, const char* want){
    while(*cmd && *want){
        if(lower_char(*cmd) != *want)
            return 0;
        cmd++;
        want++;
    }
    return *cmd == 0 && *want == 0;
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

static void print_file_text(const char* text){
    console_puts(text);
    size_t len = str_len(text);
    if(len == 0 || text[len-1] != '\n')
        console_putc('\n');
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
    char cwd[64];
    char line[96];
    if(editor_active){
        line[0] = 'e'; line[1] = 'd'; line[2] = 'i'; line[3] = 't'; line[4] = '>';
        line[5] = ' '; line[6] = 0;
        console_input_write(line);
        return;
    }
    fs_pwd(cwd, sizeof(cwd));
    size_t i = 0;
    for(size_t j=0; cwd[j] && i + 4 < sizeof(line); j++)
        line[i++] = cwd[j];
    line[i++] = ' ';
    line[i++] = '>';
    line[i++] = ' ';
    line[i] = 0;
    console_input_write(line);
}

static void redraw_input(size_t* len){
    char prefix[96];
    char line[INBUF_MAX + 96];
    if(editor_active){
        str_copy(prefix, "edit> ", sizeof(prefix));
    } else {
        char cwd[64];
        fs_pwd(cwd, sizeof(cwd));
        size_t i = 0;
        for(size_t j=0; cwd[j] && i + 4 < sizeof(prefix); j++)
            prefix[i++] = cwd[j];
        prefix[i++] = ' ';
        prefix[i++] = '>';
        prefix[i++] = ' ';
        prefix[i] = 0;
    }
    size_t i = 0;
    for(size_t j=0; prefix[j] && i + 1 < sizeof(line); j++)
        line[i++] = prefix[j];
    for(size_t j=0; inbuf[j] && i + 1 < sizeof(line); j++)
        line[i++] = inbuf[j];
    line[i] = 0;
    console_input_write(line);
    *len = str_len(inbuf);
}

static void console_echo_command(const char* line){
    if(editor_active){
        console_puts("edit> ");
        console_puts(line);
        console_putc('\n');
        return;
    }
    char cwd[64];
    fs_pwd(cwd, sizeof(cwd));
    console_puts(cwd);
    console_puts(" > ");
    console_puts(line);
    console_putc('\n');
}

static void cmd_help(void){
    console_puts("Commands:\n");
    console_puts("  help        - show this help\n");
    console_puts("  echo ARG    - print ARG\n");
    console_puts("  pwd, ls, cd - navigate RAM filesystem\n");
    console_puts("  cat FILE    - print a file\n");
    console_puts("  touch FILE  - create an empty file\n");
    console_puts("  write F TXT - replace file contents\n");
    console_puts("  mkdir DIR   - create a directory\n");
    console_puts("  rm PATH     - remove empty dir or file\n");
    console_puts("  edit FILE   - open line editor\n");
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
    console_puts("  test        - run safe command checks\n");
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

static void cmd_test(void){
    console_puts("console: ok\n");
    console_puts("timer: ticks=");
    console_write_dec(timer_ticks());
    console_putc('\n');
    console_puts("memory: entries=");
    console_write_dec(memory_map_entries());
    console_puts(" usable_kib=");
    console_write_dec(memory_usable_kib());
    console_putc('\n');
    console_puts("heap: used=");
    console_write_dec(heap_bytes_used());
    console_puts(" bytes\n");
    console_puts("paging: ");
    console_puts(paging_is_enabled() ? "on\n" : "off\n");
    console_puts("keyboard: ok if you typed this command\n");
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

static void fs_status(int r, const char* what){
    if(r == 0) return;
    console_puts(what);
    if(r == -1) console_puts(": not found\n");
    else if(r == -2) console_puts(": wrong type or not empty\n");
    else if(r == -3) console_puts(": filesystem full\n");
    else console_puts(": failed\n");
}

static void editor_eval(char* line){
    if(cmd_is(line, ".quit") || cmd_is(line, ".q")){
        editor_active = 0;
        console_puts("editor closed\n");
        return;
    }
    if(cmd_is(line, ".save") || cmd_is(line, ".w")){
        console_puts("saved ");
        console_puts(editor_path);
        console_putc('\n');
        return;
    }
    if(cmd_is(line, ".show")){
        const char* text;
        if(fs_read(editor_path, &text) == 0) print_file_text(text);
        return;
    }
    if(cmd_is(line, ".clear")){
        fs_write(editor_path, "");
        console_puts("buffer cleared\n");
        return;
    }
    fs_append_line(editor_path, line);
}

static void shell_eval(char* line){
    char* p=line;
    while(is_space(*p)) p++;
    char* cmd=p;
    while(*p && !is_space(*p)) p++;
    if(*p){
        *p = 0;
        p++;
    }
    while(is_space(*p)) p++;
    char* arg=p;

    if(cmd_is(cmd,"help") || cmd_is(cmd,"?")) cmd_help();
    else if(cmd_is(cmd,"about") || cmd_is(cmd,"ver")) cmd_about();
    else if(cmd_is(cmd,"cls") || cmd_is(cmd,"clear")) console_clear_output();
    else if(cmd_is(cmd,"fault") || cmd_is(cmd,"panic")) cmd_fault();
    else if(cmd_is(cmd,"echo")) { console_puts(arg); console_putc('\n'); }
    else if(cmd_is(cmd,"pwd")) { char cwd[64]; fs_pwd(cwd, sizeof(cwd)); console_puts(cwd); console_putc('\n'); }
    else if(cmd_is(cmd,"ls") || cmd_is(cmd,"dir")) fs_ls(arg);
    else if(cmd_is(cmd,"cd")) { if(arg[0] == 0) fs_cd("/home"); else fs_status(fs_cd(arg), "cd"); }
    else if(cmd_is(cmd,"cat") || cmd_is(cmd,"type")) {
        const char* text;
        if(arg[0] == 0) console_puts("usage: cat FILE\n");
        else if(fs_read(arg, &text) == 0) print_file_text(text);
        else console_puts("cat: not found\n");
    }
    else if(cmd_is(cmd,"touch")) {
        if(arg[0] == 0) console_puts("usage: touch FILE\n");
        else fs_status(fs_touch(arg), "touch");
    }
    else if(cmd_is(cmd,"mkdir")) {
        if(arg[0] == 0) console_puts("usage: mkdir DIR\n");
        else fs_status(fs_mkdir(arg), "mkdir");
    }
    else if(cmd_is(cmd,"rm") || cmd_is(cmd,"del")) {
        if(arg[0] == 0) console_puts("usage: rm PATH\n");
        else fs_status(fs_rm(arg), "rm");
    }
    else if(cmd_is(cmd,"write")) {
        char* rest;
        const char* path = first_arg(arg, &rest);
        if(path[0] == 0 || rest[0] == 0) console_puts("usage: write FILE TEXT\n");
        else fs_status(fs_write(path, rest), "write");
    }
    else if(cmd_is(cmd,"edit")) {
        if(arg[0] == 0) {
            console_puts("usage: edit FILE\n");
        } else {
            str_copy(editor_path, arg, sizeof(editor_path));
            fs_touch(editor_path);
            editor_active = 1;
            console_puts("Editing ");
            console_puts(editor_path);
            console_puts(". Type lines to append. Commands: .show .clear .save .quit\n");
        }
    }
    else if(cmd_is(cmd,"ticks")) { console_puts("ticks="); console_write_dec(timer_ticks()); console_putc('\n'); }
    else if(cmd_is(cmd,"uptime")) { console_puts("uptime="); console_write_dec(timer_uptime_seconds()); console_puts("s\n"); }
    else if(cmd_is(cmd,"sleep")) {
        uint32_t ticks = parse_u32(arg);
        if(ticks == 0) console_puts("usage: sleep N\n");
        else { sleep_ticks(ticks); console_puts("awake\n"); }
    }
    else if(cmd_is(cmd,"mem") || cmd_is(cmd,"memory")) {
        console_puts("total_kib="); console_write_dec(memory_total_kib());
        console_puts(" usable_kib="); console_write_dec(memory_usable_kib());
        console_puts(" entries="); console_write_dec(memory_map_entries());
        console_putc('\n');
    }
    else if(cmd_is(cmd,"mmap") || cmd_is(cmd,"map")) memory_print_map();
    else if(cmd_is(cmd,"heap")) {
        console_puts("heap_start="); console_write_hex(heap_start());
        console_puts(" heap_next="); console_write_hex(heap_current());
        console_puts(" used="); console_write_dec(heap_bytes_used());
        console_puts(" bytes\n");
    }
    else if(cmd_is(cmd,"alloc") || cmd_is(cmd,"malloc")) {
        uint32_t size = parse_u32(arg);
        if(size == 0) {
            console_puts("usage: alloc N\n");
        } else {
            void* ptr = kmalloc(size);
            console_puts("allocated ");
            console_write_dec(size);
            console_puts(" bytes at ");
            console_write_hex((uint32_t)ptr);
            console_putc('\n');
        }
    }
    else if(cmd_is(cmd,"paging") || cmd_is(cmd,"page")) {
        console_puts("paging=");
        console_puts(paging_is_enabled() ? "on" : "off");
        console_puts(" directory=");
        console_write_hex(paging_directory_addr());
        console_putc('\n');
    }
    else if(cmd_is(cmd,"regs")) cmd_regs();
    else if(cmd_is(cmd,"test") || cmd_is(cmd,"selftest")) cmd_test();
    else if(cmd_is(cmd,"halt") || cmd_is(cmd,"shutdown")) { console_puts("Halting.\n"); for(;;) __asm__ __volatile__("cli; hlt"); }
    else if(cmd_is(cmd,"reboot") || cmd_is(cmd,"restart")) cmd_reboot();
    else if(*cmd==0) {}
    else {
        console_puts("Unknown command: '");
        console_puts(cmd);
        console_puts("'. Type 'help'.\n");
    }
}

void kmain(uint32_t mb_magic, uint32_t mb_info_addr){
    console_init();
    fs_init();
    prompt();
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
            inbuf[len]=0;
            console_input_clear();
            console_echo_command(inbuf);
            if(!editor_active)
                history_add(inbuf);
            history_view = -1;
            if(editor_active)
                editor_eval(inbuf);
            else
                shell_eval(inbuf);
            len=0;
            inbuf[0]=0;
            prompt();
            continue;
        }
        if(c=='\b'){
            if(len>0){
                len--;
                inbuf[len]=0;
                redraw_input(&len);
            }
            continue;
        }
        if(len<INBUF_MAX-1){
            inbuf[len++]=c;
            inbuf[len]=0;
            redraw_input(&len);
        }
    }
}
