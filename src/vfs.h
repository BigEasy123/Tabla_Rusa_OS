#ifndef VFS_H
#define VFS_H

void vfs_init(void);
int vfs_can_write(const char* path);
void vfs_mounts_cmd(void);

#endif
