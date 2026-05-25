#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "events.h"
#include "fd.h"
#include "fs.h"
#include "security.h"
#include "vfs.h"

#define FD_MAX 8

struct fd_entry {
    int used;
    int id;
    char path[64];
    char mode[4];
    size_t offset;
};

static struct fd_entry fds[FD_MAX];

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

static void str_copy(char* dst, const char* src, size_t max){
    size_t i = 0;
    if(max == 0) return;
    while(i + 1 < max && src[i]){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
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

static int parse_i32(const char* s){
    int value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (*s - '0');
        s++;
    }
    return value;
}

static int fd_can_write_mode(const char* mode){
    return mode[0] == 'w' || mode[0] == 'a' || (mode[0] == 'r' && mode[1] == 'w');
}

static struct fd_entry* fd_find(int fd){
    for(size_t i=0; i<FD_MAX; i++)
        if(fds[i].used && fds[i].id == fd)
            return &fds[i];
    return 0;
}

void fd_init(void){
    for(size_t i=0; i<FD_MAX; i++){
        fds[i].used = 0;
        fds[i].id = (int)i;
    }
    fs_append_line("/var/log/system.log", "fd: file descriptor table online");
}

int fd_open(const char* path, const char* mode){
    if(mode[0] != 'r' && !vfs_can_write(path))
        return -2;
    if(mode[0] != 'r' && !security_can_write(path))
        return -3;
    if(mode[0] == 'r'){
        const char* text;
        if(fs_read(path, &text) != 0)
            return -1;
    } else {
        if(fs_touch(path) != 0)
            return -1;
    }
    for(size_t i=0; i<FD_MAX; i++){
        if(!fds[i].used){
            fds[i].used = 1;
            fds[i].offset = 0;
            str_copy(fds[i].path, path, sizeof(fds[i].path));
            str_copy(fds[i].mode, mode, sizeof(fds[i].mode));
            return fds[i].id;
        }
    }
    return -4;
}

int fd_close(int fd){
    struct fd_entry* entry = fd_find(fd);
    if(!entry) return -1;
    entry->used = 0;
    return 0;
}

int fd_read(int fd, const char** out){
    struct fd_entry* entry = fd_find(fd);
    if(!entry) return -1;
    return fs_read(entry->path, out);
}

int fd_write(int fd, const char* text){
    struct fd_entry* entry = fd_find(fd);
    if(!entry || !fd_can_write_mode(entry->mode)) return -1;
    if(!vfs_can_write(entry->path) || !security_can_write(entry->path)) return -2;
    events_emit("fs.write");
    return fs_write(entry->path, text);
}

void fd_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<FD_MAX; i++){
            if(fds[i].used){
                console_puts("fd ");
                console_write_dec((uint32_t)fds[i].id);
                console_puts(" ");
                console_puts(fds[i].mode);
                console_puts(" ");
                console_puts(fds[i].path);
                console_putc('\n');
            }
        }
    } else if(str_eq(action, "open")){
        const char* path = first_arg(rest, &rest);
        const char* mode = first_arg(rest, &rest);
        int fd = fd_open(path, mode[0] ? mode : "r");
        if(fd < 0) console_puts("fd: open failed\n");
        else {
            console_puts("fd=");
            console_write_dec((uint32_t)fd);
            console_putc('\n');
        }
    } else if(str_eq(action, "read")){
        int id = parse_i32(first_arg(rest, &rest));
        const char* text;
        if(fd_read(id, &text) != 0) console_puts("fd: read failed\n");
        else console_puts(text);
    } else if(str_eq(action, "write")){
        int id = parse_i32(first_arg(rest, &rest));
        if(fd_write(id, rest) != 0) console_puts("fd: write failed\n");
        else console_puts("fd: written\n");
    } else if(str_eq(action, "close")){
        int id = parse_i32(first_arg(rest, &rest));
        if(fd_close(id) != 0) console_puts("fd: close failed\n");
        else console_puts("fd: closed\n");
    } else {
        console_puts("usage: fd list | open PATH MODE | read FD | write FD TEXT | close FD\n");
    }
}
