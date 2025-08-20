.section .multiboot
    .align 4
    .long 0x1BADB002        # magic
    .long 0x0               # flags
    .long -(0x1BADB002)     # checksum

.section .text
.global _start
_start:
    mov $kernel_stack_top, %esp   # set up stack
    call kernel_main              # call C kernel
.hang:
    jmp .hang                     # loop forever

.section .bss
    .space 16384                   # 16 KB stack
kernel_stack_top:
