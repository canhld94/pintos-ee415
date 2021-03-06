/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
#include "filesys/directory.h"

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b
#define FRACT_BITS (14)

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of processes in THREAD_BLOCKED state, that is, processes
   that are blocked. 
   We can identicate the blocked thread by check its blocked time
   but we dont want to control the blocked_time for all threads
   for every ticks, it's costly*/
static struct list blocked_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* System load */
static int system_load;

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
static long long system_ticks;  /* # of timer ticks in the system */
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
static void schedule();
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
static void reduce_block_ticks();
static int is_not_idle(struct thread *t);
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
  list_init (&all_list);
  list_init(&blocked_list);
  system_load = 0;

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING; // this is main and has already running
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
/*  As the dfinition of sema, thread_start create the idle thread first and then wait 
    untill the idle thread end
    So, till now we have an inital thread first (main in init.c), then go to idle thread 
    is the very first thread is add to ready queue. When Idle thread is done then main thread
    can continue
    */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
/*  Cannot find the implementaion till now , but we can assume that assume that everytime
    we got a tick, cpu call the schedule.
     */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

static int is_not_idle(struct thread *t)
{
  if(t == idle_thread) return 0;
  else return 1;
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  /* TODO: reduce number of ticks in blocked list */
  reduce_block_ticks();
  struct thread *t = thread_current ();

  /* Update statistics. */
  if(thread_mlfqs)
    system_ticks++;
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
  {
    kernel_ticks++;
    if(thread_mlfqs)
      t->recent_cpu += (1 << FRACT_BITS);
  }
  /* Calculate system load and recent cpu */
  if(system_ticks % TIMER_FREQ == 0 && thread_mlfqs)
  {
      system_load = (59 * system_load) / 60 +  
                    ((1 << FRACT_BITS) / 60) * (list_size(&ready_list) + is_not_idle(t));
      int recent_cpu_coe = (system_load << (FRACT_BITS + 1)) / ((system_load << 1) + (1 << FRACT_BITS));
      struct list_elem *e = list_head (&all_list);
      while ((e = list_next (e)) != list_end (&all_list)) 
        {
          struct thread *p = list_entry(e, struct thread, allelem);
          p->recent_cpu = recent_cpu_coe * (p->recent_cpu >> FRACT_BITS) 
                        + (p->nicess << FRACT_BITS);
        }
  }
  
  if (++thread_ticks >= TIME_SLICE)
  {
      /* if mlqfs calculate thread priority */
    if(thread_mlfqs)
    {
      struct list_elem *e = list_head (&all_list);
      while ((e = list_next (e)) != list_end (&all_list)) 
        {
          struct thread *p = list_entry(e, struct thread, allelem);
          int new_priority = PRI_MAX - (p->recent_cpu >> (FRACT_BITS + 2)) - (p->nicess << 1);
          if(new_priority > PRI_MAX) new_priority = PRI_MAX;
          if(new_priority < PRI_MIN) new_priority = PRI_MIN;
          p->priority = new_priority;
        }
    }
    /* Enforce preemption. */
    intr_yield_on_return ();
  }
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
  DBG_MSG_THREAD("[%s] created %s\n", thread_name(), t->name);

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
  /* Init the file descriptor array */
  t->ofile = palloc_get_page(0);
  int i;
  for (i = 0; i < NOFILE; i++)
  {
    t->ofile[i].file = NULL;
    t->ofile[i].mfile = NULL;
    t->ofile[i].dir = NULL;
    t->ofile[i].mmap_start = NULL;
    t->ofile[i].mmap_end = NULL;
  }
    /* Init the thread directory */
  if(thread_current()->cur_dir != NULL)
  {
    t->cur_dir = dir_reopen(thread_current()->cur_dir);
    t->workdir = t->cur_dir;
  }
  /* Add to run queue. */
  thread_unblock (t);
  return tid;
}

/* Puts the current thread (running in cpu) to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
/*  While a thread is running, it is already remove from ready list
    therefore, when schedule is called, it will not be scheduled again
    untill it is unblock (by a right person at a right time)*/
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
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
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
    /* if t have priority higher than current thread then run it imediately 
      not the idle thread because the first context switching is complex
      the idle thread is stupid but we want it to run */
  if(t->priority >= thread_get_priority() && thread_current() != idle_thread && !intr_context())
  {
    thread_yield();
  }
  intr_set_level (old_level);
}

/* Put current thread to sleep ticks timer tick */
void
thread_sleep(int64_t ticks){
  if(ticks == 0) return;
  else
  // set blocked time to ticks
  {
    enum intr_level old_level = intr_disable();
    thread_current()->blocked_ticks = ticks; /* tick time cout to 0 */
    list_push_back(&blocked_list, &thread_current()->elem);
    thread_block();
    intr_set_level (old_level);
  }
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
  ASSERT (t->status == THREAD_RUNNING || t->status == THREAD_ZOOMBIE);

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
  DBG_MSG_THREAD("[%s] thread exiting...\n", thread_name());
  ASSERT (!intr_context ());

#ifdef USERPROG
  if(thread_current()->my_elf != NULL) file_close(thread_current()->my_elf);
  ASSERT(lock_held_by_current_thread(&thread_current()->internal_lock))
  process_exit ();
  DBG_MSG_THREAD("[%s] get my lock\n", thread_name());
  lock_release(&thread_current()->internal_lock); /* Release internal lock, if parrent want then accquire it */
  DBG_MSG_THREAD("[%s] release my lock\n", thread_name());
  if(lock_try_acquire(&thread_current()->parrent_lock) == false) /* Parrent didn't wait */
  {
    thread_current()->status = THREAD_ZOOMBIE;  /* Cannot die :( */
    lock_acquire(&thread_current()->parrent_lock); /* Get my parrent lock back */
    DBG_MSG_THREAD("[%s] get my parrent lock\n", thread_name());
  }
  /* Parrent waited for me, exit normally */
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

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) {
    list_push_back (&ready_list, &cur->elem);
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
  struct thread *c = thread_current();
  c->priority = new_priority;
  c->non_donated_priority = new_priority;
  /* Make sure that new priority higer than donated priority */
  struct thread *m = list_entry(list_max(&c->waiters, thread_cmp, NULL),
                               struct thread, wait_elem);
  enum intr_level old_level = intr_disable();
  if(!list_empty(&c->waiters) && new_priority < m->priority)
  {
    c->priority = m->priority;
  }
  else c->priority = new_priority;
  intr_set_level(old_level);
  /* Yield if thread is no longer highest priority */
  struct thread *t = list_entry(list_begin(&ready_list), struct thread, elem);
  if(c->priority < t->priority)
  {
    thread_yield();
  }  
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{
  struct thread *c = thread_current();
  /* Ajust nice */
  if(nice > 20) nice = 20;
  if(nice < -20) nice = -20;
  c->nicess = nice;
  int new_priority = PRI_MAX - (c->recent_cpu >> (FRACT_BITS + 2)) - (c->nicess << 1);
  /* Ajust priority */
  if(new_priority > PRI_MAX) new_priority = PRI_MAX;
  if(new_priority < PRI_MIN) new_priority = PRI_MIN;
  thread_set_priority(new_priority);
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  /* Not yet implemented. */
  return thread_current()->nicess;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  /* Not yet implemented. */
  return (system_load*100) >> FRACT_BITS;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return (thread_current()->recent_cpu*100) >> FRACT_BITS;
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
  t->magic = THREAD_MAGIC;
  list_init(&t->waiters);
  t->waitee = NULL;
  old_level = intr_disable ();
  t->blocked_ticks = 0;
  if(t == initial_thread) 
  {
    t->nicess =0;
    t->recent_cpu = 0;
  }
  else 
  {
    t->nicess = thread_current()->nicess;
    t->recent_cpu = thread_current()->recent_cpu;
  }
  if(!thread_mlfqs)
  {
    t->priority = priority;
    t->non_donated_priority = priority;
  }
  else
  {
    int mlfqs_priority = PRI_MAX - (t->recent_cpu >> (FRACT_BITS + 2)) - (t->nicess << 1);
    if(mlfqs_priority > PRI_MAX) mlfqs_priority = PRI_MAX;
    if(mlfqs_priority < PRI_MIN) mlfqs_priority = PRI_MIN;
    t->priority = mlfqs_priority;
    t->non_donated_priority = mlfqs_priority;
  }
  #ifdef USERPROG
  lock_init(&t->internal_lock);
  lock_init(&t->parrent_lock);
  list_init(&t->childs);
  if(t != initial_thread)
  {
    t->parrent = thread_current();
    list_push_back(&thread_current()->childs, &t->child_elem);
    lock_acquire(&t->parrent_lock);
  }
  t->my_elf = NULL;
  #endif // USERPROG
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
  if (list_empty (&ready_list))
    return idle_thread;
  else
  {
    /* Choose thread with highest priority */
    struct list_elem *e = list_max(&ready_list, thread_cmp, NULL);
    list_remove(e);
    return list_entry(e, struct thread, elem);
  }
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
  if(cur != idle_thread) process_activate ();
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
  // DBG_MSG_THREAD("[%s] get CPU\n", thread_name());
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

/* TODO: implement of reduce_blocked_ticks */
static void
reduce_block_ticks(){
  if(list_empty(&blocked_list)) 
  {
    return;
  }
  else
  {
    /* travel the list and decrease blocked_ticks */
    struct list_elem *cur; /* an list iterator */
    struct thread *t;
    for (cur = list_begin (&blocked_list); cur != list_end (&blocked_list);)
     {  
        t = list_entry(cur, struct thread, elem);  /* get the thread in list  */
        t->blocked_ticks--;
        if(t->blocked_ticks == 0) 
        {
          cur = list_remove(cur);
          /* We dont want to use thread_unblock here */
          list_push_back (&ready_list, &t->elem);
          t->status = THREAD_READY;     
          /* If waked up thread have higest priority, yield on return */
          if(t->priority > thread_current()->priority)
          {
            intr_yield_on_return();
          }   
        }
        else 
        {
          cur = cur->next;
        }
     }
     return;
    /* code */
  }
  
}

/* TODO: Implement list_less_fuct */
bool 
thread_cmp( const struct list_elem *a,
            const struct list_elem *b,
            void * aux) 
{
    struct thread *t_a = list_entry(a, struct thread, elem);
    struct thread *t_b = list_entry(b, struct thread, elem);
    if(t_a->priority < t_b->priority) return true;
    else return false;
}

int is_not_initial(struct thread *t)
{
  if(t == initial_thread) return 0;
  else return 1;
}