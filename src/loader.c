#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "lang.h"
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
    {"/bin/mathbench.trx", "mathbench", "trx-bytecode", "JOB", "PRINT mathbench\nCALL math bench math\nJOB math-worker 16\nTICK 16\n"},
    {"/bin/gui.trx", "gui", "trx-bytecode", "SERVICE", "PRINT gui-script\nCALL gui start\nSERVICE gui start\nTICK 4\n"},
    {"/bin/netup.trx", "netup", "trx-bytecode", "NET", "PRINT netup-script\nCALL net up\nSERVICE network start\nTICK 4\n"},
    {"/bin/physics.trx", "physics", "trx-bytecode", "JOB", "PRINT physics-worker\nCALL math phys fields 1 2 3 | 4 5 6\nJOB physics-worker 24\nTICK 24\n"},
    {"/bin/readme.trx", "readme", "trx-bytecode", "READ", "READ /home/readme.txt\nTICK 2\n"},
    {"/bin/paint.trx", "paint", "trx-bytecode", "GFX", "PRINT vector-paint\nCALL gfx scene\nJOB gfx-vector 12\nTICK 12\n"}
};

static void (*call_handler)(char* command) = 0;

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

static void make_program_path(const char* name, char* out, size_t max){
    size_t pos = 0;
    const char* prefix = "/home/projects/";
    for(size_t i=0; prefix[i] && pos + 1 < max; i++)
        out[pos++] = prefix[i];
    for(size_t i=0; name[i] && pos + 6 < max; i++)
        out[pos++] = name[i];
    out[pos++] = '.';
    out[pos++] = 'r';
    out[pos++] = 'u';
    out[pos++] = 's';
    out[pos++] = 'a';
    out[pos] = 0;
}

static size_t copy_part(char* dst, size_t pos, size_t max, const char* src){
    for(size_t i=0; src[i] && pos + 1 < max; i++)
        dst[pos++] = src[i];
    dst[pos] = 0;
    return pos;
}

static void make_program_text(const char* name, const char* bytecode, char* out, size_t max){
    size_t pos = 0;
    pos = copy_part(out, pos, max, "name=");
    pos = copy_part(out, pos, max, name);
    pos = copy_part(out, pos, max, "\nkind=rusa-source\nentry=bytecode\nabi=rusa:0.1\nformat=TRX1\nbytecode:\n");
    pos = copy_part(out, pos, max, bytecode);
    if(pos + 1 < max && (pos == 0 || out[pos - 1] != '\n')){
        out[pos++] = '\n';
        out[pos] = 0;
    }
}

static const char* find_bytecode_section(const char* text){
    const char* marker = "bytecode:\n";
    for(size_t i=0; text[i]; i++){
        size_t j = 0;
        while(marker[j] && text[i + j] == marker[j])
            j++;
        if(marker[j] == 0)
            return text + i + j;
    }
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
            "bytecode:\n",
            programs[i].bytecode,
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
        "CALL shell command\n"
        "JOB name ticks\n"
        "SERVICE name start|stop\n"
        "TICK n\n"
        "HALT\n");
    fs_append_line("/var/log/system.log", "loader: script executable table online");
}

void loader_set_call_handler(void (*handler)(char* command)){
    call_handler = handler;
}

static int trx_run_line(char* line){
    char* rest;
    const char* op = first_arg(line, &rest);
    if(op[0] == 0) return 0;
    if(str_eq(op, "PRINT")){
        console_puts(rest);
        console_putc('\n');
    } else if(str_eq(op, "READ")){
        const char* text;
        const char* path = first_arg(rest, &rest);
        if(fs_read(path, &text) == 0) console_puts(text);
        else console_puts("trx READ: not found\n");
    } else if(str_eq(op, "CALL")){
        if(call_handler)
            call_handler(rest);
        else
            console_puts("trx CALL: no shell bridge\n");
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
        return 1;
    } else {
        console_puts("trx: unknown opcode ");
        console_puts(op);
        console_putc('\n');
    }
    return 0;
}

static void trx_run(const char* code){
    char buf[256];
    size_t pos = 0;
    for(size_t i=0; code[i]; i++){
        if(code[i] == '\n' || pos + 1 >= sizeof(buf)){
            buf[pos] = 0;
            if(trx_run_line(buf)) return;
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
    const char* file_text = 0;
    const char* bytecode = 0;
    const char* run_path = path;
    const char* run_kind = "trx-file";
    const char* run_entry = "bytecode";
    if(prog){
        run_path = prog->path;
        run_kind = prog->kind;
        run_entry = prog->entry;
    }
    if(fs_read(run_path, &file_text) != 0 && !prog){
        char project_path[64];
        make_program_path(path, project_path, sizeof(project_path));
        run_path = project_path;
        if(fs_read(run_path, &file_text) != 0)
            return -1;
    }
    if(file_text)
        bytecode = find_bytecode_section(file_text);
    if(!bytecode && prog)
        bytecode = prog->bytecode;
    if(!bytecode && file_text)
        return lang_run_source(file_text, run_path, args);
    if(!bytecode)
        return -1;
    process_set_running("compute", 1);
    jobs_account("program-loader", 3);
    console_puts("exec ");
    console_puts(run_path);
    console_puts(" kind=");
    console_puts(run_kind);
    if(args && args[0]){
        console_puts(" args=");
        console_puts(args);
    }
    console_putc('\n');
    console_puts("entry: ");
    console_puts(run_entry);
    console_putc('\n');
    trx_run(bytecode);
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
        const char* file_text = 0;
        const char* bytecode = 0;
        if(!prog) console_puts("loader: program not found\n");
        else {
            if(fs_read(prog->path, &file_text) == 0)
                bytecode = find_bytecode_section(file_text);
            console_puts(bytecode ? bytecode : prog->bytecode);
        }
    } else if(str_eq(action, "new")){
        const char* name = first_arg(rest, &rest);
        char path[64];
        char text[256];
        if(name[0] == 0){
            console_puts("usage: loader new NAME\n");
            return;
        }
        make_program_path(name, path, sizeof(path));
        make_program_text(name, "PRINT hello-from-rusa\nHALT\n", text, sizeof(text));
        fs_write(path, text);
        console_puts("created ");
        console_puts(path);
        console_putc('\n');
    } else if(str_eq(action, "write")){
        const char* name = first_arg(rest, &rest);
        char path[64];
        char text[256];
        if(name[0] == 0 || rest[0] == 0){
            console_puts("usage: loader write NAME BYTECODE_LINE\n");
            return;
        }
        make_program_path(name, path, sizeof(path));
        make_program_text(name, rest, text, sizeof(text));
        fs_write(path, text);
        console_puts("wrote ");
        console_puts(path);
        console_putc('\n');
    } else if(str_eq(action, "append")){
        const char* name = first_arg(rest, &rest);
        char path[64];
        char text[512];
        char bytecode[256];
        const char* old_text;
        const char* old_bytecode = "";
        if(name[0] == 0 || rest[0] == 0){
            console_puts("usage: loader append NAME BYTECODE_LINE\n");
            return;
        }
        make_program_path(name, path, sizeof(path));
        if(fs_read(path, &old_text) == 0){
            const char* found = find_bytecode_section(old_text);
            if(found) old_bytecode = found;
        }
        size_t pos = 0;
        pos = copy_part(bytecode, pos, sizeof(bytecode), old_bytecode);
        if(pos && bytecode[pos - 1] != '\n')
            pos = copy_part(bytecode, pos, sizeof(bytecode), "\n");
        pos = copy_part(bytecode, pos, sizeof(bytecode), rest);
        pos = copy_part(bytecode, pos, sizeof(bytecode), "\n");
        make_program_text(name, bytecode, text, sizeof(text));
        fs_write(path, text);
        console_puts("appended ");
        console_puts(path);
        console_putc('\n');
    } else if(str_eq(action, "show")){
        const char* name = first_arg(rest, &rest);
        char path[64];
        const char* text;
        make_program_path(name, path, sizeof(path));
        if(fs_read(path, &text) == 0) console_puts(text);
        else console_puts("loader: source not found\n");
    } else if(str_eq(action, "clear")){
        const char* name = first_arg(rest, &rest);
        char path[64];
        char text[256];
        make_program_path(name, path, sizeof(path));
        make_program_text(name, "", text, sizeof(text));
        fs_write(path, text);
        console_puts("cleared ");
        console_puts(path);
        console_putc('\n');
    } else if(str_eq(action, "edit")){
        const char* name = first_arg(rest, &rest);
        char path[64];
        char command[80] = "edit ";
        size_t pos = 5;
        make_program_path(name, path, sizeof(path));
        for(size_t i=0; path[i] && pos + 1 < sizeof(command); i++)
            command[pos++] = path[i];
        command[pos] = 0;
        if(call_handler) call_handler(command);
        else console_puts("loader: no editor bridge\n");
    } else if(str_eq(action, "run") || str_eq(action, "exec")){
        const char* name = first_arg(rest, &rest);
        if(loader_run(name, rest) != 0)
            console_puts("loader: program not found\n");
    } else {
        console_puts("usage: loader list | info NAME | bytecode NAME | new/write/append/show/clear/edit/run NAME\n");
    }
}
