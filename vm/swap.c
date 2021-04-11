#include "swap.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"

struct hash swap_table;
struct lock sw_lock;
struct block *sw_device;
block_sector_t gsector=0;

static bool initialized =false;

#define lock_sw() lock_acquire(&sw_lock);
#define unlock_sw() lock_release(&sw_lock);

unsigned sw_hash(const struct hash_elem*, void *aux);
bool sw_less(const struct hash_elem*, const struct hash_elem *,void *);
void sw_write_frame(block_sector_t sector, struct frame *f);

struct swap * 
swe_hvalue(const struct hash_elem *elem)
{
    return elem==NULL? NULL: hash_entry(elem, struct swap, helem);
}

struct swap *
swe_lvalue(const struct list_elem *elem)
{
    return elem ==NULL? NULL:list_entry(elem, struct swap, lelem);
}

unsigned 
sw_hash(const struct hash_elem *f_, void *aux UNUSED)
{
    const struct swap *swe= hash_entry(f_, struct swap, helem);
    return hash_bytes(&swe->sector, sizeof swe->sector);
}

bool
sw_less(
    const struct hash_elem *a,
    const struct hash_elem *b,
    void * aux UNUSED)
{
    struct swap *swe1=swe_hvalue(a);
    struct swap *swe2=swe_hvalue(b);
    
    return swe1->sector < swe2->sector;
}

void
sw_write_frame(block_sector_t sector, struct frame *f)
{
    for (int i =0;i<8;i++){
        block_write(sw_device, sector+i, f->address +512*i);
    }
}

void
init_swap_table(void)
{
    if(!initialized)
    {
        sw_device=block_get_role(BLOCK_SWAP);
        hash_init(&swap_table, sw_hash, sw_less, NULL);
        lock_init(&sw_lock);
        initialized=true;
    }
}

