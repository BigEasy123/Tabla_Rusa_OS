#include <stddef.h>
#include "console.h"
#include "fs.h"
#include "events.h"

#define EVENT_MAX 6

struct event_rule {
    const char* name;
    const char* trigger;
    const char* action;
    int enabled;
};

static struct event_rule event_rules[EVENT_MAX] = {
    {"download-log", "file.created:/home/downloads", "log system download event", 1},
    {"editor-focus", "process.start:editor", "window editor focus", 1},
    {"net-audit", "net.link:up", "log network link up", 1},
    {"security-audit", "security.mode:change", "log security mode change", 1},
    {"service-audit", "service.state:change", "log service state change", 1},
    {"fs-audit", "fs.write", "log filesystem mutation", 1}
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

void events_init(void){
    fs_append_line("/var/log/system.log", "events: bus online");
}

void events_emit(const char* trigger){
    for(size_t i=0; i<EVENT_MAX; i++){
        if(event_rules[i].enabled && str_eq(event_rules[i].trigger, trigger)){
            fs_append_line("/var/log/system.log", event_rules[i].action);
            console_puts("event ");
            console_puts(event_rules[i].name);
            console_puts(" -> ");
            console_puts(event_rules[i].action);
            console_putc('\n');
        }
    }
}

void events_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<EVENT_MAX; i++){
            console_puts(event_rules[i].enabled ? "[on]  " : "[off] ");
            console_puts(event_rules[i].name);
            console_puts(" when ");
            console_puts(event_rules[i].trigger);
            console_puts(" -> ");
            console_puts(event_rules[i].action);
            console_putc('\n');
        }
        return;
    }
    for(size_t i=0; i<EVENT_MAX; i++){
        if(str_eq(event_rules[i].name, name)){
            if(str_eq(action, "enable")){
                event_rules[i].enabled = 1;
            } else if(str_eq(action, "disable")){
                event_rules[i].enabled = 0;
            } else if(str_eq(action, "emit")){
                events_emit(event_rules[i].trigger);
            } else {
                console_puts("usage: event list | enable NAME | disable NAME | emit NAME\n");
            }
            return;
        }
    }
    console_puts("event: not found\n");
}
