#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/malloc.h"

struct hash frame_table;
struct lock ft_lock;
bool initialized = false;

unsigned ft_hash (const struct hash_elem *, void *aux);
bool ft_less (const struct hash_elem *, const struct hash_elem *, void *);

unsigned
ft_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const fte_t *ftentry = hash_entry (f_, fte_t, helem);
  return hash_bytes (&ftentry->frame, sizeof ftentry->frame);
}


bool
ft_less (
    const struct hash_elem *a,
    const struct hash_elem *b,
    void * aux UNUSED)
{
  fte_t *fte1 = hash_entry(a, fte_t, helem);
  fte_t *fte2 = hash_entry(b, fte_t, helem);

  return fte1->frame < fte2->frame;
}


void
init_frame_table(void)
{
  if (!initialized) 
  {
    hash_init(&frame_table, ft_hash, ft_less, NULL);
    lock_init(&ft_lock);
    initialized = true;
  }
}


fte_t *
ft_insert(void *frame)
{
  fte_t *current;
  struct hash_elem *curr;
  
  current =  malloc(sizeof(fte_t));
  current->owner = thread_current();
  current->frame = frame;

  lock_acquire(&ft_lock);

  curr = hash_insert(&frame_table, &current->helem); 

  if (curr == NULL) {
    list_push_back(&current->owner->frames, &current->elem);
    current = NULL;
  } else {
    free(current);
    current = hash_entry(curr, fte_t, helem);
  }

  lock_release(&ft_lock);

  return current;
}


fte_t *
ft_remove(void *frame)
{
  fte_t *fte = NULL;
  fte_t target;
  struct hash_elem *curr;

  target.frame = frame;

  lock_acquire(&ft_lock);

  curr = hash_delete(&frame_table, &target.helem); 

  if (curr != NULL) {
    fte = hash_entry(curr, fte_t, helem);
    list_remove(&fte->elem);
  }

  lock_release(&ft_lock);

  return fte;
}



