#ifndef __MMF_H__
#define __MMF_H__
#include <list.h>
#include <stdio.h>
#include <debug.h>
#include "threads/pte.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "filesys/off_t.h"

struct mfile
{
    struct list_elem elem;
    void * start;
    void * end; // it's the last address mapped in file (included)
    int fd;
};

static inline off_t
mf_offset(struct mfile *s, void *fault_addr)
{
  ASSERT(pg_ofs(s->start) == 0);
  ASSERT(fault_addr >= s->start && fault_addr <= s->end);
  return ((uint64_t) pg_round_down(fault_addr) - (uint64_t) s->start);
}

static inline unsigned
mf_read_bytes(struct mfile *s, void *fault_addr)
{ 
  ASSERT(pg_ofs(s->start) == 0);
  ASSERT(fault_addr >= s->start && fault_addr < s->end);
  return fault_addr >= pg_round_down(s->end) && fault_addr < pg_round_up(s->end) ? 
    (uint64_t) s->end - (uint64_t) pg_round_down(s->end) + 1: PGSIZE;
}

struct mfile *find_mfile(struct thread *t, void *fault_addr);

#endif
