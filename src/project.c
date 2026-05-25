#include <stddef.h>
#include "console.h"
#include "fs.h"
#include "loader.h"
#include "project.h"

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

static void project_path(const char* name, const char* suffix, char* out, size_t max){
    size_t pos = 0;
    const char* prefix = "/home/projects/";
    for(size_t i=0; prefix[i] && pos + 1 < max; i++) out[pos++] = prefix[i];
    for(size_t i=0; name[i] && pos + 1 < max; i++) out[pos++] = name[i];
    for(size_t i=0; suffix[i] && pos + 1 < max; i++) out[pos++] = suffix[i];
    out[pos] = 0;
}

void project_init(void){
    fs_mkdir("/home/projects");
    fs_append_line("/var/log/system.log", "project: workspace online");
}

void project_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        fs_ls("/home/projects");
    } else if(str_eq(action, "new")){
        const char* name = first_arg(rest, &rest);
        char src[64];
        char docs[64];
        char text[256];
        if(name[0] == 0){
            console_puts("usage: project new NAME\n");
            return;
        }
        project_path(name, ".rusa", src, sizeof(src));
        project_path(name, ".md", docs, sizeof(docs));
        fs_write(src,
            "name=project\nkind=rusa-source\nentry=bytecode\nabi=rusa:0.1\nformat=TRX1\nbytecode:\n"
            "PRINT hello-from-project\n"
            "HALT\n");
        size_t pos = 0;
        const char* parts[] = {"# ", name, "\n\nRusa project workspace.\nRun with: project run ", name, "\n", 0};
        for(size_t p=0; parts[p]; p++)
            for(size_t i=0; parts[p][i] && pos + 1 < sizeof(text); i++)
                text[pos++] = parts[p][i];
        text[pos] = 0;
        fs_write(docs, text);
        console_puts("project created ");
        console_puts(name);
        console_putc('\n');
    } else if(str_eq(action, "run")){
        const char* name = first_arg(rest, &rest);
        if(loader_run(name, rest) != 0)
            console_puts("project: run failed\n");
    } else if(str_eq(action, "docs")){
        const char* name = first_arg(rest, &rest);
        char docs[64];
        const char* text;
        project_path(name, ".md", docs, sizeof(docs));
        if(fs_read(docs, &text) == 0) console_puts(text);
        else console_puts("project: docs not found\n");
    } else if(str_eq(action, "edit")){
        const char* name = first_arg(rest, &rest);
        char src[64];
        project_path(name, ".rusa", src, sizeof(src));
        console_puts("open with: edit ");
        console_puts(src);
        console_putc('\n');
    } else {
        console_puts("usage: project list | new NAME | run NAME | docs NAME | edit NAME\n");
    }
}
