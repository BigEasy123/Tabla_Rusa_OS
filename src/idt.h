#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* -------- IDT entry -------- */
struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

/* -------- IDT pointer -------- */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* -------- IDT functions -------- */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);
void pic_remap(void);

/* implemented in assembly */
extern void idt_load(void);
extern void irq1_stub(void);

#endif
