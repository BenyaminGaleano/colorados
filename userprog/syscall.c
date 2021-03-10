#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "pagedir.h"
#include "threads/palloc.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

typedef union {
  struct {
    char magic : 1; // ignore 0 or 1 because they're reserved
    char is_fd : 1;
    uint32_t index : 10;
  } descriptor;

  int value;
} fd_t;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*
 *  
 *...
 *  arg1
 *  syscall
 *
 */

static void
syscall_handler (struct intr_frame *f) 
{
  /** @colorados */
  struct thread *t = thread_current();
  void *st = pagedir_get_page(t->pagedir, ((uint8_t *) PHYS_BASE) - PGSIZE);
  /* printf("Syscall into stack %d\n", *((int *)(st + stack_offset(f->esp)))); */
  st = st + stack_offset(f->esp);
  /** @colorados */

  switch (stkcast(st, uint32_t))
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    exit(stkcast(st + 4, int));
    break;
  case SYS_EXEC:
    break;
  case SYS_CREATE:
    break;
  case SYS_REMOVE:
    break;
  case SYS_OPEN:
    f->eax = open(stkcast(st + 4, char *));
    break;
  case SYS_FILESIZE:
    break;
  case SYS_READ:
    f->eax = read(stkcast(st + 4, int), stkcast(st + 8, void *), stkcast(st + 12, unsigned));
    break;
  case SYS_WRITE:
    f->eax = write(stkcast(st + 4, uint32_t), stkcast(st + 8, void *), stkcast(st + 12, size_t));
    break;
  case SYS_SEEK:
    break;
  case SYS_TELL:
    break;
  case SYS_CLOSE:
    break;
  default:
    printf ("system call!\n");
    thread_exit ();
    break;
  }
}
//
void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
  struct thread *current = thread_current();
  //missing condition process parent
  printf ("%s: exit(%d)\n",current->name, status);
  current->exit_status=status;
  thread_unblock(current->father);

  if (current->files != NULL) {
    palloc_free_page(current->files);
    /** TODO: close files of the current thread */
  }
  thread_exit();
}

pid_t exec (const char *file)
{
  return NULL;
}

int wait (pid_t pid)
{
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size)
{
  struct lock filesys_create_lock;
  lock_init(&filesys_create_lock);

  lock_acquire(&filesys_create_lock);
  bool answer = filesys_create(file, initial_size);
  lock_release(&filesys_create_lock);

  return answer;
}

bool remove (const char *file)
{
  struct lock filesys_remove_lock;
  lock_init(&filesys_remove_lock);

  lock_acquire(&filesys_remove_lock);
  bool answer = filesys_remove(file);
  lock_release(&filesys_remove_lock);

  return answer;
}

int open (const char *file)
{
  struct file *file_open;
  struct lock filesys_open_lock;
  struct thread *t = thread_current();
  lock_init(&filesys_open_lock);
  fd_t fd;
  fd.value = -1;

  lock_acquire(&filesys_open_lock);
  file_open = filesys_open(file);

  if(file_open != NULL && t->files != NULL)
  {
    fd.value = 0;
    fd.descriptor.is_fd = 1;
    fd.descriptor.index = t->afid++;
    stkcast(t->files + fd.descriptor.index*4, void *) = file_open;
  }

  lock_release(&filesys_open_lock);

  return fd.value;
}

int filesize (int fd)
{
  struct thread *t = thread_current();

  fd_t ffd;
  ffd.value = fd;
  unsigned i = ffd.descriptor.index;

  if (t->files == NULL || i > 1023) {
    return 0;
  }

  return file_length(stkcast(t->files + i*4, struct file *));
}

int read (int fd, void *buffer, unsigned length)
{
  if (fd == 0) {
    
  }
  struct thread *t = thread_current();
  unsigned i = ((fd_t) fd).descriptor.index;

  if (t->files == NULL || i > 1023) {
    return 0;
  }
  return file_read(stkcast(t->files + i*4, void *), buffer, length);
}

int write (int fd, const void *buffer, unsigned length)
{
  if (fd == 1) {
    putbuf(buffer, length);
    return length;
  }

  struct thread *t = thread_current();
  fd_t fdes = (fd_t)fd;

  return file_write(stkcast(t->files + fdes.descriptor.index * 4, void *), buffer, length);
}

void seek (int fd, unsigned position)
{
  fd_t fdes = (fd_t)fd;
  struct thread *t = thread_current();
  file_seek(stkcast(t->files + fdes.descriptor.index * 4, void *), position);
}

unsigned tell (int fd)
{
  fd_t fdes = (fd_t)fd;
  struct thread *t = thread_current();
  return file_tell(stkcast(t->files + fdes.descriptor.index * 4, void *));
}

void close (int fd)
{
  fd_t fdes = (fd_t)fd;
  struct thread *t = thread_current();
  file_close(stkcast(t->files + fdes.descriptor.index * 4, void *));
}
