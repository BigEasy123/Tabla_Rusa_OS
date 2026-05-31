#include "paging.h"

#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_SIZE 4096

static uint32_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t first_page_tables[4][1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t extra_page_tables[16][1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t extra_page_table_used[16];
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

static uint32_t* ensure_table(uint32_t dir_index){
    if(page_directory[dir_index] & PAGE_PRESENT)
        return (uint32_t*)(page_directory[dir_index] & 0xFFFFF000U);
    for(uint32_t i=0; i<16; i++){
        if(!extra_page_table_used[i]){
            extra_page_table_used[i] = 1;
            for(uint32_t j=0; j<1024; j++)
                extra_page_tables[i][j] = PAGE_RW;
            page_directory[dir_index] = ((uint32_t)extra_page_tables[i]) | PAGE_PRESENT | PAGE_RW;
            return extra_page_tables[i];
        }
    }
    return 0;
}

void paging_identity_map_range(uint32_t addr, uint32_t size){
    uint32_t start = addr & ~(PAGE_SIZE - 1U);
    uint32_t end = (addr + size + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
    for(uint32_t p=start; p<end; p += PAGE_SIZE){
        uint32_t dir = p >> 22;
        uint32_t tbl = (p >> 12) & 0x3FF;
        uint32_t* table = ensure_table(dir);
        if(table)
            table[tbl] = p | PAGE_PRESENT | PAGE_RW;
    }
    if(enabled)
        __asm__ __volatile__("mov %0, %%cr3" : : "r"(page_directory) : "memory");
}

int paging_is_enabled(void){
    return enabled;
}

uint32_t paging_directory_addr(void){
    return (uint32_t)page_directory;
}
