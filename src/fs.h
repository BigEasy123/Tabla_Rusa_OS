#ifndef FS_H
#define FS_H

#include <stddef.h>

void fs_init(void);
int fs_mkdir(const char* path);
int fs_touch(const char* path);
int fs_write(const char* path, const char* text);
int fs_append_line(const char* path, const char* text);
int fs_read(const char* path, const char** out);
int fs_rm(const char* path);
int fs_cd(const char* path);
void fs_pwd(char* out, size_t max);
void fs_ls(const char* path);

#endif
