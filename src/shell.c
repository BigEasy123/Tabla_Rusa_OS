#include "console.h"
#include "fs.h"
#include "shell.h"
#include <stdint.h>

#define SHELL_INBUF_MAX 128
#define SHELL_HISTORY_MAX 8

static char inbuf[SHELL_INBUF_MAX];
static size_t input_cursor = 0;
static char history[SHELL_HISTORY_MAX][SHELL_INBUF_MAX];
static size_t history_count = 0;
static int history_view = -1;
static int editor_mode = 0;
static void (*eval_handler)(char* line) = 0;

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

void shell_intro(void){
    console_puts(" _______    _     _         ____                  \n");
    console_puts("|__   __|  | |   | |       |  _ \\                 \n");
    console_puts("   | | __ _| |__ | | __ _  | |_) |_   _ ___  __ _ \n");
    console_puts("   | |/ _` | '_ \\| |/ _` | |  _ <| | | / __|/ _` |\n");
    console_puts("   | | (_| | |_) | | (_| | | |_) | |_| \\__ \\ (_| |\n");
    console_puts("   |_|\\__,_|_.__/|_|\\__,_| |____/ \\__,_|___/\\__,_|\n");
    console_puts("Tabla Rusa OS 0.0.6 - native shell, math kernel, live objects\n");
}

void shell_structure_cmd(void){
    console_puts("structure:\n");
    console_puts("  kernel: memory paging idt timer keyboard heap\n");
    console_puts("  runtime: shell object-dispatch events jobs scheduler process-table\n");
    console_puts("  storage: vfs ramfs descriptors procfs sysfs devfs pkgfs mathfs\n");
    console_puts("  services: security logger package network gui events taskman\n");
    console_puts("  science: math objects physics job queue latex converter benchmarks\n");
    console_puts("  graphics: vga text desktop vector paths framebuffer descriptor\n");
    console_puts("  execution: /bin TRX1 bytecode loader run PROGRAM\n");
    console_puts("next good layers: ELF binary loader, framebuffer pixels, sockets API, disk\n");
}

void shell_set_eval_handler(void (*handler)(char* line)){
    eval_handler = handler;
}

void shell_eval(char* line){
    if(eval_handler)
        eval_handler(line);
    else
        console_puts("shell: no dispatcher installed\n");
}

void shell_session_init(void){
    inbuf[0] = 0;
    input_cursor = 0;
    history_count = 0;
    history_view = -1;
    editor_mode = 0;
}

void shell_set_editor_mode(int active){
    editor_mode = active;
}

char* shell_input_buffer(void){
    return inbuf;
}

size_t shell_cursor(void){
    return input_cursor;
}

void shell_set_cursor(size_t cursor){
    input_cursor = cursor;
}

void shell_prompt(void){
    char cwd[64];
    char line[112];
    if(editor_mode){
        str_copy(line, "edit> ", sizeof(line));
        console_input_write(line);
        return;
    }
    fs_pwd(cwd, sizeof(cwd));
    size_t i = 0;
    line[i++] = 't';
    line[i++] = 'r';
    line[i++] = ':';
    for(size_t j=0; cwd[j] && i + 4 < sizeof(line); j++)
        line[i++] = cwd[j];
    line[i++] = ' ';
    line[i++] = '$';
    line[i++] = ' ';
    line[i] = 0;
    console_input_write(line);
}

void shell_redraw(size_t* len){
    char prefix[96];
    char line[SHELL_INBUF_MAX + 96];
    if(editor_mode){
        str_copy(prefix, "edit> ", sizeof(prefix));
    } else {
        char cwd[64];
        fs_pwd(cwd, sizeof(cwd));
        size_t i = 0;
        prefix[i++] = 't';
        prefix[i++] = 'r';
        prefix[i++] = ':';
        for(size_t j=0; cwd[j] && i + 4 < sizeof(prefix); j++)
            prefix[i++] = cwd[j];
        prefix[i++] = ' ';
        prefix[i++] = '$';
        prefix[i++] = ' ';
        prefix[i] = 0;
    }
    size_t prefix_len = str_len(prefix);
    *len = str_len(inbuf);
    if(input_cursor > *len)
        input_cursor = *len;
    size_t i = 0;
    for(size_t j=0; prefix[j] && i + 1 < sizeof(line); j++)
        line[i++] = prefix[j];
    for(size_t j=0; inbuf[j] && i + 1 < sizeof(line); j++)
        line[i++] = inbuf[j];
    line[i] = 0;
    console_input_write_at(line, prefix_len + input_cursor);
}

void shell_set_input_text(const char* text, size_t* len){
    str_copy(inbuf, text, SHELL_INBUF_MAX);
    *len = str_len(inbuf);
    input_cursor = *len;
    shell_redraw(len);
}

void shell_set_input_text_cursor(const char* text, size_t cursor, size_t* len){
    str_copy(inbuf, text, SHELL_INBUF_MAX);
    *len = str_len(inbuf);
    input_cursor = cursor > *len ? *len : cursor;
    shell_redraw(len);
}

void shell_insert_char(char c, size_t* len){
    if(*len >= SHELL_INBUF_MAX - 1)
        return;
    if(input_cursor > *len)
        input_cursor = *len;
    for(size_t i=*len + 1; i>input_cursor; i--)
        inbuf[i] = inbuf[i - 1];
    inbuf[input_cursor++] = c;
    (*len)++;
    inbuf[*len] = 0;
    shell_redraw(len);
}

void shell_backspace(size_t* len){
    if(*len == 0 || input_cursor == 0)
        return;
    if(input_cursor > *len)
        input_cursor = *len;
    for(size_t i=input_cursor - 1; i<*len; i++)
        inbuf[i] = inbuf[i + 1];
    (*len)--;
    input_cursor--;
    inbuf[*len] = 0;
    shell_redraw(len);
}

void shell_echo_command(const char* line){
    if(editor_mode){
        console_puts("edit> ");
        console_puts(line);
        console_putc('\n');
        return;
    }
    char cwd[64];
    fs_pwd(cwd, sizeof(cwd));
    console_puts("tr:");
    console_puts(cwd);
    console_puts(" $ ");
    console_puts(line);
    console_putc('\n');
}

void shell_history_add(const char* line){
    if(line[0] == 0)
        return;
    if(history_count < SHELL_HISTORY_MAX){
        str_copy(history[history_count++], line, SHELL_INBUF_MAX);
        return;
    }
    for(size_t i=1; i<SHELL_HISTORY_MAX; i++)
        str_copy(history[i-1], history[i], SHELL_INBUF_MAX);
    str_copy(history[SHELL_HISTORY_MAX-1], line, SHELL_INBUF_MAX);
}

void shell_history_cmd(void){
    for(size_t i=0; i<history_count; i++){
        console_write_dec((uint32_t)(i + 1));
        console_puts("  ");
        console_puts(history[i]);
        console_putc('\n');
    }
}

int shell_history_prev(size_t* len){
    if(history_count == 0)
        return 0;
    if(history_view < 0) history_view = (int)history_count - 1;
    else if(history_view > 0) history_view--;
    str_copy(inbuf, history[history_view], SHELL_INBUF_MAX);
    *len = str_len(inbuf);
    input_cursor = *len;
    shell_redraw(len);
    return 1;
}

int shell_history_next(size_t* len){
    if(history_view >= 0 && history_view + 1 < (int)history_count){
        history_view++;
        str_copy(inbuf, history[history_view], SHELL_INBUF_MAX);
    } else {
        history_view = -1;
        inbuf[0] = 0;
    }
    *len = str_len(inbuf);
    input_cursor = *len;
    shell_redraw(len);
    return 1;
}

void shell_reset_input(size_t* len){
    *len = 0;
    inbuf[0] = 0;
    input_cursor = 0;
    history_view = -1;
}
