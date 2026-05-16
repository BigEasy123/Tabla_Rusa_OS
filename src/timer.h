#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t hz);
void timer_handler(void);
uint32_t timer_ticks(void);
uint32_t timer_uptime_seconds(void);
void sleep_ticks(uint32_t ticks);

#endif
