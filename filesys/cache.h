#ifndef __CACHE_H__
#define __CACHE_H__
#include "threads/synch.h"
#include "devices/block.h"
#include <stdbool.h>

struct cache_block {
    struct condition readers;
    unsigned active_readers;
    unsigned rwaiters;
    struct condition writers;
    unsigned active_writers;
    unsigned wwaiters;
    struct lock lock;
    bool accessed;
    bool dirty;
    block_sector_t sector;
    unsigned char data[512];
};

void buffer_cache_init(void);
void buffer_cache_write(block_sector_t sector, const void *buffer);
void buffer_cache_read(block_sector_t sector, void *buffer, block_sector_t lazy);

#endif
