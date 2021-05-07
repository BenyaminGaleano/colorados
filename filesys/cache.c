#include "filesys/cache.h"
#include "filesys/filesys.h"
#include <debug.h>

#define CACHE_SIZE 64

static struct cache_block buffer_cache[CACHE_SIZE];
static unsigned char clock_hand = 0;


/// sector: sector target
/// data: pointer that will filled with the pointer to cache block data
void locate_block_data(block_sector_t sector, void **data);
/// sector: sector target
/// data: pointer that will filled with the pointer to cache block
void locate_block(block_sector_t sector, struct cache_block **block);


void
buffer_cache_init()
{
    for (int i = 0; i < CACHE_SIZE; i++) {
        buffer_cache[i].accessed = false;
        buffer_cache[i].dirty = false;
        buffer_cache[i].sector = -1;
        lock_init(&buffer_cache[i].lock);
        cond_init(&buffer_cache[i].readers);
        cond_init(&buffer_cache[i].writers);
    }
}

void
locate_block(block_sector_t sector, struct cache_block **block)
{
    ASSERT(sector != -1);
    ASSERT(block != NULL);
    struct cache_block *free_block = NULL;

    for (int i = 0; i < CACHE_SIZE; i++) {
        if (free_block == NULL && buffer_cache[i].sector == -1) {
            free_block = buffer_cache + i;
        } else if (sector == buffer_cache[i].sector) {
            *block = buffer_cache + i;
            break;
        }
    }

    // TODO: load block into cache
}


void
locate_block_data(block_sector_t sector, void **data)
{
    ASSERT(data != NULL);
    ASSERT(sector != -1);
    struct cache_block *block = NULL;
    locate_block(sector, &block);
    ASSERT(block != NULL);

    *data = block->data;
}


void
buffer_cache_read(block_sector_t sector, void *buffer)
{
    // TODO: copy from block cache data to buffer (memcpy)
}


