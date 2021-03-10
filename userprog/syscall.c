#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "pagedir.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
/** @colorados */
struct lock filesys_lock;
/** @colorados */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
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
    break;
  case SYS_FILESIZE:
    break;
  case SYS_READ:
    break;
  case SYS_WRITE:
    write(stkcast(st + 4, uint32_t), stkcast(st + 8, void *), stkcast(st + 12, size_t));
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
  printf ("%s: exit(%d)\n",current->name, status);
  current->exit_status=status;
  thread_unblock(current->father);

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
  lock_acquire(&filesys_lock);
  bool answer = filesys_create(file, initial_size);
  lock_release(&filesys_lock);

  return answer;
}

bool remove (const char *file)
{
  lock_acquire(&filesys_lock);
  bool answer = filesys_remove(file);
  lock_release(&filesys_lock);

  return answer;
}

int open (const char *file)
{
  struct file *file_open;
  int answer = -1;

  lock_acquire(&filesys_lock);
  file_open = filesys_open(file);
  if(file_open != NULL)
  {
    //fd
  }
  lock_release(&filesys_lock);
  
  return answer;
}

int filesize (int fd)
{
  return file_length(fd);
}

int read (int fd, void *buffer, unsigned length)
{
  return file_read(fd, buffer, length);
}

int write (int fd, const void *buffer, unsigned length)
{
  if (fd == 1) {
    putbuf(buffer, length);
  }

  return NULL;
}

void seek (int fd, unsigned position)
{
  struct file * file;
  file_seek(file, position); 
}

unsigned tell (int fd)
{
  file_tell(fd);
  return NULL;
}

void close (int fd)
{
  file_close(fd); 
}
