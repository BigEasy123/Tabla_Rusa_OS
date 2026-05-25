#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "lang.h"

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

void lang_init(void){
    fs_mkdir("/share/rusa");
    fs_mkdir("/lib/rusa");
    fs_write("/share/rusa/README",
        "Rusa is the native Tabla Rusa OS language.\n"
        "The OS interface is the language: file[], process[], service[], window[], program[].\n");
    fs_write("/share/rusa/keywords",
        "inspect\nspawn\non\nif\nelse\nparallel\nrun\nfile\nprocess\nservice\nwindow\nprogram\nmath\nphys\n");
    fs_write("/share/rusa/examples",
        "inspect memory\n"
        "file[\"/home/readme.txt\"].read()\n"
        "program[\"physics\"].run()\n"
        "math phys fields 1 2 3 | 4 5 6\n"
        "loader write demo PRINT hello-from-rusa\n"
        "loader append demo CALL lang keywords\n"
        "run demo\n");
    fs_write("/share/rusa/objects",
        "file: read exists open write\n"
        "process: trace stop\n"
        "service: start stop status\n"
        "window: focus info\n"
        "program: run\n"
        "net: use net socket plus fd read/write\n");
    fs_write("/lib/rusa/std.trx",
        "name=std\nkind=rusa-stdlib\nentry=bytecode\nabi=rusa:0.1\nformat=TRX1\nbytecode:\n"
        "PRINT Rusa standard library loaded\n"
        "CALL object types\n"
        "HALT\n");
    fs_append_line("/var/log/system.log", "lang: Rusa language docs online");
}

void lang_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "about")){
        console_puts("Rusa 0.1 - Tabla Rusa OS native language\n");
        console_puts("principle: OS objects are language objects\n");
        console_puts("docs: /share/rusa/README /share/rusa/keywords /share/rusa/examples\n");
    } else if(str_eq(action, "keywords")){
        const char* text;
        if(fs_read("/share/rusa/keywords", &text) == 0) console_puts(text);
    } else if(str_eq(action, "examples")){
        const char* text;
        if(fs_read("/share/rusa/examples", &text) == 0) console_puts(text);
    } else if(str_eq(action, "docs")){
        const char* text;
        if(fs_read("/share/rusa/README", &text) == 0) console_puts(text);
    } else if(str_eq(action, "stdlib")){
        const char* text;
        if(fs_read("/lib/rusa/std.trx", &text) == 0) console_puts(text);
    } else if(str_eq(action, "std")){
        console_puts("Rusa std modules: file net math gui project\n");
        console_puts("use: lang import std\n");
    } else if(str_eq(action, "import")){
        const char* name = first_arg(rest, &rest);
        if(str_eq(name, "std")){
            console_puts("import std: file, net, math, gui, project helpers available\n");
        } else {
            console_puts("import: module not found\n");
        }
    } else if(str_eq(action, "object")){
        const char* name = first_arg(rest, &rest);
        const char* text;
        if(fs_read("/share/rusa/objects", &text) != 0){
            console_puts("object docs missing\n");
        } else if(name[0] == 0){
            console_puts(text);
        } else {
            console_puts("Rusa object ");
            console_puts(name);
            console_puts(": see /share/rusa/objects\n");
        }
    } else {
        console_puts("usage: lang about | keywords | examples | docs | stdlib | std | import std | object [NAME]\n");
    }
}
