#ifndef __FRAME_H__
#define __FRAME_H__

#include <hash.h>
#include "threads/thread.h"

typedef struct {
  struct hash_elem helem;
  void *frame;
  struct thread owner;
} fte_t;


void init_frame_table(void);

#endif

