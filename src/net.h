#ifndef NET_H
#define NET_H

void net_init(void);
int net_is_link_up(void);
int net_fd_write(int fd, const char* text);
int net_fd_read(int fd, const char** out);
void net_cmd(char* arg);

#endif
