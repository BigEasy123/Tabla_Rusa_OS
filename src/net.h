#ifndef NET_H
#define NET_H

#include <stdint.h>

struct net_socket_info {
    int used;
    int id;
    uint32_t owner_pid;
    int fd;
    const char* proto;
    const char* state;
    uint32_t local_port;
    uint32_t remote_port;
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t flood_score;
    const char* rx;
};

enum net_connection_state {
    NET_CONN_CLOSED,
    NET_CONN_LISTENING,
    NET_CONN_CONNECTING,
    NET_CONN_CONNECTED,
    NET_CONN_CLOSING,
    NET_CONN_ERROR
};

enum net_port_state {
    NET_PORT_FREE,
    NET_PORT_BOUND,
    NET_PORT_LISTENING,
    NET_PORT_BLOCKED,
    NET_PORT_RESERVED
};

struct net_connection_info {
    int used;
    uint32_t socket_id;
    uint32_t peer_id;
    uint32_t owner_pid;
    const char* proto;
    enum net_connection_state state;
    uint32_t local_port;
    uint32_t remote_port;
    uint32_t bytes_tx;
    uint32_t bytes_rx;
    uint32_t last_activity;
};

struct net_port_info {
    uint32_t port;
    enum net_port_state state;
    uint32_t owner_pid;
    uint32_t socket_id;
    const char* service;
};

void net_init(void);
int net_is_link_up(void);
int net_socket_create(uint32_t owner_pid, const char* proto);
int net_socket_close(uint32_t socket_id);
int net_bind(uint32_t socket_id, uint32_t port);
int net_listen(uint32_t socket_id);
int net_accept(uint32_t listener_id);
int net_connect(uint32_t socket_id, uint32_t port);
int net_send(uint32_t socket_id, const char* text);
int net_recv(uint32_t socket_id, const char** out);
int net_poll(uint32_t socket_id);
uint32_t net_shutdown_idle(uint32_t max_idle_ticks);
uint32_t net_connection_list(struct net_connection_info* out, uint32_t max);
uint32_t net_port_list(struct net_port_info* out, uint32_t max);
int net_fd_write(uint32_t pid, int fd, const char* text);
int net_fd_read(uint32_t pid, int fd, const char** out);
uint32_t net_packet_count(void);
uint32_t net_socket_count(void);
int net_socket_at(uint32_t index, struct net_socket_info* out);
int net_shield_enabled(void);
int net_ip_masking_enabled(void);
uint32_t net_flood_threshold(void);
uint32_t net_listen_port(void);
void net_disconnect_all(void);
void net_cmd(char* arg);

#endif
