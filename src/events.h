#ifndef EVENTS_H
#define EVENTS_H

void events_init(void);
void events_set_source_handler(void (*handler)(const char* source));
int events_register_source(const char* name, const char* trigger, const char* source);
void events_emit(const char* trigger);
void events_cmd(char* arg);

#endif
