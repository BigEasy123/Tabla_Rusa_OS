#include <stddef.h>
#include "console.h"
#include "events.h"
#include "fs.h"
#include "process.h"
#include "security.h"
#include "service.h"

#define SERVICE_MAX 6

struct service_info {
    const char* name;
    int running;
    const char* provides;
};

static struct service_info services[SERVICE_MAX] = {
    {"logger", 1, "audit and system logs"},
    {"network", 0, "TCP/IP stack foundation"},
    {"gui", 0, "window compositor foundation"},
    {"package", 1, "package registry manifests"},
    {"security", 1, "capability policy and audit"},
    {"events", 1, "reactive rule dispatcher"}
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

static struct service_info* service_find_local(const char* name){
    for(size_t i=0; i<SERVICE_MAX; i++)
        if(str_eq(services[i].name, name))
            return &services[i];
    return 0;
}

void service_init(void){
    fs_append_line("/var/log/system.log", "service: manager online");
}

void service_set_running(const char* name, int running){
    struct service_info* svc = service_find_local(name);
    if(!svc) return;
    svc->running = running;
    if(str_eq(name, "network")) process_set_running("network", running);
    if(str_eq(name, "gui")) process_set_running("gui", running);
    events_emit("service.state:change");
}

int service_is_running(const char* name){
    struct service_info* svc = service_find_local(name);
    return svc ? svc->running : 0;
}

void service_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<SERVICE_MAX; i++){
            console_puts(services[i].running ? "[on]  " : "[off] ");
            console_puts(services[i].name);
            console_puts(" - ");
            console_puts(services[i].provides);
            console_putc('\n');
        }
        return;
    }
    struct service_info* svc = service_find_local(name);
    if(svc == 0){
        console_puts("service: not found\n");
        return;
    }
    if((str_eq(action, "start") || str_eq(action, "stop") || str_eq(action, "restart")) &&
       !security_can_service_control()){
        console_puts("service: permission denied\n");
        security_audit("service: blocked control attempt");
        return;
    }
    if(str_eq(action, "start")){
        service_set_running(svc->name, 1);
        fs_append_line("/var/log/system.log", "service: started");
        console_puts("started ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(str_eq(action, "stop")){
        service_set_running(svc->name, 0);
        fs_append_line("/var/log/system.log", "service: stopped");
        console_puts("stopped ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(str_eq(action, "restart")){
        service_set_running(svc->name, 0);
        service_set_running(svc->name, 1);
        fs_append_line("/var/log/system.log", "service: restarted");
        console_puts("restarted ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(str_eq(action, "status")){
        console_puts(svc->name);
        console_puts(svc->running ? " running - " : " stopped - ");
        console_puts(svc->provides);
        console_putc('\n');
    } else {
        console_puts("usage: service list | start NAME | stop NAME | restart NAME | status NAME\n");
    }
}
