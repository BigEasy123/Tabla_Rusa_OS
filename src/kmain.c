#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "block.h"
#include "editor.h"
#include "events.h"
#include "fb.h"
#include "fd.h"
#include "fs.h"
#include "gfx.h"
#include "gui.h"
#include "heap.h"
#include "idt.h"
#include "jobs.h"
#include "keyboard.h"
#include "lang.h"
#include "loader.h"
#include "mathlib.h"
#include "memory.h"
#include "mouse.h"
#include "net.h"
#include "object.h"
#include "paging.h"
#include "privacy.h"
#include "process.h"
#include "project.h"
#include "security.h"
#include "sched.h"
#include "service.h"
#include "shell.h"
#include "taskman.h"
#include "tests.h"
#include "timer.h"
#include "vfs.h"
#include "window.h"

#define INBUF_MAX 128
#define HISTORY_MAX 8
#define PKG_MAX 11
#define MOUNT_MAX 5
#define USER_MAX 4
#define SERVICE_MAX 6
#define EVENT_MAX 4

struct pkg_manifest {
    const char* name;
    const char* version;
    const char* summary;
    const char* compat;
    const char* files;
    int installed;
};

static struct pkg_manifest packages[PKG_MAX] = {
    {"core", "0.1", "base Tabla runtime objects", "tabla:0.1;i386;abi=flat", "/pkg/core.manifest", 1},
    {"editor", "0.1", "numbered text editor shell object", "tabla:0.1;i386;abi=flat", "/pkg/editor.manifest", 0},
    {"shellkit", "0.1", "shell aliases and inspection helpers", "tabla:0.1;i386;abi=flat", "/pkg/shellkit.manifest", 0},
    {"fs-tools", "0.1", "filesystem command helpers", "tabla:0.1;i386;abi=flat", "/pkg/fs-tools.manifest", 0},
    {"gui-core", "0.0", "framebuffer compositor foundation", "tabla:0.1;i386;abi=kernel", "/pkg/gui-core.manifest", 0},
    {"net-tcpip", "0.0", "TCP/IP stack foundation", "tabla:0.1;i386;abi=kernel", "/pkg/net-tcpip.manifest", 0},
    {"sec-core", "0.0", "capability and audit policy foundation", "tabla:0.1;i386;abi=kernel", "/pkg/sec-core.manifest", 1},
    {"net-stub", "0.0", "portable network package placeholder", "tabla:any;arch=any;abi=manifest", "/pkg/net-stub.manifest", 0},
    {"rusa-core", "0.1", "Rusa native language keywords and object model", "rusa:0.1;i386;abi=language", "/pkg/rusa-core.manifest", 1},
    {"rusa-stdlib", "0.1", "Rusa TRX standard library package", "rusa:0.1;arch=any;abi=trx", "/pkg/rusa-stdlib.manifest", 1},
    {"rusa-docs", "0.1", "Rusa examples, docs, and tutorials", "rusa:0.1;arch=any;abi=docs", "/pkg/rusa-docs.manifest", 1}
};

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
    {"pkgfs", "/pkg", "rw", "package manifest registry"}
};

static int gui_running = 0;
static int security_locked = 1;
static int net_link_up = 0;
static int tcp_listen_port = 0;
static char current_user[16] = "root";

struct user_info {
    char name[16];
    const char* caps;
    int active;
};

struct service_info {
    const char* name;
    int running;
    const char* provides;
};

struct event_rule {
    const char* name;
    const char* trigger;
    const char* action;
    int enabled;
};

static struct user_info users[USER_MAX] = {
    {"root", "all", 1},
    {"guest", "fs.read,shell.run", 1},
    {"", "", 0},
    {"", "", 0}
};

static struct service_info services[SERVICE_MAX] = {
    {"logger", 1, "audit and system logs"},
    {"network", 0, "TCP/IP stack foundation"},
    {"gui", 0, "window compositor foundation"},
    {"package", 1, "package registry manifests"},
    {"security", 1, "capability policy and audit"},
    {"events", 1, "reactive rule dispatcher"}
};

static struct event_rule event_rules[EVENT_MAX] = {
    {"download-log", "file.created:/home/downloads", "log system download event", 1},
    {"editor-focus", "process.start:editor", "window editor focus", 1},
    {"net-audit", "net.link:up", "log network link up", 1},
    {"security-audit", "security.mode:change", "log security mode change", 1}
};

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z')
        return (char)(c - 'A' + 'a');
    return c;
}

static int cmd_is(const char* cmd, const char* want){
    while(*cmd && *want){
        if(lower_char(*cmd) != *want)
            return 0;
        cmd++;
        want++;
    }
    return *cmd == 0 && *want == 0;
}

static const char* first_arg(char* arg, char** rest);

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
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

static void print_file_text(const char* text){
    console_puts(text);
    size_t len = str_len(text);
    if(len == 0 || text[len-1] != '\n')
        console_putc('\n');
}

static void history_add(const char* line){
    shell_history_add(line);
}

static void prompt(void){
    shell_set_editor_mode(editor_is_active());
    shell_prompt();
}

static void redraw_input(size_t* len){
    shell_set_editor_mode(editor_is_active());
    shell_redraw(len);
}

static void input_insert_char(char c, size_t* len){
    shell_insert_char(c, len);
}

static void input_backspace(size_t* len){
    shell_backspace(len);
}

static void console_echo_command(const char* line){
    shell_set_editor_mode(editor_is_active());
    shell_echo_command(line);
}

static void cmd_help(void){
    console_puts("Commands:\n");
    console_puts("  help        - show this help\n");
    console_puts("  echo ARG    - print ARG\n");
    console_puts("  pwd, ls, cd - navigate RAM filesystem\n");
    console_puts("  whoami      - print current user\n");
    console_puts("  uname       - print kernel/platform identity\n");
    console_puts("  history     - show recent shell commands\n");
    console_puts("  scroll A    - terminal scrollback: up/down/top/bottom/status\n");
    console_puts("  cat FILE    - print a file\n");
    console_puts("  block ARGS  - block device: status/read/write/save/load\n");
    console_puts("  fd ARGS     - file descriptors: open/read/write/close/list\n");
    console_puts("  cp/mv/stat/tree - inspect and move filesystem objects\n");
    console_puts("  touch FILE  - create an empty file\n");
    console_puts("  write F TXT - replace file contents\n");
    console_puts("  mkdir DIR   - create a directory\n");
    console_puts("  rm PATH     - remove empty dir or file\n");
    console_puts("  edit FILE   - open numbered line editor; Up/Down move, Esc exits\n");
    console_puts("  cls         - clear screen\n");
    console_puts("  about       - kernel info\n");
    console_puts("  fault       - test exception handler\n");
    console_puts("  ticks       - show PIT ticks\n");
    console_puts("  uptime      - show uptime seconds\n");
    console_puts("  sleep N     - sleep N ticks\n");
    console_puts("  mem         - show memory summary\n");
    console_puts("  mmap        - print memory map\n");
    console_puts("  heap        - show heap state\n");
    console_puts("  alloc N     - bump-allocate N bytes\n");
    console_puts("  paging      - show paging state\n");
    console_puts("  mounts      - show filesystem stack mounts\n");
    console_puts("  gui ARGS    - GUI: status/start/desktop/tab/focus/move/windows\n");
    console_puts("  window ARGS - window objects: list/focus/move/info\n");
    console_puts("  ps, kill P  - process table and stop process\n");
    console_puts("  compute A   - scientific scheduler profile/status/bench\n");
    console_puts("  math TOPIC  - linear algebra, number theory, groups, stats\n");
    console_puts("  gfx ARGS    - vector graphics: line/rect/circle/scene\n");
    console_puts("  fb ARGS     - framebuffer: status/mode/surface/mouse/blit\n");
    console_puts("  mouse ARGS  - crosshair pointer: status/set/move/click\n");
    console_puts("  keyboard A  - keyboard detect/status/locks/layout/repeat/key list\n");
    console_puts("  hardware A  - CPU/display/memory/input detection and control summary\n");
    console_puts("  job ARGS    - generic OS job scheduler: list/run/priority\n");
    console_puts("  sched ARGS  - cooperative scheduler: list/yield/wake/sleep/quantum\n");
    console_puts("  taskman A   - task manager: top/ps/jobs/services/kill/boost\n");
    console_puts("  loader A    - executable loader: list/info/run\n");
    console_puts("  lang A      - Rusa language docs: about/keywords/examples/docs\n");
    console_puts("  object A    - unified native object dispatcher\n");
    console_puts("  service A   - service manager: list/start/stop/restart/status\n");
    console_puts("  user/cap A  - users and capabilities\n");
    console_puts("  event A     - reactive event rules\n");
    console_puts("  net ARGS    - network stack: status/up/down/ip/udp/tcp\n");
    console_puts("  security A  - security: status/audit/lock/unlock\n");
    console_puts("  structure   - show current OS subsystem layout\n");
    console_puts("  log ARGS    - logs: show/write/clear system|security|network\n");
    console_puts("  regs        - show basic CPU flags\n");
    console_puts("  test        - run safe command checks\n");
    console_puts("  pkg ARGS    - package registry: list/info/install/remove/compat\n");
    console_puts("  project A   - Rusa projects: list/new/run/docs/edit\n");
    console_puts("  halt        - stop CPU\n");
    console_puts("  reboot      - keyboard-controller reboot\n");
    console_puts("Native language forms:\n");
    console_puts("  inspect memory|heap|paging|processes|filesystem|gui|security|network\n");
    console_puts("  file[\"PATH\"].read() / exists() / write(\"TEXT\") / append(\"TEXT\")\n");
    console_puts("  process[\"shell\"].trace() / process[\"editor\"].restart()\n");
    console_puts("  service[\"network\"].start() / window[\"shell\"].focus()\n");
    console_puts("  user[\"guest\"].login() / event[\"net-audit\"].emit()\n");
    console_puts("  package[\"editor\"].install() / info() / remove()\n");
    console_puts("  spawn editor [FILE]\n");
}

static void boot_full_stack(void){
    char net_up[] = "up";
    console_puts("autostart: network, GUI desktop, mouse crosshair, scheduler\n");
    net_cmd(net_up);
    gui_enter_desktop();
}

static void cmd_about(void){
    console_puts("Tabla Rusa OS 0.0.6 (i386, multiboot2)\n");
    console_puts("native shell/language runtime: tabla:0.1\n");
    console_puts("subsystems: vfs security gui-foundation tcpip-foundation audit\n");
}

static void cmd_fault(void){
    __asm__ __volatile__("ud2");
}

static void cmd_regs(void){
    uint32_t eflags, cr0, cr3;
    __asm__ __volatile__("pushf; pop %0" : "=r"(eflags));
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    console_puts("EFLAGS=");
    console_write_hex(eflags);
    console_puts(" CR0=");
    console_write_hex(cr0);
    console_puts(" CR3=");
    console_write_hex(cr3);
    console_putc('\n');
}

static int cpu_has_cpuid(void){
    uint32_t before;
    uint32_t after;
    __asm__ __volatile__(
        "pushfl\n"
        "pushfl\n"
        "popl %0\n"
        "movl %0, %1\n"
        "xorl $0x00200000, %0\n"
        "pushl %0\n"
        "popfl\n"
        "pushfl\n"
        "popl %0\n"
        "popfl\n"
        : "=&r"(after), "=&r"(before)
        :
        : "cc");
    return ((after ^ before) & 0x00200000U) != 0;
}

static void cpu_cpuid(uint32_t leaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d){
    __asm__ __volatile__("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf));
}

static void cmd_hardware(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || cmd_is(action, "status") || cmd_is(action, "all")){
        console_puts("hardware: cpu keyboard framebuffer memory pointer controls\n");
        cmd_hardware("cpu");
        cmd_hardware("keyboard");
        cmd_hardware("gpu");
        cmd_hardware("memory");
        console_puts("control surfaces: keyboard, fb, mouse, compute, privacy, net, sched\n");
    } else if(cmd_is(action, "cpu")){
        uint32_t a = 0, b = 0, c = 0, d = 0;
        char vendor[13];
        console_puts("cpu arch=i386 cpuid=");
        console_puts(cpu_has_cpuid() ? "yes" : "no");
        if(cpu_has_cpuid()){
            cpu_cpuid(0, &a, &b, &c, &d);
            ((uint32_t*)vendor)[0] = b;
            ((uint32_t*)vendor)[1] = d;
            ((uint32_t*)vendor)[2] = c;
            vendor[12] = 0;
            console_puts(" vendor=");
            console_puts(vendor);
            cpu_cpuid(1, &a, &b, &c, &d);
            console_puts(" family=");
            console_write_dec((a >> 8) & 0xF);
            console_puts(" model=");
            console_write_dec((a >> 4) & 0xF);
            console_puts(" stepping=");
            console_write_dec(a & 0xF);
            console_puts(" features=");
            if(d & (1U << 0)) console_puts("fpu ");
            if(d & (1U << 4)) console_puts("tsc ");
            if(d & (1U << 23)) console_puts("mmx ");
            if(d & (1U << 25)) console_puts("sse ");
            if(d & (1U << 26)) console_puts("sse2 ");
            if(c & (1U << 0)) console_puts("sse3 ");
            if(c & (1U << 9)) console_puts("ssse3 ");
        }
        console_putc('\n');
    } else if(cmd_is(action, "gpu") || cmd_is(action, "display") || cmd_is(action, "framebuffer")){
        console_puts("display framebuffer=");
        console_puts(fb_hardware_ready() ? "hardware" : "soft");
        console_puts(" mode=");
        console_write_dec(fb_width());
        console_putc('x');
        console_write_dec(fb_height());
        console_putc('x');
        console_write_dec(fb_bpp());
        console_puts(" pitch=");
        console_write_dec(fb_pitch());
        console_puts(" type=");
        console_write_dec(fb_type());
        console_puts(" control=fb mode|cursor|clear|saver gui wallpaper\n");
    } else if(cmd_is(action, "keyboard") || cmd_is(action, "keys")){
        keyboard_cmd("status");
        console_puts("control=keyboard caps|num|scroll|layout|repeat|keys\n");
    } else if(cmd_is(action, "memory") || cmd_is(action, "mem")){
        console_puts("memory total_kib=");
        console_write_dec(memory_total_kib());
        console_puts(" usable_kib=");
        console_write_dec(memory_usable_kib());
        console_puts(" entries=");
        console_write_dec(memory_map_entries());
        console_puts(" control=mmap heap paging\n");
    } else if(cmd_is(action, "control")){
        console_puts("hardware controls:\n");
        console_puts("  keyboard caps on|off, keyboard num on|off, keyboard repeat fast\n");
        console_puts("  fb mode W H BPP, fb cursor dot|cross|target, gui wallpaper MODE\n");
        console_puts("  mouse set X Y, mouse click, privacy network on|off, net up|down\n");
        console_puts("  compute vector|bench, sched quantum TASK N, service start|stop NAME\n");
    } else {
        console_puts("usage: hardware status|cpu|gpu|keyboard|memory|control\n");
    }
}

static inline void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port){
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static void cmd_reboot(void){
    console_puts("Rebooting...\n");
    while(inb(0x64) & 0x02) {}
    outb(0x64, 0xFE);
    for(;;) __asm__ __volatile__("hlt");
}

static void __attribute__((unused)) cmd_test(void){
    console_puts("console: ok\n");
    console_puts("timer: ticks=");
    console_write_dec(timer_ticks());
    console_putc('\n');
    console_puts("memory: entries=");
    console_write_dec(memory_map_entries());
    console_puts(" usable_kib=");
    console_write_dec(memory_usable_kib());
    console_putc('\n');
    console_puts("heap: used=");
    console_write_dec(heap_bytes_used());
    console_puts(" bytes\n");
    console_puts("paging: ");
    console_puts(paging_is_enabled() ? "on\n" : "off\n");
    console_puts("keyboard: ok if you typed this command\n");
}

static void cmd_uname(void){
    console_puts("TablaRusaOS 0.0.6 i386 multiboot2 tabla:0.1\n");
}

static void cmd_whoami(void){
    console_puts(security_current_user());
    console_putc('\n');
}

static void cmd_history(void){
    shell_history_cmd();
}

static struct service_info* service_find(const char* name){
    for(size_t i=0; i<SERVICE_MAX; i++)
        if(cmd_is(services[i].name, name))
            return &services[i];
    return 0;
}

static struct user_info* user_find(const char* name){
    for(size_t i=0; i<USER_MAX; i++)
        if(users[i].active && cmd_is(users[i].name, name))
            return &users[i];
    return 0;
}

static const char* log_path_for(const char* name){
    if(cmd_is(name, "security")) return "/var/log/security.log";
    if(cmd_is(name, "network") || cmd_is(name, "net")) return "/var/log/network.log";
    return "/var/log/system.log";
}

static void os_log(const char* name, const char* message){
    fs_append_line(log_path_for(name), message);
}

static void event_emit(const char* trigger){
    for(size_t i=0; i<EVENT_MAX; i++){
        if(event_rules[i].enabled && cmd_is(event_rules[i].trigger, trigger)){
            os_log("system", event_rules[i].action);
            console_puts("event ");
            console_puts(event_rules[i].name);
            console_puts(" -> ");
            console_puts(event_rules[i].action);
            console_putc('\n');
        }
    }
}

static void __attribute__((unused)) cmd_mounts(void){
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

static void __attribute__((unused)) cmd_gui(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || cmd_is(action, "status")){
        console_puts("gui=");
        console_puts(gui_running ? "running" : "stopped");
        console_puts(" backend=vga-text planned=framebuffer\n");
        console_puts("objects: compositor[planned] window-manager[planned] event-bus[planned]\n");
    } else if(cmd_is(action, "start")){
        gui_running = 1;
        service_find("gui")->running = 1;
        process_set_running("gui", 1);
        os_log("system", "gui: compositor foundation started");
        console_puts("gui: compositor foundation started\n");
    } else if(cmd_is(action, "stop")){
        gui_running = 0;
        service_find("gui")->running = 0;
        process_set_running("gui", 0);
        os_log("system", "gui: compositor foundation stopped");
        console_puts("gui: stopped\n");
    } else if(cmd_is(action, "windows")){
        window_list();
    } else {
        console_puts("usage: gui status | start | stop | windows\n");
    }
}

static void __attribute__((unused)) cmd_security(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || cmd_is(action, "status")){
        console_puts("secure_mode=");
        console_puts(security_locked ? "on" : "permissive");
        console_puts(" user=root ring=0 capabilities=all\n");
        console_puts("policy: deny unsigned kernel packages, audit privileged actions\n");
    } else if(cmd_is(action, "audit")){
        const char* text;
        if(fs_read("/var/log/security.log", &text) == 0) print_file_text(text);
    } else if(cmd_is(action, "lock")){
        security_locked = 1;
        os_log("security", "security: secure_mode enabled");
        event_emit("security.mode:change");
        console_puts("security: secure_mode enabled\n");
    } else if(cmd_is(action, "unlock")){
        security_locked = 0;
        os_log("security", "security: permissive mode requested by root");
        event_emit("security.mode:change");
        console_puts("security: permissive mode enabled\n");
    } else {
        console_puts("usage: security status | audit | lock | unlock\n");
    }
}

static void __attribute__((unused)) cmd_net(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || cmd_is(action, "status")){
        console_puts("lo: up ip=127.0.0.1/8\n");
        console_puts("eth0: ");
        console_puts(net_link_up ? "up" : "down");
        console_puts(" ip=0.0.0.0/0 driver=stub\n");
        console_puts("tcp: ");
        console_puts(tcp_listen_port ? "listening port " : "closed\n");
        if(tcp_listen_port){
            console_write_dec((uint32_t)tcp_listen_port);
            console_putc('\n');
        }
    } else if(cmd_is(action, "up")){
        net_link_up = 1;
        service_find("network")->running = 1;
        process_set_running("network", 1);
        os_log("network", "net: eth0 marked up");
        event_emit("net.link:up");
        console_puts("net: eth0 up (stub link)\n");
    } else if(cmd_is(action, "down")){
        net_link_up = 0;
        tcp_listen_port = 0;
        service_find("network")->running = 0;
        process_set_running("network", 0);
        os_log("network", "net: eth0 marked down");
        console_puts("net: eth0 down\n");
    } else if(cmd_is(action, "ip")){
        console_puts("IPv4 stack: loopback active, ARP table empty, routing default unavailable\n");
    } else if(cmd_is(action, "tcp")){
        uint32_t port = parse_u32(rest);
        if(port == 0){
            console_puts("TCP states: CLOSED LISTEN SYN-SENT ESTABLISHED FIN-WAIT\n");
            console_puts("usage: net tcp PORT   (records a listening stub socket)\n");
        } else {
            tcp_listen_port = (int)port;
            os_log("network", "net: tcp listen socket registered");
            console_puts("tcp: listening stub on port ");
            console_write_dec(port);
            console_putc('\n');
        }
    } else {
        console_puts("usage: net status | up | down | ip | tcp [PORT]\n");
    }
}

static void cmd_log(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    const char* path = log_path_for(name);
    if(action[0] == 0 || cmd_is(action, "show")){
        const char* text;
        if(fs_read(path, &text) == 0) print_file_text(text);
        else console_puts("log: not found\n");
    } else if(cmd_is(action, "write")){
        if(name[0] == 0 || rest[0] == 0){
            console_puts("usage: log write system|security|network MESSAGE\n");
            return;
        }
        os_log(name, rest);
        console_puts("log: written\n");
    } else if(cmd_is(action, "clear")){
        fs_write(path, "");
        console_puts("log: cleared\n");
    } else {
        console_puts("usage: log show [system|security|network] | write NAME MSG | clear NAME\n");
    }
}

static void cmd_ps(void){
    process_list();
}

static void cmd_compute(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || cmd_is(action, "status")){
        process_compute_report();
        return;
    }
    if(cmd_is(action, "start")){
        process_set_running("compute", 1);
        process_set_compute("compute", "scientific", 95, 0);
        process_tick("compute", 1);
        os_log("system", "compute: scientific worker started");
        console_puts("compute: scientific worker started priority=95 class=scientific\n");
    } else if(cmd_is(action, "stop")){
        process_set_running("compute", 0);
        os_log("system", "compute: scientific worker stopped");
        console_puts("compute: stopped\n");
    } else if(cmd_is(action, "vector")){
        process_set_running("compute", 1);
        process_set_compute("compute", "vector", 98, 0);
        process_tick("compute", 4);
        os_log("system", "compute: vector profile selected");
        console_puts("compute: vector profile selected priority=98\n");
    } else if(cmd_is(action, "bench")){
        process_set_running("compute", 1);
        process_set_compute("compute", "scientific", 99, 0);
        process_tick("compute", 16);
        console_puts("bench: simulated matrix workload ticks+=16\n");
    } else {
        console_puts("usage: compute status | start | stop | vector | bench\n");
    }
}

static void cmd_kill(char* arg){
    if(arg[0] == 0){
        console_puts("usage: kill PROCESS\n");
        return;
    }
    if(process_stop(arg) != 0){
        console_puts("kill: protected or unknown process\n");
        return;
    }
    if(cmd_is(arg, "editor") && editor_is_active())
        editor_close();
    os_log("system", "process: stopped by shell");
    console_puts("stopped process ");
    console_puts(arg);
    console_putc('\n');
}

static void service_set(struct service_info* svc, int running){
    svc->running = running;
    if(cmd_is(svc->name, "network")){
        net_link_up = running;
        process_set_running("network", running);
    } else if(cmd_is(svc->name, "gui")){
        gui_running = running;
        process_set_running("gui", running);
    }
}

static void __attribute__((unused)) cmd_service(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || cmd_is(action, "list")){
        for(size_t i=0; i<SERVICE_MAX; i++){
            console_puts(services[i].running ? "[on]  " : "[off] ");
            console_puts(services[i].name);
            console_puts(" - ");
            console_puts(services[i].provides);
            console_putc('\n');
        }
        return;
    }
    struct service_info* svc = service_find(name);
    if(svc == 0){
        console_puts("service: not found\n");
        return;
    }
    if(cmd_is(action, "start")){
        service_set(svc, 1);
        os_log("system", "service: started");
        console_puts("started ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(cmd_is(action, "stop")){
        service_set(svc, 0);
        os_log("system", "service: stopped");
        console_puts("stopped ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(cmd_is(action, "restart")){
        service_set(svc, 0);
        service_set(svc, 1);
        os_log("system", "service: restarted");
        console_puts("restarted ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(cmd_is(action, "status")){
        console_puts(svc->name);
        console_puts(svc->running ? " running - " : " stopped - ");
        console_puts(svc->provides);
        console_putc('\n');
    } else {
        console_puts("usage: service list | start NAME | stop NAME | restart NAME | status NAME\n");
    }
}

static void cmd_window(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || cmd_is(action, "list")){
        window_list();
        return;
    }
    struct window_info* win = window_find(name);
    if(win == 0){
        console_puts("window: not found\n");
        return;
    }
    if(cmd_is(action, "focus")){
        window_focus(win->name);
        console_puts("focused window ");
        console_puts(win->name);
        console_putc('\n');
    } else if(cmd_is(action, "move")){
        char* yarg;
        uint32_t x = parse_u32(rest);
        first_arg(rest, &yarg);
        uint32_t y = parse_u32(yarg);
        window_move(win->name, (int)x, (int)y);
        console_puts("moved window ");
        console_puts(win->name);
        console_putc('\n');
    } else if(cmd_is(action, "info")){
        console_puts(win->name);
        console_puts(win->focused ? " focused " : " ");
        console_puts(win->surface);
        console_putc('\n');
    } else {
        console_puts("usage: window list | focus NAME | move NAME X Y | info NAME\n");
    }
}

static void __attribute__((unused)) cmd_user(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || cmd_is(action, "list")){
        for(size_t i=0; i<USER_MAX; i++)
            if(users[i].active){
                console_puts(cmd_is(users[i].name, current_user) ? "* " : "  ");
                console_puts(users[i].name);
                console_puts(" caps=");
                console_puts(users[i].caps);
                console_putc('\n');
            }
        return;
    }
    if(cmd_is(action, "add")){
        for(size_t i=0; i<USER_MAX; i++){
            if(!users[i].active){
                str_copy(users[i].name, name, sizeof(users[i].name));
                users[i].caps = "fs.read,shell.run";
                users[i].active = 1;
                os_log("security", "user: added account");
                console_puts("added user ");
                console_puts(name);
                console_putc('\n');
                return;
            }
        }
        console_puts("user: table full\n");
    } else if(cmd_is(action, "login")){
        if(user_find(name) == 0){
            console_puts("login: unknown user\n");
            return;
        }
        str_copy(current_user, name, sizeof(current_user));
        os_log("security", "user: login");
        console_puts("logged in as ");
        console_puts(current_user);
        console_putc('\n');
    } else {
        console_puts("usage: user list | add NAME | login NAME\n");
    }
}

static void __attribute__((unused)) cmd_cap(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    struct user_info* user = user_find(name);
    if(action[0] == 0 || cmd_is(action, "list")){
        for(size_t i=0; i<USER_MAX; i++)
            if(users[i].active){
                console_puts(users[i].name);
                console_puts(" caps=");
                console_puts(users[i].caps);
                console_putc('\n');
            }
        return;
    }
    if(user == 0){
        console_puts("cap: unknown user\n");
        return;
    }
    if(cmd_is(action, "grant")){
        user->caps = "fs.read,fs.write,shell.run,service.control";
        os_log("security", "capability: granted elevated set");
        console_puts("granted elevated capabilities to ");
        console_puts(user->name);
        console_putc('\n');
    } else if(cmd_is(action, "drop")){
        user->caps = "fs.read,shell.run";
        os_log("security", "capability: dropped to default set");
        console_puts("dropped capabilities for ");
        console_puts(user->name);
        console_putc('\n');
    } else {
        console_puts("usage: cap list | grant USER | drop USER\n");
    }
}

static void __attribute__((unused)) cmd_event(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    const char* name = first_arg(rest, &rest);
    if(action[0] == 0 || cmd_is(action, "list")){
        for(size_t i=0; i<EVENT_MAX; i++){
            console_puts(event_rules[i].enabled ? "[on]  " : "[off] ");
            console_puts(event_rules[i].name);
            console_puts(" when ");
            console_puts(event_rules[i].trigger);
            console_puts(" -> ");
            console_puts(event_rules[i].action);
            console_putc('\n');
        }
        return;
    }
    for(size_t i=0; i<EVENT_MAX; i++){
        if(cmd_is(event_rules[i].name, name)){
            if(cmd_is(action, "enable")) event_rules[i].enabled = 1;
            else if(cmd_is(action, "disable")) event_rules[i].enabled = 0;
            else if(cmd_is(action, "emit")) event_emit(event_rules[i].trigger);
            else console_puts("usage: event list | enable NAME | disable NAME | emit NAME\n");
            return;
        }
    }
    console_puts("event: not found\n");
}

static void cmd_stat(char* arg){
    int type;
    size_t size;
    if(arg[0] == 0 || fs_stat(arg, &type, &size) != 0){
        console_puts("stat: not found\n");
        return;
    }
    console_puts(type == 1 ? "directory " : "file ");
    console_puts(arg);
    console_puts(" size=");
    console_write_dec((uint32_t)size);
    console_putc('\n');
}

static const char* first_arg(char* arg, char** rest){
    while(is_space(*arg)) arg++;
    char* start = arg;
    while(*arg && !is_space(*arg)) arg++;
    if(*arg){
        *arg = 0;
        arg++;
    }
    while(is_space(*arg)) arg++;
    *rest = arg;
    return start;
}

static void fs_status(int r, const char* what){
    if(r == 0) return;
    console_puts(what);
    if(r == -1) console_puts(": not found\n");
    else if(r == -2) console_puts(": wrong type or not empty\n");
    else if(r == -3) console_puts(": filesystem full\n");
    else console_puts(": failed\n");
}

static int can_modify_path(const char* path, const char* op){
    if(!vfs_can_write(path)){
        console_puts(op);
        console_puts(": read-only filesystem namespace\n");
        security_audit("vfs: blocked write to read-only namespace");
        return 0;
    }
    if(!security_can_write(path)){
        console_puts(op);
        console_puts(": permission denied\n");
        return 0;
    }
    events_emit("fs.write");
    return 1;
}

static struct pkg_manifest* pkg_find(const char* name){
    for(size_t i=0; i<PKG_MAX; i++)
        if(cmd_is(packages[i].name, name))
            return &packages[i];
    return 0;
}

static int pkg_is_kernel_facing(struct pkg_manifest* pkg){
    return cmd_is(pkg->name, "gui-core") || cmd_is(pkg->name, "net-tcpip");
}

static const char* pkg_trust(struct pkg_manifest* pkg){
    if(cmd_is(pkg->name, "core") || cmd_is(pkg->name, "sec-core"))
        return "trusted";
    if(pkg_is_kernel_facing(pkg))
        return "unsigned-kernel";
    return "userland";
}

static void pkg_write_manifest(struct pkg_manifest* pkg){
    char text[512];
    size_t pos = 0;
    const char* fields[] = {
        "name=", pkg->name, "\n",
        "version=", pkg->version, "\n",
        "summary=", pkg->summary, "\n",
        "compat=", pkg->compat, "\n",
        "trust=", pkg_trust(pkg), "\n",
        "state=", pkg->installed ? "installed" : "available", "\n",
        0
    };
    for(size_t f=0; fields[f]; f++)
        for(size_t i=0; fields[f][i] && pos + 1 < sizeof(text); i++)
            text[pos++] = fields[f][i];
    text[pos] = 0;
    fs_write(pkg->files, text);
}

static void pkg_print(struct pkg_manifest* pkg){
    console_puts(pkg->installed ? "[installed] " : "[available] ");
    console_puts(pkg->name);
    console_puts(" ");
    console_puts(pkg->version);
    console_puts(" - ");
    console_puts(pkg->summary);
    console_putc('\n');
}

static void pkg_info(struct pkg_manifest* pkg){
    console_puts("name: ");
    console_puts(pkg->name);
    console_puts("\nversion: ");
    console_puts(pkg->version);
    console_puts("\nsummary: ");
    console_puts(pkg->summary);
    console_puts("\ncompat: ");
    console_puts(pkg->compat);
    console_puts("\ntrust: ");
    console_puts(pkg_trust(pkg));
    console_puts("\nmanifest: ");
    console_puts(pkg->files);
    console_puts("\nstate: ");
    console_puts(pkg->installed ? "installed\n" : "available\n");
}

static void pkg_bootstrap(void){
    fs_mkdir("/pkg");
    for(size_t i=0; i<PKG_MAX; i++)
        pkg_write_manifest(&packages[i]);
}

static void pkg_install(struct pkg_manifest* pkg){
    if(pkg->installed){
        console_puts("pkg: already installed\n");
        return;
    }
    if(pkg_is_kernel_facing(pkg) && !security_can_install_kernel_package(pkg->name)){
        console_puts("pkg: blocked by secure_mode; run security unlock for experimental kernel packages\n");
        return;
    }
    pkg->installed = 1;
    pkg_write_manifest(pkg);
    if(cmd_is(pkg->name, "rusa-core") || cmd_is(pkg->name, "rusa-docs") || cmd_is(pkg->name, "rusa-stdlib"))
        lang_init();
    if(cmd_is(pkg->name, "gui-core"))
        fb_init();
    if(cmd_is(pkg->name, "net-tcpip"))
        net_init();
    console_puts("installed ");
    console_puts(pkg->name);
    console_puts(" for ");
    console_puts(pkg->compat);
    console_putc('\n');
}

static void pkg_remove(struct pkg_manifest* pkg){
    if(cmd_is(pkg->name, "core")){
        console_puts("pkg: core is required\n");
        return;
    }
    if(!pkg->installed){
        console_puts("pkg: not installed\n");
        return;
    }
    pkg->installed = 0;
    pkg_write_manifest(pkg);
    console_puts("removed ");
    console_puts(pkg->name);
    console_putc('\n');
}

static void cmd_pkg(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || cmd_is(action, "list")){
        for(size_t i=0; i<PKG_MAX; i++)
            pkg_print(&packages[i]);
        return;
    }
    if(cmd_is(action, "compat")){
        console_puts("host: tabla:0.1;i386;abi=flat\n");
        console_puts("portable manifest target: tabla:any;arch=any;abi=manifest\n");
        return;
    }
    if(cmd_is(action, "help")){
        console_puts("usage: pkg list | info NAME | install NAME | remove NAME | compat\n");
        return;
    }
    const char* name = first_arg(rest, &rest);
    struct pkg_manifest* pkg = pkg_find(name);
    if(name[0] == 0){
        console_puts("pkg: missing package name\n");
        return;
    }
    if(pkg == 0){
        console_puts("pkg: package not found\n");
        return;
    }
    if(cmd_is(action, "info")) pkg_info(pkg);
    else if(cmd_is(action, "install")) pkg_install(pkg);
    else if(cmd_is(action, "remove") || cmd_is(action, "rm")) pkg_remove(pkg);
    else console_puts("pkg: unknown action\n");
}

static const char* skip_space_const(const char* s){
    while(is_space(*s)) s++;
    return s;
}

static int str_starts(const char* s, const char* prefix){
    while(*prefix){
        if(*s++ != *prefix++)
            return 0;
    }
    return 1;
}

static int read_quoted(const char** cursor, char* out, size_t max){
    const char* p = skip_space_const(*cursor);
    size_t i = 0;
    if(*p != '"')
        return 0;
    p++;
    while(*p && *p != '"'){
        if(i + 1 < max)
            out[i++] = *p;
        p++;
    }
    if(*p != '"')
        return 0;
    out[i] = 0;
    *cursor = p + 1;
    return 1;
}

static int parse_indexed_object(const char* line, const char* object, char* key, size_t key_max, const char** rest){
    const char* p = skip_space_const(line);
    if(!str_starts(p, object))
        return 0;
    p += str_len(object);
    p = skip_space_const(p);
    if(*p != '[')
        return 0;
    p++;
    if(!read_quoted(&p, key, key_max))
        return 0;
    p = skip_space_const(p);
    if(*p != ']')
        return 0;
    *rest = p + 1;
    return 1;
}

static int parse_call_text_arg(const char* rest, const char* call, char* arg, size_t arg_max){
    const char* p = skip_space_const(rest);
    if(!str_starts(p, call))
        return 0;
    p += str_len(call);
    if(!read_quoted(&p, arg, arg_max))
        return 0;
    p = skip_space_const(p);
    return str_starts(p, ")");
}

static void native_inspect(const char* target){
    if(cmd_is(target, "memory")){
        console_puts("memory total_kib=");
        console_write_dec(memory_total_kib());
        console_puts(" usable_kib=");
        console_write_dec(memory_usable_kib());
        console_puts(" map_entries=");
        console_write_dec(memory_map_entries());
        console_putc('\n');
    } else if(cmd_is(target, "heap")){
        console_puts("heap start=");
        console_write_hex(heap_start());
        console_puts(" next=");
        console_write_hex(heap_current());
        console_puts(" used=");
        console_write_dec(heap_bytes_used());
        console_puts(" bytes\n");
    } else if(cmd_is(target, "paging")){
        console_puts("paging enabled=");
        console_puts(paging_is_enabled() ? "true" : "false");
        console_puts(" directory=");
        console_write_hex(paging_directory_addr());
        console_putc('\n');
    } else if(cmd_is(target, "processes") || cmd_is(target, "process")){
        cmd_ps();
    } else if(cmd_is(target, "compute") || cmd_is(target, "scientific")){
        process_compute_report();
    } else if(cmd_is(target, "filesystem") || cmd_is(target, "fs")){
        vfs_mounts_cmd();
    } else if(cmd_is(target, "gui")){
        char arg[] = "status";
        gui_cmd(arg);
    } else if(cmd_is(target, "security")){
        char arg[] = "status";
        security_cmd(arg);
    } else if(cmd_is(target, "network") || cmd_is(target, "net")){
        char arg[] = "status";
        net_cmd(arg);
    } else if(cmd_is(target, "logs") || cmd_is(target, "log")){
        char arg[] = "show system";
        cmd_log(arg);
    } else {
        console_puts("inspect: unknown target\n");
    }
}

static void native_spawn(const char* arg){
    char tmp[INBUF_MAX];
    char* rest;
    str_copy(tmp, arg, sizeof(tmp));
    const char* name = first_arg(tmp, &rest);
    if(cmd_is(name, "editor")){
        const char* path = rest[0] ? rest : "notes.txt";
        console_puts("spawned process[\"editor\"] path=");
        console_puts(path);
        console_putc('\n');
        editor_open(path);
    } else if(cmd_is(name, "logger")){
        console_puts("spawned process[\"logger\"] state=virtual\n");
    } else {
        console_puts("spawn: unknown process\n");
    }
}

static int native_file_expr(const char* line){
    char path[64];
    char text[INBUF_MAX];
    const char* rest;
    const char* body;
    const char* current;
    if(!parse_indexed_object(line, "file", path, sizeof(path), &rest))
        return 0;
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("file: expected method call\n");
        return 1;
    }
    body = rest + 1;
    if(str_starts(body, "read()")){
        if(fs_read(path, &current) == 0) print_file_text(current);
        else console_puts("file.read: not found\n");
    } else if(str_starts(body, "exists()")){
        console_puts(fs_read(path, &current) == 0 ? "true\n" : "false\n");
    } else if(str_starts(body, "stat()")){
        cmd_stat(path);
    } else if(parse_call_text_arg(body, "write(", text, sizeof(text))){
        fs_status(fs_write(path, text), "file.write");
    } else if(parse_call_text_arg(body, "append(", text, sizeof(text))){
        fs_status(fs_append_line(path, text), "file.append");
    } else {
        console_puts("file: unknown method\n");
    }
    return 1;
}

static int native_process_expr(const char* line){
    char name[32];
    const char* rest;
    if(!parse_indexed_object(line, "process", name, sizeof(name), &rest))
        return 0;
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("process: expected method call\n");
        return 1;
    }
    rest++;
    if(str_starts(rest, "trace()")){
        struct process_info* proc = process_find(name);
        if(proc){
            console_puts("process[\"");
            console_puts(name);
            console_puts("\"] state=");
            console_puts(proc->running ? "running" : "stopped");
            console_puts(" pid=");
            console_write_dec(proc->pid);
            console_puts(" priority=");
            console_write_dec(proc->priority);
            console_puts(" class=");
            console_puts(proc->workload);
            console_putc('\n');
        } else {
            console_puts("process.trace: not found\n");
        }
    } else if(str_starts(rest, "restart()")){
        if(cmd_is(name, "editor")){
            const char* path = editor_path_current();
            editor_open(path[0] ? path : "notes.txt");
            console_puts("process[\"editor\"] restarted\n");
        } else if(cmd_is(name, "shell")){
            size_t len = 0;
            shell_reset_input(&len);
            console_puts("process[\"shell\"] restarted\n");
        } else {
            console_puts("process.restart: unsupported process\n");
        }
    } else {
        console_puts("process: unknown method\n");
    }
    return 1;
}

static int native_package_expr(const char* line){
    char name[32];
    const char* rest;
    struct pkg_manifest* pkg;
    if(!parse_indexed_object(line, "package", name, sizeof(name), &rest))
        return 0;
    pkg = pkg_find(name);
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("package: expected method call\n");
        return 1;
    }
    if(pkg == 0){
        console_puts("package: not found\n");
        return 1;
    }
    rest++;
    if(str_starts(rest, "info()")) pkg_info(pkg);
    else if(str_starts(rest, "install()")) pkg_install(pkg);
    else if(str_starts(rest, "remove()")) pkg_remove(pkg);
    else if(str_starts(rest, "compat()")) {
        console_puts(pkg->compat);
        console_putc('\n');
    } else {
        console_puts("package: unknown method\n");
    }
    return 1;
}

static int native_dir_expr(const char* line){
    char path[64];
    const char* rest;
    if(!parse_indexed_object(line, "dir", path, sizeof(path), &rest))
        return 0;
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("dir: expected method call\n");
        return 1;
    }
    rest++;
    if(str_starts(rest, "list()")) fs_ls(path);
    else if(str_starts(rest, "tree()")) fs_tree(path);
    else if(str_starts(rest, "stat()")) cmd_stat(path);
    else console_puts("dir: unknown method\n");
    return 1;
}

static int native_service_expr(const char* line){
    char name[32];
    const char* rest;
    struct service_info* svc;
    if(!parse_indexed_object(line, "service", name, sizeof(name), &rest))
        return 0;
    svc = service_find(name);
    if(svc == 0){
        console_puts("service: not found\n");
        return 1;
    }
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("service: expected method call\n");
        return 1;
    }
    rest++;
    if(str_starts(rest, "start()")){
        service_set(svc, 1);
        console_puts("started ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(str_starts(rest, "stop()")){
        service_set(svc, 0);
        console_puts("stopped ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(str_starts(rest, "restart()")){
        service_set(svc, 0);
        service_set(svc, 1);
        console_puts("restarted ");
        console_puts(svc->name);
        console_putc('\n');
    } else if(str_starts(rest, "status()")){
        console_puts(svc->running ? "running\n" : "stopped\n");
    } else {
        console_puts("service: unknown method\n");
    }
    return 1;
}

static int native_window_expr(const char* line){
    char name[32];
    char text[32];
    const char* rest;
    struct window_info* win;
    if(!parse_indexed_object(line, "window", name, sizeof(name), &rest))
        return 0;
    win = window_find(name);
    if(win == 0){
        console_puts("window: not found\n");
        return 1;
    }
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("window: expected method call\n");
        return 1;
    }
    rest++;
    if(str_starts(rest, "focus()")){
        window_focus(win->name);
        console_puts("focused ");
        console_puts(win->name);
        console_putc('\n');
    } else if(str_starts(rest, "info()")){
        console_puts(win->name);
        console_puts(win->focused ? " focused\n" : " unfocused\n");
    } else if(parse_call_text_arg(rest, "move(", text, sizeof(text))){
        win->x = (int)parse_u32(text);
        win->y = 0;
        console_puts("window moved on x axis\n");
    } else {
        console_puts("window: unknown method\n");
    }
    return 1;
}

static int native_mount_expr(const char* line){
    char path[32];
    const char* rest;
    if(!parse_indexed_object(line, "mount", path, sizeof(path), &rest))
        return 0;
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("mount: expected method call\n");
        return 1;
    }
    rest++;
    if(str_starts(rest, "status()")){
        for(size_t i=0; i<MOUNT_MAX; i++)
            if(cmd_is(mounts[i].path, path)){
                console_puts(mounts[i].fs);
                console_puts(" ");
                console_puts(mounts[i].mode);
                console_puts(" ");
                console_puts(mounts[i].note);
                console_putc('\n');
                return 1;
            }
        console_puts("mount: not found\n");
    } else {
        console_puts("mount: unknown method\n");
    }
    return 1;
}

static int native_user_expr(const char* line){
    char name[32];
    const char* rest;
    struct user_info* user;
    if(!parse_indexed_object(line, "user", name, sizeof(name), &rest))
        return 0;
    user = user_find(name);
    if(user == 0){
        console_puts("user: not found\n");
        return 1;
    }
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("user: expected method call\n");
        return 1;
    }
    rest++;
    if(str_starts(rest, "login()")){
        str_copy(current_user, user->name, sizeof(current_user));
        os_log("security", "user: native login");
        console_puts("logged in as ");
        console_puts(current_user);
        console_putc('\n');
    } else if(str_starts(rest, "caps()")){
        console_puts(user->caps);
        console_putc('\n');
    } else {
        console_puts("user: unknown method\n");
    }
    return 1;
}

static int native_event_expr(const char* line){
    char name[32];
    const char* rest;
    if(!parse_indexed_object(line, "event", name, sizeof(name), &rest))
        return 0;
    rest = skip_space_const(rest);
    if(*rest != '.'){
        console_puts("event: expected method call\n");
        return 1;
    }
    rest++;
    for(size_t i=0; i<EVENT_MAX; i++){
        if(cmd_is(event_rules[i].name, name)){
            if(str_starts(rest, "emit()")) event_emit(event_rules[i].trigger);
            else if(str_starts(rest, "enable()")) event_rules[i].enabled = 1;
            else if(str_starts(rest, "disable()")) event_rules[i].enabled = 0;
            else if(str_starts(rest, "info()")){
                console_puts(event_rules[i].trigger);
                console_puts(" -> ");
                console_puts(event_rules[i].action);
                console_putc('\n');
            } else {
                console_puts("event: unknown method\n");
            }
            return 1;
        }
    }
    console_puts("event: not found\n");
    return 1;
}

static int native_eval(char* line){
    char* p = line;
    while(is_space(*p)) p++;
    if(object_eval(p))
        return 1;
    if(str_starts(p, "inspect ")){
        native_inspect(skip_space_const(p + 8));
        return 1;
    }
    if(str_starts(p, "spawn ")){
        native_spawn(skip_space_const(p + 6));
        return 1;
    }
    if(native_file_expr(p))
        return 1;
    if(native_process_expr(p))
        return 1;
    if(native_package_expr(p))
        return 1;
    if(native_dir_expr(p))
        return 1;
    if(native_service_expr(p))
        return 1;
    if(native_window_expr(p))
        return 1;
    if(native_mount_expr(p))
        return 1;
    if(native_user_expr(p))
        return 1;
    if(native_event_expr(p))
        return 1;
    return 0;
}

static void kernel_shell_dispatch(char* line){
    char* p=line;
    while(is_space(*p)) p++;
    if(native_eval(p))
        return;
    char* cmd=p;
    while(*p && !is_space(*p)) p++;
    if(*p){
        *p = 0;
        p++;
    }
    while(is_space(*p)) p++;
    char* arg=p;

    if(cmd_is(cmd,"help") || cmd_is(cmd,"?")) cmd_help();
    else if(cmd_is(cmd,"about") || cmd_is(cmd,"ver")) cmd_about();
    else if(cmd_is(cmd,"uname")) cmd_uname();
    else if(cmd_is(cmd,"whoami")) cmd_whoami();
    else if(cmd_is(cmd,"history")) cmd_history();
    else if(cmd_is(cmd,"scroll") || cmd_is(cmd,"term")) console_scroll_cmd(arg);
    else if(cmd_is(cmd,"cls") || cmd_is(cmd,"clear")) console_clear_output();
    else if(cmd_is(cmd,"fault") || cmd_is(cmd,"panic")) cmd_fault();
    else if(cmd_is(cmd,"echo")) { console_puts(arg); console_putc('\n'); }
    else if(cmd_is(cmd,"pwd")) { char cwd[64]; fs_pwd(cwd, sizeof(cwd)); console_puts(cwd); console_putc('\n'); }
    else if(cmd_is(cmd,"ls") || cmd_is(cmd,"dir")) fs_ls(arg);
    else if(cmd_is(cmd,"cd")) { if(arg[0] == 0) fs_cd("/home"); else fs_status(fs_cd(arg), "cd"); }
    else if(cmd_is(cmd,"cat") || cmd_is(cmd,"type")) {
        const char* text;
        if(arg[0] == 0) console_puts("usage: cat FILE\n");
        else if(fs_read(arg, &text) == 0) print_file_text(text);
        else console_puts("cat: not found\n");
    }
    else if(cmd_is(cmd,"block") || cmd_is(cmd,"disk")) block_cmd(arg);
    else if(cmd_is(cmd,"fd") || cmd_is(cmd,"fds")) fd_cmd(arg);
    else if(cmd_is(cmd,"stat")) cmd_stat(arg);
    else if(cmd_is(cmd,"tree")) fs_tree(arg);
    else if(cmd_is(cmd,"cp") || cmd_is(cmd,"copy")) {
        char* rest;
        const char* src = first_arg(arg, &rest);
        const char* dst = first_arg(rest, &rest);
        if(src[0] == 0 || dst[0] == 0) console_puts("usage: cp SRC DST\n");
        else if(can_modify_path(dst, "cp")) fs_status(fs_copy(src, dst), "cp");
    }
    else if(cmd_is(cmd,"mv") || cmd_is(cmd,"move")) {
        char* rest;
        const char* src = first_arg(arg, &rest);
        const char* dst = first_arg(rest, &rest);
        if(src[0] == 0 || dst[0] == 0) console_puts("usage: mv SRC DST\n");
        else if(can_modify_path(src, "mv") && can_modify_path(dst, "mv")) fs_status(fs_move(src, dst), "mv");
    }
    else if(cmd_is(cmd,"touch")) {
        if(arg[0] == 0) console_puts("usage: touch FILE\n");
        else if(can_modify_path(arg, "touch")) fs_status(fs_touch(arg), "touch");
    }
    else if(cmd_is(cmd,"mkdir")) {
        if(arg[0] == 0) console_puts("usage: mkdir DIR\n");
        else if(can_modify_path(arg, "mkdir")) fs_status(fs_mkdir(arg), "mkdir");
    }
    else if(cmd_is(cmd,"rm") || cmd_is(cmd,"del")) {
        if(arg[0] == 0) console_puts("usage: rm PATH\n");
        else if(can_modify_path(arg, "rm")) fs_status(fs_rm(arg), "rm");
    }
    else if(cmd_is(cmd,"write")) {
        char* rest;
        const char* path = first_arg(arg, &rest);
        if(path[0] == 0 || rest[0] == 0) console_puts("usage: write FILE TEXT\n");
        else if(can_modify_path(path, "write")) fs_status(fs_write(path, rest), "write");
    }
    else if(cmd_is(cmd,"edit")) {
        if(arg[0] == 0) {
            console_puts("usage: edit FILE\n");
        } else if(!can_modify_path(arg, "edit")) {
            return;
        } else {
            editor_open(arg);
        }
    }
    else if(cmd_is(cmd,"ticks")) { console_puts("ticks="); console_write_dec(timer_ticks()); console_putc('\n'); }
    else if(cmd_is(cmd,"uptime")) { console_puts("uptime="); console_write_dec(timer_uptime_seconds()); console_puts("s\n"); }
    else if(cmd_is(cmd,"sleep")) {
        uint32_t ticks = parse_u32(arg);
        if(ticks == 0) console_puts("usage: sleep N\n");
        else { sleep_ticks(ticks); console_puts("awake\n"); }
    }
    else if(cmd_is(cmd,"mem") || cmd_is(cmd,"memory")) {
        console_puts("total_kib="); console_write_dec(memory_total_kib());
        console_puts(" usable_kib="); console_write_dec(memory_usable_kib());
        console_puts(" entries="); console_write_dec(memory_map_entries());
        console_putc('\n');
    }
    else if(cmd_is(cmd,"mmap") || cmd_is(cmd,"map")) memory_print_map();
    else if(cmd_is(cmd,"heap")) {
        console_puts("heap_start="); console_write_hex(heap_start());
        console_puts(" heap_next="); console_write_hex(heap_current());
        console_puts(" used="); console_write_dec(heap_bytes_used());
        console_puts(" bytes\n");
    }
    else if(cmd_is(cmd,"mounts") || cmd_is(cmd,"fsstack")) vfs_mounts_cmd();
    else if(cmd_is(cmd,"gui")) gui_cmd(arg);
    else if(cmd_is(cmd,"window") || cmd_is(cmd,"win")) cmd_window(arg);
    else if(cmd_is(cmd,"ps") || cmd_is(cmd,"processes")) cmd_ps();
    else if(cmd_is(cmd,"compute") || cmd_is(cmd,"sci")) cmd_compute(arg);
    else if(cmd_is(cmd,"math")) math_cmd(arg);
    else if(cmd_is(cmd,"gfx") || cmd_is(cmd,"vector")) gfx_cmd(arg);
    else if(cmd_is(cmd,"fb") || cmd_is(cmd,"framebuffer")) fb_cmd(arg);
    else if(cmd_is(cmd,"mouse") || cmd_is(cmd,"pointer")) mouse_cmd(arg);
    else if(cmd_is(cmd,"keyboard") || cmd_is(cmd,"keys") || cmd_is(cmd,"kb")) keyboard_cmd(arg);
    else if(cmd_is(cmd,"hardware") || cmd_is(cmd,"hw") || cmd_is(cmd,"devices")) cmd_hardware(arg);
    else if(cmd_is(cmd,"kill")) cmd_kill(arg);
    else if(cmd_is(cmd,"loader") || cmd_is(cmd,"exec")) loader_cmd(arg);
    else if(cmd_is(cmd,"trx")) loader_cmd(arg);
    else if(cmd_is(cmd,"lang") || cmd_is(cmd,"rusa")) lang_cmd(arg);
    else if(cmd_is(cmd,"project") || cmd_is(cmd,"proj")) project_cmd(arg);
    else if(cmd_is(cmd,"object") || cmd_is(cmd,"obj")) object_cmd(arg);
    else if(cmd_is(cmd,"run")) {
        char* rest;
        const char* path = first_arg(arg, &rest);
        if(path[0] == 0) console_puts("usage: run PROGRAM [ARGS]\n");
        else if(loader_run(path, rest) != 0) console_puts("run: program not found\n");
    }
    else if(cmd_is(cmd,"service") || cmd_is(cmd,"svc")) service_cmd(arg);
    else if(cmd_is(cmd,"user")) security_user_cmd(arg);
    else if(cmd_is(cmd,"cap") || cmd_is(cmd,"caps")) security_cap_cmd(arg);
    else if(cmd_is(cmd,"event") || cmd_is(cmd,"on")) events_cmd(arg);
    else if(cmd_is(cmd,"net") || cmd_is(cmd,"network")) net_cmd(arg);
    else if(cmd_is(cmd,"privacy") || cmd_is(cmd,"control") || cmd_is(cmd,"connections")) privacy_cmd(arg);
    else if(cmd_is(cmd,"security") || cmd_is(cmd,"sec")) security_cmd(arg);
    else if(cmd_is(cmd,"job") || cmd_is(cmd,"jobs")) jobs_cmd(arg);
    else if(cmd_is(cmd,"sched") || cmd_is(cmd,"scheduler")) sched_cmd(arg);
    else if(cmd_is(cmd,"taskman") || cmd_is(cmd,"tasks") || cmd_is(cmd,"top")) taskman_cmd(arg);
    else if(cmd_is(cmd,"structure") || cmd_is(cmd,"roadmap")) shell_structure_cmd();
    else if(cmd_is(cmd,"log") || cmd_is(cmd,"logs")) cmd_log(arg);
    else if(cmd_is(cmd,"alloc") || cmd_is(cmd,"malloc")) {
        uint32_t size = parse_u32(arg);
        if(size == 0) {
            console_puts("usage: alloc N\n");
        } else {
            void* ptr = kmalloc(size);
            console_puts("allocated ");
            console_write_dec(size);
            console_puts(" bytes at ");
            console_write_hex((uint32_t)ptr);
            console_putc('\n');
        }
    }
    else if(cmd_is(cmd,"paging") || cmd_is(cmd,"page")) {
        console_puts("paging=");
        console_puts(paging_is_enabled() ? "on" : "off");
        console_puts(" directory=");
        console_write_hex(paging_directory_addr());
        console_putc('\n');
    }
    else if(cmd_is(cmd,"regs")) cmd_regs();
    else if(cmd_is(cmd,"pkg") || cmd_is(cmd,"package")) cmd_pkg(arg);
    else if(cmd_is(cmd,"test") || cmd_is(cmd,"selftest")) tests_cmd();
    else if(cmd_is(cmd,"halt") || cmd_is(cmd,"shutdown")) { console_puts("Halting.\n"); for(;;) __asm__ __volatile__("cli; hlt"); }
    else if(cmd_is(cmd,"reboot") || cmd_is(cmd,"restart")) cmd_reboot();
    else if(*cmd==0) {}
    else {
        console_puts("Unknown command: '");
        console_puts(cmd);
        console_puts("'. Type 'help'.\n");
    }
}

void kmain(uint32_t mb_magic, uint32_t mb_info_addr){
    console_init();
    fb_bootstrap(mb_info_addr);
    shell_session_init();
    editor_init();
    fs_init();
    block_init();
    process_init();
    process_set_compute("compute", "scientific", 90, 0);
    window_init();
    events_init();
    security_init();
    privacy_init();
    service_init();
    vfs_init();
    fd_init();
    lang_init();
    project_init();
    net_init();
    gui_init();
    fb_init();
    mouse_init();
    jobs_init();
    sched_init();
    loader_set_call_handler(shell_eval);
    lang_set_call_handler(shell_eval);
    shell_set_eval_handler(kernel_shell_dispatch);
    loader_init();
    gfx_init();
    taskman_init();
    pkg_bootstrap();
    shell_intro();
    console_puts("Tabla Rusa OS 0.0.6 booting\n");
    console_puts("shell: tabla native runtime, type 'help' or 'pkg list'\n");
    os_log("system", "boot: kernel entered kmain");

    memory_init(mb_magic, mb_info_addr);
    serial_puts("memory ok\n");
    heap_init();
    serial_puts("heap ok\n");
    paging_init();
    fb_map_hardware();
    serial_puts("paging ok\n");
    idt_init();
    serial_puts("idt ok\n");
    pic_remap();
    serial_puts("pic ok\n");
    timer_init(100);
    serial_puts("timer ok\n");
    keyboard_install();
    serial_puts("keyboard ok\n");
    mouse_install();
    serial_puts("mouse ok\n");
    os_log("system", "boot: core drivers initialized");

    __asm__ __volatile__("sti");

    console_puts("Ready.\n");
    boot_full_stack();
    int gui_desktop_mode = 1;
    char gui_launch_cmd[128];

    size_t len=0;
    for(;;){
        int key = kb_read_key();
        if(key==0){
            if(gui_desktop_mode && gui_take_terminal_request()){
                gui_active_terminal_command(gui_launch_cmd, sizeof(gui_launch_cmd));
                gui_desktop_mode = 0;
                console_framebuffer_terminal(1);
                console_input_clear();
                console_puts("Terminal opened from GUI desktop.\n");
                prompt();
                if(gui_launch_cmd[0]){
                    console_puts(gui_launch_cmd);
                    console_putc('\n');
                    history_add(gui_launch_cmd);
                    shell_eval(gui_launch_cmd);
                    prompt();
                }
                continue;
            }
            if(gui_desktop_mode)
                gui_tick();
            __asm__ __volatile__("hlt");
            continue;
        }

        if(gui_desktop_mode){
            if(gui_key_captures(key)){
                gui_handle_key(key);
                continue;
            }
            if(key == '\n' || gui_take_terminal_request()){
                gui_active_terminal_command(gui_launch_cmd, sizeof(gui_launch_cmd));
                gui_desktop_mode = 0;
                console_framebuffer_terminal(1);
                console_input_clear();
                console_puts("Terminal opened from GUI desktop.\n");
                prompt();
                if(gui_launch_cmd[0]){
                    console_puts(gui_launch_cmd);
                    console_putc('\n');
                    history_add(gui_launch_cmd);
                    shell_eval(gui_launch_cmd);
                    prompt();
                }
                continue;
            }
            if(key == 27){
                gui_enter_desktop();
                continue;
            }
            gui_handle_key(key);
            continue;
        }

        if(key == KB_KEY_PAGE_UP){
            console_scroll(12);
            continue;
        }
        if(key == KB_KEY_PAGE_DOWN){
            console_scroll(-12);
            continue;
        }

        if(editor_is_active() && key == KB_KEY_UP){
            editor_move(-1, &len);
            continue;
        }
        if(editor_is_active() && key == KB_KEY_DOWN){
            editor_move(1, &len);
            continue;
        }
        if(editor_is_active() && key == KB_KEY_LEFT){
            editor_move_horizontal(-1, &len);
            continue;
        }
        if(editor_is_active() && key == KB_KEY_RIGHT){
            editor_move_horizontal(1, &len);
            continue;
        }
        if(key == KB_KEY_LEFT){
            if(shell_cursor() > 0)
                shell_set_cursor(shell_cursor() - 1);
            redraw_input(&len);
            continue;
        }
        if(key == KB_KEY_RIGHT){
            if(shell_cursor() < len)
                shell_set_cursor(shell_cursor() + 1);
            redraw_input(&len);
            continue;
        }
        if(key == KB_KEY_HOME){
            shell_set_cursor(0);
            redraw_input(&len);
            continue;
        }
        if(key == KB_KEY_END){
            shell_set_cursor(len);
            redraw_input(&len);
            continue;
        }
        if(key == KB_KEY_DELETE){
            char* inbuf = shell_input_buffer();
            size_t cur = shell_cursor();
            if(cur < len){
                for(size_t i=cur; i<len; i++)
                    inbuf[i] = inbuf[i + 1];
                len--;
                redraw_input(&len);
            }
            continue;
        }
        if(key == KB_KEY_UP){
            shell_history_prev(&len);
            continue;
        }
        if(key == KB_KEY_DOWN){
            shell_history_next(&len);
            continue;
        }
        if(key > 0xFF)
            continue;

        char c = (char)key;
        if(c == 27){
            if(editor_is_active()){
                console_input_clear();
                editor_close();
            } else {
                shell_reset_input(&len);
                gui_desktop_mode = 1;
                console_framebuffer_terminal(0);
                gui_enter_desktop();
                continue;
            }
            shell_reset_input(&len);
            prompt();
            continue;
        }
        if(c=='\n'){
            char* inbuf = shell_input_buffer();
            inbuf[len]=0;
            console_input_clear();
            console_echo_command(inbuf);
            if(!editor_is_active())
                history_add(inbuf);
            if(editor_is_active()){
                editor_eval(inbuf, &len);
                if(editor_is_active())
                    continue;
            } else {
                shell_eval(inbuf);
            }
            shell_reset_input(&len);
            prompt();
            continue;
        }
        if(c=='\b'){
            input_backspace(&len);
            continue;
        }
        input_insert_char(c, &len);
    }
}
