#ifndef SERVICE_H
#define SERVICE_H

void service_init(void);
void service_set_running(const char* name, int running);
int service_is_running(const char* name);
void service_cmd(char* arg);

#endif
