#include "fs.h"
#include "console.h"
#include <stdint.h>

#define FS_MAX_NODES 192
#define FS_NAME_MAX 32
#define FS_CONTENT_MAX 1024

enum fs_type {
    FS_UNUSED = 0,
    FS_DIR,
    FS_FILE
};

struct fs_node {
    enum fs_type type;
    int parent;
    char name[FS_NAME_MAX];
    char content[FS_CONTENT_MAX];
    size_t size;
    uint8_t permissions;
};

static struct fs_node nodes[FS_MAX_NODES];
static int cwd = 0;

static int streq(const char* a, const char* b){
    while(*a && *b){
        if(*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static void str_copy(char* dst, const char* src, size_t max){
    size_t i = 0;
    if(max == 0) return;
    while(i + 1 < max && src[i]){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static size_t str_len(const char* s){
    size_t len = 0;
    while(s[len]) len++;
    return len;
}

static const char* skip_slashes(const char* s){
    while(*s == '/') s++;
    return s;
}

static int alloc_node(void){
    for(int i=0; i<FS_MAX_NODES; i++)
        if(nodes[i].type == FS_UNUSED)
            return i;
    return -1;
}

static int find_child(int parent, const char* name){
    for(int i=0; i<FS_MAX_NODES; i++)
        if(nodes[i].type != FS_UNUSED && nodes[i].parent == parent && streq(nodes[i].name, name))
            return i;
    return -1;
}

static int read_part(const char** path, char* part){
    const char* p = skip_slashes(*path);
    size_t i = 0;
    while(*p && *p != '/'){
        if(i + 1 < FS_NAME_MAX)
            part[i++] = *p;
        p++;
    }
    part[i] = 0;
    *path = p;
    return i > 0;
}

static int resolve(const char* path){
    int current = (path && path[0] == '/') ? 0 : cwd;
    char part[FS_NAME_MAX];
    if(path == 0 || path[0] == 0)
        return current;
    while(read_part(&path, part)){
        if(streq(part, "."))
            continue;
        if(streq(part, "..")){
            if(current != 0)
                current = nodes[current].parent;
            continue;
        }
        int child = find_child(current, part);
        if(child < 0)
            return -1;
        current = child;
    }
    return current;
}

static int resolve_parent(const char* path, char* name){
    int current = (path && path[0] == '/') ? 0 : cwd;
    char part[FS_NAME_MAX];
    char last[FS_NAME_MAX];
    last[0] = 0;
    if(path == 0 || path[0] == 0)
        return -1;
    while(read_part(&path, part)){
        str_copy(last, part, FS_NAME_MAX);
        const char* rest = skip_slashes(path);
        if(*rest == 0)
            break;
        if(streq(part, "."))
            continue;
        if(streq(part, "..")){
            if(current != 0) current = nodes[current].parent;
            continue;
        }
        int child = find_child(current, part);
        if(child < 0 || nodes[child].type != FS_DIR)
            return -1;
        current = child;
    }
    if(last[0] == 0 || streq(last, ".") || streq(last, ".."))
        return -1;
    str_copy(name, last, FS_NAME_MAX);
    return current;
}

static int create_node(const char* path, enum fs_type type){
    char name[FS_NAME_MAX];
    int parent = resolve_parent(path, name);
    if(parent < 0 || nodes[parent].type != FS_DIR)
        return -1;
    if(find_child(parent, name) >= 0)
        return -2;
    int id = alloc_node();
    if(id < 0)
        return -3;
    nodes[id].type = type;
    nodes[id].parent = parent;
    nodes[id].size = 0;
    nodes[id].permissions = FS_PERM_READ | FS_PERM_WRITE;
    nodes[id].content[0] = 0;
    str_copy(nodes[id].name, name, FS_NAME_MAX);
    return id;
}

void fs_init(void){
    for(int i=0; i<FS_MAX_NODES; i++)
        nodes[i].type = FS_UNUSED;
    nodes[0].type = FS_DIR;
    nodes[0].parent = 0;
    nodes[0].permissions = FS_PERM_READ | FS_PERM_WRITE;
    nodes[0].name[0] = 0;
    cwd = 0;
    fs_mkdir("/home");
    fs_mkdir("/home/docs");
    fs_mkdir("/home/math");
    fs_mkdir("/home/projects");
    fs_mkdir("/home/downloads");
    fs_mkdir("/bin");
    fs_mkdir("/boot");
    fs_mkdir("/config");
    fs_mkdir("/dev");
    fs_mkdir("/lib");
    fs_mkdir("/mnt");
    fs_mkdir("/pkg");
    fs_mkdir("/proc");
    fs_mkdir("/services");
    fs_mkdir("/share");
    fs_mkdir("/share/docs");
    fs_mkdir("/system");
    fs_mkdir("/system/gui");
    fs_mkdir("/system/net");
    fs_mkdir("/system/security");
    fs_mkdir("/tmp");
    fs_mkdir("/users");
    fs_mkdir("/users/root");
    fs_mkdir("/var");
    fs_mkdir("/var/log");
    fs_write("/home/readme.txt",
        "Welcome to Tabla Rusa OS.\n"
        "Try: ls, cat readme.txt, edit notes.txt. Up/Down move lines; Esc exits.\n"
        "Try: pkg list, pkg info editor, package[\"editor\"].install()\n"
        "Explore: cd /system, cd /var/log, gui status, net status, security status\n"
        "Native forms: inspect memory, file[\"readme.txt\"].read(), spawn editor notes.txt\n");
    fs_write("/boot/kernel.cfg", "kernel=tabla-rusa\narch=i386\nruntime=tabla:0.1\n");
    fs_write("/config/system.conf", "hostname=tabla\nsecure_mode=on\ngui=planned\nnetwork=loopback\n");
    fs_write("/config/gui.conf", "wallpaper=lava\nwallpaper_live=no\ncursor=dot\neditor_mode=paper\neditor_path=/home/notes.txt\n");
    fs_write("/dev/keyboard", "device=ps2-keyboard\nstate=active\n");
    fs_write("/dev/console", "device=vga-text-console\nstate=active\n");
    fs_write("/proc/version", "Tabla Rusa OS 0.0.5 i386 tabla:0.1\n");
    fs_write("/proc/mounts", "ramfs / rw\nprocfs /proc ro\nsysfs /system ro\npkgfs /pkg rw\n");
    fs_write("/system/gui/README", "GUI foundation: compositor, windows, themes, events.\n");
    fs_write("/system/net/README", "Network foundation: device, IP, ARP, TCP, UDP layers.\n");
    fs_write("/system/security/README", "Security foundation: rings, capabilities, audit log, policy.\n");
    fs_write("/share/docs/fs-stack.txt", "VFS -> ramfs now; planned: devfs, procfs, pkgfs, persistent diskfs.\n");
    fs_write("/share/docs/tcpip-stack.txt", "TCP/IP scaffold: link, IPv4, ARP, ICMP, UDP, TCP sockets.\n");
    fs_write("/share/docs/gui-roadmap.txt", "GUI scaffold: framebuffer, compositor, window objects, input events.\n");
    fs_write("/share/docs/native-objects.txt", "Objects: file, dir, mount, process, service, window, package.\n");
    fs_write("/share/docs/scientific-scheduling.txt",
        "Tabla process table tracks priority, cpu_hint, workload class, and ticks.\n"
        "Use compute status, compute vector, compute bench, and process[\"compute\"].trace().\n");
    fs_write("/share/docs/math-kernel.txt",
        "Tabla math kernel commands:\n"
        "math vec dot 1 2 3 | 4 5 6\n"
        "math vec add 1 2 | 10 20\n"
        "math mat det2 1 2 3 4\n"
        "math mat det3 1 0 0 0 1 0 0 0 1\n"
        "math num gcd 84 30, math num modpow 2 10 17, math num prime 97\n"
        "math group cyclic 5, math group units 12, math stats 1 2 3 4\n"
        "math object vector a 1 2 3, math object matrix A 1 2 3 4\n"
        "math rat add 1/3 1/6, math mat inv2 1 2 3 4, math modmat inv2 11 1 2 3 4\n"
        "math sym diff 1 0 -1, math object save A /home/math/A.obj\n"
        "math job submit vec dot 1 2 | 3 4, math job run 1\n"
        "math latex vec 1 2 3, math latex mat2 1 2 3 4\n"
        "All math commands mark the compute process with a workload class and ticks.\n");
    fs_write("/services/logger", "state=running\nprovides=audit and system logs\n");
    fs_write("/services/network", "state=stopped\nprovides=TCP/IP stack foundation\n");
    fs_write("/services/gui", "state=stopped\nprovides=window compositor foundation\n");
    fs_write("/users/root/profile", "user=root\ncaps=all\n");
    fs_write("/users/guest.profile", "user=guest\ncaps=fs.read,shell.run\n");
    fs_write("/var/log/system.log", "boot: ram filesystem initialized\n");
    fs_write("/var/log/security.log", "security: policy loaded secure_mode=on\n");
    fs_write("/var/log/network.log", "net: loopback initialized tcp_state=closed\n");
    fs_cd("/home");
}

int fs_mkdir(const char* path){
    int r = create_node(path, FS_DIR);
    return r >= 0 ? 0 : r;
}

int fs_touch(const char* path){
    int existing = resolve(path);
    if(existing >= 0)
        return nodes[existing].type == FS_FILE ? 0 : -2;
    int r = create_node(path, FS_FILE);
    return r >= 0 ? 0 : r;
}

int fs_write(const char* path, const char* text){
    int id = resolve(path);
    if(id < 0){
        int r = create_node(path, FS_FILE);
        if(r < 0) return r;
        id = r;
    }
    if(nodes[id].type != FS_FILE)
        return -2;
    if(!(nodes[id].permissions & FS_PERM_WRITE))
        return -4;
    str_copy(nodes[id].content, text, FS_CONTENT_MAX);
    nodes[id].size = str_len(nodes[id].content);
    return 0;
}

int fs_append_line(const char* path, const char* text){
    int id = resolve(path);
    if(id < 0){
        int r = create_node(path, FS_FILE);
        if(r < 0) return r;
        id = r;
    }
    if(nodes[id].type != FS_FILE)
        return -2;
    if(!(nodes[id].permissions & FS_PERM_WRITE))
        return -4;
    size_t pos = nodes[id].size;
    for(size_t i=0; text[i] && pos + 2 < FS_CONTENT_MAX; i++)
        nodes[id].content[pos++] = text[i];
    if(pos + 1 < FS_CONTENT_MAX)
        nodes[id].content[pos++] = '\n';
    nodes[id].content[pos] = 0;
    nodes[id].size = pos;
    return 0;
}

int fs_read(const char* path, const char** out){
    int id = resolve(path);
    if(id < 0 || nodes[id].type != FS_FILE)
        return -1;
    if(!(nodes[id].permissions & FS_PERM_READ))
        return -2;
    *out = nodes[id].content;
    return 0;
}

int fs_rm(const char* path){
    int id = resolve(path);
    if(id <= 0)
        return -1;
    for(int i=0; i<FS_MAX_NODES; i++)
        if(nodes[i].type != FS_UNUSED && nodes[i].parent == id)
            return -2;
    nodes[id].type = FS_UNUSED;
    return 0;
}

int fs_cd(const char* path){
    int id = resolve(path);
    if(id < 0 || nodes[id].type != FS_DIR)
        return -1;
    cwd = id;
    return 0;
}

int fs_stat(const char* path, int* type, size_t* size){
    int id = resolve(path);
    if(id < 0)
        return -1;
    if(type)
        *type = nodes[id].type == FS_DIR ? 1 : 2;
    if(size)
        *size = nodes[id].type == FS_FILE ? nodes[id].size : 0;
    return 0;
}

int fs_chmod(const char* path, uint8_t permissions){
    int id = resolve(path);
    if(id < 0)
        return -1;
    nodes[id].permissions = permissions & (FS_PERM_READ | FS_PERM_WRITE | FS_PERM_EXEC);
    return 0;
}

uint8_t fs_permissions(const char* path){
    int id = resolve(path);
    if(id < 0)
        return 0;
    return nodes[id].permissions;
}

int fs_can_read(const char* path){
    return (fs_permissions(path) & FS_PERM_READ) != 0;
}

int fs_can_write(const char* path){
    return (fs_permissions(path) & FS_PERM_WRITE) != 0;
}

void fs_permission_string(const char* path, char* out, size_t max){
    uint8_t p = fs_permissions(path);
    if(max == 0)
        return;
    if(max < 4){
        out[0] = 0;
        return;
    }
    out[0] = (p & FS_PERM_READ) ? 'r' : '-';
    out[1] = (p & FS_PERM_WRITE) ? 'w' : '-';
    out[2] = (p & FS_PERM_EXEC) ? 'x' : '-';
    out[3] = 0;
}

int fs_copy(const char* src, const char* dst){
    int id = resolve(src);
    if(id < 0 || nodes[id].type != FS_FILE)
        return -1;
    return fs_write(dst, nodes[id].content);
}

int fs_move(const char* src, const char* dst){
    int r = fs_copy(src, dst);
    if(r != 0)
        return r;
    return fs_rm(src);
}

int fs_rename(const char* path, const char* new_name){
    int id = resolve(path);
    if(id <= 0 || !new_name || !new_name[0])
        return -1;
    if(find_child(nodes[id].parent, new_name) >= 0)
        return -2;
    str_copy(nodes[id].name, new_name, FS_NAME_MAX);
    return 0;
}

void fs_pwd(char* out, size_t max){
    char tmp[96];
    size_t len = 0;
    int stack[16];
    int depth = 0;
    int cur = cwd;
    while(cur != 0 && depth < 16){
        stack[depth++] = cur;
        cur = nodes[cur].parent;
    }
    tmp[len++] = '/';
    for(int i=depth-1; i>=0; i--){
        const char* name = nodes[stack[i]].name;
        for(size_t j=0; name[j] && len + 1 < sizeof(tmp); j++)
            tmp[len++] = name[j];
        if(i > 0 && len + 1 < sizeof(tmp))
            tmp[len++] = '/';
    }
    tmp[len] = 0;
    str_copy(out, tmp, max);
}

void fs_ls(const char* path){
    int id = (path && path[0]) ? resolve(path) : cwd;
    if(id < 0){
        console_puts("ls: not found\n");
        return;
    }
    if(nodes[id].type == FS_FILE){
        console_puts(nodes[id].name);
        console_putc('\n');
        return;
    }
    for(int i=0; i<FS_MAX_NODES; i++){
        if(nodes[i].type != FS_UNUSED && nodes[i].parent == id && i != id){
            console_puts(nodes[i].type == FS_DIR ? "[d] " : "[f] ");
            console_puts(nodes[i].name);
            console_putc('\n');
        }
    }
}

static void tree_indent(int depth){
    for(int i=0; i<depth; i++)
        console_puts("  ");
}

static void tree_node(int id, int depth){
    tree_indent(depth);
    console_puts(nodes[id].type == FS_DIR ? "[d] " : "[f] ");
    if(id == 0)
        console_puts("/");
    else
        console_puts(nodes[id].name);
    if(nodes[id].type == FS_FILE){
        console_puts(" ");
        console_write_dec((uint32_t)nodes[id].size);
        console_puts("B");
    }
    console_putc('\n');
    if(nodes[id].type != FS_DIR)
        return;
    for(int i=0; i<FS_MAX_NODES; i++)
        if(nodes[i].type != FS_UNUSED && nodes[i].parent == id && i != id)
            tree_node(i, depth + 1);
}

void fs_tree(const char* path){
    int id = (path && path[0]) ? resolve(path) : cwd;
    if(id < 0){
        console_puts("tree: not found\n");
        return;
    }
    tree_node(id, 0);
}

int fs_child_count(const char* path){
    int id = (path && path[0]) ? resolve(path) : cwd;
    int count = 0;
    if(id < 0 || nodes[id].type != FS_DIR)
        return 0;
    for(int i=0; i<FS_MAX_NODES; i++)
        if(nodes[i].type != FS_UNUSED && nodes[i].parent == id)
            count++;
    return count;
}

int fs_child_name(const char* path, int index, char* out, size_t max, int* type){
    int id = (path && path[0]) ? resolve(path) : cwd;
    int n = 0;
    if(id < 0 || nodes[id].type != FS_DIR)
        return -1;
    for(int i=0; i<FS_MAX_NODES; i++){
        if(nodes[i].type != FS_UNUSED && nodes[i].parent == id){
            if(n == index){
                str_copy(out, nodes[i].name, max);
                if(type)
                    *type = nodes[i].type == FS_DIR ? 1 : 2;
                return 0;
            }
            n++;
        }
    }
    return -1;
}

void fs_join_path(const char* dir, const char* name, char* out, size_t max){
    size_t pos = 0;
    if(max == 0) return;
    if(!dir || !dir[0])
        dir = "/";
    while(dir[pos] && pos + 1 < max){
        out[pos] = dir[pos];
        pos++;
    }
    if(pos > 1 && out[pos - 1] != '/' && pos + 1 < max)
        out[pos++] = '/';
    if(pos == 0 && pos + 1 < max)
        out[pos++] = '/';
    for(size_t i=0; name && name[i] && pos + 1 < max; i++)
        out[pos++] = name[i];
    out[pos] = 0;
}
