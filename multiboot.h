/* multiboot.h */
#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

/* Multiboot header, placed in .multiboot section */
__attribute__((section(".multiboot"), used))
static const struct {
    uint32_t magic;
    uint32_t flags;
    uint32_t checksum;
} multiboot_header = {
    0x1BADB002,   // magic number
    0x00010003,   // flags: align modules + memory info + video
    -(0x1BADB002 + 0x00010003) // checksum
};

#endif
