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
int fs_copy(const char* src, const char* dst);
int fs_move(const char* src, const char* dst);
int fs_rename(const char* path, const char* new_name);
int fs_stat(const char* path, int* type, size_t* size);
void fs_pwd(char* out, size_t max);
void fs_ls(const char* path);
void fs_tree(const char* path);
int fs_child_count(const char* path);
int fs_child_name(const char* path, int index, char* out, size_t max, int* type);
void fs_join_path(const char* dir, const char* name, char* out, size_t max);

#endif
