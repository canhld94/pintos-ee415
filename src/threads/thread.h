/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H
#define USERPROG
#define DEBUG 0
#define NOFILE 128
#include <debug.h>
#include <list.h>
#include "hash.h"
#include <stdint.h>
#include <filesys/filesys.h>
#include <threads/synch.h>
#include "filesys/file.h"
#include "filesys/directory.h"

/* Debug  */
#if(DEBUG > 3)
  #define DBG_MSG_THREAD printf
#else
  #define DBG_MSG_THREAD(...) do {} while (0)
#endif 

#if(DEBUG > 2)
  #define DBG_MSG_USERPROG printf
#else
  #define DBG_MSG_USERPROG(...) do {} while (0)
#endif 

#if(DEBUG > 1)
  #define DBG_MSG_VM printf
#else 
  #define DBG_MSG_VM(...) do{} while (0)
#endif

#if(DEBUG > 0)
  #define DBG_MSG_FS printf
#else 
  #define DBG_MSG_FS(...) do{} while (0)
#endif

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING,       /* About to be destroyed. */
    THREAD_ZOOMBIE      /* Thread parrent didn't call */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* An opened file */
struct openning_file
{
  struct file *file;
  struct file *mfile;
  struct dir *dir;
  uint8_t *mmap_start;
  uint8_t *mmap_end;

};

/* Page management */
struct page_mgm
{
  struct lock lock;
  struct hash *page_table;
};

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element for ready threads list. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    struct thread *parrent;             /* Parrent of this process */
    struct list childs;                 /* Childs of this process */
    struct list_elem child_elem;        /* List element for child */
    struct openning_file *ofile;
    struct lock internal_lock;          /* My own lock */
    struct lock parrent_lock;           /* My parrent lock */
    int userprog_status;
    struct file *my_elf;

    /* Used in Project 3 - VM */
    struct page_mgm *page_mgm;

    /* Used in Project 4 - VM */
    struct dir *cur_dir;                /* Process current directory */
    struct dir *workdir;               /* Directory that process works on */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
    /* Member for alarm-clock */
    int64_t blocked_ticks;              /* Time of waiting  */
    /* Member for priority schedule */
    int non_donated_priority;
    struct list_elem wait_elem;         /* List elemen for waiting list */
    struct list waiters;                /* List of thread wating for this thread */   
    struct thread *waitee;              /* Thread that this thread is waiting for*/
    /* Member for advanced scheduler */
    int nicess;                         /* Nice value */
    int recent_cpu;                     /* Recent CPU */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);
void thread_sleep(int64_t ticks);
int is_not_initial(struct thread *t);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

bool thread_cmp( const struct list_elem *a,
            const struct list_elem *b,
            void * aux);

#endif /* threads/thread.h */
