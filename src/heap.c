#include "heap.h"

extern uint8_t kernel_end;

static uintptr_t heap_base = 0;
static uintptr_t heap_next = 0;

static uintptr_t align_up(uintptr_t value, uintptr_t align){
    return (value + align - 1) & ~(align - 1);
}

void heap_init(void){
    heap_base = align_up((uintptr_t)&kernel_end, 16);
    heap_next = heap_base;
}

void* kmalloc(size_t size){
    if(size == 0)
        return 0;
    uintptr_t p = align_up(heap_next, 16);
    heap_next = p + size;
    return (void*)p;
}

uint32_t heap_start(void){
    return (uint32_t)heap_base;
}

uint32_t heap_current(void){
    return (uint32_t)heap_next;
}

uint32_t heap_bytes_used(void){
    return (uint32_t)(heap_next - heap_base);
}
