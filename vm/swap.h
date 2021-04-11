#ifndef __SWAP_H__
#define __SWAP_H__
#include "threads/thread.h"
#include "devices/block.h"
#include "vm/frame.h"
#include <hash.h>
#include <list.h>

struct swap
{
    struct hash_elem helem;
    struct list_elem lelem;
    block_sector_t sector; //key
    struct thread *owner;
    void *uaddr;
};

void init_swap_table(void);
struct swap *swe_hvalue(const struct hash_elem *elem);
struct swap *swe_lvalue(const struct list_elem *elem);




#endif
