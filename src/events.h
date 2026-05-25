#ifndef EVENTS_H
#define EVENTS_H

void events_init(void);
void events_emit(const char* trigger);
void events_cmd(char* arg);

#endif
