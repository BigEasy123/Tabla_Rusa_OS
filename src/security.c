#include <stddef.h>
#include "console.h"
#include "events.h"
#include "fs.h"
#include "security.h"

#define USER_MAX 4

struct user_info {
    char name[16];
    const char* caps;
    int active;
};

static struct user_info users[USER_MAX] = {
    {"root", "all", 1},
    {"guest", "fs.read,shell.run", 1},
    {"", "", 0},
    {"", "", 0}
};

static char current_user[16] = "root";
static int secure_mode = 1;

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

static int str_starts(const char* s, const char* prefix){
    while(*prefix){
        if(*s++ != *prefix++) return 0;
    }
    return 1;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
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

static struct user_info* user_find(const char* name){
    for(size_t i=0; i<USER_MAX; i++)
        if(users[i].active && str_eq(users[i].name, name))
            return &users[i];
    return 0;
}

static int current_is_root(void){
    return str_eq(current_user, "root");
}

void security_init(void){
    fs_append_line("/var/log/security.log", "security: capability policy online");
}

const char* security_current_user(void){
    return current_user;
}

int security_is_locked(void){
    return secure_mode;
}

int security_can_write(const char* path){
    if(current_is_root()) return 1;
    if(str_starts(path, "/system") || str_starts(path, "/boot") || str_starts(path, "/proc")){
        security_audit("security: blocked protected namespace write");
        return 0;
    }
    struct user_info* user = user_find(current_user);
    return user && str_eq(user->caps, "fs.read,fs.write,shell.run,service.control");
}

int security_can_service_control(void){
    if(current_is_root()) return 1;
    struct user_info* user = user_find(current_user);
    return user && str_eq(user->caps, "fs.read,fs.write,shell.run,service.control");
}

int security_can_install_kernel_package(const char* pkg_name){
    if(!secure_mode) return 1;
    if(str_eq(pkg_name, "gui-core") || str_eq(pkg_name, "net-tcpip")){
        security_audit("security: blocked unsigned kernel package install");
        return 0;
    }
    return 1;
}

void security_audit(const char* message){
    fs_append_line("/var/log/security.log", message);
}

void security_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("secure_mode=");
        console_puts(secure_mode ? "on" : "permissive");
        console_puts(" user=");
        console_puts(current_user);
        console_puts(" ring=0 policy=capability-audit\n");
    } else if(str_eq(action, "audit")){
        const char* text;
        if(fs_read("/var/log/security.log", &text) == 0) console_puts(text);
    } else if(str_eq(action, "lock")){
        secure_mode = 1;
        security_audit("security: secure_mode enabled");
        events_emit("security.mode:change");
        console_puts("security: secure_mode enabled\n");
    } else if(str_eq(action, "unlock")){
        secure_mode = 0;
        security_audit("security: permissive mode requested");
        events_emit("security.mode:change");
        console_puts("security: permissive mode enabled\n");
    } else {
        console_puts("usage: security status | audit | lock | unlock\n");
    }
}

void security_user_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<USER_MAX; i++)
            if(users[i].active){
                console_puts(str_eq(users[i].name, current_user) ? "* " : "  ");
                console_puts(users[i].name);
                console_puts(" caps=");
                console_puts(users[i].caps);
                console_putc('\n');
            }
        return;
    }
    if(str_eq(action, "add")){
        if(!current_is_root()){
            console_puts("user: permission denied\n");
            return;
        }
        for(size_t i=0; i<USER_MAX; i++){
            if(!users[i].active){
                str_copy(users[i].name, name, sizeof(users[i].name));
                users[i].caps = "fs.read,shell.run";
                users[i].active = 1;
                security_audit("user: added account");
                console_puts("added user ");
                console_puts(name);
                console_putc('\n');
                return;
            }
        }
        console_puts("user: table full\n");
    } else if(str_eq(action, "login")){
        if(user_find(name) == 0){
            console_puts("login: unknown user\n");
            return;
        }
        str_copy(current_user, name, sizeof(current_user));
        security_audit("user: login");
        console_puts("logged in as ");
        console_puts(current_user);
        console_putc('\n');
    } else {
        console_puts("usage: user list | add NAME | login NAME\n");
    }
}

void security_cap_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    struct user_info* user = user_find(name);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<USER_MAX; i++)
            if(users[i].active){
                console_puts(users[i].name);
                console_puts(" caps=");
                console_puts(users[i].caps);
                console_putc('\n');
            }
        return;
    }
    if(!current_is_root()){
        console_puts("cap: permission denied\n");
        return;
    }
    if(user == 0){
        console_puts("cap: unknown user\n");
        return;
    }
    if(str_eq(action, "grant")){
        user->caps = "fs.read,fs.write,shell.run,service.control";
        security_audit("capability: granted elevated set");
        console_puts("granted elevated capabilities to ");
        console_puts(user->name);
        console_putc('\n');
    } else if(str_eq(action, "drop")){
        user->caps = "fs.read,shell.run";
        security_audit("capability: dropped to default set");
        console_puts("dropped capabilities for ");
        console_puts(user->name);
        console_putc('\n');
    } else {
        console_puts("usage: cap list | grant USER | drop USER\n");
    }
}
