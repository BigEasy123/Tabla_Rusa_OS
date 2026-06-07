#ifndef SERVICE_H
#define SERVICE_H

#include <stdint.h>

struct service_registry_info {
    uint32_t id;
    const char* name;
    uint32_t port;
    uint32_t owner_pid;
    const char* state;
    const char* description;
    const char* permission;
    const char* health;
};

void service_init(void);
int service_register(const char* name, uint32_t port, uint32_t owner_pid,
                     const char* description, const char* permission);
uint32_t service_list_registry(struct service_registry_info* out, uint32_t max);
void service_set_running(const char* name, int running);
int service_is_running(const char* name);
void service_cmd(char* arg);

#endif
