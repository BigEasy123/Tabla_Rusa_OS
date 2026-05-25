#include <stddef.h>
#include "console.h"
#include "block.h"
#include "fb.h"
#include "fd.h"
#include "fs.h"
#include "lang.h"
#include "loader.h"
#include "memory.h"
#include "object.h"
#include "paging.h"
#include "process.h"
#include "project.h"
#include "sched.h"
#include "security.h"
#include "timer.h"
#include "tests.h"

static uint32_t pass_count = 0;
static uint32_t fail_count = 0;

static void check_result(const char* name, int ok){
    console_puts(ok ? "[ok]   " : "[fail] ");
    console_puts(name);
    console_putc('\n');
    if(ok) pass_count++;
    else fail_count++;
}

void tests_cmd(void){
    int type;
    size_t size;
    const char* text;
    int fd;
    pass_count = 0;
    fail_count = 0;
    console_puts("Tabla Rusa OS selftest\n");
    check_result("console", 1);
    console_puts("timer: ticks=");
    console_write_dec(timer_ticks());
    console_putc('\n');
    console_puts("memory: entries=");
    console_write_dec(memory_map_entries());
    console_puts(" usable_kib=");
    console_write_dec(memory_usable_kib());
    console_putc('\n');
    check_result("paging enabled", paging_is_enabled());
    check_result("fs /home", fs_stat("/home", &type, &size) == 0 && type == 1);
    check_result("fs /home/math", fs_stat("/home/math", &type, &size) == 0 && type == 1);
    check_result("fs write/read", fs_write("/tmp/selftest.txt", "selftest") == 0 &&
                 fs_read("/tmp/selftest.txt", &text) == 0 && text[0] == 's');
    fd = fd_open("/tmp/fdtest.txt", "rw");
    check_result("fd open", fd >= 0);
    check_result("fd write", fd >= 0 && fd_write(fd, "fd-ok") == 0);
    check_result("fd read", fd >= 0 && fd_read(fd, &text) == 0 && text[0] == 'f');
    check_result("fd close", fd >= 0 && fd_close(fd) == 0);
    check_result("process compute", process_find("compute") != 0);
    check_result("program /bin/hello.trx", fs_stat("/bin/hello.trx", &type, &size) == 0);
    check_result("program /bin/mathbench.trx", fs_stat("/bin/mathbench.trx", &type, &size) == 0);
    check_result("program /bin/gui.trx", fs_stat("/bin/gui.trx", &type, &size) == 0);
    check_result("program /bin/netup.trx", fs_stat("/bin/netup.trx", &type, &size) == 0);
    check_result("program /bin/physics.trx", fs_stat("/bin/physics.trx", &type, &size) == 0);
    check_result("program /bin/readme.trx", fs_stat("/bin/readme.trx", &type, &size) == 0);
    check_result("program /bin/paint.trx", fs_stat("/bin/paint.trx", &type, &size) == 0);
    check_result("loader run hello", loader_run("hello", "") == 0);
    check_result("object dispatcher", object_eval("file[\"/tmp/selftest.txt\"].exists()") == 1);
    sched_yield();
    check_result("scheduler yield", 1);
    fb_cmd("status");
    check_result("framebuffer descriptor", fs_stat("/system/gui/framebuffer.txt", &type, &size) == 0);
    check_result("vector gfx manifest", fs_stat("/system/gui/vector.txt", &type, &size) == 0);
    check_result("trx runtime docs", fs_stat("/share/docs/trx-runtime.txt", &type, &size) == 0);
    check_result("network stack descriptor", fs_stat("/system/net/stack.txt", &type, &size) == 0);
    check_result("block descriptor", fs_stat("/system/block.txt", &type, &size) == 0);
    check_result("Rusa docs", fs_stat("/share/rusa/keywords", &type, &size) == 0);
    check_result("Rusa stdlib", fs_stat("/lib/rusa/std.trx", &type, &size) == 0);
    check_result("Rusa object docs", fs_stat("/share/rusa/objects", &type, &size) == 0);
    console_puts("security user=");
    console_puts(security_current_user());
    console_puts(" secure_mode=");
    console_puts(security_is_locked() ? "on\n" : "off\n");
    console_puts("selftest pass=");
    console_write_dec(pass_count);
    console_puts(" fail=");
    console_write_dec(fail_count);
    console_putc('\n');
}
