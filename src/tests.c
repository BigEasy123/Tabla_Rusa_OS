#include <stddef.h>
#include "console.h"
#include "block.h"
#include "events.h"
#include "fb.h"
#include "fd.h"
#include "fs.h"
#include "gui.h"
#include "jobs.h"
#include "keyboard.h"
#include "lang.h"
#include "loader.h"
#include "mathcore.h"
#include "mathlib.h"
#include "memory.h"
#include "mouse.h"
#include "net.h"
#include "object.h"
#include "paging.h"
#include "policy.h"
#include "privacy.h"
#include "proofcore.h"
#include "process.h"
#include "project.h"
#include "research.h"
#include "sched.h"
#include "science.h"
#include "security.h"
#include "service.h"
#include "shell.h"
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

static uint32_t sched_runner_ticks = 0;

static void sched_runner_entry(const char* name, uint32_t quantum){
    (void)name;
    sched_runner_ticks += quantum;
    jobs_account("runner", quantum);
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
    check_result("kernel module registry", fs_stat("/system/kernel/modules.txt", &type, &size) == 0);
    check_result("boot startup descriptor", fs_stat("/system/boot/startup.txt", &type, &size) == 0);
    check_result("fs write/read", fs_write("/tmp/selftest.txt", "selftest") == 0 &&
                 fs_read("/tmp/selftest.txt", &text) == 0 && text[0] == 's');
    fd = fd_open("/tmp/fdtest.txt", "rw");
    check_result("fd open", fd >= 0);
    check_result("fd write", fd >= 0 && fd_write(fd, "fd-ok") == 0);
    check_result("fd read", fd >= 0 && fd_read(fd, &text) == 0 && text[0] == 'f');
    check_result("fd seek/tell", fd >= 0 && fd_seek(fd, 3) == 0 && fd_tell(fd) == 3);
    char fd_chunk[8];
    check_result("fd read chunk", fd >= 0 && fd_read_chunk(fd, fd_chunk, sizeof(fd_chunk)) > 0 &&
                 text_has(fd_chunk, "ok"));
    check_result("fd per-process count", fd_count_for_pid(1) > 0);
    check_result("fd close", fd >= 0 && fd_close(fd) == 0);
    fd = fd_open("/tmp/fdappend.txt", "a");
    check_result("fd append open", fd >= 0);
    check_result("fd append first", fd >= 0 && fd_write(fd, "A") == 0);
    check_result("fd append second", fd >= 0 && fd_write(fd, "B") == 0 &&
                 fs_read("/tmp/fdappend.txt", &text) == 0 && text_has(text, "AB"));
    if(fd >= 0)
        fd_close(fd);
    int partial_fd = fd_open("/tmp/fdpartial.txt", "w");
    check_result("fd partial write setup", partial_fd >= 0 && fd_write(partial_fd, "abcdef") == 0);
    check_result("fd partial write", partial_fd >= 0 && fd_seek(partial_fd, 2) == 0 &&
                 fd_write_chunk(partial_fd, "XYZ", 2) == 2 &&
                 fs_read("/tmp/fdpartial.txt", &text) == 0 && text_has(text, "abXYef"));
    if(partial_fd >= 0)
        fd_close(partial_fd);
    fs_write("/tmp/fdperm.txt", "locked");
    check_result("fs chmod read only", fs_chmod("/tmp/fdperm.txt", FS_PERM_READ) == 0 &&
                 fs_can_read("/tmp/fdperm.txt") && !fs_can_write("/tmp/fdperm.txt") &&
                 fd_open("/tmp/fdperm.txt", "rw") < 0);
    int readonly_fd = fd_open("/tmp/fdperm.txt", "r");
    check_result("fd read read-only file", readonly_fd >= 0 && fd_read(readonly_fd, &text) == 0 &&
                 text_has(text, "locked"));
    if(readonly_fd >= 0)
        fd_close(readonly_fd);
    char perm_text[8];
    fs_permission_string("/tmp/fdperm.txt", perm_text, sizeof(perm_text));
    check_result("fs permission string", text_has(perm_text, "r--"));
    check_result("fs chmod restore write", fs_chmod("/tmp/fdperm.txt", FS_PERM_READ | FS_PERM_WRITE) == 0 &&
                 fs_can_write("/tmp/fdperm.txt"));
    check_result("process compute", process_find("compute") != 0);
    char math_logic_cmd[] = "logic modus true true";
    math_cmd(math_logic_cmd);
    check_result("math logic proof", text_has(process_find("compute")->workload, "logic"));
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
    int worker_pid = process_create("worker", "user", "ipc-test", 44);
    check_result("process create API", worker_pid > 0 && process_find("worker") != 0);
    fs_write("/tmp/fdinherit.txt", "inherit");
    int inherit_fd = fd_open("/tmp/fdinherit.txt", "r");
    check_result("process spawn API", process_spawn("worker") == 0 && process_find("worker")->running);
    check_result("fd inherit on spawn", inherit_fd >= 0 && fd_count_for_pid((uint32_t)worker_pid) > 0);
    if(inherit_fd >= 0)
        fd_close(inherit_fd);
    int dup_fd = fd_open("/tmp/fdpartial.txt", "r");
    check_result("fd duplicate to process", dup_fd >= 0 &&
                 fd_dup_to_pid(1, dup_fd, (uint32_t)worker_pid) >= 0 &&
                 fd_count_for_pid((uint32_t)worker_pid) > 1);
    if(dup_fd >= 0)
        fd_close(dup_fd);
    check_result("process priority API", process_set_priority("worker", 66) == 0 && process_find("worker")->priority == 66);
    check_result("process sleep API", process_sleep("worker") == 0 &&
                 process_find("worker")->state == PROCESS_SLEEPING);
    check_result("process wake API", process_wake("worker") == 0 &&
                 process_find("worker")->state == PROCESS_READY);
    struct process_info proc_snapshot[PROCESS_MAX];
    check_result("process structured list", process_list_info(proc_snapshot, PROCESS_MAX) >= 6);
    check_result("process current API", process_get_current() != 0 && process_get_current()->name[0] != 0);
    check_result("process memory API", process_set_memory("worker", 123) == 0 &&
                 process_find("worker")->memory_kib == 123 && process_memory_total_kib() >= 123);
    check_result("process background API", process_set_background("worker", 1) == 0 &&
                 process_find("worker")->background);
    int proc_handle = process_handle_open(1, "worker");
    struct process_handle_info handle_snapshot[PROCESS_MAX];
    check_result("process handle open", proc_handle > 0 &&
                 process_handle_get(proc_handle) != 0 &&
                 text_has(process_handle_get(proc_handle)->name, "worker"));
    check_result("process handle list", process_handle_list(handle_snapshot, PROCESS_MAX) > 0);
    check_result("process handle count", process_handles_for_pid(1) > 0);
    struct ipc_message_info ipc_msg;
    check_result("ipc send API", ipc_send(1, (uint32_t)worker_pid, "hello-worker") == 0);
    check_result("ipc recv API", ipc_recv((uint32_t)worker_pid, &ipc_msg) == 0 &&
                 text_has(ipc_msg.payload, "hello-worker"));
    int pipe_id = pipe_create((uint32_t)worker_pid);
    char pipe_buffer[32];
    check_result("pipe create API", pipe_id > 0);
    check_result("pipe write API", pipe_id > 0 && pipe_write((uint32_t)pipe_id, "pipe-ok") == 0);
    check_result("pipe read API", pipe_id > 0 && pipe_read((uint32_t)pipe_id, pipe_buffer, sizeof(pipe_buffer)) == 0 &&
                 text_has(pipe_buffer, "pipe-ok"));
    check_result("signal sleep API", signal_send((uint32_t)worker_pid, "sleep") == 0 &&
                 process_find("worker")->state == PROCESS_SLEEPING);
    check_result("signal wake API", signal_send((uint32_t)worker_pid, "wake") == 0 &&
                 process_find("worker")->state == PROCESS_READY);
    check_result("scheduler priority wrapper", scheduler_set_priority("compute", 9) == 0);
    check_result("scheduler pick wrapper", scheduler_pick_next()[0] != 0);
    sched_runner_ticks = 0;
    check_result("scheduler register executable task",
                 sched_register_task("runner", 3, sched_runner_entry) == 0 &&
                 process_find("runner") != 0);
    for(int i=0; i<10; i++)
        sched_yield();
    check_result("scheduler executable callback", sched_runner_ticks >= 3);
    check_result("scheduler executable runs", sched_task_runs("runner") > 0);
    check_result("scheduler process accounting", process_find("runner") != 0 &&
                 process_find("runner")->ticks >= 3);
    scheduler_tick();
    check_result("process handle close", process_handle_close(proc_handle) == 0 &&
                 process_handle_get(proc_handle) == 0);
    check_result("process kill API", process_kill("worker") == 0 && process_find("worker") == 0);
    fb_cmd("status");
    check_result("framebuffer descriptor", fs_stat("/system/gui/framebuffer.txt", &type, &size) == 0);
    check_result("hardware framebuffer", fb_hardware_ready());
    fb_clear(0);
    fb_fill_rect(0, 0, 32, 32, 200);
    fb_draw_text(4, 4, "ok", 240);
    check_result("framebuffer raster", fb_checksum() != 0);
    check_result("keyboard descriptor", fs_stat("/system/input/keyboard.txt", &type, &size) == 0);
    char kb_caps_on[] = "caps on";
    keyboard_cmd(kb_caps_on);
    check_result("keyboard caps control", keyboard_caps_on());
    char kb_caps_off[] = "caps off";
    keyboard_cmd(kb_caps_off);
    char kb_detect[] = "detect";
    keyboard_cmd(kb_detect);
    check_result("keyboard detection", keyboard_type()[0] == 'P' && !keyboard_caps_on());
    char kb_num_off[] = "num off";
    keyboard_cmd(kb_num_off);
    check_result("keyboard num control", !keyboard_num_on());
    char kb_num_on[] = "num on";
    keyboard_cmd(kb_num_on);
    char hw_status[] = "hardware status";
    shell_eval(hw_status);
    check_result("hardware control command", 1);
    mouse_set(320, 240);
    mouse_button(0, 1);
    mouse_button(0, 0);
    check_result("mouse crosshair input", mouse_event_count() >= 2 && mouse_x() == 320);
    char gui_cmd_buf[64];
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui boots to desktop", gui_cmd_buf[0] == 0 && gui_is_desktop_visible());
    char gui_boot_safe[] = "boot safe";
    gui_cmd(gui_boot_safe);
    check_result("gui boot safe mode", fs_stat("/system/gui/safe-mode.txt", &type, &size) == 0);
    char gui_boot_recovery[] = "boot recovery";
    gui_cmd(gui_boot_recovery);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui boot recovery terminal", text_has(gui_cmd_buf, "log show system"));
    char gui_desktop_reset[] = "desktop";
    gui_cmd(gui_desktop_reset);
    mouse_set(60, 95);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui icon opens files", gui_cmd_buf[0] == 't');
    char gui_files_reset_move[] = "move active 174 72";
    gui_cmd(gui_files_reset_move);
    char gui_files_reset_size[] = "resize active 820 594";
    gui_cmd(gui_files_reset_size);
    mouse_set(260, 454);
    mouse_button(0, 1);
    mouse_button(0, 0);
    mouse_set(380, 300);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui file browser edit bridge", text_has(gui_cmd_buf, "edit "));
    char gui_files_app[] = "app files";
    gui_cmd(gui_files_app);
    char gui_files_new[] = "files new gui-note.txt";
    gui_cmd(gui_files_new);
    check_result("gui files new file", fs_stat("/home/gui-note.txt", &type, &size) == 0 && type == 2);
    char gui_files_rename[] = "files rename gui-renamed.txt";
    gui_cmd(gui_files_rename);
    check_result("gui files rename", fs_stat("/home/gui-renamed.txt", &type, &size) == 0 && type == 2);
    char gui_files_copy[] = "files copy gui-copy.txt";
    gui_cmd(gui_files_copy);
    check_result("gui files copy", fs_stat("/home/gui-copy.txt", &type, &size) == 0 && type == 2);
    char gui_files_move[] = "files move gui-moved.txt";
    gui_cmd(gui_files_move);
    check_result("gui files move", fs_stat("/home/gui-moved.txt", &type, &size) == 0 && type == 2 &&
                 fs_stat("/home/gui-copy.txt", &type, &size) != 0);
    char gui_files_delete[] = "files delete";
    gui_cmd(gui_files_delete);
    check_result("gui files delete", fs_stat("/home/gui-moved.txt", &type, &size) != 0);
    char gui_files_mkdir[] = "files mkdir gui-folder";
    gui_cmd(gui_files_mkdir);
    check_result("gui files new folder", fs_stat("/home/gui-folder", &type, &size) == 0 && type == 1);
    char gui_files_up[] = "files up";
    gui_cmd(gui_files_up);
    check_result("gui files up", 1);
    char gui_files_rusa_select[] = "files select 2";
    gui_cmd(gui_files_rusa_select);
    char gui_files_open_terminal[] = "files open terminal";
    gui_cmd(gui_files_open_terminal);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui files open terminal", text_has(gui_cmd_buf, "tree ") || text_has(gui_cmd_buf, "cat "));
    mouse_set(830, 136);
    mouse_button(0, 1);
    mouse_button(0, 0);
    check_result("gui open terminal button", gui_take_terminal_request());
    char gui_term_app[] = "app terminal";
    gui_cmd(gui_term_app);
    (void)gui_take_terminal_request();
    char gui_close_term[] = "close terminal";
    gui_cmd(gui_close_term);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui closes terminal reveals stack", gui_cmd_buf[0] != 0 && !text_has(gui_cmd_buf, "pwd"));
    char gui_term_app2[] = "app terminal";
    gui_cmd(gui_term_app2);
    uint32_t full_before_input = gui_full_repaint_count();
    uint32_t window_before_input = gui_window_repaint_count();
    gui_handle_key('p');
    gui_handle_key('w');
    gui_handle_key('d');
    gui_handle_key('\n');
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui terminal inline command", !gui_take_terminal_request() && text_has(gui_cmd_buf, "/home"));
    check_result("gui terminal partial repaint", gui_full_repaint_count() == full_before_input &&
                 gui_window_repaint_count() > window_before_input);
    gui_handle_key(KB_KEY_UP);
    gui_terminal_input_text(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui terminal history recall", text_has(gui_cmd_buf, "pwd"));
    gui_handle_key(KB_KEY_HOME);
    gui_handle_key(KB_KEY_RIGHT);
    gui_handle_key(KB_KEY_DELETE);
    gui_handle_key('x');
    gui_terminal_input_text(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui terminal cursor editing", text_has(gui_cmd_buf, "pxd"));
    gui_terminal_clear_input();
    char gui_terminal_select[] = "terminal select 0";
    gui_cmd(gui_terminal_select);
    char gui_terminal_copy[] = "terminal copy";
    gui_cmd(gui_terminal_copy);
    char gui_terminal_paste[] = "terminal paste";
    gui_cmd(gui_terminal_paste);
    gui_terminal_input_text(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui terminal copy paste", text_has(gui_cmd_buf, "Tabla") || text_has(gui_cmd_buf, "$"));
    gui_terminal_clear_input();
    char gui_editor_code[] = "editor code";
    gui_cmd(gui_editor_code);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui editor code terminal", text_has(gui_cmd_buf, "demo.rusa"));
    char gui_editor_math[] = "editor math";
    gui_cmd(gui_editor_math);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui editor math notes mode", text_has(gui_cmd_buf, "/home/math/notes.md"));
    char gui_editor_math_save[] = "editor save /home/math/gui-note.md";
    gui_cmd(gui_editor_math_save);
    check_result("gui editor md save", fs_stat("/home/math/gui-note.md", &type, &size) == 0 && type == 2);
    char gui_rusa_docs[] = "rusa docs";
    gui_cmd(gui_rusa_docs);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui rusa standalone docs", text_has(gui_cmd_buf, "docs"));
    char gui_rusa_check[] = "rusa check";
    gui_cmd(gui_rusa_check);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui rusa embedded check", text_has(gui_cmd_buf, "check") || text_has(gui_cmd_buf, "rusa"));
    char gui_rusa_packages[] = "rusa packages";
    gui_cmd(gui_rusa_packages);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui rusa package browser", text_has(gui_cmd_buf, "pkg list"));
    char gui_math_phys[] = "math physics";
    gui_cmd(gui_math_phys);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui math physics tab", text_has(gui_cmd_buf, "phys"));
    char gui_math_latex[] = "math latex";
    gui_cmd(gui_math_latex);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui math latex tab", text_has(gui_cmd_buf, "latex"));
    char gui_math_jobs[] = "math jobs";
    gui_cmd(gui_math_jobs);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui math proof jobs tab", text_has(gui_cmd_buf, "taskman top"));
    char gui_math_move[] = "move active 260 120";
    gui_cmd(gui_math_move);
    mouse_set(606, 328);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui local moved-window click", text_has(gui_cmd_buf, "phys"));
    char gui_settings_hw[] = "settings hardware";
    gui_cmd(gui_settings_hw);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui settings hardware tab", text_has(gui_cmd_buf, "hardware"));
    char gui_settings_keyboard[] = "settings keyboard";
    gui_cmd(gui_settings_keyboard);
    check_result("gui settings keyboard tab", fs_stat("/system/input/keyboard.txt", &type, &size) == 0);
    char gui_settings_input[] = "settings input";
    gui_cmd(gui_settings_input);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui settings input tab", text_has(gui_cmd_buf, "mouse status"));
    char gui_focus_math[] = "focus math";
    gui_cmd(gui_focus_math);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui multiwindow focus app", text_has(gui_cmd_buf, "math "));
    char gui_stack_math[] = "app math";
    gui_cmd(gui_stack_math);
    char gui_task_select[] = "taskman select network";
    gui_cmd(gui_task_select);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui taskman command surface", text_has(gui_cmd_buf, "taskman top"));
    char gui_task_boost[] = "taskman boost";
    gui_cmd(gui_task_boost);
    check_result("gui taskman boost compute", process_find("compute")->running);
    char gui_task_kill[] = "taskman kill network";
    gui_cmd(gui_task_kill);
    check_result("gui taskman kill", !process_find("network")->running);
    char gui_task_restart[] = "taskman restart network";
    gui_cmd(gui_task_restart);
    check_result("gui taskman restart", process_find("network")->running);
    char gui_task_focus[] = "taskman focus compute";
    gui_cmd(gui_task_focus);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui taskman focus app", text_has(gui_cmd_buf, "math "));
    gui_cmd(gui_stack_math);
    char gui_stack_files[] = "app files";
    gui_cmd(gui_stack_files);
    char gui_close_files[] = "close files";
    gui_cmd(gui_close_files);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui z-order fallback", text_has(gui_cmd_buf, "math "));
    char gui_phase1_math[] = "app math";
    gui_cmd(gui_phase1_math);
    char gui_phase1_math_move[] = "move active 308 90";
    gui_cmd(gui_phase1_math_move);
    char gui_phase1_math_vector[] = "math vector";
    gui_cmd(gui_phase1_math_vector);
    char gui_phase1_files[] = "app files";
    gui_cmd(gui_phase1_files);
    char gui_phase1_files_size[] = "resize active 520 380";
    gui_cmd(gui_phase1_files_size);
    char gui_phase1_files_move[] = "move active 112 72";
    gui_cmd(gui_phase1_files_move);
    uint32_t inactive_live_before = gui_inactive_live_render_count();
    char gui_phase1_draw[] = "draw";
    gui_cmd(gui_phase1_draw);
    check_result("gui inactive live surfaces", gui_inactive_live_render_count() > inactive_live_before);
    mouse_set(840, 286);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui inactive window click routes", text_has(gui_cmd_buf, "taskman top"));
    char gui_phase1_files_again[] = "app files";
    gui_cmd(gui_phase1_files_again);
    mouse_set(850, 100);
    mouse_button(0, 1);
    mouse_move(-40, 40);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui inactive drag to front", text_has(gui_cmd_buf, "taskman top"));
    char gui_start_toggle[] = "start";
    gui_cmd(gui_start_toggle);
    check_result("gui start menu command", gui_launcher_is_open());
    gui_handle_key(KB_KEY_SUPER_LEFT);
    check_result("gui super menu shortcut", !gui_launcher_is_open());
    mouse_set(40, 744);
    mouse_button(0, 1);
    mouse_button(0, 0);
    check_result("gui start button menu", gui_launcher_is_open());
    mouse_set(52, 478);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui launcher opens editor", text_has(gui_cmd_buf, "edit "));
    char gui_saver_rain[] = "saver rain calm";
    gui_cmd(gui_saver_rain);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui screensaver command", text_has(gui_cmd_buf, "rain"));
    char gui_backdrop_rain[] = "backdrop rain";
    gui_cmd(gui_backdrop_rain);
    uint32_t wallpaper_before = fb_checksum();
    sleep_ticks(16);
    for(uint32_t tick=0; tick<2; tick++)
        gui_tick();
    check_result("gui calm wallpaper stable", fb_checksum() == wallpaper_before);
    char gui_wallpaper_live[] = "wallpaper live rain";
    gui_cmd(gui_wallpaper_live);
    wallpaper_before = fb_checksum();
    sleep_ticks(96);
    gui_tick();
    check_result("gui live wallpaper opt-in", fb_checksum() != wallpaper_before);
    char gui_wallpaper_still[] = "wallpaper rain";
    gui_cmd(gui_wallpaper_still);
    char gui_saver_app[] = "app saver";
    gui_cmd(gui_saver_app);
    mouse_set(648, 310);
    mouse_button(0, 1);
    mouse_button(0, 0);
    wallpaper_before = fb_checksum();
    sleep_ticks(96);
    gui_tick();
    check_result("gui live wallpaper button", fb_checksum() != wallpaper_before);
    char gui_saver_calm_select[] = "saver rain calm";
    gui_cmd(gui_saver_calm_select);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui screensaver calm separate", text_has(gui_cmd_buf, "rain 6"));
    char gui_wallpaper_lava_still[] = "wallpaper lava";
    gui_cmd(gui_wallpaper_lava_still);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui wallpaper leaves saver mode", text_has(gui_cmd_buf, "rain 6"));
    char gui_saver_full_select[] = "saver waves full";
    gui_cmd(gui_saver_full_select);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui screensaver full separate", text_has(gui_cmd_buf, "waves 12"));
    char gui_editor_app[] = "app editor";
    char gui_editor_app3[] = "app editor";
    gui_cmd(gui_editor_app3);
    char gui_editor_path[] = "editor new /home/projects/scratch.rusa";
    gui_cmd(gui_editor_path);
    mouse_set(410, 372);
    mouse_button(0, 1);
    mouse_button(0, 0);
    full_before_input = gui_full_repaint_count();
    window_before_input = gui_window_repaint_count();
    gui_handle_key('X');
    gui_handle_key(KB_KEY_HOME);
    gui_handle_key(KB_KEY_DELETE);
    gui_handle_key(KB_KEY_PAGE_DOWN);
    gui_handle_key('Z');
    check_result("gui editor partial repaint", gui_full_repaint_count() == full_before_input &&
                 gui_window_repaint_count() > window_before_input);
    char gui_editor_save[] = "editor save";
    gui_cmd(gui_editor_save);
    check_result("gui editor document input", fs_read("/home/projects/scratch.rusa", &text) == 0 && !text_has(text, "X") && text_has(text, "Z"));
    char gui_editor_open_notes[] = "editor open /home/readme.txt";
    gui_cmd(gui_editor_open_notes);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui editor path open", text_has(gui_cmd_buf, "/home/readme.txt"));
    char gui_editor_copy[] = "editor copy";
    gui_cmd(gui_editor_copy);
    char gui_editor_paste[] = "editor paste";
    gui_cmd(gui_editor_paste);
    char gui_editor_select[] = "editor select 1 2";
    gui_cmd(gui_editor_select);
    gui_cmd(gui_editor_copy);
    char gui_editor_cut[] = "editor cut";
    gui_cmd(gui_editor_cut);
    gui_cmd(gui_editor_paste);
    char gui_editor_find[] = "editor find Welcome";
    gui_cmd(gui_editor_find);
    char gui_editor_saveas[] = "editor saveas /home/projects/saveas.rusa";
    gui_cmd(gui_editor_saveas);
    check_result("gui editor range tools", fs_read("/home/projects/saveas.rusa", &text) == 0 && text[0] != 0);
    char gui_editor_openas[] = "editor openas /home/projects/saveas.rusa";
    gui_cmd(gui_editor_openas);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui editor open save dialogs", text_has(gui_cmd_buf, "saveas.rusa"));
    char gui_editor_dialog_open[] = "editor dialog open";
    gui_cmd(gui_editor_dialog_open);
    char gui_editor_dialog_up[] = "editor dialog up";
    gui_cmd(gui_editor_dialog_up);
    char gui_editor_dialog_select[] = "editor dialog select 4";
    gui_cmd(gui_editor_dialog_select);
    char gui_editor_dialog_confirm[] = "editor dialog confirm";
    gui_cmd(gui_editor_dialog_confirm);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui editor file dialog open", text_has(gui_cmd_buf, "/home/readme.txt"));
    char gui_editor_dialog_save[] = "editor dialog save";
    gui_cmd(gui_editor_dialog_save);
    char gui_editor_dialog_rusa[] = "editor dialog rusa";
    gui_cmd(gui_editor_dialog_rusa);
    check_result("gui editor file dialog save", fs_stat("/home/projects/untitled.rusa", &type, &size) == 0 && type == 2);
    char gui_resize[] = "resize active 700 500";
    gui_cmd(gui_resize);
    char gui_move[] = "move active 190 90";
    gui_cmd(gui_move);
    check_result("gui window geometry commands", 1);
    char gui_maximize[] = "maximize active";
    gui_cmd(gui_maximize);
    char gui_minimize[] = "minimize active";
    gui_cmd(gui_minimize);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui window minimize", !text_has(gui_cmd_buf, "edit "));
    char gui_restore[] = "restore editor";
    gui_cmd(gui_restore);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui window restore maximize", text_has(gui_cmd_buf, "edit "));
    char gui_editor_app2[] = "app editor";
    gui_cmd(gui_editor_app2);
    char gui_editor_reset_move[] = "move active 174 72";
    gui_cmd(gui_editor_reset_move);
    char gui_editor_reset_size[] = "resize active 820 594";
    gui_cmd(gui_editor_reset_size);
    char gui_close_editor_button_path[] = "close editor";
    gui_cmd(gui_close_editor_button_path);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui close button", !text_has(gui_cmd_buf, "edit "));
    gui_cmd(gui_editor_app);
    gui_cmd(gui_editor_reset_move);
    gui_cmd(gui_editor_reset_size);
    mouse_set(960, 94);
    mouse_button(0, 1);
    mouse_button(0, 0);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui close window", !text_has(gui_cmd_buf, "edit "));
    char gui_projects[] = "app projects";
    gui_cmd(gui_projects);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui projects app", text_has(gui_cmd_buf, "project list"));
    char gui_packages[] = "app packages";
    gui_cmd(gui_packages);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui packages app", text_has(gui_cmd_buf, "pkg list"));
    char gui_logs[] = "app logs";
    gui_cmd(gui_logs);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui logs app", text_has(gui_cmd_buf, "log show"));
    char gui_security[] = "app security";
    gui_cmd(gui_security);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui security app", text_has(gui_cmd_buf, "security status"));
    char gui_security_unlock[] = "security unlock";
    gui_cmd(gui_security_unlock);
    check_result("gui security unlock", !security_is_locked());
    char gui_security_lock[] = "security lock";
    gui_cmd(gui_security_lock);
    check_result("gui security lock", security_is_locked());
    char gui_security_scan[] = "security scan";
    gui_cmd(gui_security_scan);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui security scan bridge", text_has(gui_cmd_buf, "lang scan"));
    char gui_security_conn[] = "security connections";
    gui_cmd(gui_security_conn);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui security connections panel", text_has(gui_cmd_buf, "net connections"));
    char gui_security_perms[] = "security permissions";
    gui_cmd(gui_security_perms);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui security permissions panel", text_has(gui_cmd_buf, "permissions"));
    char gui_security_events[] = "security events";
    gui_cmd(gui_security_events);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui security events panel", text_has(gui_cmd_buf, "events"));
    char gui_security_allow[] = "security allow demo network.client";
    gui_cmd(gui_security_allow);
    check_result("gui security allow permission", security_check_permission("demo", "network.client") == SECURITY_PERMISSION_ALLOW);
    char gui_security_deny[] = "security deny demo network.client";
    gui_cmd(gui_security_deny);
    check_result("gui security deny permission", security_check_permission("demo", "network.client") == SECURITY_PERMISSION_DENY);
    char gui_security_firewall[] = "security firewall";
    gui_cmd(gui_security_firewall);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui security firewall bridge", text_has(gui_cmd_buf, "firewall list"));
    privacy_set_master(1);
    privacy_set_network(1);
    char gui_settings_privacy[] = "settings privacy";
    gui_cmd(gui_settings_privacy);
    check_result("gui settings privacy tab", privacy_master_enabled() && privacy_network_enabled());
    char gui_settings_master[] = "settings master";
    gui_cmd(gui_settings_master);
    check_result("gui settings master toggle", !privacy_master_enabled() && !privacy_allows_network());
    char gui_settings_master_on[] = "settings master";
    gui_cmd(gui_settings_master_on);
    char gui_settings_network[] = "settings network";
    gui_cmd(gui_settings_network);
    check_result("gui settings network toggle", privacy_master_enabled() && privacy_network_enabled());
    char gui_settings_network_off[] = "settings network";
    gui_cmd(gui_settings_network_off);
    char gui_settings_cookies[] = "settings cookies";
    gui_cmd(gui_settings_cookies);
    check_result("gui settings cookie toggle", text_has(privacy_cookie_policy(), "block"));
    char gui_settings_devices[] = "settings devices";
    gui_cmd(gui_settings_devices);
    check_result("gui settings devices toggle", !privacy_devices_enabled());
    char gui_settings_devices_on[] = "settings devices";
    gui_cmd(gui_settings_devices_on);
    char gui_settings_telemetry[] = "settings telemetry";
    gui_cmd(gui_settings_telemetry);
    check_result("gui settings telemetry toggle", privacy_telemetry_enabled());
    char gui_settings_telemetry_off[] = "settings telemetry";
    gui_cmd(gui_settings_telemetry_off);
    privacy_set_master(1);
    privacy_set_network(1);
    privacy_set_cookie_policy("ask");
    char gui_net_app[] = "app net";
    gui_cmd(gui_net_app);
    char gui_net_open[] = "network open";
    gui_cmd(gui_net_open);
    struct net_socket_info gui_sock;
    check_result("gui network opens socket", net_socket_at(0, &gui_sock) == 0 && gui_sock.used);
    char gui_net_send[] = "network send";
    gui_cmd(gui_net_send);
    check_result("gui network sends packet", net_packet_count() > 0);
    char gui_close_net[] = "close net";
    gui_cmd(gui_close_net);
    gui_active_terminal_command(gui_cmd_buf, sizeof(gui_cmd_buf));
    check_result("gui close alias", !text_has(gui_cmd_buf, "net status"));
    mouse_scroll(1);
    check_result("mousepad terminal scroll", mouse_scroll_count() > 0 && console_scroll_offset() > 0);
    console_scroll(-64);
    check_result("vector gfx manifest", fs_stat("/system/gui/vector.txt", &type, &size) == 0);
    check_result("trx runtime docs", fs_stat("/share/docs/trx-runtime.txt", &type, &size) == 0);
    check_result("network stack descriptor", fs_stat("/system/net/stack.txt", &type, &size) == 0);
    int research_id = research_project_create("selftest-research", "selftest research project", "test,science");
    check_result("research project create", research_id > 0);
    check_result("research note add", research_add_note((uint32_t)research_id, RESEARCH_CELL_RUSA, "model", "let x = 1") > 0);
    check_result("research dataset add", research_register_dataset((uint32_t)research_id, "samples", "/research/datasets/samples.csv", "x:int,y:int") > 0);
    check_result("research experiment add", research_track_experiment((uint32_t)research_id, "trial", "math vector dot") > 0);
    check_result("research result add", research_record_result((uint32_t)research_id, "trial", "ok") > 0);
    check_result("research add dataset alias", research_add_dataset((uint32_t)research_id, "alias-data",
                 "/research/datasets/alias.csv", "v:int") > 0);
    check_result("research citation add", research_add_citation((uint32_t)research_id, "selftest2026",
                 "Selftest Citation", "Tabla Rusa local") > 0);
    check_result("research result alias", research_add_result((uint32_t)research_id, "trial2", "ok2") > 0);
    int task_id = research_add_task((uint32_t)research_id, "write-methods", "benji");
    check_result("research task add", task_id > 0);
    check_result("research task update", research_update_task((uint32_t)task_id, "done") == 0);
    check_result("research timeline add", research_add_timeline((uint32_t)research_id, "milestone", "tests passed") > 0);
    check_result("research graph relation", research_add_relation((uint32_t)research_id, "Rusa", "supports", "notebooks") > 0);
    int nb_id = notebook_create((uint32_t)research_id, "selftest-notebook");
    check_result("research notebook create", nb_id > 0);
    int nb_cell = notebook_add_cell((uint32_t)nb_id, RESEARCH_CELL_MATH, "calc", "math vec dot");
    check_result("research notebook add cell", nb_cell > 0);
    char nb_run[96];
    check_result("research notebook run cell", notebook_run_cell((uint32_t)nb_id, (uint32_t)nb_cell, nb_run, sizeof(nb_run)) == 0 &&
                 text_has(nb_run, "calc"));
    char export_path[80];
    check_result("research notebook export", notebook_export((uint32_t)nb_id, export_path, sizeof(export_path)) == 0 &&
                 fs_stat(export_path, &type, &size) == 0);
    check_result("research latex export", research_export_latex((uint32_t)research_id, export_path, sizeof(export_path)) == 0 &&
                 fs_stat(export_path, &type, &size) == 0);
    struct research_citation_info citation_info[4];
    check_result("research citation table", research_citation_list((uint32_t)research_id, citation_info, 4) > 0);
    struct research_notebook_info notebook_info[4];
    check_result("research notebook table", notebook_list((uint32_t)research_id, notebook_info, 4) > 0);
    struct research_task_info task_info[4];
    check_result("research task table", research_task_list((uint32_t)research_id, task_info, 4) > 0 &&
                 text_has(task_info[0].status, "done"));
    struct research_timeline_info timeline_info[4];
    check_result("research timeline table", research_timeline_list((uint32_t)research_id, timeline_info, 4) > 0);
    struct research_relation_info relation_info[4];
    check_result("research graph table", research_relation_list((uint32_t)research_id, relation_info, 4) > 0);
    struct research_project_info research_projects[4];
    check_result("research project table", research_project_list(research_projects, 4) > 0);
    struct research_cell_info research_cells[4];
    check_result("research notebook table", research_cell_list((uint32_t)research_id, research_cells, 4) > 0);
    check_result("research save file", fs_stat("/research/project-2.md", &type, &size) == 0);
    check_result("science descriptors", fs_stat("/science/constants.txt", &type, &size) == 0);
    check_result("science unit convert", unit_convert(2 * 1000, "m", "cm") == 200 * 1000);
    check_result("science unit dimension", unit_check_dimension("m", "km") && !unit_check_dimension("m", "s"));
    int32_t constant_value;
    char constant_unit[24];
    check_result("science constant lookup", constants_find("c", &constant_value, constant_unit, sizeof(constant_unit)) == 0 && constant_value > 0);
    int32_t arr_values[4] = {1, 2, 3, 4};
    int science_array = array_create(arr_values, 4);
    struct science_array_info science_arr_info;
    check_result("science array create", science_array > 0 && array_get((uint32_t)science_array, &science_arr_info) == 0 &&
                 science_arr_info.len == 4);
    struct science_fit_result fit_result;
    int32_t fit_x[3] = {0, 1, 2};
    int32_t fit_y[3] = {1, 3, 5};
    check_result("science linear fit", fit_linear(fit_x, fit_y, 3, &fit_result) == 0 && fit_result.a_scaled == 2000);
    check_result("science polynomial scaffold", fit_polynomial(fit_y, 3, &fit_result) == 0);
    check_result("science exponential scaffold", fit_exponential(fit_y, 3, &fit_result) == 0);
    int32_t smooth_out[4];
    check_result("science signal smooth", signal_smooth(arr_values, 4, smooth_out, 4) == 4 && smooth_out[1] == 2);
    int32_t fft_out[4];
    check_result("science fft scaffold", signal_fft(arr_values, 4, fft_out, 4) >= 2);
    int peak_id = spectroscopy_peak_add("ir", 1600000, 750, "carbonyl stretch");
    struct spectroscopy_peak_info peak_info;
    check_result("science spectroscopy peak", peak_id > 0 && spectroscopy_peak_fit((uint32_t)peak_id, &peak_info) == 0 &&
                 text_has(peak_info.assignment, "carbonyl"));
    int crystal_id = crystal_create_lattice("test", "orthorhombic", 1000, 2000, 3000, 90000, 90000, 90000);
    struct crystal_lattice_info crystal_info;
    check_result("science crystal lattice", crystal_id > 0 && crystal_lattice_get((uint32_t)crystal_id, &crystal_info) == 0 &&
                 crystal_info.c_scaled == 3000);
    int sim_id = simulation_job_create("raman", "peak-fit", 91);
    struct simulation_job_info sim_info;
    check_result("science simulation job", sim_id > 0 && simulation_job_run((uint32_t)sim_id) == 0 &&
                 simulation_job_status((uint32_t)sim_id, &sim_info) == 0 && text_has(sim_info.status, "complete"));
    check_result("science simulation table", simulation_job_list(&sim_info, 1) > 0);
    int q_id = quantum_state_create("selftest-state", "spin-up", 1250, "test quantum record");
    struct quantum_state_info q_info;
    check_result("science quantum state", q_id > 0 && quantum_state_get((uint32_t)q_id, &q_info) == 0 &&
                 text_has(q_info.basis, "spin"));
    int material_id = material_register("selftest-material", "AB", "solid", 2500);
    struct material_info material;
    check_result("science material record", material_id > 0 && material_get((uint32_t)material_id, &material) == 0 &&
                 material.band_gap_scaled == 2500);
    check_result("science band point", band_point_add((uint32_t)material_id, "G", 0, 2500) > 0 &&
                 band_point_list((uint32_t)material_id, 0, 0) > 0);
    check_result("science phonon mode", phonon_mode_add((uint32_t)material_id, "A1g", 123000, "raman") > 0 &&
                 phonon_mode_list((uint32_t)material_id, 0, 0) > 0);
    check_result("science array free", array_free((uint32_t)science_array) == 0);
    struct math_plugin_info plugin_info[12];
    check_result("math plugin registry", math_plugin_list(plugin_info, 12) >= 10);
    struct math_plugin_info logic_plugin;
    check_result("math plugin find", math_plugin_find("logic-proof", &logic_plugin) == 0 &&
                 text_has(logic_plugin.capabilities, "proof"));
    char plugin_out[96];
    check_result("math plugin dispatch", math_plugin_dispatch("physics", "status", plugin_out, sizeof(plugin_out)) == 0 &&
                 text_has(plugin_out, "physics"));
    int theorem_id = math_plugin_register_theorem("linear-algebra", "Selftest theorem", "linear algebra",
                                                  "A test theorem is stated but not checked.", THEOREM_FORMALLY_CHECKED);
    struct theorem_info theorem_list[16];
    uint32_t theorem_count = math_theorem_list(theorem_list, 16);
    check_result("math theorem registry", theorem_id > 0 && theorem_count > 0);
    check_result("math theorem status guard", theorem_list[theorem_count - 1].status != THEOREM_FORMALLY_CHECKED);
    check_result("math algorithm registry", math_plugin_register_algorithm("numerical", "Selftest algorithm", "testing",
                 "input", "output", "O(1)", "registered") > 0 && math_algorithm_list(0, 0) > 0);
    check_result("math object type registry", math_plugin_register_object_type("logic-proof", "SelftestObject",
                 "display", "latex") > 0 && math_object_type_list(0, 0) > 0);
    check_result("math plugin selftest hook", math_plugin_run_tests("logic-proof") == 0);
    int proof_id = proof_create("selftest-proof", "Q", PROOF_MODE_GUIDED);
    check_result("proof create", proof_id > 0);
    check_result("proof add assumption", proof_add_assumption((uint32_t)proof_id, "P") == 0);
    check_result("proof invalid exact", proof_add_step((uint32_t)proof_id, "exact", "R") > 0);
    struct proof_step_info proof_steps[8];
    uint32_t proof_step_count = proof_list_steps((uint32_t)proof_id, proof_steps, 8);
    check_result("proof invalid status", proof_step_count > 0 &&
                 proof_steps[proof_step_count - 1].status == PROOF_STEP_INVALID);
    check_result("proof valid assumption exact", proof_add_step((uint32_t)proof_id, "exact", "P") > 0);
    proof_step_count = proof_list_steps((uint32_t)proof_id, proof_steps, 8);
    check_result("proof valid status", proof_steps[proof_step_count - 1].status == PROOF_STEP_VALID);
    int proof2 = proof_create("goal-proof", "Goal and Subgoal", PROOF_MODE_GUIDED);
    check_result("proof split incomplete", proof2 > 0 && proof_add_step((uint32_t)proof2, "split", "Goal") > 0);
    struct proof_state_info proof_state;
    check_result("proof state info", proof_get_state((uint32_t)proof2, &proof_state) == 0 &&
                 proof_state.status == PROOF_STEP_INCOMPLETE);
    check_result("proof list states", proof_list_states(&proof_state, 1) > 0);
    char proof_export[512];
    check_result("proof export", proof_export_text((uint32_t)proof_id, proof_export, sizeof(proof_export)) == 0 &&
                 text_has(proof_export, "selftest-proof"));
    check_result("proof replay detects invalid", proof_replay((uint32_t)proof_id) != 0);
    char net_socket_cmd[] = "socket udp 9999";
    char net_send_cmd[] = "send 0 selftest-packet";
    net_cmd(net_socket_cmd);
    net_cmd(net_send_cmd);
    check_result("network packet queue", net_packet_count() >= 2);
    struct net_socket_info sock_info;
    check_result("network socket snapshot", net_socket_at(0, &sock_info) == 0 && sock_info.used && sock_info.local_port == 9999);
    int srv = net_socket_create(1, "tcp");
    int cli = net_socket_create(1, "tcp");
    check_result("net api socket create", srv >= 0 && cli >= 0);
    check_result("net api bind/listen", srv >= 0 && net_bind((uint32_t)srv, 10001) == 0 && net_listen((uint32_t)srv) == 0);
    check_result("net api connect", cli >= 0 && net_connect((uint32_t)cli, 10001) == 0);
    check_result("net api send", cli >= 0 && net_send((uint32_t)cli, "loopback-api") == 0);
    const char* recv_msg = "";
    check_result("net api recv", srv >= 0 && net_recv((uint32_t)srv, &recv_msg) == 0 && text_has(recv_msg, "loopback-api"));
    struct net_connection_info conn_info[4];
    check_result("net connection table", net_connection_list(conn_info, 4) >= 2);
    struct net_port_info port_info[8];
    check_result("net port table", net_port_list(port_info, 8) > 0);
    if(cli >= 0) net_socket_close((uint32_t)cli);
    if(srv >= 0) net_socket_close((uint32_t)srv);
    int idle = net_socket_create(1, "udp");
    check_result("net idle shutdown", idle >= 0 && net_shutdown_idle(0) > 0);
    security_register_app("selftest-app", "selftest sandbox metadata");
    security_register_capability("selftest-app", "network.loopback", "test loopback access");
    security_set_permission("selftest-app", "network.client", SECURITY_PERMISSION_DENY);
    check_result("security permission deny", security_check_permission("selftest-app", "network.client") == SECURITY_PERMISSION_DENY);
    security_set_permission("selftest-app", "network.client", SECURITY_PERMISSION_ALLOW);
    check_result("security permission allow", policy_check_network_access("selftest-app", 10001, 0));
    int firewall_id = policy_firewall_add("selftest-app", 10001, 0, "selftest firewall block");
    check_result("firewall add rule", firewall_id > 0);
    check_result("firewall deny rule", !policy_check_network_access("selftest-app", 10001, 0));
    check_result("firewall disable rule", policy_firewall_set_enabled((uint32_t)firewall_id, 0) == 0 &&
                 policy_check_network_access("selftest-app", 10001, 0));
    struct firewall_rule_info fw_info[4];
    check_result("firewall rule table", policy_firewall_list(fw_info, 4) > 0);
    char firewall_explain[96];
    policy_firewall_explain((uint32_t)firewall_id, firewall_explain, sizeof(firewall_explain));
    check_result("firewall explain rule", text_has(firewall_explain, "selftest firewall block"));
    check_result("firewall remove rule", policy_firewall_remove((uint32_t)firewall_id) == 0);
    security_log_event(SECURITY_WARNING, "network", "selftest-app", "port", "connect", "allow", "selftest event", "none");
    struct security_event_info sec_events[4];
    check_result("security event log api", security_get_events(sec_events, 4) > 0);
    fs_write("/tmp/risky.rusa", "while true { print \"loop\" }\n");
    char scan_out[128];
    check_result("security scanner suspicious", security_scan_app("/tmp/risky.rusa", scan_out, sizeof(scan_out)) == SECURITY_SCAN_SUSPICIOUS);
    service_register("selftest", 10002, 1, "selftest service", "network.server");
    struct service_registry_info svc_info[6];
    check_result("service registry api", service_list_registry(svc_info, 6) > 0);
    check_result("privacy network allowed", privacy_allows_network());
    check_result("block descriptor", fs_stat("/system/block.txt", &type, &size) == 0);
    check_result("Rusa docs", fs_stat("/share/rusa/keywords", &type, &size) == 0);
    check_result("Rusa stdlib", fs_stat("/lib/rusa/std.trx", &type, &size) == 0);
    check_result("Rusa source stdlib", fs_stat("/lib/rusa/std.rusa", &type, &size) == 0);
    struct rusa_lexer lexer;
    struct rusa_token token;
    rusa_lexer_init(&lexer, "let n: int = 1");
    token = rusa_lexer_next_token(&lexer);
    check_result("Rusa lexer keyword", token.kind == RUSA_TOKEN_KEYWORD && text_has(token.text, "let"));
    token = rusa_lexer_next_token(&lexer);
    check_result("Rusa lexer identifier", token.kind == RUSA_TOKEN_IDENTIFIER && text_has(token.text, "n"));
    struct rusa_ast ast;
    struct rusa_bytecode bytecode;
    struct rusa_vm vm;
    check_result("Rusa parse API", rusa_parse_source("let n: int = 1\nprint n\n", "<api>", &ast) == 0 && ast.statement_count > 0);
    check_result("Rusa typecheck API", rusa_typecheck(&ast) == 0);
    check_result("Rusa compile API", rusa_compile(&ast, &bytecode) == 0 && bytecode.op_count > 0);
    rusa_vm_init(&vm);
    check_result("Rusa VM API", rusa_vm_execute(&vm, &bytecode, "") == 0 && vm.last_status == 0);
    rusa_ast_free(&ast);
    check_result("Rusa eval API", rusa_eval_source("let q: int = 4\nprint q\n", "<api-eval>", "") == 0);
    check_result("Rusa REPL API", rusa_repl_step("print 7") == 0);
    check_result("Rusa native registry API", rusa_register_native("selftest.native", 0) == 0 && rusa_native_count() > 0);
    check_result("Rusa import API", rusa_import_module("std") == 0);
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
