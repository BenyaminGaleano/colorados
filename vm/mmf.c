#include "vm/mmf.h"
#include <list.h>
#include "userprog/syscall.h"
#include "threads/palloc.h"

struct mfile *find_mfile(struct thread *t, void *fault_addr)
{
    struct list_elem *i=list_begin(&t->mfiles);
    struct mfile *s =NULL;

    while(i!=list_end(&t->mfiles))
    {
        s=list_entry(i, struct mfile, elem);
        if(fault_addr >= s->start && fault_addr <= s->end)
          break;
        i=list_next(i);
        s=NULL;
    }
    return s;
}


struct mfile *
mf_byId(struct thread *t, int mapid)
{
  struct mfile *mf = NULL;
  struct list_elem *i = list_begin(&t->mfiles);

  while(i != list_end(&t->mfiles))
  {
    mf = list_entry(i, struct mfile, elem);
    if(mf->mapid == mapid)
      break;
    i = list_next(i);
    mf = NULL;
  }

  return mf;
}


void
mf_load_page(struct thread *t, void *fault_addr)
{
  ASSERT(pagedir_is_mmap(t->pagedir, fault_addr));
  struct mfile *mf = find_mfile(t, fault_addr);
  ASSERT(mf != NULL);

  void *kpage = palloc_get_page(PAL_USER | PAL_ZERO);

  fsys_lock();
  file_seek(mf->f, mf_offset(mf, fault_addr));
  file_read(mf->f, kpage, mf_page_bytes(mf, fault_addr));
  fsys_unlock();

  ASSERT(pagedir_reinstall(t->pagedir, pg_round_down(fault_addr), kpage));
}

void
mf_store_page(struct frame *frame)
{
    ASSERT(frame != NULL);
    ASSERT(frame->uaddr != NULL);
    ASSERT(frame->inclock);
    ASSERT(frame->owner != NULL);
    ASSERT(pagedir_is_mmap(frame->owner->pagedir, frame->uaddr));

    struct mfile *mf = find_mfile(frame->owner, frame->uaddr);
    ASSERT(mf != NULL);

    fsys_lock();
    file_seek(mf->f, mf_offset(mf, frame->uaddr));
    file_write(mf->f, frame->address, mf_page_bytes(mf, frame->uaddr));
    fsys_unlock();
}

void
mf_deallocate_pages(struct mfile *mf)
{
  ASSERT(mf != NULL);
  ASSERT(pg_ofs(mf->start) == 0);

  struct frame *frame;

  for (void *ipage = mf->start; ipage <= mf->end; ipage+= PGSIZE) {
    frame = frame_lookup_user(ipage);
    if (frame == NULL)
      continue;
      
    ASSERT(pagedir_is_mmap(frame->owner->pagedir, ipage));

    if (pagedir_is_dirty(frame->owner->pagedir, frame->uaddr)) {
      mf_store_page(frame);
    }
  }
}

