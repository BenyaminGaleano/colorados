#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <string.h>

struct hash frame_table;
struct list clock;
struct list_elem *hand;
struct lock ft_lock;
bool initialized = false;

#define lock_ft() lock_acquire(&ft_lock)
#define unlock_ft() lock_release(&ft_lock)

unsigned ft_hash (const struct hash_elem *, void *aux);
bool ft_less (const struct hash_elem *, const struct hash_elem *, void *);
struct frame *_frame_chg_owner(struct frame *f);


struct frame *
fte_hvalue(const struct hash_elem *elem)
{
  return elem == NULL ? NULL : hash_entry(elem, struct frame, helem);
}


struct frame *
fte_lvalue(const struct list_elem *elem)
{
  return elem == NULL ? NULL : list_entry(elem, struct frame, elem);
}


unsigned
ft_hash (const struct hash_elem *f_, void *aux UNUSED)
{
  const struct frame *ftentry = hash_entry (f_, struct frame, helem);
  return hash_bytes (&ftentry->address, sizeof ftentry->address);
}


bool
ft_less (
    const struct hash_elem *a,
    const struct hash_elem *b,
    void * aux UNUSED)
{
  struct frame *fte1 = fte_hvalue(a);
  struct frame *fte2 = fte_hvalue(b);

  return fte1->address < fte2->address;
}


void
init_frame_table(void)
{
  if (!initialized) 
  {
    hash_init(&frame_table, ft_hash, ft_less, NULL);
    lock_init(&ft_lock);
    list_init(&clock);
    printf("Actualmente pesa %u\n\n", sizeof(struct frame));
    initialized = true;
  }
}


struct frame *
ft_insert(void *frame)
{
  struct frame *current;
  struct hash_elem *curr;
  
  current =  malloc(sizeof(struct frame));
  current->owner = thread_current();
  current->address = frame;
  current->uaddr = NULL;
  current->inclock = false;

  lock_ft();

  curr = hash_insert(&frame_table, &current->helem); 

  if (curr == NULL) {
    list_push_back(&current->owner->frames, &current->elem);
    current = NULL;
  } else {
    free(current);
    current = fte_hvalue(curr);
  }

  unlock_ft();

  return current;
}


struct frame *
ft_remove(void *frame)
{
  struct frame *fte = NULL;
  struct frame target;
  struct hash_elem *curr;

  target.address = frame;

  lock_ft();

  curr = hash_delete(&frame_table, &target.helem); 

  if (curr != NULL) {
    fte = fte_hvalue(curr);
    list_remove(&fte->elem);
    if (fte->inclock) {
      list_remove(&fte->eclock);
    }
  }

  unlock_ft();

  return fte;
}


struct frame *
frame_lookup (void *address)
{
  ASSERT(pg_ofs(address) == 0)

  struct frame f;
  struct hash_elem *e;

  f.address = address;
  e = hash_find (&frame_table, &f.helem);
  return e != NULL ? fte_hvalue(e) : NULL;
}


struct frame *
_frame_chg_owner(struct frame *frame)
{
  if (frame == NULL) {
    return NULL;
  }

  list_remove(&frame->elem);
  frame->owner = thread_current();
  list_push_back(&frame->owner->frames, &frame->elem);

  return frame;
}


struct frame *
frame_change_owner(struct frame *frame)
{
  lock_ft();

  frame = _frame_chg_owner(frame);

  unlock_ft();

  return frame;
}


struct frame *
ft_update(void *frame_addr)
{
  struct frame *f = NULL;
  
  f = frame_lookup(frame_addr);

  if (f == NULL)
    return NULL;

  f = frame_change_owner(f);

  return f;
}


struct frame *
frame_lookup_user (void *uaddr)
{
  ASSERT(pg_ofs(uaddr) == 0)
  struct frame *f;
  struct thread *t = thread_current(); 

  f = frame_lookup(pagedir_get_page(t->pagedir, uaddr));

  return f;
}


void
clock_add(struct frame *f)
{
  ASSERT(f != NULL)
  if (!f->inclock)
  {
    list_push_back(&clock, &f->eclock);
    hand = list_begin(&clock);
  }

  f->inclock = true;
}


struct frame *
clock_replace(void)
{
  ASSERT(list_size(&clock) > 0)
  struct frame *f;

  PANIC("Need a swap and mmap implementation");

  lock_ft();
  while (true) {
    if (hand == list_end(&clock)) {
      hand = list_begin(&clock);
      continue;
    }

    f = list_entry(hand, struct frame, eclock);
    hand = list_next(hand);

    ASSERT(f != NULL && f->owner != NULL);

    if (pagedir_is_accessed(f->owner->pagedir, f->uaddr))
    {
      pagedir_set_accessed(f->owner->pagedir, f->uaddr, false);
    } else
      break;
  }

  bool exe = pagedir_is_exe(f->owner->pagedir, f->uaddr);
  bool dirty = pagedir_is_dirty(f->owner->pagedir, f->uaddr);
  bool mmap = pagedir_is_mmap(f->owner->pagedir, f->uaddr);

  // special case when is a executable page
  if ((!exe && !mmap) || (exe && dirty))
  {
    // TODO: store in swap
    pagedir_set_swap(f->owner->pagedir, f->uaddr, true);
  } else if (mmap && dirty)
  {
    // TODO: store in file
  }

  pagedir_clear_page(f->owner->pagedir, f->uaddr);
  _frame_chg_owner(f);
  memset(f->address, 0, PGSIZE);
  unlock_ft();

  return f;
}

