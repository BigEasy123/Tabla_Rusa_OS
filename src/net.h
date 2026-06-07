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

void net_init(void);
int net_is_link_up(void);
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
