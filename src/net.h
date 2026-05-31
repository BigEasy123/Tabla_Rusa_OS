#ifndef NET_H
#define NET_H

#include <stdint.h>

void net_init(void);
int net_is_link_up(void);
int net_fd_write(uint32_t pid, int fd, const char* text);
int net_fd_read(uint32_t pid, int fd, const char** out);
uint32_t net_packet_count(void);
void net_disconnect_all(void);
void net_cmd(char* arg);

#endif
