#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "fd.h"
#include "fs.h"
#include "loader.h"
#include "object.h"
#include "process.h"
#include "service.h"
#include "window.h"

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
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

static int str_starts(const char* s, const char* prefix){
    while(*prefix){
        if(*s++ != *prefix++) return 0;
    }
    return 1;
}

static const char* skip_space(const char* s){
    while(is_space(*s)) s++;
    return s;
}

static size_t str_len(const char* s){
    size_t len = 0;
    while(s[len]) len++;
    return len;
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

static int read_quoted(const char** cursor, char* out, size_t max){
    const char* p = skip_space(*cursor);
    size_t i = 0;
    if(*p != '"') return 0;
    p++;
    while(*p && *p != '"'){
        if(i + 1 < max) out[i++] = *p;
        p++;
    }
    if(*p != '"') return 0;
    out[i] = 0;
    *cursor = p + 1;
    return 1;
}

static int parse_object(const char* line, char* type, size_t type_max, char* key, size_t key_max, const char** rest){
    const char* p = skip_space(line);
    size_t i = 0;
    while((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')){
        if(i + 1 < type_max) type[i++] = lower_char(*p);
        p++;
    }
    type[i] = 0;
    p = skip_space(p);
    if(*p != '[') return 0;
    p++;
    if(!read_quoted(&p, key, key_max)) return 0;
    p = skip_space(p);
    if(*p != ']') return 0;
    p++;
    p = skip_space(p);
    if(*p != '.') return 0;
    *rest = p + 1;
    return type[0] != 0;
}

int object_eval(const char* line){
    char type[24];
    char key[64];
    char arg[96];
    const char* rest;
    if(!parse_object(line, type, sizeof(type), key, sizeof(key), &rest))
        return 0;
    if(str_eq(type, "file")){
        if(str_starts(rest, "read()")){
            const char* text;
            if(fs_read(key, &text) == 0) console_puts(text);
            else console_puts("object file: not found\n");
        } else if(str_starts(rest, "exists()")){
            int ftype;
            size_t size;
            console_puts(fs_stat(key, &ftype, &size) == 0 ? "true\n" : "false\n");
        } else if(str_starts(rest, "open()")){
            int fd = fd_open(key, "rw");
            if(fd < 0) console_puts("object file: open failed\n");
            else {
                console_puts("fd=");
                console_write_dec((uint32_t)fd);
                console_putc('\n');
            }
        } else if(str_starts(rest, "write(")){
            rest += str_len("write(");
            int out_fd = fd_open(key, "w");
            if(out_fd >= 0 && read_quoted(&rest, arg, sizeof(arg)) && fd_write(out_fd, arg) == 0){
                fd_close(out_fd);
                console_puts("file.write: ok\n");
            } else {
                if(out_fd >= 0) fd_close(out_fd);
                console_puts("file.write: failed\n");
            }
        } else {
            console_puts("object file methods: read exists open write\n");
        }
    } else if(str_eq(type, "process")){
        struct process_info* proc = process_find(key);
        if(!proc) console_puts("object process: not found\n");
        else if(str_starts(rest, "trace()")){
            console_puts(proc->name);
            console_puts(" pid=");
            console_write_dec(proc->pid);
            console_puts(" priority=");
            console_write_dec(proc->priority);
            console_puts(" ticks=");
            console_write_dec(proc->ticks);
            console_putc('\n');
        } else if(str_starts(rest, "stop()")){
            process_stop(key);
            console_puts("process.stop: ok\n");
        } else {
            console_puts("object process methods: trace stop\n");
        }
    } else if(str_eq(type, "service")){
        if(str_starts(rest, "start()")){
            service_set_running(key, 1);
            console_puts("service.start: ok\n");
        } else if(str_starts(rest, "stop()")){
            service_set_running(key, 0);
            console_puts("service.stop: ok\n");
        } else if(str_starts(rest, "status()")){
            console_puts(service_is_running(key) ? "running\n" : "stopped\n");
        } else {
            console_puts("object service methods: start stop status\n");
        }
    } else if(str_eq(type, "window")){
        if(str_starts(rest, "focus()")){
            window_focus(key);
            console_puts("window.focus: ok\n");
        } else if(str_starts(rest, "info()")){
            struct window_info* win = window_find(key);
            if(!win) console_puts("window: not found\n");
            else {
                console_puts(win->name);
                console_puts(win->focused ? " focused\n" : " unfocused\n");
            }
        } else {
            console_puts("object window methods: focus info\n");
        }
    } else if(str_eq(type, "program")){
        if(str_starts(rest, "run()")){
            if(loader_run(key, "") != 0) console_puts("program.run: not found\n");
        } else {
            console_puts("object program methods: run\n");
        }
    } else {
        console_puts("object: unknown type\n");
    }
    return 1;
}

void object_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "types")){
        console_puts("object types: file process service window program\n");
        console_puts("forms: file[\"/home/a\"].open(), process[\"compute\"].trace(), program[\"hello\"].run()\n");
    } else if(str_eq(action, "eval")){
        if(!object_eval(rest))
            console_puts("object: parse failed\n");
    } else {
        console_puts("usage: object types | eval EXPR\n");
    }
}
