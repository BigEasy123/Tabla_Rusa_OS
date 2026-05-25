#ifndef JOBS_H
#define JOBS_H

void jobs_init(void);
void jobs_account(const char* name, uint32_t ticks);
void jobs_cmd(char* arg);

#endif
