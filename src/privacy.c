#include "console.h"
#include "events.h"
#include "fs.h"
#include "net.h"
#include "privacy.h"
#include "process.h"
#include "service.h"

static int master_privacy = 1;
static int network_allowed = 1;
static int devices_allowed = 1;
static int telemetry_allowed = 0;
static const char* cookie_policy = "ask";

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

static void print_onoff(const char* name, int on, const char* note){
    console_puts(on ? "[on]  " : "[off] ");
    console_puts(name);
    console_puts(" - ");
    console_puts(note);
    console_putc('\n');
}

void privacy_init(void){
    fs_write("/system/security/privacy.txt",
        "master_privacy=on\n"
        "network=on\n"
        "devices=on\n"
        "telemetry=off\n"
        "cookies=ask\n");
    fs_append_line("/var/log/security.log", "privacy: control center online");
}

int privacy_allows_network(void){
    return master_privacy && network_allowed;
}

int privacy_master_enabled(void){
    return master_privacy;
}

int privacy_network_enabled(void){
    return network_allowed;
}

int privacy_devices_enabled(void){
    return devices_allowed;
}

int privacy_telemetry_enabled(void){
    return telemetry_allowed;
}

const char* privacy_cookie_policy(void){
    return cookie_policy;
}

static void privacy_disconnect(void){
    network_allowed = 0;
    service_set_running("network", 0);
    process_set_running("network", 0);
    net_disconnect_all();
    fs_append_line("/var/log/security.log", "privacy: network disconnected");
    events_emit("privacy.network:off");
}

static void privacy_connect(void){
    network_allowed = 1;
    fs_append_line("/var/log/security.log", "privacy: network allowed");
    events_emit("privacy.network:on");
}

void privacy_set_master(int enabled){
    master_privacy = enabled ? 1 : 0;
    if(!master_privacy)
        privacy_disconnect();
    fs_append_line("/var/log/security.log", master_privacy ? "privacy: master switch on" : "privacy: master switch off");
}

void privacy_set_network(int enabled){
    if(enabled)
        privacy_connect();
    else
        privacy_disconnect();
}

void privacy_set_devices(int enabled){
    devices_allowed = enabled ? 1 : 0;
    fs_append_line("/var/log/security.log", devices_allowed ? "privacy: devices allowed" : "privacy: devices blocked");
}

void privacy_set_telemetry(int enabled){
    telemetry_allowed = enabled ? 1 : 0;
    fs_append_line("/var/log/security.log", telemetry_allowed ? "privacy: telemetry allowed" : "privacy: telemetry off");
}

void privacy_set_cookie_policy(const char* policy){
    if(str_eq(policy, "block"))
        cookie_policy = "block";
    else if(str_eq(policy, "allow"))
        cookie_policy = "allow";
    else
        cookie_policy = "ask";
    fs_append_line("/var/log/security.log", "privacy: cookie policy changed");
}

static void privacy_status(void){
    console_puts("PRIVACY CENTER\n");
    print_onoff("master privacy", master_privacy, "main permission for connections and device features");
    print_onoff("network", network_allowed, "internet/loopback sockets and ports");
    print_onoff("devices", devices_allowed, "keyboard, mouse, framebuffer, block devices");
    print_onoff("telemetry", telemetry_allowed, "background reporting; currently local-only and off");
    console_puts("cookies: ");
    console_puts(cookie_policy);
    console_puts(" - plain meaning: ");
    if(str_eq(cookie_policy, "block")) console_puts("programs should not store tracking/session tokens\n");
    else if(str_eq(cookie_policy, "allow")) console_puts("programs may store simple session tokens\n");
    else console_puts("ask before storing session or tracking tokens\n");
    console_puts("\nConnections and ports:\n");
    char sockets[] = "sockets";
    net_cmd(sockets);
    console_puts("Network stats:\n");
    char stats[] = "stats";
    net_cmd(stats);
    console_puts("Programs:\n");
    process_list();
}

void privacy_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status") || str_eq(action, "center")){
        privacy_status();
    } else if(str_eq(action, "on") || str_eq(action, "lock")){
        privacy_set_master(1);
        console_puts("privacy: master switch on\n");
    } else if(str_eq(action, "off")){
        privacy_set_master(0);
        console_puts("privacy: master switch off, network disconnected\n");
    } else if(str_eq(action, "network")){
        const char* value = first_arg(rest, &rest);
        if(str_eq(value, "on")){
            privacy_set_network(1);
            if(master_privacy)
                console_puts("privacy: network allowed\n");
            else
                console_puts("privacy: network marked on, but master privacy is off; use privacy on\n");
        } else if(str_eq(value, "off") || str_eq(value, "disconnect")){
            privacy_set_network(0);
            console_puts("privacy: network disconnected\n");
        } else {
            console_puts("usage: privacy network on|off\n");
        }
    } else if(str_eq(action, "devices")){
        const char* value = first_arg(rest, &rest);
        privacy_set_devices(!str_eq(value, "off"));
        console_puts(devices_allowed ? "privacy: devices allowed\n" : "privacy: devices marked off\n");
    } else if(str_eq(action, "telemetry")){
        const char* value = first_arg(rest, &rest);
        privacy_set_telemetry(str_eq(value, "on"));
        console_puts(telemetry_allowed ? "privacy: telemetry allowed\n" : "privacy: telemetry off\n");
    } else if(str_eq(action, "cookies") || str_eq(action, "cookie")){
        const char* value = first_arg(rest, &rest);
        if(str_eq(value, "block") || str_eq(value, "ask") || str_eq(value, "allow")){
            privacy_set_cookie_policy(value);
            console_puts("privacy: cookie policy set to ");
            console_puts(cookie_policy);
            console_putc('\n');
        } else {
            console_puts("usage: privacy cookies block|ask|allow\n");
        }
    } else if(str_eq(action, "ports")){
        char sockets[] = "sockets";
        net_cmd(sockets);
    } else if(str_eq(action, "connections")){
        char trace[] = "trace";
        net_cmd(trace);
    } else if(str_eq(action, "programs")){
        process_list();
    } else {
        console_puts("usage: privacy status | on | off | network on|off | cookies block|ask|allow | ports | connections | programs | devices on|off | telemetry on|off\n");
    }
}
