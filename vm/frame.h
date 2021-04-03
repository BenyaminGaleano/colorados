#ifndef __FRAME_H__
#define __FRAME_H__

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/malloc.h"

struct frame {
  struct list_elem elem;
  struct hash_elem helem;
  void *address;
  void *uaddr;
  struct thread *owner;
};


void init_frame_table(void);

struct frame *ft_insert(void *frame_addr);
struct frame *ft_remove(void *frame_addr);
struct frame *ft_update(void *frame_addr);
struct frame *frame_lookup(void *frame_addr);

struct frame *frame_change_owner(struct frame *frame);

struct frame *fte_hvalue(const struct hash_elem *elem);
struct frame *fte_lvalue(const struct list_elem *elem);

#endif

