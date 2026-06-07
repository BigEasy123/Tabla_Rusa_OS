#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "events.h"
#include "fd.h"
#include "fs.h"
#include "jobs.h"
#include "privacy.h"
#include "policy.h"
#include "process.h"
#include "service.h"
#include "timer.h"
#include "net.h"

#define SOCKET_MAX 4
#define PACKET_MAX 12
#define PACKET_PAYLOAD_MAX 64
#define PORT_MAX 8

enum packet_dir {
    PKT_TX,
    PKT_RX
};

struct net_packet {
    int used;
    uint32_t id;
    enum packet_dir dir;
    const char* iface;
    const char* proto;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t checksum;
    uint32_t len;
    char payload[PACKET_PAYLOAD_MAX];
};

struct socket_entry {
    int used;
    int id;
    uint32_t owner_pid;
    int fd;
    const char* proto;
    const char* state;
    uint32_t local_port;
    uint32_t remote_port;
    char rx[PACKET_PAYLOAD_MAX];
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t connect_attempts;
    uint32_t flood_score;
    uint32_t peer_id;
    uint32_t bytes_tx;
    uint32_t bytes_rx;
    uint32_t last_activity;
};

struct port_entry {
    int used;
    uint32_t port;
    enum net_port_state state;
    uint32_t owner_pid;
    uint32_t socket_id;
    const char* service;
};

struct net_stats {
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t drops;
    uint32_t checksum_errors;
    uint32_t state_changes;
};

static int link_up = 0;
static int tcp_listen_port = 0;
static int shield_enabled = 1;
static int ip_masking = 1;
static uint32_t flood_threshold = 5;
static struct socket_entry sockets[SOCKET_MAX];
static struct net_packet packets[PACKET_MAX];
static struct port_entry ports[PORT_MAX];
static struct net_stats stats;
static char fd_read_buffer[96];
static uint32_t next_packet_id = 1;

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

static void copy_text(char* dst, size_t max, const char* src){
    size_t i = 0;
    if(max == 0) return;
    while(src && src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static enum net_connection_state conn_state_from_text(const char* state){
    if(str_eq(state, "LISTEN") || str_eq(state, "LISTENING")) return NET_CONN_LISTENING;
    if(str_eq(state, "SYN-SENT") || str_eq(state, "CONNECTING")) return NET_CONN_CONNECTING;
    if(str_eq(state, "ESTABLISHED") || str_eq(state, "CONNECTED") || str_eq(state, "OPEN")) return NET_CONN_CONNECTED;
    if(str_eq(state, "CLOSING") || str_eq(state, "FIN-WAIT") || str_eq(state, "TIME-WAIT")) return NET_CONN_CLOSING;
    if(str_eq(state, "ERROR")) return NET_CONN_ERROR;
    return NET_CONN_CLOSED;
}

static const char* conn_state_text(enum net_connection_state state){
    if(state == NET_CONN_LISTENING) return "LISTENING";
    if(state == NET_CONN_CONNECTING) return "CONNECTING";
    if(state == NET_CONN_CONNECTED) return "CONNECTED";
    if(state == NET_CONN_CLOSING) return "CLOSING";
    if(state == NET_CONN_ERROR) return "ERROR";
    return "CLOSED";
}

static const char* port_state_text(enum net_port_state state){
    if(state == NET_PORT_BOUND) return "BOUND";
    if(state == NET_PORT_LISTENING) return "LISTENING";
    if(state == NET_PORT_BLOCKED) return "BLOCKED";
    if(state == NET_PORT_RESERVED) return "RESERVED";
    return "FREE";
}

static uint32_t ip_loopback(void){
    return 0x7F000001U;
}

static void print_ip(uint32_t ip){
    if(ip_masking){
        console_puts("masked.ip");
        return;
    }
    console_write_dec((ip >> 24) & 0xFF);
    console_putc('.');
    console_write_dec((ip >> 16) & 0xFF);
    console_putc('.');
    console_write_dec((ip >> 8) & 0xFF);
    console_putc('.');
    console_write_dec(ip & 0xFF);
}

static uint32_t checksum_packet(const char* payload, uint32_t src_port, uint32_t dst_port){
    uint32_t sum = src_port ^ (dst_port << 16) ^ 0xC0DEC0DEU;
    for(size_t i=0; payload && payload[i]; i++)
        sum = (sum << 5) ^ (sum >> 2) ^ (uint32_t)payload[i];
    return sum;
}

static struct socket_entry* socket_by_id(uint32_t id){
    if(id >= SOCKET_MAX || !sockets[id].used)
        return 0;
    return &sockets[id];
}

static struct socket_entry* socket_by_fd(uint32_t pid, int fd){
    for(int i=0; i<SOCKET_MAX; i++)
        if(sockets[i].used && sockets[i].owner_pid == pid && sockets[i].fd == fd)
            return &sockets[i];
    return 0;
}

static struct port_entry* port_by_number(uint32_t port){
    for(int i=0; i<PORT_MAX; i++)
        if(ports[i].used && ports[i].port == port)
            return &ports[i];
    return 0;
}

static struct port_entry* port_alloc(uint32_t port){
    struct port_entry* existing = port_by_number(port);
    if(existing)
        return existing;
    for(int i=0; i<PORT_MAX; i++){
        if(!ports[i].used){
            ports[i].used = 1;
            ports[i].port = port;
            ports[i].state = NET_PORT_FREE;
            ports[i].owner_pid = 0;
            ports[i].socket_id = 0;
            ports[i].service = "loopback";
            return &ports[i];
        }
    }
    return 0;
}

static struct socket_entry* listener_for_port(uint32_t port){
    for(int i=0; i<SOCKET_MAX; i++)
        if(sockets[i].used && sockets[i].local_port == port &&
           (str_eq(sockets[i].state, "LISTEN") || str_eq(sockets[i].state, "LISTENING")))
            return &sockets[i];
    return 0;
}

static struct net_packet* alloc_packet(void){
    for(int i=0; i<PACKET_MAX; i++){
        if(!packets[i].used){
            packets[i].used = 1;
            packets[i].id = next_packet_id++;
            return &packets[i];
        }
    }
    stats.drops++;
    return 0;
}

static void packet_fill(struct net_packet* pkt, enum packet_dir dir, const char* proto,
                        uint32_t src_port, uint32_t dst_port, const char* payload){
    pkt->dir = dir;
    pkt->iface = "lo";
    pkt->proto = proto;
    pkt->src_ip = ip_loopback();
    pkt->dst_ip = ip_loopback();
    pkt->src_port = src_port;
    pkt->dst_port = dst_port;
    copy_text(pkt->payload, sizeof(pkt->payload), payload);
    pkt->len = 0;
    while(pkt->payload[pkt->len]) pkt->len++;
    pkt->checksum = checksum_packet(pkt->payload, src_port, dst_port);
}

static void socket_set_state(struct socket_entry* sock, const char* state){
    if(!str_eq(sock->state, state)){
        sock->state = state;
        stats.state_changes++;
    }
}

static void deliver_loopback(struct socket_entry* sock, const char* payload){
    struct net_packet* rx = alloc_packet();
    struct socket_entry* dst = socket_by_id(sock->peer_id);
    if(!rx) return;
    packet_fill(rx, PKT_RX, sock->proto, sock->remote_port, sock->local_port, payload);
    if(rx->checksum != checksum_packet(rx->payload, rx->src_port, rx->dst_port)){
        stats.checksum_errors++;
        return;
    }
    if(!dst)
        dst = sock;
    copy_text(dst->rx, sizeof(dst->rx), rx->payload);
    dst->rx_packets++;
    dst->bytes_rx += rx->len;
    dst->last_activity = timer_ticks();
    stats.rx_packets++;
}

static int socket_send(struct socket_entry* sock, const char* payload){
    struct net_packet* tx;
    if(str_eq(sock->proto, "tcp") && !str_eq(sock->state, "ESTABLISHED") &&
       !str_eq(sock->state, "CONNECTED") && !str_eq(sock->state, "LISTEN") &&
       !str_eq(sock->state, "LISTENING")){
        stats.drops++;
        return -1;
    }
    tx = alloc_packet();
    if(!tx) return -1;
    sock->flood_score++;
    if(shield_enabled && sock->flood_score > flood_threshold){
        socket_set_state(sock, "CLOSED");
        stats.drops++;
        fs_append_line("/var/log/network.log", "net shield: port closed after flood pattern");
        console_puts("net shield: port auto-closed to stop flood traffic\n");
        return -1;
    }
    packet_fill(tx, PKT_TX, sock->proto, sock->local_port, sock->remote_port, payload);
    sock->tx_packets++;
    sock->bytes_tx += tx->len;
    sock->last_activity = timer_ticks();
    stats.tx_packets++;
    deliver_loopback(sock, payload);
    fs_append_line("/var/log/network.log", "net: loopback packet queued");
    jobs_account("network-packet", 2);
    return 0;
}

static void socket_open(const char* proto, uint32_t port){
    int id;
    struct socket_entry* sock;
    if(!privacy_allows_network()){
        console_puts("network blocked by privacy switch. Use: privacy network on\n");
        stats.drops++;
        return;
    }
    id = net_socket_create(1, proto);
    if(id < 0 || net_bind((uint32_t)id, port) != 0){
        if(id >= 0) net_socket_close((uint32_t)id);
        console_puts("socket: table full or port unavailable\n");
        return;
    }
    if(str_eq(proto, "tcp")) net_listen((uint32_t)id);
    else socket_set_state(&sockets[id], "OPEN");
    sock = &sockets[id];
    console_puts("socket id=");
    console_write_dec((uint32_t)id);
    console_puts(" fd=");
    if(sock->fd >= 0) console_write_dec((uint32_t)sock->fd);
    else console_puts("none");
    console_puts(" ");
    console_puts(sock->proto);
    console_puts(" owner=");
    console_write_dec(sock->owner_pid);
    console_puts(" port=");
    console_write_dec(port);
    console_puts(" state=");
    console_puts(sock->state);
    console_puts(" shield=");
    console_puts(shield_enabled ? "on" : "off");
    console_putc('\n');
}

static void print_packet(const struct net_packet* pkt){
    console_puts(pkt->dir == PKT_TX ? "tx#" : "rx#");
    console_write_dec(pkt->id);
    console_puts(" ");
    console_puts(pkt->iface);
    console_puts(" ");
    console_puts(pkt->proto);
    console_puts(" ");
    print_ip(pkt->src_ip);
    console_putc(':');
    console_write_dec(pkt->src_port);
    console_puts(" -> ");
    print_ip(pkt->dst_ip);
    console_putc(':');
    console_write_dec(pkt->dst_port);
    console_puts(" len=");
    console_write_dec(pkt->len);
    console_puts(" sum=");
    console_write_hex(pkt->checksum);
    console_puts(" data=");
    console_puts(pkt->payload);
    console_putc('\n');
}

static void print_packets(void){
    for(int i=0; i<PACKET_MAX; i++)
        if(packets[i].used)
            print_packet(&packets[i]);
}

static void print_sockets(void){
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
            console_puts(" remote=");
            console_write_dec(sockets[i].remote_port);
            console_puts(" fd=");
            if(sockets[i].fd >= 0) console_write_dec((uint32_t)sockets[i].fd);
            else console_puts("none");
            console_puts(" owner=");
            console_write_dec(sockets[i].owner_pid);
            console_puts(" tx=");
            console_write_dec(sockets[i].tx_packets);
            console_puts(" rx=");
            console_write_dec(sockets[i].rx_packets);
            console_puts(" flood=");
            console_write_dec(sockets[i].flood_score);
            console_puts(" data=");
            console_puts(sockets[i].rx);
            console_putc('\n');
        }
    }
}

int net_socket_create(uint32_t owner_pid, const char* proto){
    for(int i=0; i<SOCKET_MAX; i++){
        if(!sockets[i].used){
            char label[16] = "socket:0";
            label[7] = (char)('0' + i);
            sockets[i].used = 1;
            sockets[i].id = i;
            sockets[i].proto = str_eq(proto, "udp") ? "udp" : "tcp";
            sockets[i].state = "CLOSED";
            sockets[i].owner_pid = owner_pid ? owner_pid : 1;
            sockets[i].local_port = 0;
            sockets[i].remote_port = 0;
            sockets[i].rx[0] = 0;
            sockets[i].rx_packets = 0;
            sockets[i].tx_packets = 0;
            sockets[i].connect_attempts = 0;
            sockets[i].flood_score = 0;
            sockets[i].peer_id = SOCKET_MAX;
            sockets[i].bytes_tx = 0;
            sockets[i].bytes_rx = 0;
            sockets[i].last_activity = timer_ticks();
            sockets[i].fd = fd_open_socket(label);
            return i;
        }
    }
    stats.drops++;
    return -1;
}

int net_bind(uint32_t socket_id, uint32_t port){
    struct socket_entry* sock = socket_by_id(socket_id);
    struct port_entry* entry;
    if(!sock || port == 0)
        return -1;
    entry = port_alloc(port);
    if(!entry || (entry->state != NET_PORT_FREE && entry->socket_id != socket_id)){
        stats.drops++;
        return -1;
    }
    sock->local_port = port;
    sock->state = "BOUND";
    sock->last_activity = timer_ticks();
    entry->state = NET_PORT_BOUND;
    entry->owner_pid = sock->owner_pid;
    entry->socket_id = socket_id;
    entry->service = str_eq(sock->proto, "udp") ? "udp-loopback" : "tcp-loopback";
    return 0;
}

int net_listen(uint32_t socket_id){
    struct socket_entry* sock = socket_by_id(socket_id);
    struct port_entry* entry;
    if(!sock || sock->local_port == 0)
        return -1;
    if(!policy_check_network_access("network", sock->local_port, 1))
        return -1;
    entry = port_by_number(sock->local_port);
    if(!entry || entry->state == NET_PORT_BLOCKED || entry->state == NET_PORT_RESERVED)
        return -1;
    sock->state = "LISTENING";
    sock->last_activity = timer_ticks();
    entry->state = NET_PORT_LISTENING;
    tcp_listen_port = (int)sock->local_port;
    return 0;
}

int net_accept(uint32_t listener_id){
    struct socket_entry* listener = socket_by_id(listener_id);
    if(!listener || !str_eq(listener->state, "LISTENING"))
        return -1;
    return (int)listener_id;
}

int net_connect(uint32_t socket_id, uint32_t port){
    struct socket_entry* sock = socket_by_id(socket_id);
    struct socket_entry* listener;
    if(!sock || !privacy_allows_network())
        return -1;
    if(!policy_check_network_access("network", port, 0))
        return -1;
    listener = listener_for_port(port);
    sock->connect_attempts++;
    if(shield_enabled && sock->connect_attempts > flood_threshold){
        socket_set_state(sock, "CLOSED");
        stats.drops++;
        return -1;
    }
    if(!listener){
        socket_set_state(sock, "ERROR");
        stats.drops++;
        return -1;
    }
    sock->remote_port = port;
    sock->local_port = sock->local_port ? sock->local_port : (40000 + socket_id);
    sock->peer_id = (uint32_t)listener->id;
    listener->peer_id = socket_id;
    socket_set_state(sock, "CONNECTING");
    socket_set_state(sock, "CONNECTED");
    listener->remote_port = sock->local_port;
    listener->last_activity = timer_ticks();
    sock->last_activity = timer_ticks();
    return 0;
}

int net_send(uint32_t socket_id, const char* text){
    struct socket_entry* sock = socket_by_id(socket_id);
    if(!sock || !privacy_allows_network())
        return -1;
    if(str_eq(sock->state, "BOUND") && str_eq(sock->proto, "udp"))
        socket_set_state(sock, "OPEN");
    return socket_send(sock, text);
}

int net_recv(uint32_t socket_id, const char** out){
    struct socket_entry* sock = socket_by_id(socket_id);
    uint32_t i = 0;
    const char* src;
    if(!sock || !out)
        return -1;
    src = sock->rx[0] ? sock->rx : "<empty>";
    while(src[i] && i + 1 < sizeof(fd_read_buffer)){
        fd_read_buffer[i] = src[i];
        i++;
    }
    fd_read_buffer[i] = 0;
    *out = fd_read_buffer;
    sock->rx[0] = 0;
    sock->last_activity = timer_ticks();
    return 0;
}

int net_poll(uint32_t socket_id){
    struct socket_entry* sock = socket_by_id(socket_id);
    if(!sock)
        return -1;
    return sock->rx[0] ? 1 : 0;
}

uint32_t net_shutdown_idle(uint32_t max_idle_ticks){
    uint32_t now = timer_ticks();
    uint32_t closed = 0;
    for(uint32_t i=0; i<SOCKET_MAX; i++){
        if(!sockets[i].used)
            continue;
        if(max_idle_ticks == 0 || (now >= sockets[i].last_activity && now - sockets[i].last_activity >= max_idle_ticks)){
            if(net_socket_close(i) == 0)
                closed++;
        }
    }
    return closed;
}

int net_socket_close(uint32_t socket_id){
    struct socket_entry* sock = socket_by_id(socket_id);
    struct socket_entry* peer;
    struct port_entry* entry;
    if(!sock)
        return -1;
    peer = socket_by_id(sock->peer_id);
    if(peer){
        peer->peer_id = SOCKET_MAX;
        socket_set_state(peer, "CLOSED");
    }
    entry = port_by_number(sock->local_port);
    if(entry && entry->socket_id == socket_id){
        entry->state = NET_PORT_FREE;
        entry->owner_pid = 0;
        entry->socket_id = 0;
        entry->service = "free";
    }
    socket_set_state(sock, "CLOSED");
    sock->used = 0;
    return 0;
}

uint32_t net_connection_list(struct net_connection_info* out, uint32_t max){
    uint32_t count = 0;
    for(int i=0; i<SOCKET_MAX; i++){
        if(!sockets[i].used)
            continue;
        if(out && count < max){
            out[count].used = sockets[i].used;
            out[count].socket_id = (uint32_t)sockets[i].id;
            out[count].peer_id = sockets[i].peer_id;
            out[count].owner_pid = sockets[i].owner_pid;
            out[count].proto = sockets[i].proto;
            out[count].state = conn_state_from_text(sockets[i].state);
            out[count].local_port = sockets[i].local_port;
            out[count].remote_port = sockets[i].remote_port;
            out[count].bytes_tx = sockets[i].bytes_tx;
            out[count].bytes_rx = sockets[i].bytes_rx;
            out[count].last_activity = sockets[i].last_activity;
        }
        count++;
    }
    return count;
}

uint32_t net_port_list(struct net_port_info* out, uint32_t max){
    uint32_t count = 0;
    for(int i=0; i<PORT_MAX; i++){
        if(!ports[i].used)
            continue;
        if(out && count < max){
            out[count].port = ports[i].port;
            out[count].state = ports[i].state;
            out[count].owner_pid = ports[i].owner_pid;
            out[count].socket_id = ports[i].socket_id;
            out[count].service = ports[i].service;
        }
        count++;
    }
    return count;
}

void net_init(void){
    for(int i=0; i<SOCKET_MAX; i++){
        sockets[i].used = 0;
        sockets[i].id = i;
        sockets[i].owner_pid = 0;
        sockets[i].rx[0] = 0;
        sockets[i].fd = -1;
        sockets[i].state = "CLOSED";
        sockets[i].connect_attempts = 0;
        sockets[i].flood_score = 0;
        sockets[i].peer_id = SOCKET_MAX;
        sockets[i].bytes_tx = 0;
        sockets[i].bytes_rx = 0;
        sockets[i].last_activity = 0;
    }
    for(int i=0; i<PORT_MAX; i++){
        ports[i].used = 0;
        ports[i].port = 0;
        ports[i].state = NET_PORT_FREE;
        ports[i].owner_pid = 0;
        ports[i].socket_id = 0;
        ports[i].service = "free";
    }
    for(int i=0; i<PACKET_MAX; i++)
        packets[i].used = 0;
    stats.tx_packets = 0;
    stats.rx_packets = 0;
    stats.drops = 0;
    stats.checksum_errors = 0;
    stats.state_changes = 0;
    fs_write("/system/net/stack.txt",
        "layers=link,arp,ipv4,icmp,udp,tcp,sockets\n"
        "loopback=127.0.0.1/8\n"
        "queues=tx,rx\n"
        "packet_max=12\n"
        "socket_max=4\n");
    fs_append_line("/var/log/network.log", "net: packet queues and loopback stack online");
}

int net_fd_write(uint32_t pid, int fd, const char* text){
    struct socket_entry* sock = socket_by_fd(pid, fd);
    if(!sock) return -1;
    if(!privacy_allows_network()){
        stats.drops++;
        return -1;
    }
    return socket_send(sock, text);
}

int net_fd_read(uint32_t pid, int fd, const char** out){
    struct socket_entry* sock = socket_by_fd(pid, fd);
    size_t j = 0;
    const char* src;
    if(!sock) return -1;
    src = sock->rx[0] ? sock->rx : "<empty>";
    while(src[j] && j + 1 < sizeof(fd_read_buffer)){
        fd_read_buffer[j] = src[j];
        j++;
    }
    fd_read_buffer[j++] = '\n';
    fd_read_buffer[j] = 0;
    sock->rx[0] = 0;
    *out = fd_read_buffer;
    return 0;
}

int net_is_link_up(void){
    return link_up;
}

uint32_t net_packet_count(void){
    uint32_t count = 0;
    for(int i=0; i<PACKET_MAX; i++)
        if(packets[i].used)
            count++;
    return count;
}

uint32_t net_socket_count(void){
    return SOCKET_MAX;
}

int net_socket_at(uint32_t index, struct net_socket_info* out){
    struct socket_entry* sock;
    if(!out || index >= SOCKET_MAX)
        return -1;
    sock = &sockets[index];
    out->used = sock->used;
    out->id = sock->id;
    out->owner_pid = sock->owner_pid;
    out->fd = sock->fd;
    out->proto = sock->proto;
    out->state = sock->state;
    out->local_port = sock->local_port;
    out->remote_port = sock->remote_port;
    out->rx_packets = sock->rx_packets;
    out->tx_packets = sock->tx_packets;
    out->flood_score = sock->flood_score;
    out->rx = sock->rx;
    return 0;
}

int net_shield_enabled(void){
    return shield_enabled;
}

int net_ip_masking_enabled(void){
    return ip_masking;
}

uint32_t net_flood_threshold(void){
    return flood_threshold;
}

uint32_t net_listen_port(void){
    return (uint32_t)tcp_listen_port;
}

void net_disconnect_all(void){
    for(int i=0; i<SOCKET_MAX; i++)
        if(sockets[i].used && str_eq(sockets[i].proto, "tcp"))
            socket_set_state(&sockets[i], "CLOSED");
    link_up = 0;
    tcp_listen_port = 0;
}

void net_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("lo: up ip=127.0.0.1/8 queue=ready\n");
        console_puts("eth0: ");
        console_puts(link_up ? "up" : "down");
        console_puts(" ip=0.0.0.0/0 driver=stub\n");
        console_puts("packets=");
        console_write_dec(net_packet_count());
        console_puts(" tx=");
        console_write_dec(stats.tx_packets);
        console_puts(" rx=");
        console_write_dec(stats.rx_packets);
        console_puts(" drops=");
        console_write_dec(stats.drops);
        console_puts(" shield=");
        console_puts(shield_enabled ? "on" : "off");
        console_puts(" ip_mask=");
        console_puts(ip_masking ? "on" : "off");
        console_putc('\n');
    } else if(str_eq(action, "iface") || str_eq(action, "ifaces")){
        console_puts("lo    up   ip=127.0.0.1/8 packets=");
        console_write_dec(net_packet_count());
        console_putc('\n');
        console_puts("eth0  ");
        console_puts(link_up ? "up   " : "down ");
        console_puts("ip=0.0.0.0/0 driver=stub\n");
    } else if(str_eq(action, "up")){
        if(!privacy_allows_network()){
            console_puts("net: blocked by privacy switch. Use: privacy network on\n");
            return;
        }
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
        console_puts("UDP: OPEN -> send datagram -> RX loopback queue\n");
    } else if(str_eq(action, "tcp")){
        uint32_t port = parse_u32(rest);
        if(port == 0){
            console_puts("TCP states: CLOSED LISTEN SYN-SENT ESTABLISHED FIN-WAIT TIME-WAIT\n");
            console_puts("usage: net tcp PORT\n");
        } else {
            if(!privacy_allows_network()){
                console_puts("tcp: blocked by privacy switch\n");
                return;
            }
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
        socket_open(proto, port);
    } else if(str_eq(action, "listen")){
        uint32_t port = parse_u32(rest);
        int id = net_socket_create(1, "tcp");
        if(id >= 0 && net_bind((uint32_t)id, port) == 0 && net_listen((uint32_t)id) == 0){
            console_puts("listening socket=");
            console_write_dec((uint32_t)id);
            console_puts(" port=");
            console_write_dec(port);
            console_putc('\n');
        } else {
            console_puts("listen: unavailable or blocked\n");
        }
    } else if(str_eq(action, "sockets")){
        print_sockets();
    } else if(str_eq(action, "connections") || str_eq(action, "conn")){
        struct net_connection_info conns[SOCKET_MAX];
        uint32_t n = net_connection_list(conns, SOCKET_MAX);
        for(uint32_t i=0; i<n && i<SOCKET_MAX; i++){
            console_puts("conn sock=");
            console_write_dec(conns[i].socket_id);
            console_puts(" peer=");
            if(conns[i].peer_id < SOCKET_MAX) console_write_dec(conns[i].peer_id);
            else console_puts("none");
            console_puts(" ");
            console_puts(conn_state_text(conns[i].state));
            console_puts(" local=");
            console_write_dec(conns[i].local_port);
            console_puts(" remote=");
            console_write_dec(conns[i].remote_port);
            console_puts(" tx=");
            console_write_dec(conns[i].bytes_tx);
            console_puts(" rx=");
            console_write_dec(conns[i].bytes_rx);
            console_putc('\n');
        }
    } else if(str_eq(action, "ports")){
        struct net_port_info info[PORT_MAX];
        uint32_t n = net_port_list(info, PORT_MAX);
        for(uint32_t i=0; i<n && i<PORT_MAX; i++){
            console_puts("port ");
            console_write_dec(info[i].port);
            console_puts(" ");
            console_puts(port_state_text(info[i].state));
            console_puts(" owner=");
            console_write_dec(info[i].owner_pid);
            console_puts(" socket=");
            console_write_dec(info[i].socket_id);
            console_puts(" service=");
            console_puts(info[i].service);
            console_putc('\n');
        }
    } else if(str_eq(action, "close")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        console_puts(net_socket_close(id) == 0 ? "socket closed\n" : "close: bad socket\n");
    } else if(str_eq(action, "fd")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        struct socket_entry* sock = socket_by_id(id);
        if(!sock) console_puts("fd: bad socket\n");
        else {
            console_puts("socket ");
            console_write_dec(id);
            console_puts(" fd=");
            if(sock->fd >= 0) console_write_dec((uint32_t)sock->fd);
            else console_puts("none");
            console_putc('\n');
        }
    } else if(str_eq(action, "connect")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        uint32_t port = parse_u32(first_arg(rest, &rest));
        console_puts(net_connect(id, port) == 0 ? "socket connected state=CONNECTED\n" : "connect: blocked or unavailable\n");
    } else if(str_eq(action, "send")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        if(net_send(id, rest) != 0) console_puts("send: socket not ready\n");
        else console_puts("packet queued on loopback\n");
    } else if(str_eq(action, "recv")){
        uint32_t id = parse_u32(first_arg(rest, &rest));
        const char* msg;
        if(net_recv(id, &msg) != 0) console_puts("recv: bad socket\n");
        else {
            console_puts(msg);
            console_putc('\n');
        }
    } else if(str_eq(action, "packets") || str_eq(action, "trace")){
        print_packets();
    } else if(str_eq(action, "stats")){
        console_puts("net stats tx=");
        console_write_dec(stats.tx_packets);
        console_puts(" rx=");
        console_write_dec(stats.rx_packets);
        console_puts(" drops=");
        console_write_dec(stats.drops);
        console_puts(" checksum_errors=");
        console_write_dec(stats.checksum_errors);
        console_puts(" state_changes=");
        console_write_dec(stats.state_changes);
        console_puts(" shield=");
        console_puts(shield_enabled ? "on" : "off");
        console_puts(" masked_ip=");
        console_puts(ip_masking ? "on" : "off");
        console_putc('\n');
    } else if(str_eq(action, "shield")){
        const char* value = first_arg(rest, &rest);
        if(str_eq(value, "on")) shield_enabled = 1;
        else if(str_eq(value, "off")) shield_enabled = 0;
        else if(str_eq(value, "threshold")){
            uint32_t n = parse_u32(rest);
            if(n) flood_threshold = n;
        } else if(str_eq(value, "mask")){
            const char* setting = first_arg(rest, &rest);
            ip_masking = str_eq(setting, "off") ? 0 : 1;
        } else {
            console_puts("shield=");
            console_puts(shield_enabled ? "on" : "off");
            console_puts(" threshold=");
            console_write_dec(flood_threshold);
            console_puts(" ip_mask=");
            console_puts(ip_masking ? "on" : "off");
            console_putc('\n');
            return;
        }
        console_puts("net shield updated\n");
    } else if(str_eq(action, "flush")){
        for(int i=0; i<PACKET_MAX; i++)
            packets[i].used = 0;
        console_puts("net: packet queues flushed\n");
    } else {
        console_puts("usage: net status|iface|up|down|ip|udp|tcp PORT|arp|route|open tcp|udp PORT|listen PORT|socket tcp|udp PORT|fd ID|sockets|connections|ports|close ID|connect ID PORT|send ID MSG|recv ID|packets|trace|stats|shield [on|off|threshold N|mask on|off]|flush\n");
    }
}
