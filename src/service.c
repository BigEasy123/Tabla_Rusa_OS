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
    uint32_t port;
    uint32_t owner_pid;
    int enabled;
    const char* permission;
    const char* health;
};

static struct service_info services[SERVICE_MAX] = {
    {"logger", 1, "audit and system logs", 0, 3, 1, "security.view", "ok"},
    {"network", 0, "TCP/IP stack foundation", 9999, 4, 1, "network.server", "idle"},
    {"gui", 0, "window compositor foundation", 0, 1, 1, "device.framebuffer", "ok"},
    {"package", 1, "package registry manifests", 0, 1, 1, "filesystem.read", "ok"},
    {"security", 1, "capability policy and audit", 0, 1, 1, "security.modify", "ok"},
    {"events", 1, "reactive rule dispatcher", 0, 1, 1, "settings.modify", "ok"}
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

int service_register(const char* name, uint32_t port, uint32_t owner_pid,
                     const char* description, const char* permission){
    for(size_t i=0; i<SERVICE_MAX; i++){
        if(str_eq(services[i].name, name)){
            services[i].port = port;
            services[i].owner_pid = owner_pid;
            services[i].provides = description;
            services[i].permission = permission;
            services[i].enabled = 1;
            services[i].health = "registered";
            return 0;
        }
    }
    return -1;
}

uint32_t service_list_registry(struct service_registry_info* out, uint32_t max){
    uint32_t count = 0;
    for(size_t i=0; i<SERVICE_MAX; i++){
        if(out && count < max){
            out[count].id = (uint32_t)i;
            out[count].name = services[i].name;
            out[count].port = services[i].port;
            out[count].owner_pid = services[i].owner_pid;
            out[count].state = services[i].enabled ? (services[i].running ? "running" : "stopped") : "disabled";
            out[count].description = services[i].provides;
            out[count].permission = services[i].permission;
            out[count].health = services[i].health;
        }
        count++;
    }
    return count;
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
            console_puts(!services[i].enabled ? "[disabled] " : (services[i].running ? "[on]  " : "[off] "));
            console_puts(services[i].name);
            console_puts(" - ");
            console_puts(services[i].provides);
            console_puts(" port=");
            console_write_dec(services[i].port);
            console_puts(" perm=");
            console_puts(services[i].permission);
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
        svc->enabled = 1;
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
        svc->enabled = 1;
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
        console_puts(" health=");
        console_puts(svc->health);
        console_putc('\n');
    } else if(str_eq(action, "disable")){
        service_set_running(svc->name, 0);
        svc->enabled = 0;
        svc->health = "disabled";
        console_puts("disabled ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(str_eq(action, "enable")){
        svc->enabled = 1;
        svc->health = "enabled";
        console_puts("enabled ");
        console_puts(svc->name);
        console_putc('\n');
    } else {
        console_puts("usage: service list | start NAME | stop NAME | restart NAME | status NAME | enable NAME | disable NAME\n");
    }
}
