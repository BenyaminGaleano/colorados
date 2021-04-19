#include "vm/mmf.h"
#include <list.h>

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



