#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "loader.h"
#include "process.h"

#define PROGRAM_MAX 7

struct program_info {
    const char* path;
    const char* name;
    const char* kind;
    const char* entry;
    const char* bytecode;
};

static struct program_info programs[PROGRAM_MAX] = {
    {"/bin/hello.trx", "hello", "trx-bytecode", "PRINT", "PRINT hello-from-trx\nTICK 1\n"},
    {"/bin/mathbench.trx", "mathbench", "trx-bytecode", "JOB", "PRINT mathbench\nJOB math-worker 16\nTICK 16\n"},
    {"/bin/gui.trx", "gui", "trx-bytecode", "SERVICE", "PRINT gui-script\nSERVICE gui start\nTICK 4\n"},
    {"/bin/netup.trx", "netup", "trx-bytecode", "NET", "PRINT netup-script\nSERVICE network start\nTICK 4\n"},
    {"/bin/physics.trx", "physics", "trx-bytecode", "JOB", "PRINT physics-worker\nJOB physics-worker 24\nTICK 24\n"},
    {"/bin/readme.trx", "readme", "trx-bytecode", "READ", "READ /home/readme.txt\nTICK 2\n"},
    {"/bin/paint.trx", "paint", "trx-bytecode", "GFX", "PRINT vector-paint\nJOB gfx-vector 12\nTICK 12\n"}
};

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
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

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static struct program_info* program_find(const char* key){
    for(size_t i=0; i<PROGRAM_MAX; i++)
        if(str_eq(programs[i].path, key) || str_eq(programs[i].name, key))
            return &programs[i];
    return 0;
}

void loader_init(void){
    fs_mkdir("/bin");
    for(size_t i=0; i<PROGRAM_MAX; i++){
        char manifest[160];
        size_t pos = 0;
        const char* parts[] = {
            "name=", programs[i].name, "\n",
            "kind=", programs[i].kind, "\n",
            "entry=", programs[i].entry, "\n",
            "abi=tabla:0.1-script\n",
            "format=TRX1\n",
            0
        };
        for(size_t p=0; parts[p]; p++)
            for(size_t j=0; parts[p][j] && pos + 1 < sizeof(manifest); j++)
                manifest[pos++] = parts[p][j];
        manifest[pos] = 0;
        fs_write(programs[i].path, manifest);
    }
    fs_write("/share/docs/trx-runtime.txt",
        "TRX1 bytecode instructions:\n"
        "PRINT text\n"
        "READ /path\n"
        "JOB name ticks\n"
        "SERVICE name start|stop\n"
        "TICK n\n"
        "HALT\n");
    fs_append_line("/var/log/system.log", "loader: script executable table online");
}

static void trx_run_line(char* line){
    char* rest;
    const char* op = first_arg(line, &rest);
    if(op[0] == 0) return;
    if(str_eq(op, "PRINT")){
        console_puts(rest);
        console_putc('\n');
    } else if(str_eq(op, "READ")){
        const char* text;
        const char* path = first_arg(rest, &rest);
        if(fs_read(path, &text) == 0) console_puts(text);
        else console_puts("trx READ: not found\n");
    } else if(str_eq(op, "JOB")){
        const char* name = first_arg(rest, &rest);
        jobs_account(name, parse_u32(rest));
    } else if(str_eq(op, "SERVICE")){
        const char* name = first_arg(rest, &rest);
        const char* action = first_arg(rest, &rest);
        if(str_eq(action, "start")) process_set_running(name, 1);
        else if(str_eq(action, "stop")) process_set_running(name, 0);
    } else if(str_eq(op, "TICK")){
        uint32_t ticks = parse_u32(rest);
        process_tick("compute", ticks);
        jobs_account("program-loader", ticks);
    } else if(str_eq(op, "HALT")){
        console_puts("trx halt\n");
    } else {
        console_puts("trx: unknown opcode ");
        console_puts(op);
        console_putc('\n');
    }
}

static void trx_run(const char* code){
    char buf[256];
    size_t pos = 0;
    for(size_t i=0; code[i]; i++){
        if(code[i] == '\n' || pos + 1 >= sizeof(buf)){
            buf[pos] = 0;
            trx_run_line(buf);
            pos = 0;
        } else {
            buf[pos++] = code[i];
        }
    }
    if(pos){
        buf[pos] = 0;
        trx_run_line(buf);
    }
}

int loader_run(const char* path, const char* args){
    struct program_info* prog = program_find(path);
    if(!prog)
        return -1;
    process_set_running("compute", 1);
    jobs_account("program-loader", 3);
    console_puts("exec ");
    console_puts(prog->path);
    console_puts(" kind=");
    console_puts(prog->kind);
    if(args && args[0]){
        console_puts(" args=");
        console_puts(args);
    }
    console_putc('\n');
    console_puts("entry: ");
    console_puts(prog->entry);
    console_putc('\n');
    trx_run(prog->bytecode);
    return 0;
}

void loader_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<PROGRAM_MAX; i++){
            console_puts(programs[i].path);
            console_puts(" ");
            console_puts(programs[i].kind);
            console_puts(" -> ");
            console_puts(programs[i].entry);
            console_putc('\n');
        }
    } else if(str_eq(action, "info")){
        const char* name = first_arg(rest, &rest);
        struct program_info* prog = program_find(name);
        if(!prog) console_puts("loader: program not found\n");
        else {
            console_puts("path=");
            console_puts(prog->path);
            console_puts(" name=");
            console_puts(prog->name);
            console_puts(" kind=");
            console_puts(prog->kind);
            console_puts(" abi=tabla:0.1-script format=TRX1\n");
        }
    } else if(str_eq(action, "bytecode")){
        const char* name = first_arg(rest, &rest);
        struct program_info* prog = program_find(name);
        if(!prog) console_puts("loader: program not found\n");
        else console_puts(prog->bytecode);
    } else if(str_eq(action, "run") || str_eq(action, "exec")){
        const char* name = first_arg(rest, &rest);
        if(loader_run(name, rest) != 0)
            console_puts("loader: program not found\n");
    } else {
        console_puts("usage: loader list | info NAME | bytecode NAME | run NAME [ARGS]\n");
    }
}
