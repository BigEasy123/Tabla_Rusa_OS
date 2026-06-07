#ifndef POLICY_H
#define POLICY_H

#include <stdint.h>

struct firewall_rule_info {
    uint32_t id;
    int enabled;
    char app[24];
    uint32_t port;
    int allow;
    char explanation[96];
};

int policy_firewall_add(const char* app, uint32_t port, int allow, const char* explanation);
int policy_firewall_remove(uint32_t id);
int policy_firewall_set_enabled(uint32_t id, int enabled);
uint32_t policy_firewall_list(struct firewall_rule_info* out, uint32_t max);
int policy_firewall_test(const char* app, uint32_t port, int server);
void policy_firewall_explain(uint32_t id, char* out, uint32_t max);
int policy_check_network_access(const char* app, uint32_t port, int server);
int policy_check_file_access(const char* app, const char* path, int write);
int policy_check_device_access(const char* app, const char* device);
int policy_check_process_access(const char* app, const char* process, const char* action);
void policy_cmd(char* arg);

#endif
