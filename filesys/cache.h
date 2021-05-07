#ifndef __CACHE_H__
#define __CACHE_H__
#include "threads/synch.h"

struct cache_block {
    struct condition readers;
    struct condition writers;
    struct lock lock;
    bool accessed;
    bool dirty;
    unsigned char data[512];
};

#endif
