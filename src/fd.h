#ifndef FD_H
#define FD_H

#include <stddef.h>

void fd_init(void);
int fd_open(const char* path, const char* mode);
int fd_open_socket(const char* label);
int fd_close(int fd);
int fd_read(int fd, const char** out);
int fd_write(int fd, const char* text);
void fd_cmd(char* arg);

#endif
