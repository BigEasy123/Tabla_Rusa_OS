#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_MAX 6

struct process_info {
    const char* name;
    uint32_t pid;
    int running;
    const char* kind;
    uint32_t priority;
    uint32_t cpu_hint;
    const char* workload;
    uint32_t ticks;
    uint32_t switches;
    uint32_t last_run_tick;
};

void process_init(void);
struct process_info* process_find(const char* name);
void process_set_running(const char* name, int running);
void process_set_compute(const char* name, const char* workload, uint32_t priority, uint32_t cpu_hint);
void process_tick(const char* name, uint32_t ticks);
void process_context_switch(const char* name, uint32_t tick);
void process_list(void);
void process_compute_report(void);
int process_stop(const char* name);

#endif
