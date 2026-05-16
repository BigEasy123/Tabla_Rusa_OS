#include "memory.h"
#include "console.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289
#define MULTIBOOT_TAG_TYPE_MMAP 6

struct mb2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct mb2_mmap_tag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed));

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

static uint32_t total_kib = 0;
static uint32_t usable_kib = 0;
static uint32_t mmap_entries = 0;
static uint32_t mmap_addr = 0;
static uint32_t mmap_entry_size = 0;
static uint32_t mmap_size = 0;

static uint32_t align8(uint32_t value){
    return (value + 7U) & ~7U;
}

static uint32_t kib_from_u64(uint64_t value){
    uint64_t kib = value / 1024U;
    if(kib > 0xFFFFFFFFU)
        return 0xFFFFFFFFU;
    return (uint32_t)kib;
}

void memory_init(uint32_t magic, uint32_t mb_info_addr){
    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC || mb_info_addr == 0)
        return;

    uint32_t total_size = *(uint32_t*)mb_info_addr;
    uint32_t ptr = mb_info_addr + 8;
    uint32_t end = mb_info_addr + total_size;

    while(ptr + sizeof(struct mb2_tag) <= end){
        struct mb2_tag* tag = (struct mb2_tag*)ptr;
        if(tag->type == 0)
            break;
        if(tag->type == MULTIBOOT_TAG_TYPE_MMAP){
            struct mb2_mmap_tag* mmap = (struct mb2_mmap_tag*)ptr;
            mmap_addr = ptr + sizeof(struct mb2_mmap_tag);
            mmap_entry_size = mmap->entry_size;
            mmap_size = tag->size - sizeof(struct mb2_mmap_tag);
            for(uint32_t off=0; off<mmap_size; off += mmap_entry_size){
                struct mb2_mmap_entry* entry = (struct mb2_mmap_entry*)(mmap_addr + off);
                uint32_t len_kib = kib_from_u64(entry->len);
                total_kib += len_kib;
                if(entry->type == 1)
                    usable_kib += len_kib;
                mmap_entries++;
            }
        }
        ptr += align8(tag->size);
    }
}

uint32_t memory_total_kib(void){ return total_kib; }
uint32_t memory_usable_kib(void){ return usable_kib; }
uint32_t memory_map_entries(void){ return mmap_entries; }

void memory_print_map(void){
    console_puts("Memory map entries: ");
    console_write_dec(mmap_entries);
    console_putc('\n');
    if(mmap_addr == 0){
        console_puts("No Multiboot2 memory map found.\n");
        return;
    }
    for(uint32_t off=0; off<mmap_size; off += mmap_entry_size){
        struct mb2_mmap_entry* entry = (struct mb2_mmap_entry*)(mmap_addr + off);
        console_puts("  base=");
        console_write_hex((uint32_t)entry->addr);
        console_puts(" len_kib=");
        console_write_dec(kib_from_u64(entry->len));
        console_puts(" type=");
        console_write_dec(entry->type);
        console_putc('\n');
    }
}
