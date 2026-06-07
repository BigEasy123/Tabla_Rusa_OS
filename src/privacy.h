#ifndef PRIVACY_H
#define PRIVACY_H

void privacy_init(void);
int privacy_allows_network(void);
int privacy_master_enabled(void);
int privacy_network_enabled(void);
int privacy_devices_enabled(void);
int privacy_telemetry_enabled(void);
const char* privacy_cookie_policy(void);
void privacy_set_master(int enabled);
void privacy_set_network(int enabled);
void privacy_set_devices(int enabled);
void privacy_set_telemetry(int enabled);
void privacy_set_cookie_policy(const char* policy);
void privacy_cmd(char* arg);

#endif
