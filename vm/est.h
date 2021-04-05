#ifdef  __EST_H__
#define __EST_H__
#include <list.h>
#include <studio.h>

struct exe_segment
{
    struct list_elem elem;
    void * start;
    void * end;
    size_t read_bytes;
    off_t offset;  
};

#endif
