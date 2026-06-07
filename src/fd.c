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

static size_t str_len(const char* s){
    size_t len = 0;
    while(s && s[len])
        len++;
    return len;
}

static void str_append(char* dst, const char* src, size_t max){
    size_t i = 0;
    size_t j = 0;
    if(max == 0)
        return;
    while(dst[i] && i + 1 < max)
        i++;
    while(src && src[j] && i + 1 < max)
        dst[i++] = src[j++];
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
        if(!fs_can_read(path))
            return -5;
        if(fs_read(path, &text) != 0)
            return -1;
    } else {
        if(fs_stat(path, 0, 0) == 0 && !fs_can_write(path))
            return -5;
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
    if(!fs_can_read(entry->path))
        return -2;
    int r = fs_read(entry->path, out);
    if(r == 0)
        entry->offset = str_len(*out);
    return r;
}

int fd_read_chunk(int fd, char* out, size_t max){
    const char* text;
    size_t len;
    size_t i = 0;
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    if(!entry || !out || max == 0)
        return -1;
    if(str_eq(entry->type, "socket"))
        return -2;
    if(!fs_can_read(entry->path))
        return -4;
    if(fs_read(entry->path, &text) != 0)
        return -3;
    len = str_len(text);
    if(entry->offset > len)
        entry->offset = len;
    while(text[entry->offset] && i + 1 < max){
        out[i++] = text[entry->offset++];
    }
    out[i] = 0;
    return (int)i;
}

int fd_write(int fd, const char* text){
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    char merged[512];
    const char* existing;
    if(!entry || !fd_can_write_mode(entry->mode)) return -1;
    if(str_eq(entry->type, "socket"))
        return net_fd_write(entry->owner_pid, fd, text);
    if(!vfs_can_write(entry->path) || !security_can_write(entry->path) || !fs_can_write(entry->path)) return -2;
    if(entry->mode[0] == 'a'){
        merged[0] = 0;
        if(fs_read(entry->path, &existing) == 0)
            str_copy(merged, existing, sizeof(merged));
        str_append(merged, text, sizeof(merged));
        events_emit("fs.write");
        if(fs_write(entry->path, merged) != 0)
            return -3;
        entry->offset = str_len(merged);
        return 0;
    }
    events_emit("fs.write");
    if(fs_write(entry->path, text) != 0)
        return -3;
    entry->offset = str_len(text);
    return 0;
}

int fd_write_chunk(int fd, const char* text, size_t count){
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    char merged[512];
    const char* existing = "";
    size_t existing_len;
    size_t write_len = 0;
    size_t pos = 0;
    size_t suffix_start;
    if(!entry || !fd_can_write_mode(entry->mode) || !text)
        return -1;
    if(str_eq(entry->type, "socket"))
        return -2;
    if(!vfs_can_write(entry->path) || !security_can_write(entry->path) || !fs_can_write(entry->path))
        return -3;
    if(fs_read(entry->path, &existing) != 0)
        existing = "";
    existing_len = str_len(existing);
    if(entry->offset > existing_len)
        entry->offset = existing_len;
    while(write_len < count && text[write_len])
        write_len++;
    for(size_t i=0; i<entry->offset && pos + 1 < sizeof(merged); i++)
        merged[pos++] = existing[i];
    for(size_t i=0; i<write_len && pos + 1 < sizeof(merged); i++)
        merged[pos++] = text[i];
    suffix_start = entry->offset + write_len;
    if(suffix_start < existing_len){
        for(size_t i=suffix_start; existing[i] && pos + 1 < sizeof(merged); i++)
            merged[pos++] = existing[i];
    }
    merged[pos] = 0;
    events_emit("fs.write");
    if(fs_write(entry->path, merged) != 0)
        return -4;
    entry->offset += write_len;
    return (int)write_len;
}

int fd_seek(int fd, size_t offset){
    const char* text;
    size_t len;
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    if(!entry || str_eq(entry->type, "socket"))
        return -1;
    if(fs_read(entry->path, &text) != 0)
        return -2;
    len = str_len(text);
    entry->offset = offset > len ? len : offset;
    return 0;
}

size_t fd_tell(int fd){
    struct fd_entry* entry = fd_find_for_pid(current_pid(), fd);
    if(!entry)
        return 0;
    return entry->offset;
}

int fd_dup_to_pid(uint32_t from_pid, int fd, uint32_t to_pid){
    struct fd_entry* src = fd_find_for_pid(from_pid, fd);
    struct fd_entry* dst;
    if(!src)
        return -1;
    dst = alloc_for_pid(to_pid);
    if(!dst)
        return -2;
    dst->type = src->type;
    dst->offset = src->offset;
    str_copy(dst->path, src->path, sizeof(dst->path));
    str_copy(dst->mode, src->mode, sizeof(dst->mode));
    return dst->local_id;
}

int fd_inherit(uint32_t parent_pid, uint32_t child_pid){
    struct fd_table* table = table_for_pid(parent_pid);
    int copied = 0;
    if(!table)
        return -1;
    for(size_t i=0; i<FD_PER_PROC_MAX; i++){
        if(table->entries[i].used &&
           fd_dup_to_pid(parent_pid, table->entries[i].local_id, child_pid) >= 0)
            copied++;
    }
    return copied;
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
    } else if(str_eq(action, "chunk")){
        int id = parse_i32(first_arg(rest, &rest));
        char chunk[96];
        if(fd_read_chunk(id, chunk, sizeof(chunk)) < 0) console_puts("fd: chunk failed\n");
        else console_puts(chunk);
    } else if(str_eq(action, "seek")){
        int id = parse_i32(first_arg(rest, &rest));
        size_t offset = (size_t)parse_i32(first_arg(rest, &rest));
        if(fd_seek(id, offset) != 0) console_puts("fd: seek failed\n");
        else console_puts("fd: seek ok\n");
    } else if(str_eq(action, "tell")){
        int id = parse_i32(first_arg(rest, &rest));
        console_write_dec((uint32_t)fd_tell(id));
        console_putc('\n');
    } else if(str_eq(action, "write")){
        int id = parse_i32(first_arg(rest, &rest));
        if(fd_write(id, rest) != 0) console_puts("fd: write failed\n");
        else console_puts("fd: written\n");
    } else if(str_eq(action, "pwrite") || str_eq(action, "write-chunk")){
        int id = parse_i32(first_arg(rest, &rest));
        int count = parse_i32(first_arg(rest, &rest));
        int wrote = fd_write_chunk(id, rest, count > 0 ? (size_t)count : str_len(rest));
        if(wrote < 0) console_puts("fd: partial write failed\n");
        else {
            console_puts("fd: partial wrote ");
            console_write_dec((uint32_t)wrote);
            console_putc('\n');
        }
    } else if(str_eq(action, "dup")){
        const char* from_name = first_arg(rest, &rest);
        int id = parse_i32(first_arg(rest, &rest));
        const char* to_name = first_arg(rest, &rest);
        int dup = fd_dup_to_pid(pid_for_name(from_name), id, pid_for_name(to_name));
        if(dup < 0) console_puts("fd: dup failed\n");
        else {
            console_puts("fd: dup ");
            console_write_dec((uint32_t)dup);
            console_putc('\n');
        }
    } else if(str_eq(action, "inherit")){
        const char* from_name = first_arg(rest, &rest);
        const char* to_name = first_arg(rest, &rest);
        int copied = fd_inherit(pid_for_name(from_name), pid_for_name(to_name));
        if(copied < 0) console_puts("fd: inherit failed\n");
        else {
            console_puts("fd: inherited ");
            console_write_dec((uint32_t)copied);
            console_putc('\n');
        }
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
        console_puts("usage: fd list [PROC] | all | open PATH MODE | openfor PROC PATH MODE | read FD | chunk FD | seek FD OFFSET | tell FD | write FD TEXT | pwrite FD COUNT TEXT | dup FROM FD TO | inherit FROM TO | close FD | closeproc PROC\n");
    }
}
