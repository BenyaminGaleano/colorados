#include "filesys/cache.h"
#include <bitmap.h>
#include <debug.h>

#define CACHE_SIZE 64

static struct bitmap *free_cache;
static struct cache_block buffer_cache[CACHE_SIZE];

void
buffer_cache_init()
{
    free_cache = bitmap_create(CACHE_SIZE);
    if (free_cache == NULL)
        PANIC ("bitmap creation failed--file system device is too large");

    for (int i = 0; i < CACHE_SIZE; i++) {
        buffer_cache[i].accessed = false;
        buffer_cache[i].dirty = false;
        buffer_cache[i].sector = -1;
        lock_init(&buffer_cache[i].lock);
        cond_init(&buffer_cache[i].readers);
        cond_init(&buffer_cache[i].writers);
    }
}


