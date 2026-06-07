#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

typedef void (*sched_task_entry_fn)(const char* name, uint32_t quantum);

void sched_init(void);
void sched_on_timer(void);
void sched_yield(void);
const char* sched_current_name(void);
uint32_t sched_total_switches(void);
int sched_register_task(const char* name, uint32_t quantum, sched_task_entry_fn entry);
int sched_task_ready(const char* name);
uint32_t sched_task_runs(const char* name);
void sched_cmd(char* arg);
void scheduler_init(void);
void scheduler_tick(void);
const char* scheduler_pick_next(void);
int scheduler_set_priority(const char* name, uint32_t priority);

#endif
