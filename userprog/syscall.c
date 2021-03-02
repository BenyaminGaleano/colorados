#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  switch (*(int *)f->esp)  // toca ver donde se guardan
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
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
  printf ("%s: exit(%d)\n",current->name);
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
  return filesys_create(file, initial_size);
}

bool remove (const char *file)
{
  return filesys_remove(file);
}

int open (const char *file)
{
  return filesys_open(file);
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
