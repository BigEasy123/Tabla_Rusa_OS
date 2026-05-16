[BITS 32]

; -----------------------------
; IDT loader
; -----------------------------
global idt_load
extern idt_ptr

idt_load:
    lidt [idt_ptr]
    ret

; -----------------------------
; Common interrupt entry
; -----------------------------
extern isr_common_handler

isr_common_stub:
    pusha
    push esp
    call isr_common_handler
    add esp, 4
    popa
    add esp, 8
    iretd

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1_stub
irq%1_stub:
    push dword 0
    push dword %2
    jmp isr_common_stub
%endmacro

; CPU exceptions. The CPU pushes an error code for 8, 10-14, 17, 21, and 29-30.
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31

; PIC IRQs after remap: IRQ0-15 become vectors 32-47.
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47
