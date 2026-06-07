#ifndef FD_H
#define FD_H

#include <stddef.h>
#include <stdint.h>

void fd_init(void);
int fd_open(const char* path, const char* mode);
int fd_open_socket(const char* label);
int fd_open_for_pid(uint32_t pid, const char* path, const char* mode);
int fd_open_socket_for_pid(uint32_t pid, const char* label);
int fd_close(int fd);
int fd_read(int fd, const char** out);
int fd_read_chunk(int fd, char* out, size_t max);
int fd_write(int fd, const char* text);
int fd_seek(int fd, size_t offset);
size_t fd_tell(int fd);
int fd_close_process(uint32_t pid);
uint32_t fd_count_for_pid(uint32_t pid);
void fd_cmd(char* arg);

#endif
