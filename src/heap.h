#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

void heap_init(void);
void* kmalloc(size_t size);
uint32_t heap_start(void);
uint32_t heap_current(void);
uint32_t heap_bytes_used(void);

#endif
