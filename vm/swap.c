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

struct swap *
swap_lookup(block_sector_t sector)
{
    ASSERT(sector % 8 == 0);
    
    struct swap s;
    struct hash_elem *e;

    s.sector = sector;
    e = hash_find(&swap_table, &s.helem);
    return e != NULL ? swe_hvalue(e) : NULL;
}

struct swap *
swap_lookup_user(struct thread *t, void *uaddr)
{
    struct list_elem *i = list_begin(&t->swps);
    struct swap *s;

    while (i != list_end(&t->swps))
    {
        s = swe_lvalue(i);

        if(s->uaddr == uaddr)
            return s;
        
        i = list_next(i);
    }
    
    return NULL;
}

void
sw_logout(void)
{
    struct thread *t = thread_current();
    struct swap *s;

    lock_sw();
    while(list_size(&t->swps) > 0)
    {
        s = swe_lvalue(list_pop_back(&t->swps));
        ASSERT(hash_delete(&swap_table, &s->helem) != NULL);
        free(s);
    }

    unlock_sw();
}

block_sector_t
swap_get_slot(void)
{
    block_sector_t piv = gsector;
    while (swap_lookup(gsector) != NULL)
    {
        gsector += 8;

        if(piv == gsector){
            PANIC("Swap is full");
        }

        if(gsector == block_size(sw_device))
        {
            gsector = 0;
        }
    }
    
    return gsector;
}