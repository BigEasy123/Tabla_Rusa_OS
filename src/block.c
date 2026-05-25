#include <stdint.h>
#include "block.h"
#include "console.h"
#include "fs.h"
#include "jobs.h"

#define BLOCK_COUNT 8
#define BLOCK_SIZE 64

static char blocks[BLOCK_COUNT][BLOCK_SIZE];

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

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static void block_write(uint32_t id, const char* text){
    uint32_t i = 0;
    if(id >= BLOCK_COUNT) return;
    while(text[i] && i + 1 < BLOCK_SIZE){
        blocks[id][i] = text[i];
        i++;
    }
    blocks[id][i] = 0;
}

void block_init(void){
    for(uint32_t i=0; i<BLOCK_COUNT; i++)
        blocks[i][0] = 0;
    block_write(0, "Tabla block device sector zero");
    fs_mkdir("/mnt/disk");
    fs_write("/system/block.txt", "device=ram-sector\nblocks=8\nblock_size=64\n");
    fs_append_line("/var/log/system.log", "block: ram sector device online");
}

void block_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    jobs_account("block-io", 1);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("block ram0 blocks=");
        console_write_dec(BLOCK_COUNT);
        console_puts(" size=");
        console_write_dec(BLOCK_SIZE);
        console_putc('\n');
    } else if(str_eq(action, "read")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        if(id >= BLOCK_COUNT) console_puts("block: out of range\n");
        else {
            console_puts(blocks[id]);
            console_putc('\n');
        }
    } else if(str_eq(action, "write")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        if(id >= BLOCK_COUNT) console_puts("block: out of range\n");
        else {
            block_write(id, rest);
            console_puts("block: written\n");
        }
    } else if(str_eq(action, "save")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        const char* path = first_arg(rest, &rest);
        if(id >= BLOCK_COUNT || path[0] == 0) console_puts("usage: block save ID PATH\n");
        else if(fs_write(path, blocks[id]) == 0) console_puts("block: saved\n");
        else console_puts("block: save failed\n");
    } else if(str_eq(action, "load")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        const char* path = first_arg(rest, &rest);
        const char* text;
        if(id >= BLOCK_COUNT || path[0] == 0) console_puts("usage: block load ID PATH\n");
        else if(fs_read(path, &text) == 0){
            block_write(id, text);
            console_puts("block: loaded\n");
        } else console_puts("block: load failed\n");
    } else {
        console_puts("usage: block status | read ID | write ID TEXT | save ID PATH | load ID PATH\n");
    }
}
