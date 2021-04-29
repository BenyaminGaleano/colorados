#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include <string.h>
#include "pagedir.h"
#include "threads/malloc.h"
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
#include "devices/shutdown.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

/** @colorados */
typedef union {
  struct {
    uint8_t magic : 1; // ignore 0 or 1 because they're reserved
    uint8_t is_fd : 1;
    uint32_t index : 10;
    int padding: 20;
  } descriptor;

  int value;
} fd_t;
void checkbytes(void *, int);
int stdout_and_check(int fd, const void *buffer, unsigned length);
int stdin_and_check(int fd, void *buffer, unsigned length);
void check_file(char *file);
int sys_mmap(int fd, void *addr);
void sys_munmap(int mapid);
struct lock filesys_lock;
struct lock filesys_lock;
struct lock mmap_lock;
/** @colorados */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
  lock_init(&mmap_lock);
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
  thread_current()->st_kernel_save = st;
  if (st + 16 > PHYS_BASE) {
    exit(-1);
  }

  checkbytes(st, 4);

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

    check_file(stkcast(st + 4, char *));

    lock_acquire(&filesys_lock);
    f->eax = create(stkcast(st + 4, char *), stkcast(st + 8, unsigned));
    lock_release(&filesys_lock);
    break;
  case SYS_REMOVE:
    checkbytes(st, 8);
    check_file(stkcast(st + 4, char *));

    lock_acquire(&filesys_lock);
    f->eax = remove(stkcast(st + 4, char *));
    lock_release(&filesys_lock);
    break;
  case SYS_OPEN:
    checkbytes(st, 8);
    check_file(stkcast(st + 4, char *));

    lock_acquire(&filesys_lock);
    f->eax = open(stkcast(st + 4, char *));
    lock_release(&filesys_lock);
    break;
  case SYS_FILESIZE:
    checkbytes(st, 8);
    lock_acquire(&filesys_lock);
    f->eax = filesize(stkcast(st +  4, int));
    lock_release(&filesys_lock);
    break;
  case SYS_READ:
    checkbytes(st, 16);
    f->eax = stdin_and_check(stkcast(st + 4, uint32_t), stkcast(st + 8, void *), stkcast(st + 12, size_t));

    if (f->eax != -1) {
      break;
    }

    lock_acquire(&filesys_lock);
    f->eax = read(stkcast(st + 4, int), stkcast(st + 8, void *), stkcast(st + 12, unsigned));
    lock_release(&filesys_lock);
    break;
  case SYS_WRITE:
    checkbytes(st, 16);
    f->eax = stdout_and_check(stkcast(st + 4, uint32_t), stkcast(st + 8, void *), stkcast(st + 12, size_t));

    if (f->eax != -1) {
      break;
    }

    lock_acquire(&filesys_lock);
    f->eax = write(stkcast(st + 4, uint32_t), stkcast(st + 8, void *), stkcast(st + 12, size_t));
    lock_release(&filesys_lock);
    break;
  case SYS_SEEK:
    checkbytes(st, 12);
    lock_acquire(&filesys_lock);
    seek(stkcast(st + 4, int), stkcast(st + 8, unsigned));
    lock_release(&filesys_lock);
    break;
  case SYS_TELL:
    checkbytes(st, 8);
    lock_acquire(&filesys_lock);
    f->eax = tell(stkcast(st + 4, int));
    lock_release(&filesys_lock);
    break;
  case SYS_CLOSE:
    checkbytes(st, 8);
    lock_acquire(&filesys_lock);
    close(stkcast(st + 4, int));
    lock_release(&filesys_lock);
    break;
  case SYS_MMAP:
    checkbytes(st, 16);
    f->eax = sys_mmap(stkcast(st + 4, int), stkcast(st + 8, void *));
    break;
  case SYS_MUNMAP:
    checkbytes(st, 8);
    sys_munmap(stkcast(st + 4, int));
    break;
  default:
    printf ("system call!\n");
    thread_exit ();
    break;
  }
}

void check_file(char *file)
{
  char *checkf = file;

  if (file != NULL) {
    while (get_user(checkf) != 0) {
      if (get_user(checkf) == -1) {
        exit(-1);
      }
      checkf++;
    }
  } else {
    exit(-1);
  }
}

int stdin_and_check(int fd, void *buffer, unsigned length)
{
  if (buffer == NULL || buffer > PHYS_BASE) {
    exit(-1);
  }
  for (unsigned int i = 0; i < length; i++)
  {
    if(get_user(buffer+1)==-1||put_user(buffer+1,0)==false){
      exit(-1);
    }
  }
  
  if (fd == 0) {
    for (unsigned int i = 0; i < length; i++) {
      if (put_user(buffer + i, input_getc()) == false) {
        return i;
      }
    }
    return length;
  }

  if (fd == 1) {
    return 0;
  }


  return -1;
}

int stdout_and_check(int fd, const void *buffer, unsigned length)
{
  if (fd == 0) {
    return 0;
  }

  for (unsigned int i = 0; i < length; i++) {
    if (get_user(buffer + i) == -1) {
      exit(-1);
    }
  }

  if (fd == 1) {
    putbuf(buffer, length);
    return length;
  }

  return -1;
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
  current->exit_state = status;
  thread_exit();
}

pid_t exec (const char *file)
{
  struct semaphore sema;
  char *checkf = file;

  sema_init(&sema, 0);
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

  struct thread *t = NULL;
  struct thread *cur = thread_current();

  /** @warning */
  pid_t pid;
  intr_disable();
  pid = process_execute(file_mod);
  
  t = cur->child;
  t->sema_parent = &sema;
  intr_enable();
  /** @warning */

  cur->child = NULL;

  palloc_free_page(file_mod);

  if (pid == -1) {
    return -1;
  }

  sema_down(&sema);

  if (search_pstate(cur, pid)->descriptor.child == 0) {
    return -1;
  }

  return pid;
}

int wait (pid_t pid)
{
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size)
{
  bool answer;

  answer = filesys_create(file, initial_size);

  return answer;
}

bool remove (const char *file)
{
  bool answer = filesys_remove(file);
  return answer;
}

int open (const char *file)
{
  struct file *file_open;
  struct thread *t = thread_current();
  fd_t fd;
  fd.value = -1;
  
  file_open = filesys_open(file);

  if(file_open != NULL && t->files != NULL)
  {
     if (t->afid > 1023) {
      file_close(file_open);
      exit(-1);
    }

    fd.value = 0;
    fd.descriptor.is_fd = 1;
    fd.descriptor.index = t->afid++;
    stkcast(t->files + fd.descriptor.index*4, void *) = file_open;
  }

  return fd.value;
}

int filesize (int fd)
{
  struct thread *t = thread_current();

  fd_t ffd;
  ffd.value = fd;
  unsigned i = ffd.descriptor.index;

  if (t->files == NULL || !ffd.descriptor.is_fd || ffd.descriptor.padding != 0) {
    exit(-1);
  }

  return file_length(stkcast(t->files + i*4, struct file *));
}

int read (int fd, void *buffer, unsigned length)
{

  struct thread *t = thread_current();
  unsigned i = ((fd_t) fd).descriptor.index;

  if (t->files == NULL || i > 1023) {
    return 0;
  }

  struct file *f = stkcast(t->files + i * 4, void *);

  if (f == NULL) {
    exit(-1);
  }

  off_t rlen = file_read(f, buffer, length);
  return rlen;
}

int write (int fd, const void *buffer, unsigned length)
{
  struct thread *t = thread_current();
  fd_t fdes = (fd_t)fd;
  struct file *f;

  if (t->files == NULL || !fdes.descriptor.is_fd || fdes.descriptor.padding != 0 ||
      (f = stkcast(t->files + fdes.descriptor.index * 4, struct file*)) == NULL) {
    exit(-1);
  }
  
  off_t written = file_write(f, buffer, length);
  return written;
}

void seek (int fd, unsigned position)
{
  fd_t fdes = (fd_t)fd;
  struct thread *t = thread_current();
  struct file *f;
  if (t->files == NULL ||
      !fdes.descriptor.is_fd || fdes.descriptor.padding != 0 ||
      (f = stkcast(t->files + fdes.descriptor.index * 4, struct file *)) ==
          NULL) {
    exit(-1);
  }

  file_seek(f, position);
}

unsigned tell (int fd)
{
  fd_t fdes = (fd_t)fd;
  struct thread *t = thread_current();
  if (t->files == NULL || !fdes.descriptor.is_fd ||
      fdes.descriptor.padding != 0) {
    exit(-1);
  }
  return file_tell(stkcast(t->files + fdes.descriptor.index * 4, void *));
}
void sys_closef(void *f) 
{
  lock_acquire(&filesys_lock);
  file_close(f);
  lock_release(&filesys_lock);
}

void fsys_lock(void)
{
  lock_acquire(&filesys_lock);
}

void fsys_unlock(void)
{
  lock_release(&filesys_lock);
}

void close (int fd)
{
  fd_t fdes = (fd_t)fd;
  struct thread *t = thread_current();
  struct file *f = stkcast(t->files + fdes.descriptor.index * 4, void *);

  if (t->files == NULL ||
      !fdes.descriptor.is_fd || fdes.descriptor.padding != 0) {
    exit(-1);
  }

  if (f == NULL) {
    return;
  }

  file_close(f);
  stkcast(t->files + fdes.descriptor.index * 4, void *) = NULL;
}

#ifdef VM
void 
sys_mfdestroy(struct mfile *mf)
{
  mf_deallocate_pages(mf);
  list_remove(&mf->elem);
  sys_closef(mf->f);
  free(mf);
}


int 
sys_mmap(int fd, void *addr)
{
  int size = filesize(fd);
  struct thread *t = thread_current();
  struct mfile *mf;
  struct files *f = stkcast(t->files + ((fd_t) fd).descriptor.index * 4, struct file *);

  if (size == 0 || pg_ofs(addr) != 0)
    return -1;

  if (f == NULL || addr == 0)
    return -1;

  lock_acquire(&mmap_lock);
  mf = malloc(sizeof(struct mfile));
  ASSERT(mf != NULL);
  mf->start = addr;
  mf->fd = fd;
  mf->mapid = t->gmapid++;
  mf->end = mf->start + size;
  
  //check for overlap
  for (int i = 0; i < size; i+= PGSIZE) {
    if (pagedir_get_page(t->pagedir, addr + i) != NULL ||
        pagedir_is_mmap(t->pagedir, addr + i) ||
        pagedir_is_exe(t->pagedir, addr + i)) {
      free(mf);
      return -1;
    }

    /* printf("* que %d %d\n", , ); */

    pagedir_set_page(t->pagedir, addr + i, PHYS_BASE, true);
    pagedir_set_mmap(t->pagedir, addr + i, true);
    pagedir_clear_page(t->pagedir, addr + i);
  }

  list_push_back(&t->mfiles, &mf->elem);

  lock_release(&mmap_lock);

  fsys_lock();
  mf->f = file_reopen(f);
  fsys_unlock();

  return mf->mapid;
}

void
sys_munmap(int mapid)
{
  struct thread *t = thread_current();
  struct mfile *mf;
  void *ipage = NULL;
  void *kpage = NULL;
  
  mf = mf_byId(t, mapid);

  // if fail test, you must changes it for "return;" and check if mapid is a valid id
  if (mf == NULL)
    exit(-1);

  ipage = mf->start;

  lock_acquire(&mmap_lock);
  // unmap memory
  for (; ipage <= mf->end; ipage+= PGSIZE) {
    kpage = pagedir_get_page(t->pagedir, ipage);
    if (kpage != NULL && pagedir_is_dirty(t->pagedir, ipage)) {
      mf_store_page(frame_lookup(kpage));
      pagedir_clear_page(t->pagedir, ipage);
      palloc_free_page(kpage);
    }
    pagedir_set_mmap(t->pagedir, ipage, false);
  }
  lock_release(&mmap_lock);

  list_remove(&mf->elem);
  sys_closef(mf->f);
  free(mf);

}
#endif
