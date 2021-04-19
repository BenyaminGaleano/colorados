#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#ifdef VM
#include "vm/mmf.h"
void sys_mfdestroy(struct mfile *mf);
#endif

void syscall_init (void);

void sys_closef(void *f);
void fsys_lock(void);
void fsys_unlock(void);
#endif /* userprog/syscall.h */
