#include <stddef.h>
#include "console.h"
#include "fs.h"
#include "events.h"

#define EVENT_MAX 16

struct event_rule {
    char name[32];
    char trigger[64];
    char action[256];
    int enabled;
    int source;
};

static struct event_rule event_rules[EVENT_MAX] = {
    {"download-log", "file.created:/home/downloads", "log system download event", 1, 0},
    {"editor-focus", "process.start:editor", "window editor focus", 1, 0},
    {"net-audit", "net.link:up", "log network link up", 1, 0},
    {"security-audit", "security.mode:change", "log security mode change", 1, 0},
    {"service-audit", "service.state:change", "log service state change", 1, 0},
    {"fs-audit", "fs.write", "log filesystem mutation", 1, 0}
};

static void (*source_handler)(const char* source) = 0;

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

static void copy_text(char* dst, size_t max, const char* src){
    size_t i = 0;
    if(max == 0) return;
    while(src && src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void events_init(void){
    fs_append_line("/var/log/system.log", "events: bus online");
}

void events_set_source_handler(void (*handler)(const char* source)){
    source_handler = handler;
}

int events_register_source(const char* name, const char* trigger, const char* source){
    size_t slot = EVENT_MAX;
    for(size_t i=0; i<EVENT_MAX; i++){
        if(event_rules[i].name[0] == 0 && slot == EVENT_MAX)
            slot = i;
        if(event_rules[i].name[0] && str_eq(event_rules[i].name, name)){
            slot = i;
            break;
        }
    }
    if(slot == EVENT_MAX)
        return -1;
    copy_text(event_rules[slot].name, sizeof(event_rules[slot].name), name);
    copy_text(event_rules[slot].trigger, sizeof(event_rules[slot].trigger), trigger);
    copy_text(event_rules[slot].action, sizeof(event_rules[slot].action), source);
    event_rules[slot].enabled = 1;
    event_rules[slot].source = 1;
    fs_append_line("/var/log/system.log", "events: registered Rusa source handler");
    return 0;
}

void events_emit(const char* trigger){
    for(size_t i=0; i<EVENT_MAX; i++){
        if(event_rules[i].name[0] && event_rules[i].enabled && str_eq(event_rules[i].trigger, trigger)){
            fs_append_line("/var/log/system.log", event_rules[i].action);
            console_puts("event ");
            console_puts(event_rules[i].name);
            console_puts(" -> ");
            if(event_rules[i].source){
                console_puts("rusa handler\n");
                if(source_handler)
                    source_handler(event_rules[i].action);
            } else {
                console_puts(event_rules[i].action);
                console_putc('\n');
            }
        }
    }
}

void events_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<EVENT_MAX; i++){
            if(event_rules[i].name[0] == 0) continue;
            console_puts(event_rules[i].enabled ? "[on]  " : "[off] ");
            console_puts(event_rules[i].name);
            console_puts(" when ");
            console_puts(event_rules[i].trigger);
            console_puts(" -> ");
            console_puts(event_rules[i].source ? "{rusa block}" : event_rules[i].action);
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
