#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>

#define CACHE_SIZE 64

static struct cache_block buffer_cache[CACHE_SIZE];
static unsigned char clock_hand = 0;
static bool alive = false;

#define hand_value(clock_hand) clock_hand % 64 // overflow is ok because 64 = 2^6


/// sector: sector target
/// data: pointer that will filled with the pointer to cache block
void locate_block(block_sector_t sector, struct cache_block **block);
void buffer_cache_clock_replace(struct cache_block **block, block_sector_t sector);
void cache_block_load(struct cache_block *block);
void cache_read_start(struct cache_block *block);
void cache_read_end(struct cache_block *block);
void cache_watcher(void *arg);
void read_ahead(void *arg);


/// Assume that has the lock
static inline void
cache_block_store(struct cache_block *block)
{
    ASSERT(block != NULL);
    block_write(fs_device, block->sector, block->data);
}


void
buffer_cache_init(void)
{
    for (int i = 0; i < CACHE_SIZE; i++) {
        buffer_cache[i].accessed = false;
        buffer_cache[i].dirty = false;
        buffer_cache[i].sector = -1;
        buffer_cache[i].active_readers = 0;
        buffer_cache[i].active_writers = 0;
        buffer_cache[i].rwaiters = 0;
        buffer_cache[i].wwaiters = 0;
        lock_init(&buffer_cache[i].lock);
        cond_init(&buffer_cache[i].readers);
        cond_init(&buffer_cache[i].writers);
    }
    
    alive = true;
    #ifndef VM
    thread_create("cache-watcher", PRI_DEFAULT, cache_watcher, NULL);
    #endif
}


// Assume that has the lock
void
cache_block_load(struct cache_block *cache_block)
{
    ASSERT(cache_block != NULL);
    ASSERT(cache_block->sector != -1);
    block_read(fs_device, cache_block->sector, cache_block->data);

    cache_block->accessed = true;
    cache_block->dirty = false;
    cache_block->active_readers = 0;
    cache_block->active_writers = 0;
    cache_block->rwaiters = 0;
    cache_block->wwaiters = 0;
}


void
buffer_cache_clock_replace(struct cache_block **block, block_sector_t sector)
{
    struct cache_block *iblock;
    bool access;
    for (;; clock_hand++) {
        iblock = buffer_cache + hand_value(clock_hand);

        access = lock_try_acquire(&iblock->lock);

        if (!access) {
            continue;
        }

        if (iblock->accessed) {
            iblock->accessed = false;
            lock_release(&iblock->lock);
            continue;
        }

        if (iblock->wwaiters || iblock->rwaiters) {
            lock_release(&iblock->lock);
            continue;
        }

        break;
    }


    ASSERT(iblock != NULL);

    if (iblock->dirty) {
        cache_block_store(iblock);
    }

    iblock->sector = sector;

    cache_block_load(iblock);

    *block = iblock;

    lock_release(&iblock->lock);
}


void
locate_block(block_sector_t sector, struct cache_block **block)
{
    ASSERT(sector != -1);
    ASSERT(block != NULL);
    struct cache_block *free_block = NULL;

    *block = NULL;

    for (int i = 0; i < CACHE_SIZE; i++) {
        if (free_block == NULL && buffer_cache[i].sector == -1) {
            free_block = buffer_cache + i;
            if (!lock_try_acquire(&free_block->lock)) {
                free_block = NULL;
            }
        } else if (sector == buffer_cache[i].sector) {
            *block = buffer_cache + i;
            (*block)->accessed = true;
            break;
        }
    }

    if (*block != NULL) {
        if (free_block != NULL) {
            lock_release(&free_block->lock);
        }
        return;
    }

    if (free_block != NULL) {
        free_block->sector = sector;
        cache_block_load(free_block);
        lock_release(&free_block->lock);
        *block = free_block;
        return;
    }

    *block = NULL;
    buffer_cache_clock_replace(block, sector);
    ASSERT(*block != NULL);
}


void
cache_read_start(struct cache_block *block)
{
    lock_acquire(&block->lock);
    block->rwaiters++;

    while (block->wwaiters || block->active_writers)
        cond_wait(&block->readers, &block->lock);

    block->rwaiters--;
    block->active_readers++;
    lock_release(&block->lock);
}


void
cache_read_end(struct cache_block *block)
{
    lock_acquire(&block->lock);
    block->active_readers--;

    if (!block->active_readers && block->wwaiters)
        cond_signal(&block->writers, &block->lock);
    lock_release(&block->lock);
}


void
buffer_cache_read(block_sector_t sector, void *buffer)
{
    ASSERT(sector != -1);
    ASSERT(buffer != NULL);
    
    struct cache_block *block = NULL;

    locate_block(sector, &block);

    cache_read_start(block);

    memcpy(buffer, block->data, BLOCK_SECTOR_SIZE);
    
    cache_read_end(block);
}


void
buffer_cache_write(block_sector_t sector, const void *buffer)
{
    ASSERT(sector != -1);
    ASSERT(buffer != NULL);

    struct cache_block *block = NULL;

    locate_block(sector, &block);

    lock_acquire(&block->lock);
    block->wwaiters++;

    while (block->active_writers || block->active_readers)
        cond_wait(&block->writers, &block->lock);

    block->wwaiters--;
    block->active_writers = 1;
    lock_release(&block->lock);

    memcpy(block->data, buffer, BLOCK_SECTOR_SIZE);
    block->dirty = true;

    lock_acquire(&block->lock);
    block->active_writers = 0;

    if (block->wwaiters) {
        cond_signal(&block->writers, &block->lock);
    } else if (block->rwaiters) {
        cond_broadcast(&block->writers, &block->lock);
    }
    lock_release(&block->lock);
}


void
buffer_cache_sync(bool postdie)
{
    struct cache_block *block;
    for (int i = 0; i < CACHE_SIZE; i ++) {
        if (!postdie && !alive){
            break;
        }
        block = buffer_cache + i;
        lock_acquire(&block->lock);
        if (block->dirty) {
            ASSERT(block->sector != -1);
            block->dirty = false;
            block_write(fs_device, block->sector, block->data);
        }
        lock_release(&block->lock);
    }
}


void *
buffer_cache_connect(block_sector_t sector)
{
    ASSERT(sector != -1);
    struct cache_block *block = NULL;
    locate_block(sector, &block);
    ASSERT(block != NULL);
    cache_read_start(block);
    return block->data;
}


void
buffer_cache_logout(block_sector_t sector)
{
    ASSERT(sector != -1);
    struct cache_block *block = NULL;
    locate_block(sector, &block);
    ASSERT(block != NULL);
    cache_read_end(block);
}

void
cache_watcher(void * arg UNUSED)
{
    while (true) {
        timer_sleep(TIMER_FREQ);
        if (alive) {
            buffer_cache_sync(false);
        }
    }
}

void
buffer_cache_end(void)
{
    alive = false;
    buffer_cache_sync(true);
}

void
read_ahead(void *arg)
{
    block_sector_t sector = (block_sector_t) arg;
    struct cache_block *block;
    locate_block(sector, &block);
}

void
buffer_cache_async_fetch(block_sector_t sector)
{
#ifndef VM
    if (sector != -1) {
        thread_create("read-post", PRI_DEFAULT, read_ahead, sector);
    }
#endif
}