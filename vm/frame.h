#ifndef __FRAME_H__
#define __FRAME_H__

#include <hash.h>
#include <list.h>
#include "threads/thread.h"
#include "threads/malloc.h"

typedef struct {
  struct list_elem elem;
  struct hash_elem helem;
  void *frame;
  struct thread *owner;
} fte_t;


void init_frame_table(void);

fte_t *ft_insert(void *frame);

fte_t *ft_remove(void *frame);

#endif

