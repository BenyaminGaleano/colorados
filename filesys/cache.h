#ifndef __CACHE_H__
#define __CACHE_H__
#include "threads/synch.h"
#include "devices/block.h"
#include <stdbool.h>

struct cache_block {
    struct condition readers;
    struct condition writers;
    struct lock lock;
    bool accessed;
    bool dirty;
    block_sector_t sector;    
    unsigned char data[512];
};

void buffer_cache_init(void);

#endif
