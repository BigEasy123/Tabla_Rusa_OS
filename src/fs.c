#include "fs.h"
#include "console.h"

#define FS_MAX_NODES 48
#define FS_NAME_MAX 16
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
    nodes[id].content[0] = 0;
    str_copy(nodes[id].name, name, FS_NAME_MAX);
    return id;
}

void fs_init(void){
    for(int i=0; i<FS_MAX_NODES; i++)
        nodes[i].type = FS_UNUSED;
    nodes[0].type = FS_DIR;
    nodes[0].parent = 0;
    nodes[0].name[0] = 0;
    cwd = 0;
    fs_mkdir("/home");
    fs_mkdir("/bin");
    fs_write("/home/readme.txt", "Welcome to Tabla Rusa OS.\nTry: ls, cat readme.txt, edit notes.txt\n");
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
