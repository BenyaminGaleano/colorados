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

static void syscall_handler (struct intr_frame *);

/** @colorados */
typedef union {
  struct {
    char magic : 1; // ignore 0 or 1 because they're reserved
    char is_fd : 1;
    uint32_t index : 10;
  } descriptor;

  int value;
} fd_t;
void checkbytes(void *, int);
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
  void *st = f->esp;
  // check esp
  if (st + 16 > PHYS_BASE) {
    exit(-1);
  }

  checkbytes(st, 4);

  /* printf("Syscall into stack %d\n", stkcast(st, int)); */
  /** @colorados */

  switch (stkcast(st, uint32_t))
  {
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    checkbytes(st, 8);
    exit(stkcast(st + 4, int));
    break;
  case SYS_EXEC:
    checkbytes(st, 8);
    f->eax = exec(stkcast(st + 4, char *));
    break;
  case SYS_WAIT:
    checkbytes(st, 8);
    f->eax = wait(stkcast(st + 4, pid_t));
    break;
  case SYS_CREATE:
    checkbytes(st, 12);
    f->eax = create(stkcast(st + 4, char *), stkcast(st + 8, unsigned));
    break;
  case SYS_REMOVE:
    checkbytes(st, 8);
    f->eax = remove(stkcast(st + 4, char *));
    break;
  case SYS_OPEN:
    checkbytes(st, 8);
    f->eax = open(stkcast(st + 4, char *));
    break;
  case SYS_FILESIZE:
    checkbytes(st, 8);
    f->eax = filesize(stkcast(st +  4, int));
    break;
  case SYS_READ:
    checkbytes(st, 16);
    f->eax = read(stkcast(st + 4, int), stkcast(st + 8, void *), stkcast(st + 12, unsigned));
    break;
  case SYS_WRITE:
    checkbytes(st, 16);
    f->eax = write(stkcast(st + 4, uint32_t), stkcast(st + 8, void *), stkcast(st + 12, size_t));
    break;
  case SYS_SEEK:
    checkbytes(st, 12);
    seek(stkcast(st + 4, int), stkcast(st + 8, unsigned));
    break;
  case SYS_TELL:
    checkbytes(st, 8);
    f->eax = tell(stkcast(st + 4, int));
    break;
  case SYS_CLOSE:
    checkbytes(st, 8);
    close(stkcast(st + 4, int));
    break;
  default:
    printf ("system call!\n");
    thread_exit ();
    break;
  }
}

void checkbytes(void *mem, int bytes_to_check)
{
  for (int i = 0; i < bytes_to_check; i++) {
    if (get_user(mem + i) == -1) {
      exit(-1);
    }
  }
}

void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
  struct thread *current = thread_current();
  printf ("%s: exit(%d)\n",current->name, status);
  current->exit_status=status;
  if (current->estorbo == 1)
  thread_unblock(current->father);

  if (current->files != NULL) {
    palloc_free_page(current->files);
    /** TODO: close files of the current thread and release locks ands free pages*/

    while (!list_empty(&current->locks)) {
      lock_release(list_entry(list_pop_front(&current->locks), struct lock, elem));
    }
  }
  thread_exit();
}

pid_t exec (const char *file)
{
  intr_disable();
  struct lock lk;
  lock_init(&lk);
  struct condition cond;
  char *checkf = file;
  cond_init(&cond);
  while (get_user(checkf) != 0) {
    if (get_user(checkf) == -1) {
      exit(-1);
    }
    checkf++;
  }

  void *file_mod = palloc_get_page(PAL_USER);

  if (file_mod == NULL) {
    return -1;
  }

  strlcpy(file_mod, file, PGSIZE);
  pid_t pid = process_execute(file_mod);
  palloc_free_page(file_mod);

  if (pid == -1) {
    return -1;
  }


  struct thread *t = NULL;
  void *args[2];
  args[0] = &t;
  args[1] = &pid;
  //thread_foreach(get_thread_with_id, args);
  t = thread_current()->child;
  thread_current()->child = NULL;
  t->cond_var = &cond;
  intr_enable();
  
  lock_acquire(&lk);
  cond_wait(t->cond_var, &lk);
  lock_release(&lk);

  if (search_pstate(thread_current(), pid)->descriptor.child == 0) {
    return -1;
  }

  return t->pid;
}

int wait (pid_t pid)
{
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size)
{
  bool answer;
  char *checkf = file;
  lock_acquire(&filesys_lock);
  if (file != NULL) {
    while (get_user(checkf) != 0) {
      if (get_user(checkf) == -1) {
        exit(-1);
      }
      checkf++;
    }
    answer = filesys_create(file, initial_size);
  } else {
    exit(-1);
  }
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
  struct thread *t = thread_current();
  char *checkf = file;
  fd_t fd;
  fd.value = -1;

  lock_acquire(&filesys_lock);

  if (file == NULL) {
    exit(-1);
  }

  while (get_user(checkf) != 0) {
    if (get_user(checkf) == -1) {
      exit(-1);
    }
    checkf++;
  }

  file_open = filesys_open(file);

  if(file_open != NULL && t->files != NULL)
  {
    fd.value = 0;
    fd.descriptor.is_fd = 1;
    fd.descriptor.index = t->afid++;
    stkcast(t->files + fd.descriptor.index*4, void *) = file_open;
    /* file_deny_write(file_open); */
  }

  lock_release(&filesys_lock);

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
  if (buffer == NULL || buffer > PHYS_BASE) {
    exit(-1);
  }

  if (fd == 0) {
    for (int i = 0; i < length; i++) {
      if (put_user(buffer + i, input_getc()) == false) {
        return i;
      }
    }
    return length;
  }

  if (fd == 1) {
    return 0;
  }

  for (int i = 0; i < length; i++) {
    if (put_user(buffer +  i, 0) == false) {
      exit(-1);
    }
  }

  struct thread *t = thread_current();
  unsigned i = ((fd_t) fd).descriptor.index;

  if (t->files == NULL || i > 1023) {
    return 0;
  }

  struct file *f = stkcast(t->files + i * 4, void *);

  if (f == NULL) {
    return 0;
  }

  lock_acquire(&filesys_lock);
  off_t rlen = file_read(f, buffer, length);
  lock_release(&filesys_lock);
  return rlen;
}

int write (int fd, const void *buffer, unsigned length)
{
  if (fd == 0) {
    return 0;
  }

  for (int i = 0; i < length; i++) {
    if (get_user(buffer + i) == -1) {
      exit(-1);
    }
  }

  if (fd == 1) {
    putbuf(buffer, length);
    return length;
  }

  struct thread *t = thread_current();
  fd_t fdes = (fd_t)fd;
  struct file *f;

  if (t->files == NULL || fdes.descriptor.index > 1023 ||
      (f = stkcast(t->files + fdes.descriptor.index * 4, struct file*)) == NULL) {
    return 0;
  }

  lock_acquire(&filesys_lock);
  off_t written = file_write(f, buffer, length);
  lock_release(&filesys_lock);
  return written;
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
  struct file *f = stkcast(t->files + fdes.descriptor.index * 4, void *);

  if (f == NULL) {
    return;
  }

  /* file_allow_write(f); */
  file_close(f);
  stkcast(t->files + fdes.descriptor.index * 4, void *) = NULL;
}
