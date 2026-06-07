#include "console.h"
#include "privacy.h"
#include "security.h"
#include "policy.h"

#define FIREWALL_RULE_MAX 12

struct firewall_rule {
    uint32_t id;
    int used;
    int enabled;
    char app[24];
    uint32_t port;
    int allow;
    char explanation[96];
};

static struct firewall_rule firewall_rules[FIREWALL_RULE_MAX];
static uint32_t next_firewall_rule_id = 1;

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z') return (char)(c + 32);
    return c;
}

static int str_eq(const char* a, const char* b){
    uint32_t i = 0;
    if(!a || !b) return 0;
    while(a[i] && b[i]){
        if(lower_char(a[i]) != lower_char(b[i])) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char* first_arg(const char* in, char* out, uint32_t max){
    uint32_t i = 0;
    while(in && is_space(*in)) in++;
    while(in && *in && !is_space(*in)){
        if(i + 1 < max) out[i++] = *in;
        in++;
    }
    out[i] = 0;
    while(in && is_space(*in)) in++;
    return in ? in : "";
}

static uint32_t parse_u32(const char* s){
    uint32_t v = 0;
    while(s && is_space(*s)) s++;
    while(s && *s >= '0' && *s <= '9'){
        v = v * 10u + (uint32_t)(*s - '0');
        s++;
    }
    return v;
}

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(!dst || max == 0) return;
    if(!src) src = "";
    while(src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static int rule_matches(const struct firewall_rule* rule, const char* app, uint32_t port){
    if(!rule || !rule->used || !rule->enabled) return 0;
    if(rule->port != 0 && rule->port != port) return 0;
    if(rule->app[0] == '*' && rule->app[1] == 0) return 1;
    return str_eq(rule->app, app ? app : "");
}

static int allowed_or_ask(enum security_permission_decision decision){
    return decision == SECURITY_PERMISSION_ALLOW || decision == SECURITY_PERMISSION_ASK ||
           decision == SECURITY_PERMISSION_INHERITED;
}

int policy_firewall_add(const char* app, uint32_t port, int allow, const char* explanation){
    uint32_t i;
    for(i = 0; i < FIREWALL_RULE_MAX; i++){
        if(!firewall_rules[i].used){
            firewall_rules[i].used = 1;
            firewall_rules[i].id = next_firewall_rule_id++;
            firewall_rules[i].enabled = 1;
            firewall_rules[i].port = port;
            firewall_rules[i].allow = allow ? 1 : 0;
            copy_text(firewall_rules[i].app, (app && app[0]) ? app : "*", sizeof(firewall_rules[i].app));
            copy_text(firewall_rules[i].explanation,
                      (explanation && explanation[0]) ? explanation : (allow ? "Allow trusted app traffic." : "Block risky app traffic."),
                      sizeof(firewall_rules[i].explanation));
            return (int)firewall_rules[i].id;
        }
    }
    security_log_event(SECURITY_WARNING, "policy", app, "firewall", "add",
                       "deny", "The firewall rule table is full.",
                       "Remove an older rule before adding a new one.");
    return -1;
}

int policy_firewall_remove(uint32_t id){
    uint32_t i;
    for(i = 0; i < FIREWALL_RULE_MAX; i++){
        if(firewall_rules[i].used && firewall_rules[i].id == id){
            firewall_rules[i].used = 0;
            firewall_rules[i].enabled = 0;
            return 0;
        }
    }
    return -1;
}

int policy_firewall_set_enabled(uint32_t id, int enabled){
    uint32_t i;
    for(i = 0; i < FIREWALL_RULE_MAX; i++){
        if(firewall_rules[i].used && firewall_rules[i].id == id){
            firewall_rules[i].enabled = enabled ? 1 : 0;
            return 0;
        }
    }
    return -1;
}

uint32_t policy_firewall_list(struct firewall_rule_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < FIREWALL_RULE_MAX; i++){
        if(firewall_rules[i].used){
            if(out && n < max){
                out[n].id = firewall_rules[i].id;
                out[n].enabled = firewall_rules[i].enabled;
                out[n].port = firewall_rules[i].port;
                out[n].allow = firewall_rules[i].allow;
                copy_text(out[n].app, firewall_rules[i].app, sizeof(out[n].app));
                copy_text(out[n].explanation, firewall_rules[i].explanation, sizeof(out[n].explanation));
            }
            n++;
        }
    }
    return n;
}

int policy_firewall_test(const char* app, uint32_t port, int server){
    uint32_t i;
    (void)server;
    for(i = 0; i < FIREWALL_RULE_MAX; i++){
        if(rule_matches(&firewall_rules[i], app, port)){
            if(!firewall_rules[i].allow){
                security_log_event(SECURITY_WARNING, "policy", app, "firewall", "connect",
                                   "deny", firewall_rules[i].explanation,
                                   "Disable or remove this firewall rule if the app is trusted.");
                return 0;
            }
        }
    }
    return 1;
}

void policy_firewall_explain(uint32_t id, char* out, uint32_t max){
    uint32_t i;
    for(i = 0; i < FIREWALL_RULE_MAX; i++){
        if(firewall_rules[i].used && firewall_rules[i].id == id){
            copy_text(out, firewall_rules[i].explanation, max);
            return;
        }
    }
    copy_text(out, "No firewall rule with that ID exists.", max);
}

int policy_check_network_access(const char* app, uint32_t port, int server){
    enum security_permission_decision decision;
    if(!privacy_allows_network()){
        security_log_event(SECURITY_WARNING, "network", app, "network", server ? "listen" : "connect",
                           "deny", "Privacy master/network switch blocks this connection.",
                           "Turn network on only if you trust the app.");
        return 0;
    }
    if(port < 1024 && server){
        security_log_event(SECURITY_WARNING, "policy", app, "reserved-port", "listen",
                           "deny", "Low ports are reserved for trusted services.",
                           "Use an unreserved loopback port or register a service.");
        return 0;
    }
    if(!policy_firewall_test(app, port, server))
        return 0;
    decision = security_check_permission(app, server ? "network.server" : "network.client");
    if(!allowed_or_ask(decision))
        decision = security_check_permission(app, "network.loopback");
    if(!allowed_or_ask(decision)){
        security_log_event(SECURITY_WARNING, "permission", app, "network", server ? "listen" : "connect",
                           "deny", "The app does not have network permission.",
                           "Use Security Center to allow, ask, or keep denied.");
        return 0;
    }
    return 1;
}

int policy_check_file_access(const char* app, const char* path, int write){
    enum security_permission_decision decision =
        security_check_permission(app, write ? "filesystem.write" : "filesystem.read");
    if(write && path && path[0] == '/' && path[1] == 's' && path[2] == 'y'){
        security_log_event(SECURITY_WARNING, "filesystem", app, path, "write",
                           "deny", "System paths are protected from ordinary writes.",
                           "Save files under /home unless you are changing OS configuration.");
        return 0;
    }
    return allowed_or_ask(decision);
}

int policy_check_device_access(const char* app, const char* device){
    char resource[40] = "device.";
    uint32_t i = 7;
    uint32_t j = 0;
    while(device && device[j] && i + 1 < sizeof(resource))
        resource[i++] = device[j++];
    resource[i] = 0;
    return allowed_or_ask(security_check_permission(app, resource));
}

int policy_check_process_access(const char* app, const char* process, const char* action){
    if(process && (process[0] == 'k' || process[0] == 's')){
        security_log_event(SECURITY_WARNING, "process", app, process, action ? action : "control",
                           "deny", "Protected system processes cannot be controlled by ordinary apps.",
                           "Use Task Manager only for non-protected app processes.");
        return 0;
    }
    return allowed_or_ask(security_check_permission(app, "process.kill"));
}

void policy_cmd(char* arg){
    char action[16];
    arg = (char*)first_arg(arg, action, sizeof(action));
    if(action[0] == 0 || str_eq(action, "list")){
        struct firewall_rule_info rules[FIREWALL_RULE_MAX];
        uint32_t i, n = policy_firewall_list(rules, FIREWALL_RULE_MAX);
        console_puts("firewall rules\n");
        if(n == 0){
            console_puts("  none\n");
            return;
        }
        for(i = 0; i < n && i < FIREWALL_RULE_MAX; i++){
            console_puts("  #");
            console_write_dec(rules[i].id);
            console_puts(rules[i].enabled ? " on " : " off ");
            console_puts(rules[i].allow ? "allow " : "deny ");
            console_puts(rules[i].app);
            console_puts(" port ");
            console_write_dec(rules[i].port);
            console_puts(" - ");
            console_puts(rules[i].explanation);
            console_putc('\n');
        }
    } else if(str_eq(action, "add")){
        char decision[12], app[24], port_text[12];
        int id;
        arg = (char*)first_arg(arg, decision, sizeof(decision));
        arg = (char*)first_arg(arg, app, sizeof(app));
        arg = (char*)first_arg(arg, port_text, sizeof(port_text));
        if(!decision[0] || !app[0] || !port_text[0]){
            console_puts("usage: firewall add allow|deny APP PORT [reason]\n");
            return;
        }
        id = policy_firewall_add(app, parse_u32(port_text), str_eq(decision, "allow"), arg);
        if(id < 0) console_puts("firewall: rule table full\n");
        else {
            console_puts("firewall: added rule #");
            console_write_dec((uint32_t)id);
            console_putc('\n');
        }
    } else if(str_eq(action, "remove")){
        uint32_t id = parse_u32(arg);
        console_puts(policy_firewall_remove(id) == 0 ? "firewall: removed\n" : "firewall: no such rule\n");
    } else if(str_eq(action, "enable") || str_eq(action, "disable")){
        uint32_t id = parse_u32(arg);
        console_puts(policy_firewall_set_enabled(id, str_eq(action, "enable")) == 0 ? "firewall: updated\n" : "firewall: no such rule\n");
    } else if(str_eq(action, "test")){
        char app[24], port_text[12];
        arg = (char*)first_arg(arg, app, sizeof(app));
        first_arg(arg, port_text, sizeof(port_text));
        if(!app[0] || !port_text[0]){
            console_puts("usage: firewall test APP PORT\n");
            return;
        }
        console_puts(policy_firewall_test(app, parse_u32(port_text), 0) ? "firewall: allowed\n" : "firewall: blocked\n");
    } else if(str_eq(action, "explain")){
        char explanation[96];
        policy_firewall_explain(parse_u32(arg), explanation, sizeof(explanation));
        console_puts(explanation);
        console_putc('\n');
    } else {
        console_puts("usage: firewall list | add allow|deny APP PORT [reason] | remove ID | enable ID | disable ID | test APP PORT | explain ID\n");
    }
}
