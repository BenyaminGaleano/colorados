#ifndef __FRAME_H__
#define __FRAME_H__

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/malloc.h"

struct frame {
  struct hash_elem helem;
  struct list_elem eclock;
  void *address;
  void *uaddr;
  bool inclock;
  struct thread *owner;
};


void init_frame_table(void);

struct frame *ft_insert(void *frame_addr);
struct frame *ft_remove(void *frame_addr);
struct frame *ft_update(void *frame_addr);
struct frame *frame_lookup(void *frame_addr);
struct frame *frame_lookup_user(void *uaddr);

struct frame *frame_change_owner(struct frame *frame);

struct frame *fte_hvalue(const struct hash_elem *elem);

bool ft_access(void *uaddr);
bool ft_access_multiple(void *uaddr, void *uaddr_end);

void clock_add(struct frame *f);
struct frame *clock_replace(void);

#endif

