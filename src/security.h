#ifndef SECURITY_H
#define SECURITY_H

void security_init(void);
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
