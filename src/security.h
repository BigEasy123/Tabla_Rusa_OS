#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>

enum security_severity {
    SECURITY_INFO,
    SECURITY_WARNING,
    SECURITY_CRITICAL
};

enum security_scan_result {
    SECURITY_SCAN_CLEAN,
    SECURITY_SCAN_SUSPICIOUS,
    SECURITY_SCAN_BLOCKED,
    SECURITY_SCAN_UNKNOWN
};

enum security_permission_decision {
    SECURITY_PERMISSION_ALLOW,
    SECURITY_PERMISSION_DENY,
    SECURITY_PERMISSION_ASK,
    SECURITY_PERMISSION_INHERITED,
    SECURITY_PERMISSION_DEFAULT_DENY
};

struct security_event_info {
    uint32_t id;
    uint32_t tick;
    enum security_severity severity;
    const char* category;
    const char* app;
    const char* resource;
    const char* action;
    const char* decision;
    const char* explanation;
    const char* suggestion;
};

void security_init(void);
void security_log_event(enum security_severity severity, const char* category, const char* app,
                        const char* resource, const char* action, const char* decision,
                        const char* explanation, const char* suggestion);
uint32_t security_get_events(struct security_event_info* out, uint32_t max);
int security_register_app(const char* app, const char* summary);
int security_register_capability(const char* app, const char* capability, const char* explanation);
uint32_t security_list_capabilities(const char* app, char* out, uint32_t max);
int security_set_permission(const char* app, const char* resource, enum security_permission_decision decision);
enum security_permission_decision security_check_permission(const char* app, const char* resource);
uint32_t security_list_permissions(const char* app, char* out, uint32_t max);
enum security_scan_result security_scan_app(const char* path, char* out, uint32_t max);
const char* security_current_user(void);
int security_is_locked(void);
int security_can_write(const char* path);
int security_can_service_control(void);
int security_can_install_kernel_package(const char* pkg_name);
void security_audit(const char* message);
void security_cmd(char* arg);
void security_user_cmd(char* arg);
void security_cap_cmd(char* arg);

#endif
