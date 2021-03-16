#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

void sys_closef(void *f);
void fsys_lock(void);
void fsys_unlock(void);
#endif /* userprog/syscall.h */
