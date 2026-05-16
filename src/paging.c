#include "paging.h"

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_SIZE 4096

static uint32_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t first_page_tables[4][1024] __attribute__((aligned(PAGE_SIZE)));
static int enabled = 0;

void paging_init(void){
    for(uint32_t i=0; i<1024; i++)
        page_directory[i] = PAGE_RW;

    for(uint32_t table=0; table<4; table++){
        for(uint32_t i=0; i<1024; i++){
            uint32_t addr = (table * 1024 + i) * PAGE_SIZE;
            first_page_tables[table][i] = addr | PAGE_PRESENT | PAGE_RW;
        }
        page_directory[table] = ((uint32_t)first_page_tables[table]) | PAGE_PRESENT | PAGE_RW;
    }

    __asm__ __volatile__("mov %0, %%cr3" : : "r"(page_directory) : "memory");
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0) : "memory");
    enabled = 1;
}

int paging_is_enabled(void){
    return enabled;
}

uint32_t paging_directory_addr(void){
    return (uint32_t)page_directory;
}
