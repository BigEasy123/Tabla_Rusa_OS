#include "timer.h"
#include "sched.h"

#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40
#define PIT_BASE_HZ 1193182U

static volatile uint32_t ticks = 0;
static uint32_t timer_hz = 100;

static inline void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port){
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

void timer_init(uint32_t hz){
    if(hz == 0)
        hz = 100;
    timer_hz = hz;
    uint32_t divisor = PIT_BASE_HZ / hz;

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t mask = inb(0x21);
    mask &= ~0x01;
    outb(0x21, mask);
}

void timer_handler(void){
    ticks++;
    sched_on_timer();
}

uint32_t timer_ticks(void){
    return ticks;
}

uint32_t timer_uptime_seconds(void){
    return ticks / timer_hz;
}

void sleep_ticks(uint32_t wait_ticks){
    uint32_t target = ticks + wait_ticks;
    while(ticks < target)
        __asm__ __volatile__("hlt");
}
