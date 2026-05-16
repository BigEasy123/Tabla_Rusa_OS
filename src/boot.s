/* Build with: nasm -f elf32 src/boot.S -o build/boot.o */
BITS 32
SECTION .multiboot
align 8
MB2_MAGIC      equ 0xe85250d6
MB2_ARCH_I386  equ 0
MB2_LEN        equ (mb2_end - mb2_start)
MB2_CHECKSUM   equ -(MB2_MAGIC + MB2_ARCH_I386 + MB2_LEN)

mb2_start:
    dd MB2_MAGIC
    dd MB2_ARCH_I386
    dd MB2_LEN
    dd MB2_CHECKSUM
    ; (no tags; end tag only)
    dw 0      ; type = end
    dw 0      ; flags
    dd 8      ; size
mb2_end:

SECTION .text
global _start
extern kmain

_start:
    ; set up a small stack
    mov esp, stack_top
    call kmain

.hang:
    cli
    hlt
    jmp .hang

SECTION .bss
align 16
stack_bottom:
    resb 4096
stack_top:
