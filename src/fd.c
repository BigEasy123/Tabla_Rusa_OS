#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "events.h"
#include "fd.h"
#include "fs.h"
#include "net.h"
#include "process.h"
#include "security.h"
#include "vfs.h"

#define FD_PROC_MAX PROCESS_MAX
#define FD_PER_PROC_MAX 6

struct fd_entry {
    int used;
    int local_id;
    uint32_t owner_pid;
    const char* type;
    char path[64];
    char mode[4];
    size_t offset;
};

struct fd_table {
    uint32_t pid;
    struct fd_entry entries[FD_PER_PROC_MAX];
};

static struct fd_table tables[FD_PROC_MAX];

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

static uint32_t pid_for_name(const char* name){
    struct process_info* proc = process_find(name);
    return proc ? proc->pid : 1;
}

static uint32_t current_pid(void){
    return 1;
}

static struct fd_table* table_for_pid(uint32_t pid){
    for(size_t i=0; i<FD_PROC_MAX; i++)
        if(tables[i].pid == pid)
            return &tables[i];
    return 0;
}

static struct fd_entry* fd_find_for_pid(uint32_t pid, int fd){
    struct fd_table* table = table_for_pid(pid);
    if(!table) return 0;
    for(size_t i=0; i<FD_PER_PROC_MAX; i++)
        if(table->entries[i].used && table->entries[i].local_id == fd)
            return &table->entries[i];
    return 0;
}

static struct fd_entry* alloc_for_pid(uint32_t pid){
    struct fd_table* table = table_for_pid(pid);
    if(!table) return 0;
    for(size_t i=0; i<FD_PER_PROC_MAX; i++){
        if(!table->entries[i].used){
            table->entries[i].used = 1;
            table->entries[i].local_id = (int)i;
            table->entries[i].owner_pid = pid;
            table->entries[i].offset = 0;
            return &table->entries[i];
        }
    }
    return 0;
}

static void print_entry(const struct fd_entry* entry){
    console_puts("pid ");
    console_write_dec(entry->owner_pid);
    console_puts(" fd ");
    console_write_dec((uint32_t)entry->local_id);
    console_puts(" ");
    console_puts(entry->type);
    console_puts(" ");
    console_puts(entry->mode);
    console_puts(" off=");
    console_write_dec((uint32_t)entry->offset);
    console_puts(" ");
    console_puts(entry->path);
    console_putc('\n');
}

void fd_init(void){
    for(size_t p=0; p<FD_PROC_MAX; p++){
        tables[p].pid = (uint32_t)p;
        for(size_t i=0; i<FD_PER_PROC_MAX; i++){
            tables[p].entries[i].used = 0;
            tables[p].entries[i].local_id = (int)i;
            tables[p].entries[i].owner_pid = (uint32_t)p;
        }
    }
    fs_append_line("/var/log/system.log", "fd: per-process descriptor tables online");
}

int fd_open_for_pid(uint32_t pid, const char* path, const char* mode){
    int writable = fd_can_write_mode(mode);
    struct fd_entry* entry;
    if(writable && !vfs_can_write(path))
        return -2;
    if(writable && !security_can_write(path))
        return -3;
    if(!writable){
        const char* text;
        if(fs_read(path, &text) != 0)
            return -1;
    } else {
        if(fs_touch(path) != 0)
            return -1;
    }
    entry = alloc_for_pid(pid);
    if(!entry)
        return -4;
    entry->type = "file";
    entry->offset = 0;
    str_copy(entry->path, path, sizeof(entry->path));
    str_copy(entry->mode, mode, sizeof(entry->mode));
    return entry->local_id;
}

int fd_open(const char* path, const char* mode){
    return fd_open_for_pid(current_pid(), path, mode);
}

int fd_open_socket_for_pid(uint32_t pid, const char* label){
    struct fd_entry* entry = alloc_for_pid(pid);
    if(!entry)
        return -1;
    entry->type = "socket";
    entry->offset = 0;
    str_copy(entry->path, label, sizeof(entry->path));
    str_copy(entry->mode, "rw", sizeof(entry->mode));
    return entry->local_id;
}

int fd_open_socket(const char* label){
    return fd_open_socket_for_pid(current_pid(), label);
}

int fd_close(int fd){
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    if(!entry) return -1;
    entry->used = 0;
    return 0;
}

int fd_read(int fd, const char** out){
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    if(!entry) return -1;
    if(str_eq(entry->type, "socket"))
        return net_fd_read(entry->owner_pid, fd, out);
    entry->offset++;
    return fs_read(entry->path, out);
}

int fd_write(int fd, const char* text){
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    if(!entry || !fd_can_write_mode(entry->mode)) return -1;
    if(str_eq(entry->type, "socket"))
        return net_fd_write(entry->owner_pid, fd, text);
    if(!vfs_can_write(entry->path) || !security_can_write(entry->path)) return -2;
    entry->offset++;
    events_emit("fs.write");
    return fs_write(entry->path, text);
}

int fd_close_process(uint32_t pid){
    struct fd_table* table = table_for_pid(pid);
    uint32_t closed = 0;
    if(!table) return -1;
    for(size_t i=0; i<FD_PER_PROC_MAX; i++){
        if(table->entries[i].used){
            table->entries[i].used = 0;
            closed++;
        }
    }
    return (int)closed;
}

uint32_t fd_count_for_pid(uint32_t pid){
    struct fd_table* table = table_for_pid(pid);
    uint32_t count = 0;
    if(!table) return 0;
    for(size_t i=0; i<FD_PER_PROC_MAX; i++)
        if(table->entries[i].used)
            count++;
    return count;
}

static void list_pid(uint32_t pid){
    struct fd_table* table = table_for_pid(pid);
    if(!table) return;
    for(size_t i=0; i<FD_PER_PROC_MAX; i++)
        if(table->entries[i].used)
            print_entry(&table->entries[i]);
}

static void list_all(void){
    for(size_t p=0; p<FD_PROC_MAX; p++)
        list_pid(tables[p].pid);
}

void fd_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        const char* name = first_arg(rest, &rest);
        if(name[0])
            list_pid(pid_for_name(name));
        else
            list_pid(current_pid());
    } else if(str_eq(action, "all")){
        list_all();
    } else if(str_eq(action, "open")){
        const char* path = first_arg(rest, &rest);
        const char* mode = first_arg(rest, &rest);
        int fd = fd_open(path, mode[0] ? mode : "r");
        if(fd < 0) console_puts("fd: open failed\n");
        else {
            console_puts("pid=");
            console_write_dec(current_pid());
            console_puts(" fd=");
            console_write_dec((uint32_t)fd);
            console_putc('\n');
        }
    } else if(str_eq(action, "openfor")){
        const char* proc_name = first_arg(rest, &rest);
        const char* path = first_arg(rest, &rest);
        const char* mode = first_arg(rest, &rest);
        int fd = fd_open_for_pid(pid_for_name(proc_name), path, mode[0] ? mode : "r");
        if(fd < 0) console_puts("fd: openfor failed\n");
        else {
            console_puts("pid=");
            console_write_dec(pid_for_name(proc_name));
            console_puts(" fd=");
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
    } else if(str_eq(action, "closeproc")){
        const char* proc_name = first_arg(rest, &rest);
        int closed = fd_close_process(pid_for_name(proc_name));
        if(closed < 0) console_puts("fd: closeproc failed\n");
        else {
            console_puts("fd: closed ");
            console_write_dec((uint32_t)closed);
            console_putc('\n');
        }
    } else {
        console_puts("usage: fd list [PROC] | all | open PATH MODE | openfor PROC PATH MODE | read FD | write FD TEXT | close FD | closeproc PROC\n");
    }
}
