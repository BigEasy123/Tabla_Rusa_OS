#ifndef PRIVACY_H
#define PRIVACY_H

void privacy_init(void);
int privacy_allows_network(void);
const char* privacy_cookie_policy(void);
void privacy_cmd(char* arg);

#endif
