#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

void memory_init(uint32_t magic, uint32_t mb_info_addr);
uint32_t memory_total_kib(void);
uint32_t memory_usable_kib(void);
uint32_t memory_map_entries(void);
void memory_print_map(void);

#endif
