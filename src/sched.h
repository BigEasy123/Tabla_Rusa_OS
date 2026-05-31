#ifndef SCHED_H
#define SCHED_H

void sched_init(void);
void sched_on_timer(void);
void sched_yield(void);
const char* sched_current_name(void);
uint32_t sched_total_switches(void);
void sched_cmd(char* arg);

#endif
