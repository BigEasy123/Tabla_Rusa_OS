#include <stdint.h>
#include "console.h"
#include "events.h"
#include "fd.h"
#include "fs.h"
#include "process.h"
#include "service.h"
#include "net.h"

static int link_up = 0;
static int tcp_listen_port = 0;

#define SOCKET_MAX 4

struct socket_entry {
    int used;
    int id;
    int fd;
    const char* proto;
    const char* state;
    uint32_t local_port;
    uint32_t remote_port;
    char rx[96];
};

static struct socket_entry sockets[SOCKET_MAX];
static char fd_read_buffer[96];

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

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

void net_init(void){
    for(int i=0; i<SOCKET_MAX; i++){
        sockets[i].used = 0;
        sockets[i].id = i;
        sockets[i].rx[0] = 0;
        sockets[i].fd = -1;
    }
    fs_write("/system/net/stack.txt",
        "layers=link,arp,ipv4,icmp,udp,tcp,sockets\n"
        "loopback=127.0.0.1/8\n"
        "socket_max=4\n");
    fs_append_line("/var/log/network.log", "net: loopback stack online");
}

int net_fd_write(int fd, const char* text){
    for(int i=0; i<SOCKET_MAX; i++){
        if(sockets[i].used && sockets[i].fd == fd){
            size_t j = 0;
            while(text[j] && j + 1 < sizeof(sockets[i].rx)){
                sockets[i].rx[j] = text[j];
                j++;
            }
            sockets[i].rx[j] = 0;
            fs_append_line("/var/log/network.log", "net: fd socket write queued");
            return 0;
        }
    }
    return -1;
}

int net_fd_read(int fd, const char** out){
    for(int i=0; i<SOCKET_MAX; i++){
        if(sockets[i].used && sockets[i].fd == fd){
            size_t j = 0;
            const char* src = sockets[i].rx[0] ? sockets[i].rx : "<empty>";
            while(src[j] && j + 1 < sizeof(fd_read_buffer)){
                fd_read_buffer[j] = src[j];
                j++;
            }
            fd_read_buffer[j++] = '\n';
            fd_read_buffer[j] = 0;
            sockets[i].rx[0] = 0;
            *out = fd_read_buffer;
            return 0;
        }
    }
    return -1;
}

int net_is_link_up(void){
    return link_up;
}

void net_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("lo: up ip=127.0.0.1/8\n");
        console_puts("eth0: ");
        console_puts(link_up ? "up" : "down");
        console_puts(" ip=0.0.0.0/0 driver=stub\n");
        console_puts("tcp: ");
        if(tcp_listen_port){
            console_puts("LISTEN port ");
            console_write_dec((uint32_t)tcp_listen_port);
            console_putc('\n');
        } else {
            console_puts("closed\n");
        }
        console_puts("sockets=");
        for(int i=0; i<SOCKET_MAX; i++)
            if(sockets[i].used) console_write_dec((uint32_t)sockets[i].id);
        console_putc('\n');
    } else if(str_eq(action, "up")){
        link_up = 1;
        service_set_running("network", 1);
        process_set_running("network", 1);
        fs_append_line("/var/log/network.log", "net: eth0 marked up");
        events_emit("net.link:up");
        console_puts("net: eth0 up (stub link)\n");
    } else if(str_eq(action, "down")){
        link_up = 0;
        tcp_listen_port = 0;
        service_set_running("network", 0);
        process_set_running("network", 0);
        fs_append_line("/var/log/network.log", "net: eth0 marked down");
        console_puts("net: eth0 down\n");
    } else if(str_eq(action, "ip")){
        console_puts("IPv4: loopback route 127.0.0.0/8, eth0 route 0.0.0.0/0 when up\n");
        console_puts("ICMP: echo request/reply surface reserved\n");
    } else if(str_eq(action, "udp")){
        console_puts("UDP: datagram sockets use net socket udp PORT, net send ID MSG\n");
    } else if(str_eq(action, "tcp")){
        uint32_t port = parse_u32(rest);
        if(port == 0){
            console_puts("TCP states: CLOSED LISTEN SYN-SENT ESTABLISHED FIN-WAIT TIME-WAIT\n");
            console_puts("usage: net tcp PORT\n");
        } else {
            tcp_listen_port = (int)port;
            fs_append_line("/var/log/network.log", "net: tcp listen socket registered");
            console_puts("tcp: listening stub on port ");
            console_write_dec(port);
            console_putc('\n');
        }
    } else if(str_eq(action, "arp")){
        console_puts("ARP table:\n");
        console_puts("  127.0.0.1 -> loopback\n");
        console_puts(link_up ? "  10.0.2.15 -> eth0:stub\n" : "  eth0 down\n");
    } else if(str_eq(action, "route")){
        console_puts("routes:\n");
        console_puts("  127.0.0.0/8 dev lo\n");
        if(link_up) console_puts("  0.0.0.0/0 dev eth0 metric 100\n");
    } else if(str_eq(action, "socket") || str_eq(action, "open")){
        const char* proto = first_arg(rest, &rest);
        uint32_t port = parse_u32(rest);
        for(int i=0; i<SOCKET_MAX; i++){
            if(!sockets[i].used){
                sockets[i].used = 1;
                sockets[i].proto = str_eq(proto, "udp") ? "udp" : "tcp";
                sockets[i].state = str_eq(proto, "udp") ? "OPEN" : "LISTEN";
                sockets[i].local_port = port;
                sockets[i].remote_port = 0;
                char label[16] = "socket:0";
                label[7] = (char)('0' + i);
                sockets[i].fd = fd_open_socket(label);
                sockets[i].rx[0] = 0;
                console_puts("socket id=");
                console_write_dec((uint32_t)i);
                console_puts(" fd=");
                if(sockets[i].fd >= 0) console_write_dec((uint32_t)sockets[i].fd);
                else console_puts("none");
                console_puts(" ");
                console_puts(sockets[i].proto);
                console_puts(" port=");
                console_write_dec(port);
                console_putc('\n');
                return;
            }
        }
        console_puts("socket: table full\n");
    } else if(str_eq(action, "sockets")){
        for(int i=0; i<SOCKET_MAX; i++){
            if(sockets[i].used){
                console_puts("sock ");
                console_write_dec((uint32_t)i);
                console_puts(" ");
                console_puts(sockets[i].proto);
                console_puts(" ");
                console_puts(sockets[i].state);
                console_puts(" local=");
                console_write_dec(sockets[i].local_port);
                console_puts(" fd=");
                if(sockets[i].fd >= 0) console_write_dec((uint32_t)sockets[i].fd);
                else console_puts("none");
                console_puts(" rx=");
                console_puts(sockets[i].rx);
                console_putc('\n');
            }
        }
    } else if(str_eq(action, "fd")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        if(id >= SOCKET_MAX || !sockets[id].used) console_puts("fd: bad socket\n");
        else {
            console_puts("socket ");
            console_write_dec(id);
            console_puts(" fd=");
            if(sockets[id].fd >= 0) console_write_dec((uint32_t)sockets[id].fd);
            else console_puts("none");
            console_putc('\n');
        }
    } else if(str_eq(action, "connect")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        uint32_t port = parse_u32(first_arg(rest, &rest));
        if(id >= SOCKET_MAX || !sockets[id].used) console_puts("connect: bad socket\n");
        else {
            sockets[id].remote_port = port;
            sockets[id].state = "ESTABLISHED";
            console_puts("socket connected\n");
        }
    } else if(str_eq(action, "send")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        if(id >= SOCKET_MAX || !sockets[id].used) console_puts("send: bad socket\n");
        else {
            size_t i = 0;
            while(rest[i] && i + 1 < sizeof(sockets[id].rx)){
                sockets[id].rx[i] = rest[i];
                i++;
            }
            sockets[id].rx[i] = 0;
            fs_append_line("/var/log/network.log", "net: loopback packet queued");
            console_puts("packet queued on loopback\n");
        }
    } else if(str_eq(action, "recv")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        if(id >= SOCKET_MAX || !sockets[id].used) console_puts("recv: bad socket\n");
        else {
            console_puts(sockets[id].rx[0] ? sockets[id].rx : "<empty>");
            console_putc('\n');
            sockets[id].rx[0] = 0;
        }
    } else {
        console_puts("usage: net status|up|down|ip|udp|tcp PORT|arp|route|open tcp|udp PORT|socket tcp|udp PORT|fd ID|sockets|connect ID PORT|send ID MSG|recv ID\n");
    }
}
