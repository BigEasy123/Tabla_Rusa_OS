#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

void paging_init(void);
void paging_identity_map_range(uint32_t addr, uint32_t size);
int paging_is_enabled(void);
uint32_t paging_directory_addr(void);

#endif
