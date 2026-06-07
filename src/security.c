#include <stddef.h>
#include "console.h"
#include "events.h"
#include "fs.h"
#include "timer.h"
#include "security.h"

#define USER_MAX 4
#define SECURITY_EVENT_MAX 16
#define SECURITY_APP_MAX 8
#define SECURITY_CAP_MAX 24
#define SECURITY_PERMISSION_MAX 24

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

struct app_info {
    char name[24];
    char summary[72];
    int active;
};

struct cap_info {
    char app[24];
    char capability[32];
    char explanation[96];
    int active;
};

struct permission_info {
    char app[24];
    char resource[40];
    enum security_permission_decision decision;
    int active;
};

static struct security_event_info security_events[SECURITY_EVENT_MAX];
static struct app_info apps[SECURITY_APP_MAX];
static struct cap_info caps[SECURITY_CAP_MAX];
static struct permission_info permissions[SECURITY_PERMISSION_MAX];
static uint32_t next_event_id = 1;

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

static const char* severity_text(enum security_severity severity){
    if(severity == SECURITY_WARNING) return "warning";
    if(severity == SECURITY_CRITICAL) return "critical";
    return "info";
}

static const char* decision_text(enum security_permission_decision decision){
    if(decision == SECURITY_PERMISSION_ALLOW) return "allow";
    if(decision == SECURITY_PERMISSION_DENY) return "deny";
    if(decision == SECURITY_PERMISSION_ASK) return "ask";
    if(decision == SECURITY_PERMISSION_INHERITED) return "inherited";
    return "default-deny";
}

static void append_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    uint32_t j = 0;
    if(max == 0) return;
    while(dst[i] && i + 1 < max) i++;
    while(src && src[j] && i + 1 < max)
        dst[i++] = src[j++];
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
    for(size_t i=0; i<SECURITY_EVENT_MAX; i++) security_events[i].id = 0;
    for(size_t i=0; i<SECURITY_APP_MAX; i++) apps[i].active = 0;
    for(size_t i=0; i<SECURITY_CAP_MAX; i++) caps[i].active = 0;
    for(size_t i=0; i<SECURITY_PERMISSION_MAX; i++) permissions[i].active = 0;
    security_register_app("terminal", "interactive command shell");
    security_register_app("network", "loopback network tools");
    security_register_app("editor", "file editor");
    security_register_app("rusa", "Rusa language workbench");
    security_register_app("math", "scientific compute tools");
    security_register_capability("network", "network.loopback", "Allows local loopback socket tests.");
    security_register_capability("terminal", "security.view", "Can view local security status on request.");
    security_set_permission("network", "network.loopback", SECURITY_PERMISSION_ALLOW);
    security_set_permission("network", "network.server", SECURITY_PERMISSION_ASK);
    security_set_permission("terminal", "security.view", SECURITY_PERMISSION_ALLOW);
    security_set_permission("editor", "filesystem.write", SECURITY_PERMISSION_ASK);
    fs_append_line("/var/log/security.log", "security: capability policy online");
    security_log_event(SECURITY_INFO, "security", "kernel", "policy", "init", "allow",
                       "Security tables initialized with safe defaults.",
                       "Review permissions in Security Center.");
}

void security_log_event(enum security_severity severity, const char* category, const char* app,
                        const char* resource, const char* action, const char* decision,
                        const char* explanation, const char* suggestion){
    uint32_t slot = (next_event_id - 1) % SECURITY_EVENT_MAX;
    security_events[slot].id = next_event_id++;
    security_events[slot].tick = timer_ticks();
    security_events[slot].severity = severity;
    security_events[slot].category = category ? category : "security";
    security_events[slot].app = app ? app : "system";
    security_events[slot].resource = resource ? resource : "unknown";
    security_events[slot].action = action ? action : "check";
    security_events[slot].decision = decision ? decision : "unknown";
    security_events[slot].explanation = explanation ? explanation : "No explanation provided.";
    security_events[slot].suggestion = suggestion ? suggestion : "Review this event.";
    fs_append_line("/var/log/security.log", explanation ? explanation : "security: event");
}

uint32_t security_get_events(struct security_event_info* out, uint32_t max){
    uint32_t count = 0;
    for(size_t i=0; i<SECURITY_EVENT_MAX; i++){
        if(security_events[i].id == 0)
            continue;
        if(out && count < max)
            out[count] = security_events[i];
        count++;
    }
    return count;
}

int security_register_app(const char* app, const char* summary){
    for(size_t i=0; i<SECURITY_APP_MAX; i++){
        if(apps[i].active && str_eq(apps[i].name, app))
            return 0;
    }
    for(size_t i=0; i<SECURITY_APP_MAX; i++){
        if(!apps[i].active){
            apps[i].active = 1;
            str_copy(apps[i].name, app, sizeof(apps[i].name));
            str_copy(apps[i].summary, summary ? summary : "registered app", sizeof(apps[i].summary));
            return 0;
        }
    }
    return -1;
}

int security_register_capability(const char* app, const char* capability, const char* explanation){
    for(size_t i=0; i<SECURITY_CAP_MAX; i++){
        if(caps[i].active && str_eq(caps[i].app, app) && str_eq(caps[i].capability, capability))
            return 0;
    }
    for(size_t i=0; i<SECURITY_CAP_MAX; i++){
        if(!caps[i].active){
            caps[i].active = 1;
            str_copy(caps[i].app, app, sizeof(caps[i].app));
            str_copy(caps[i].capability, capability, sizeof(caps[i].capability));
            str_copy(caps[i].explanation, explanation ? explanation : "Capability declared by app.", sizeof(caps[i].explanation));
            return 0;
        }
    }
    return -1;
}

uint32_t security_list_capabilities(const char* app, char* out, uint32_t max){
    uint32_t count = 0;
    if(out && max) out[0] = 0;
    for(size_t i=0; i<SECURITY_CAP_MAX; i++){
        if(!caps[i].active || (app && app[0] && !str_eq(caps[i].app, app)))
            continue;
        count++;
        if(out && max){
            append_text(out, caps[i].app, max);
            append_text(out, ":", max);
            append_text(out, caps[i].capability, max);
            append_text(out, "\n", max);
        }
    }
    return count;
}

int security_set_permission(const char* app, const char* resource, enum security_permission_decision decision){
    for(size_t i=0; i<SECURITY_PERMISSION_MAX; i++){
        if(permissions[i].active && str_eq(permissions[i].app, app) && str_eq(permissions[i].resource, resource)){
            permissions[i].decision = decision;
            return 0;
        }
    }
    for(size_t i=0; i<SECURITY_PERMISSION_MAX; i++){
        if(!permissions[i].active){
            permissions[i].active = 1;
            str_copy(permissions[i].app, app, sizeof(permissions[i].app));
            str_copy(permissions[i].resource, resource, sizeof(permissions[i].resource));
            permissions[i].decision = decision;
            return 0;
        }
    }
    return -1;
}

enum security_permission_decision security_check_permission(const char* app, const char* resource){
    for(size_t i=0; i<SECURITY_PERMISSION_MAX; i++){
        if(permissions[i].active && str_eq(permissions[i].app, app) && str_eq(permissions[i].resource, resource))
            return permissions[i].decision;
    }
    security_log_event(SECURITY_WARNING, "permission", app, resource, "check", "default-deny",
                       "No explicit permission exists, so the safe default is deny.",
                       "Set allow/ask only for apps you trust.");
    return SECURITY_PERMISSION_DEFAULT_DENY;
}

uint32_t security_list_permissions(const char* app, char* out, uint32_t max){
    uint32_t count = 0;
    if(out && max) out[0] = 0;
    for(size_t i=0; i<SECURITY_PERMISSION_MAX; i++){
        if(!permissions[i].active || (app && app[0] && !str_eq(permissions[i].app, app)))
            continue;
        count++;
        if(out && max){
            append_text(out, permissions[i].app, max);
            append_text(out, " ", max);
            append_text(out, permissions[i].resource, max);
            append_text(out, "=", max);
            append_text(out, decision_text(permissions[i].decision), max);
            append_text(out, "\n", max);
        }
    }
    return count;
}

enum security_scan_result security_scan_app(const char* path, char* out, uint32_t max){
    const char* text;
    enum security_scan_result result = SECURITY_SCAN_CLEAN;
    if(out && max) out[0] = 0;
    if(fs_read(path, &text) != 0){
        if(out) str_copy(out, "unknown: app or source file was not found", max);
        security_log_event(SECURITY_WARNING, "scanner", path, "file", "scan", "unknown",
                           "Scanner could not find the requested file.",
                           "Check the path and try again.");
        return SECURITY_SCAN_UNKNOWN;
    }
    if(str_starts(text, "net scan") || str_starts(text, "while true") ||
       str_starts(text, "security unlock") || str_starts(text, "process kill")){
        result = SECURITY_SCAN_SUSPICIOUS;
        if(out) str_copy(out, "suspicious: source begins with a risky command pattern", max);
    } else if(str_starts(text, "write /system") || str_starts(text, "device raw")){
        result = SECURITY_SCAN_BLOCKED;
        if(out) str_copy(out, "blocked: source targets protected system/device resources", max);
    } else {
        if(out) str_copy(out, "clean: no simple suspicious pattern found", max);
    }
    security_log_event(result == SECURITY_SCAN_CLEAN ? SECURITY_INFO : SECURITY_WARNING,
                       "scanner", path, "source", "scan",
                       result == SECURITY_SCAN_CLEAN ? "clean" : (result == SECURITY_SCAN_BLOCKED ? "blocked" : "suspicious"),
                       out ? out : "scanner result",
                       "This scanner is lightweight and not comprehensive.");
    return result;
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
    } else if(str_eq(action, "events")){
        struct security_event_info events[SECURITY_EVENT_MAX];
        uint32_t n = security_get_events(events, SECURITY_EVENT_MAX);
        for(uint32_t i=0; i<n && i<SECURITY_EVENT_MAX; i++){
            console_puts("#");
            console_write_dec(events[i].id);
            console_puts(" ");
            console_puts(severity_text(events[i].severity));
            console_puts(" ");
            console_puts(events[i].category);
            console_puts(" app=");
            console_puts(events[i].app);
            console_puts(" decision=");
            console_puts(events[i].decision);
            console_puts(" - ");
            console_puts(events[i].explanation);
            console_putc('\n');
        }
    } else if(str_eq(action, "scan")){
        char result[128];
        const char* path = first_arg(rest, &rest);
        if(!path[0]) path = "/home/projects/demo.rusa";
        security_scan_app(path, result, sizeof(result));
        console_puts(result);
        console_putc('\n');
    } else if(str_eq(action, "permissions") || str_eq(action, "perms")){
        char result[384];
        const char* app = first_arg(rest, &rest);
        security_list_permissions(app, result, sizeof(result));
        console_puts(result[0] ? result : "no permissions\n");
    } else if(str_eq(action, "capabilities") || str_eq(action, "caps")){
        char result[384];
        const char* app = first_arg(rest, &rest);
        security_list_capabilities(app, result, sizeof(result));
        console_puts(result[0] ? result : "no capabilities\n");
    } else if(str_eq(action, "allow") || str_eq(action, "deny") || str_eq(action, "ask")){
        const char* app = first_arg(rest, &rest);
        const char* resource = first_arg(rest, &rest);
        enum security_permission_decision decision =
            str_eq(action, "allow") ? SECURITY_PERMISSION_ALLOW :
            (str_eq(action, "deny") ? SECURITY_PERMISSION_DENY : SECURITY_PERMISSION_ASK);
        if(!app[0] || !resource[0]){
            console_puts("usage: security allow|deny|ask APP RESOURCE\n");
        } else if(security_set_permission(app, resource, decision) == 0){
            console_puts("permission ");
            console_puts(decision_text(decision));
            console_puts(" for ");
            console_puts(app);
            console_putc('\n');
        }
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
        console_puts("usage: security status | events | audit | scan PATH | permissions [APP] | capabilities [APP] | allow|deny|ask APP RESOURCE | lock | unlock\n");
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
