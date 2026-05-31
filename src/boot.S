[BITS 32]

; -----------------------------
; Multiboot2 header (must be in first 8 KB)
; -----------------------------
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

align 8
    dw 5          ; framebuffer request tag
    dw 0          ; optional
    dd 20         ; size
    dd 1024       ; width
    dd 768        ; height
    dd 32         ; depth

align 8
    dw 0          ; end tag type
    dw 0          ; flags
    dd 8          ; size
mb2_end:

; -----------------------------
; Kernel entry
; -----------------------------
SECTION .text
global _start
extern kmain

_start:
    cli
    mov esp, stack_top
    push ebx
    push eax
    call kmain

.hang:
    cli
    hlt
    jmp .hang

; -----------------------------
; Stack
; -----------------------------
SECTION .bss
align 16
stack_bottom:
    resb 32768
stack_top:
