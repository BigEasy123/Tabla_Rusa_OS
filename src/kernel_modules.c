#include "fs.h"
#include "kernel_modules.h"

void kernel_modules_init(void){
    fs_mkdir("/system/kernel");
    fs_write("/system/kernel/modules.txt",
        "kernel: kmain boot orchestration and compatibility dispatch\n"
        "shell: shell session, history, line editing, eval bridge\n"
        "editor: text editor command state\n"
        "gui: framebuffer desktop, windows, apps, input routing\n"
        "lang: Rusa parser, runtime, diagnostics, events\n"
        "fs: RAM filesystem, VFS, block, file descriptors\n"
        "process: process table, scheduler, jobs, task manager\n"
        "net: loopback packets, socket handles, privacy shield\n"
        "security: users, capabilities, audit, privacy center\n"
        "math: math/physics/proof commands and compute accounting\n");
    fs_write("/system/kernel/boundaries.txt",
        "rule: new app behavior belongs in gui.c or an app module, not kmain.c\n"
        "rule: command state belongs in shell/editor/lang/taskman modules\n"
        "rule: kernel boot should initialize modules and expose descriptors\n"
        "next: migrate package and legacy shell command bodies out of kmain.c\n");
}
