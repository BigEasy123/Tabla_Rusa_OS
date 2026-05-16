#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

void paging_init(void);
int paging_is_enabled(void);
uint32_t paging_directory_addr(void);

#endif
