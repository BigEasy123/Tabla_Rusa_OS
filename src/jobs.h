#ifndef JOBS_H
#define JOBS_H

#include <stdint.h>

struct job_info {
    int active;
    const char* name;
    const char* class_name;
    uint32_t priority;
    uint32_t ticks;
    const char* state;
};

void jobs_init(void);
void jobs_account(const char* name, uint32_t ticks);
const struct job_info* jobs_at(uint32_t index);
uint32_t jobs_count(void);
void jobs_cmd(char* arg);

#endif
