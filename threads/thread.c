#include "threads/thread.h"
#include <debug.h>
#include <list.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "fixed-point.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

#ifdef VM
#include "vm/frame.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of pending threads (it isn't zoom xd) */
static struct list waiting_room;

static struct list mlfqs_queues[PRI_MAX + 1];

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&waiting_room);
  list_init (&all_list);

  for (int pri = PRI_MIN; pri <= PRI_MAX; pri++) {
    list_init(mlfqs_queues + pri);
  }

  load_avg = 0;

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();

  initial_thread->nice = 0;
  initial_thread->recent_cpu = 0;

  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

pstate *search_pstate(struct thread *parent, tid_t tid) {
  pstate *ps = NULL;

  if (parent == NULL || parent->childsexit == NULL) {
    return NULL;
  }

  tid_t maxi = stkcast(parent->childsexit, tid_t);

  for (int i = 1; i <= maxi; i++) {
    if (stkcast(parent->childsexit + i * 4, tid_t) == tid) {
      ps = &stkcast(parent->childsexit + (1023 - i) * 4, pstate);
      break;
    }
  }

  return ps;
}

/* Hey, Here's a colorados code */
void update_priority(struct thread *t, void *aux UNUSED)
{
  if (t == idle_thread) {
    return;
  }

  int old_priority = t->priority;

  t->priority = PRI_MAX - pq_to_int(t->recent_cpu) / 4 - (t->nice * 2);

  if (t->priority < 0) {
    t->priority = 0;
  } else if (t->priority > 63) {
    t->priority = 63;
  }

  if (t->status == THREAD_READY && old_priority != t->priority) {
    list_remove(&t->elem);
    list_push_back(mlfqs_queues + t->priority, &t->elem);
  }
}

void update_priority_forall(void)
{
  thread_foreach(update_priority, NULL);
}

void update_recent_cpu_for(struct thread *t, void *aux UNUSED) {
  if (t == idle_thread) {
    return;
  }

  t->recent_cpu = pq_mul(pq_div((2 * load_avg), (2 * load_avg + F)), t->recent_cpu) + int_to_pq(t->nice);
}

void update_recent_cpu(void)
{
  thread_foreach(update_recent_cpu_for, NULL);
}

void update_load_avg(void)
{
  int run_val = thread_current() != idle_thread ? 1 : 0;

  struct list *queue = mlfqs_queues + PRI_MAX;

  while(queue >= mlfqs_queues) {
    run_val += list_size(queue);
    queue--;
  }

  load_avg = (59 * load_avg) / 60 + int_to_pq(run_val) / 60;

}

void increment_recent_cpu(void)
{
  struct thread *cur = thread_current();

  if (idle_thread != cur) {
    cur->recent_cpu = cur->recent_cpu + F;
  }
}

void yield_if_iam_manco(int priority)
{
  if (!intr_context () && priority > thread_get_priority()) {
    thread_yield();
  }
}

void clean_waiting_room(int64_t current_ticks)
{
  struct list_elem *thritem = list_begin(&waiting_room);
  struct list_elem *endthr = list_end(&waiting_room);
  struct thread *thread;

  while (thritem != endthr) {
    thread = list_entry(thritem, struct thread, elem);

    if(thread->sleep_until <= current_ticks) {
      thritem = list_remove(thritem);
      thread_unblock(thread);
    } else {
      thritem = list_next(thritem);
    }
  }
}

void to_waiting_room(int64_t ticks)
{
  enum intr_level old_level;
  old_level = intr_disable();

  struct thread *cthread = thread_current();
  cthread->sleep_until = timer_ticks() + ticks;

  list_push_back(&waiting_room, &cthread->elem);
  thread_block();

  intr_set_level(old_level);
}

void propagate_priority(struct thread *t)
{
  #ifdef VM
  if(t->locked_me!=NULL)
  {
    if (t->priority > t->locked_me->holder->priority) {
      t->locked_me->holder->priority = t->priority;
      propagate_priority(t->locked_me->holder);
    }
  }
  #endif
}
void sort_list_by_priority(void){
  list_sort(&ready_list,sort_list,NULL);
}

bool max_comparator(const struct list_elem * a, const struct list_elem *b, void * aux UNUSED)
{
  struct thread *t1=list_entry(a, struct thread, elem);
  struct thread *t2=list_entry(b, struct thread, elem);

  get_max_thread_priority(t1);
  get_max_thread_priority(t2);
  return t1->priority < t2->priority;
}

void get_max_thread_priority(struct thread *t){
  struct list_elem *iter = list_begin(&t->locks);
  struct thread *max;
  struct list *waiters;

  #ifdef VM
  while (iter!=list_end(&t->locks))
  {
    waiters = &list_entry(iter, struct lock, elem)->semaphore.waiters;

    if (!list_empty(waiters)) {
      max = list_entry(list_max(waiters, max_comparator, NULL), struct thread,
                       elem);
      if (t->priority < max->priority) {
        t->priority = max->priority;
      }
    }

    iter = list_next(iter);
  }
  #endif
}
/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  
  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

#ifdef VM
  init_frame_table();
#endif

  t->parent = &thread_current()->I;
  (*t->parent)->child = t;
  if ((*t->parent)->childsexit == NULL) {
    (*t->parent)->childsexit = palloc_get_page(PAL_USER | PAL_ZERO);

    if ((*t->parent)->childsexit == NULL)
      return TID_ERROR;

    stkcast((*t->parent)->childsexit, int) = 1;
  }

  t->pid = stkcast((*t->parent)->childsexit, int);
  stkcast((*t->parent)->childsexit, int) += 1;

  if (t->pid > 511) {
    return TID_ERROR;
  }

  pstate ps;
  ps.descriptor.exit = -1;
  ps.descriptor.alive = 1;
  ps.descriptor.estorbo = 0;
  ps.descriptor.child = 1;

  stkcast((*t->parent)->childsexit + (t->pid) * 4, int) = t->tid;
  stkcast((*t->parent)->childsexit + (1023 - t->pid) * 4, int) = ps.value;

  t->pid = t->tid;
  /* Add to run queue. */
  thread_unblock (t);
  if (thread_mlfqs) {
    struct thread *tt = thread_current();
    t->recent_cpu = tt->recent_cpu;
    t->nice = tt->nice;
  }

  yield_if_iam_manco(priority);
  
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  struct thread *current_thread = thread_current();
  if (!thread_mlfqs)
  {
    sort_list_by_priority();
  }
  current_thread->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  if (!thread_mlfqs)
  {
      list_insert_ordered (&ready_list, &t->elem, sort_list, NULL);
  } else
  {
      list_push_back(mlfqs_queues + t->priority, &t->elem);
  }
  t->status = THREAD_READY;
  intr_set_level (old_level);

}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

  struct lock *l;
  struct thread *cur = thread_current();
  while (!list_empty(&cur->locks)) {
    l = list_entry(list_pop_front(&cur->locks), struct lock, elem);
    lock_release(l);
  }

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

bool
sort_list (const struct list_elem * a, const struct list_elem *b, void * aux UNUSED)
{
  return list_entry(a, struct thread, elem) ->priority > list_entry(b, struct thread, elem) ->priority;
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (!thread_mlfqs)
  {
    if (cur != idle_thread)
    {
      struct thread *t = list_entry(list_begin(&ready_list), struct thread, elem);
      if (t->priority < thread_get_priority())
        list_push_front (&ready_list, &cur->elem);
      else {
        list_insert_ordered (&ready_list, &cur->elem, sort_list, NULL); 
      }
    }
  } else
  {
    if (cur != idle_thread)
      list_push_back(mlfqs_queues + cur->priority, &cur->elem);
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  struct thread *current_thread = thread_current();
  current_thread->priority = new_priority;
  current_thread->real_priority = new_priority;

  struct list_elem *e = list_begin(&ready_list);
  struct thread *t;

  while (e != list_end(&ready_list))
  {
    t = list_entry(e, struct thread, elem);

    if (t->priority > new_priority)
    {
      thread_yield();
      break;
    }

    e = list_next(e);
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  struct thread *t = thread_current();
  if (!thread_mlfqs) {
    get_max_thread_priority(t);
  }
  return t->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{
  struct thread *t = thread_current();
  t->nice = nice;
  update_priority(t, NULL);

  struct list_elem *iter = list_begin(&all_list);
  struct thread *th;

  while(iter != list_end(&all_list)) {
    th = list_entry(iter, struct thread, allelem);
    if (th->status == THREAD_READY && th->priority > t->priority) {
      thread_yield();
      break;
    }
    iter = list_next(iter);
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void) {
  return pq_to_int(round_pq(load_avg * 100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return pq_to_int(round_pq(thread_current()->recent_cpu * 100));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->real_priority = priority;
  t->magic = THREAD_MAGIC;
  list_init(&t->locks);
  t->locked_me=NULL;
  t->childsexit = NULL;
  t->estorbo = 0;
  t->files=NULL;
  t->sema_parent = NULL;
  t->I=t;
  t->parent=NULL;
  t->exit_state = -1;
  /* VM */
  list_init(&t->swps);
  list_init(&t->est);
  /* VM - end */
  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (thread_mlfqs) {
    struct list *queue = mlfqs_queues + PRI_MAX;

    while (queue >= mlfqs_queues) {
      if (!list_empty(queue)) {
        return list_entry(list_pop_front(queue), struct thread, elem);
      }
      queue--;
    }

    return idle_thread;
  }


  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
