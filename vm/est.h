#ifdef  __EST_H__
#define __EST_H__
#include <list.h>
#include <studio.h>
#include <debug.h>
#include "threads/pte.h"
#include "threads/vaddr.h"

struct exe_segment
{
    struct list_elem elem;
    void * start;
    void * end;
    size_t read_bytes;
    off_t offset;  
};

static inline unsigned
est_read_pages(struct exe_segment *s)
{
  return s->read_bytes % PGSIZE == 0 ? (s->read_bytes / PGSIZE) : (s->read_bytes / PGSIZE) + 1;
}

static inline off_t
est_offset(struct exe_segment *s, void *fault_addr)
{
  ASSERT(fault_addr >= s->start && fault_addr < s->end);
  return s->offset + ((uint64_t) pg_round_down(fault_addr) - (uint64_t) s->start);
}

static inline unsigned
est_read_bytes(struct exe_segment *s, void *fault_addr)
{
  ASSERT(fault_addr >= s->start && fault_addr < s->end);
  return ((est_offset(s, fault_addr) - s->offset + PGSIZE) / PGSIZE) <= est_read_pages(s) ? 
    (((est_offset(s, fault_addr) - s->offset + PGSIZE) / PGSIZE) == est_read_pages(s) ?
     (s->read_bytes % PGSIZE) : PGSIZE) : 0;
}

#endif
