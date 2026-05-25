#include "process.h"
#include "console.h"

static struct process_info processes[PROCESS_MAX] = {
    {"kernel", 0, 1, "kernel", 100, 0, "system", 0},
    {"shell", 1, 1, "user", 40, 0, "interactive", 0},
    {"editor", 2, 0, "user", 35, 0, "interactive", 0},
    {"logger", 3, 1, "service", 20, 0, "io", 0},
    {"network", 4, 0, "service", 55, 0, "io", 0},
    {"compute", 5, 0, "service", 90, 0, "scientific", 0}
};

static int proc_is(const char* a, const char* b){
    while(*a && *b){
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
        if(ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

void process_init(void){
    for(int i=0; i<PROCESS_MAX; i++){
        if(proc_is(processes[i].name, "kernel") || proc_is(processes[i].name, "shell") ||
           proc_is(processes[i].name, "logger"))
            processes[i].running = 1;
        else
            processes[i].running = 0;
    }
}

struct process_info* process_find(const char* name){
    for(int i=0; i<PROCESS_MAX; i++)
        if(proc_is(processes[i].name, name))
            return &processes[i];
    return 0;
}

void process_set_running(const char* name, int running){
    struct process_info* proc = process_find(name);
    if(proc)
        proc->running = running;
}

void process_set_compute(const char* name, const char* workload, uint32_t priority, uint32_t cpu_hint){
    struct process_info* proc = process_find(name);
    if(!proc)
        return;
    proc->workload = workload;
    proc->priority = priority;
    proc->cpu_hint = cpu_hint;
}

void process_tick(const char* name, uint32_t ticks){
    struct process_info* proc = process_find(name);
    if(proc)
        proc->ticks += ticks;
}

void process_list(void){
    for(int i=0; i<PROCESS_MAX; i++){
        console_write_dec(processes[i].pid);
        console_puts("  ");
        console_puts(processes[i].running ? "run " : "stop ");
        console_puts(processes[i].kind);
        console_puts("  ");
        console_puts(processes[i].name);
        console_puts(" prio=");
        console_write_dec(processes[i].priority);
        console_puts(" class=");
        console_puts(processes[i].workload);
        console_putc('\n');
    }
}

void process_compute_report(void){
    console_puts("scheduler profile: scientific-first cooperative table\n");
    console_puts("policy: prefer scientific/vector workloads, keep shell interactive\n");
    for(int i=0; i<PROCESS_MAX; i++){
        if(proc_is(processes[i].workload, "scientific") || proc_is(processes[i].workload, "vector")){
            console_puts(processes[i].name);
            console_puts(" running=");
            console_puts(processes[i].running ? "yes" : "no");
            console_puts(" priority=");
            console_write_dec(processes[i].priority);
            console_puts(" cpu_hint=");
            console_write_dec(processes[i].cpu_hint);
            console_puts(" ticks=");
            console_write_dec(processes[i].ticks);
            console_putc('\n');
        }
    }
}

int process_stop(const char* name){
    struct process_info* proc = process_find(name);
    if(proc == 0 || proc_is(proc->name, "kernel") || proc_is(proc->name, "shell"))
        return -1;
    proc->running = 0;
    return 0;
}
