#include <stddef.h>
#include "console.h"
#include "fs.h"
#include "memory.h"
#include "paging.h"
#include "process.h"
#include "security.h"
#include "timer.h"
#include "tests.h"

void tests_cmd(void){
    int type;
    size_t size;
    console_puts("console: ok\n");
    console_puts("timer: ticks=");
    console_write_dec(timer_ticks());
    console_putc('\n');
    console_puts("memory: entries=");
    console_write_dec(memory_map_entries());
    console_puts(" usable_kib=");
    console_write_dec(memory_usable_kib());
    console_putc('\n');
    console_puts("paging: ");
    console_puts(paging_is_enabled() ? "on\n" : "off\n");
    console_puts("fs /home: ");
    console_puts(fs_stat("/home", &type, &size) == 0 && type == 1 ? "ok\n" : "fail\n");
    console_puts("fs /home/math: ");
    console_puts(fs_stat("/home/math", &type, &size) == 0 && type == 1 ? "ok\n" : "fail\n");
    console_puts("process compute: ");
    console_puts(process_find("compute") ? "ok\n" : "fail\n");
    console_puts("program loader /bin/hello.trx: ");
    console_puts(fs_stat("/bin/hello.trx", &type, &size) == 0 ? "ok\n" : "fail\n");
    console_puts("vector gfx manifest: ");
    console_puts(fs_stat("/system/gui/vector.txt", &type, &size) == 0 ? "ok\n" : "fail\n");
    console_puts("framebuffer descriptor: ");
    console_puts(fs_stat("/system/gui/framebuffer.txt", &type, &size) == 0 ? "ok\n" : "fail\n");
    console_puts("trx runtime docs: ");
    console_puts(fs_stat("/share/docs/trx-runtime.txt", &type, &size) == 0 ? "ok\n" : "fail\n");
    console_puts("network stack descriptor: ");
    console_puts(fs_stat("/system/net/stack.txt", &type, &size) == 0 ? "ok\n" : "fail\n");
    console_puts("security user=");
    console_puts(security_current_user());
    console_puts(" secure_mode=");
    console_puts(security_is_locked() ? "on\n" : "off\n");
}
