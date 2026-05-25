#include "console.h"
#include "shell.h"

void shell_intro(void){
    console_puts(" _______    _     _         ____                  \n");
    console_puts("|__   __|  | |   | |       |  _ \\                 \n");
    console_puts("   | | __ _| |__ | | __ _  | |_) |_   _ ___  __ _ \n");
    console_puts("   | |/ _` | '_ \\| |/ _` | |  _ <| | | / __|/ _` |\n");
    console_puts("   | | (_| | |_) | | (_| | | |_) | |_| \\__ \\ (_| |\n");
    console_puts("   |_|\\__,_|_.__/|_|\\__,_| |____/ \\__,_|___/\\__,_|\n");
    console_puts("Tabla Rusa OS 0.0.6 - native shell, math kernel, live objects\n");
}

void shell_structure_cmd(void){
    console_puts("structure:\n");
    console_puts("  kernel: memory paging idt timer keyboard heap\n");
    console_puts("  runtime: shell object-dispatch events jobs scheduler process-table\n");
    console_puts("  storage: vfs ramfs descriptors procfs sysfs devfs pkgfs mathfs\n");
    console_puts("  services: security logger package network gui events taskman\n");
    console_puts("  science: math objects physics job queue latex converter benchmarks\n");
    console_puts("  graphics: vga text desktop vector paths framebuffer descriptor\n");
    console_puts("  execution: /bin TRX1 bytecode loader run PROGRAM\n");
    console_puts("next good layers: ELF binary loader, framebuffer pixels, sockets API, disk\n");
}
