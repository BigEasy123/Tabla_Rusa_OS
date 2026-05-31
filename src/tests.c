#include <stddef.h>
#include "console.h"
#include "block.h"
#include "events.h"
#include "fb.h"
#include "fd.h"
#include "fs.h"
#include "gui.h"
#include "lang.h"
#include "loader.h"
#include "memory.h"
#include "mouse.h"
#include "net.h"
#include "object.h"
#include "paging.h"
#include "privacy.h"
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

static int text_has(const char* haystack, const char* needle){
    uint32_t i = 0;
    if(!needle[0]) return 1;
    while(haystack[i]){
        uint32_t j = 0;
        while(haystack[i + j] && needle[j] && haystack[i + j] == needle[j]) j++;
        if(!needle[j]) return 1;
        i++;
    }
    return 0;
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
    check_result("console scrollback", console_scrollback_lines() > 0);
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
    check_result("fd per-process count", fd_count_for_pid(1) > 0);
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
    check_result("scheduler context switch", sched_total_switches() > 0 && sched_current_name()[0] != 0);
    fb_cmd("status");
    check_result("framebuffer descriptor", fs_stat("/system/gui/framebuffer.txt", &type, &size) == 0);
    check_result("hardware framebuffer", fb_hardware_ready());
    fb_clear(0);
    fb_fill_rect(0, 0, 32, 32, 200);
    fb_draw_text(4, 4, "ok", 240);
    check_result("framebuffer raster", fb_checksum() != 0);
    mouse_set(320, 240);
    mouse_button(0, 1);
    mouse_button(0, 0);
    check_result("mouse crosshair input", mouse_event_count() >= 2 && mouse_x() == 320);
    mouse_set(60, 95);
    mouse_button(0, 1);
    mouse_button(0, 0);
    char gui_cmd_buf[64];
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui icon opens files", gui_cmd_buf[0] == 't');
    mouse_set(830, 136);
    mouse_button(0, 1);
    mouse_button(0, 0);
    check_result("gui open terminal button", gui_take_terminal_request());
    char gui_editor_code[] = "editor code";
    gui_cmd(gui_editor_code);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui editor code terminal", text_has(gui_cmd_buf, "demo.rusa"));
    char gui_rusa_docs[] = "rusa docs";
    gui_cmd(gui_rusa_docs);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui rusa standalone docs", text_has(gui_cmd_buf, "docs"));
    char gui_saver_rain[] = "backdrop rain";
    gui_cmd(gui_saver_rain);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui screensaver command", text_has(gui_cmd_buf, "rain"));
    char gui_editor_app[] = "app editor";
    char gui_editor_app3[] = "app editor";
    gui_cmd(gui_editor_app3);
    mouse_set(410, 372);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_handle_key('X');
    char gui_editor_save[] = "editor save";
    gui_cmd(gui_editor_save);
    check_result("gui editor document input", fs_read("/home/projects/demo.rusa", &text) == 0 && text_has(text, "X"));
    char gui_editor_app2[] = "app editor";
    gui_cmd(gui_editor_app2);
    mouse_set(700, 136);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui close button", !text_has(gui_cmd_buf, "edit "));
    gui_cmd(gui_editor_app);
    mouse_set(960, 94);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui close window", !text_has(gui_cmd_buf, "edit "));
    mouse_scroll(1);
    check_result("mousepad terminal scroll", mouse_scroll_count() > 0 && console_scroll_offset() > 0);
    console_scroll(-64);
    check_result("vector gfx manifest", fs_stat("/system/gui/vector.txt", &type, &size) == 0);
    check_result("trx runtime docs", fs_stat("/share/docs/trx-runtime.txt", &type, &size) == 0);
    check_result("network stack descriptor", fs_stat("/system/net/stack.txt", &type, &size) == 0);
    char net_socket_cmd[] = "socket udp 9999";
    char net_send_cmd[] = "send 0 selftest-packet";
    net_cmd(net_socket_cmd);
    net_cmd(net_send_cmd);
    check_result("network packet queue", net_packet_count() >= 2);
    check_result("privacy network allowed", privacy_allows_network());
    check_result("block descriptor", fs_stat("/system/block.txt", &type, &size) == 0);
    check_result("Rusa docs", fs_stat("/share/rusa/keywords", &type, &size) == 0);
    check_result("Rusa stdlib", fs_stat("/lib/rusa/std.trx", &type, &size) == 0);
    check_result("Rusa source stdlib", fs_stat("/lib/rusa/std.rusa", &type, &size) == 0);
    check_result("Rusa source runtime", lang_run_source("let n: int = 1\nwhile n < 3 { set n = n + 1 }\nfn plus(a: int) { return a + 1 }\nprint plus(n)\n", "<selftest>", "") == 0);
    check_result("Rusa diagnostics", lang_run_source("let broken 3\n", "<selftest-bad>", "") != 0);
    check_result("Rusa nswitch", lang_run_source("let x: int = 2\nlet y: int = 5\nnswitch x, y { case 1, * { print \"bad\" } case 2, 5 { print \"hit\" } default { print \"miss\" } }\n", "<selftest-nswitch>", "") == 0);
    check_result("Rusa security scan", lang_run_source("while true { print \"loop\" }\n", "<selftest-risk>", "") != 0);
    check_result("Rusa persistent events", lang_run_source("on \"selftest.event\" { print \"event-ok\" }\n", "<selftest-event>", "") == 0);
    events_emit("selftest.event");
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
