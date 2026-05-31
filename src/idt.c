#include "idt.h"
#include "keyboard.h"
#include "mouse.h"
#include "timer.h"
#include "console.h"
#include <stdint.h>

#define IDT_PRESENT_RING0_INT_GATE 0x8E
#define KERNEL_CODE_SELECTOR 0x10

/* -------- IDT & PIC -------- */
struct idt_entry idt[256];
struct idt_ptr idt_ptr;

/* -------- I/O ports -------- */
static inline void outb(uint16_t port, uint8_t val){
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern void idt_load(void);
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void irq0_stub(void);  extern void irq1_stub(void);  extern void irq2_stub(void);  extern void irq3_stub(void);
extern void irq4_stub(void);  extern void irq5_stub(void);  extern void irq6_stub(void);  extern void irq7_stub(void);
extern void irq8_stub(void);  extern void irq9_stub(void);  extern void irq10_stub(void); extern void irq11_stub(void);
extern void irq12_stub(void); extern void irq13_stub(void); extern void irq14_stub(void); extern void irq15_stub(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags){
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

static void idt_install_stubs(void){
    void (*exceptions[32])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    void (*irqs[16])(void) = {
        irq0_stub, irq1_stub, irq2_stub, irq3_stub,
        irq4_stub, irq5_stub, irq6_stub, irq7_stub,
        irq8_stub, irq9_stub, irq10_stub, irq11_stub,
        irq12_stub, irq13_stub, irq14_stub, irq15_stub
    };

    for(int i=0;i<32;i++)
        idt_set_gate((uint8_t)i, (uint32_t)exceptions[i], KERNEL_CODE_SELECTOR, IDT_PRESENT_RING0_INT_GATE);
    for(int i=0;i<16;i++)
        idt_set_gate((uint8_t)(32+i), (uint32_t)irqs[i], KERNEL_CODE_SELECTOR, IDT_PRESENT_RING0_INT_GATE);
}

void idt_init(void){
    idt_ptr.limit = sizeof(idt)-1;
    idt_ptr.base = (uint32_t)&idt;
    for(int i=0;i<256;i++) idt_set_gate(i,0,0,0);
    idt_install_stubs();
    idt_load();
}

/* -------- Fault and IRQ dispatch -------- */
struct interrupt_frame {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} __attribute__((packed));

static const char* exception_names[32] = {
    "Divide by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode", "Device unavailable",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS", "Segment not present",
    "Stack-segment fault", "General protection fault", "Page fault", "Reserved",
    "x87 floating-point exception", "Alignment check", "Machine check", "SIMD floating-point exception",
    "Virtualization exception", "Control protection exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor injection exception", "VMM communication exception", "Security exception", "Reserved"
};

void isr_common_handler(struct interrupt_frame* frame){
    if(frame->int_no < 32){
        __asm__ __volatile__("cli");
        console_clear();
        console_puts("Tabla Rusa OS kernel panic\n\n");
        console_puts("Exception: ");
        console_puts(exception_names[frame->int_no]);
        console_puts("\nVector:    ");
        console_write_hex(frame->int_no);
        console_puts("\nError:     ");
        console_write_hex(frame->err_code);
        console_puts("\nEIP:       ");
        console_write_hex(frame->eip);
        console_puts("\nCS:        ");
        console_write_hex(frame->cs);
        console_puts("\nEFLAGS:    ");
        console_write_hex(frame->eflags);
        for(;;) __asm__ __volatile__("hlt");
    }

    if(frame->int_no == 32)
        timer_handler();
    else if(frame->int_no == 33)
        keyboard_handler();
    else if(frame->int_no == 44)
        mouse_handler();

    if(frame->int_no >= 40)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

/* -------- PIC Remap -------- */
static inline void io_wait(void){ __asm__ __volatile__("outb %%al,$0x80" : : "a"(0)); }

void pic_remap(void){
    outb(0x20,0x11); io_wait(); // ICW1
    outb(0xA0,0x11); io_wait();
    outb(0x21,0x20); io_wait(); // master offset
    outb(0xA1,0x28); io_wait(); // slave offset
    outb(0x21,0x04); io_wait(); // ICW3
    outb(0xA1,0x02); io_wait();
    outb(0x21,0x01); io_wait(); // ICW4
    outb(0xA1,0x01); io_wait();

    // Keep IRQs masked until a handler is installed for each one.
    outb(0x21,0xFF); io_wait();
    outb(0xA1,0xFF); io_wait();
}
