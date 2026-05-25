#include <stddef.h>
#include "console.h"
#include "fs.h"
#include "vfs.h"

#define MOUNT_MAX 6

struct mount_info {
    const char* fs;
    const char* path;
    const char* mode;
    const char* note;
};

static struct mount_info mounts[MOUNT_MAX] = {
    {"ramfs", "/", "rw", "volatile boot filesystem"},
    {"procfs", "/proc", "ro", "kernel introspection view"},
    {"sysfs", "/system", "ro", "subsystem object namespace"},
    {"devfs", "/dev", "rw", "device object namespace"},
    {"pkgfs", "/pkg", "rw", "package manifest registry"},
    {"mathfs", "/home/math", "rw", "scientific object workspace"}
};

static int str_starts(const char* s, const char* prefix){
    while(*prefix){
        if(*s++ != *prefix++) return 0;
    }
    return 1;
}

void vfs_init(void){
    fs_append_line("/var/log/system.log", "vfs: stack online ramfs/procfs/sysfs/devfs/pkgfs/mathfs");
}

int vfs_can_write(const char* path){
    if(str_starts(path, "/proc") || str_starts(path, "/system") || str_starts(path, "/boot"))
        return 0;
    return 1;
}

void vfs_mounts_cmd(void){
    for(size_t i=0; i<MOUNT_MAX; i++){
        console_puts(mounts[i].fs);
        console_puts(" on ");
        console_puts(mounts[i].path);
        console_puts(" ");
        console_puts(mounts[i].mode);
        console_puts(" - ");
        console_puts(mounts[i].note);
        console_putc('\n');
    }
}
