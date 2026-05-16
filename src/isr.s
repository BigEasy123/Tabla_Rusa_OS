# 0 "src/isr.S"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "src/isr.S"
; ISR wrapper for keyboard (IRQ1)
global keyboard_isr_stub
extern keyboard_handler

keyboard_isr_stub:
    pusha
    call keyboard_handler
    popa
    iretd
