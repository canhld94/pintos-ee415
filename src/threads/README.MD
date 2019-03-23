
 	
                         +-----------------+
                         |      EE415      |
                         |    PROJECT 1    |
                         | DESIGN DOCUMENT |
                         +-----------------+

---- GROUP ----

Duc-Canh Le <canhld@kaist.ac.kr>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for
>> the TAs, or extra credit, please give them here.


>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation,
>> course text, and lecture notes.

None.



                                 ALARM-CLOCK
                                 ===========

---- DATA STRUCTURES ----

>> Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration.
>> Identify the purpose of each in 25 words or less.


Added to struct thread:
    /* Member for alarm-clock */
    int64_t blocked_ticks;              /* Number of ticks that thread is blocked*/

Added global variable in thread.c
    static struct list blocked_list;    /* List of thread that is blocked by timer_sleep */

---- ALGORITHMS ----

>> Briefly describe your implementation of timer_sleep(int 64_t ticks) 

Put the thread into block mode, set the blocked_ticks to ticks and add the 
thread to the blocked_list. Everytime recieve timer ticks, travel the 
blocked_list and decrease the blocked_ticks of threads in the list. Any
threat that blocked_ticks reach 0 is removed from blocked_list and unblocked.

---- SYNCHRONIZATION ----

> What happen if a thread holding semaphore or lock sleep?
It sleeps

> thread_blocked and blocked_ticks are elements of thread but can also need to
> be accessed in interrupt handler. What happen if when a thread is trying 
> to change them an interrup happen?
When thread want to access them, it disable interrupt first.

---- RATIONALE ----

>> Critique your design, pointing out advantages and disadvantages in
>> your design choices.

We implement the sleep function as a method of threads, the timer 
sleep only call this method.

The blocked_threads list logically is not neccessary, we can use the all_list.
But if we have lots of thread in the system then traveling the all_list
is costly.

> Advantage 
This design is simple but efficient because the sleep threads are not 
scheduled. 

> Disadvantage
There could be problematic if there are lots of thread sleeps. We 
need to travel the whole blocked_listed and it may be costly. If 
the interrupt handler takes to long then it will affect the system.



                                 PRIORITY-SCHEDULING
                                 ===================

---- DATA STRUCTURES ----

>> Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration.
>> Identify the purpose of each in 25 words or less.

Added to struct thread:
  /* Member for priority schedule */
  int non_donated_priority;           /* Original priority */
  struct list_elem wait_elem;         /* List elemen for waiting list */
  struct list waiters;                /* List of thread wating for this thread */  
  struct thread *waitee;              /* Thread that this thread is waiting for */


---- ALGORITHMS ----

>> Briefly describe your implementation of priority scheduling

> Policy: Thread have highest priority in ready queue always run first
> Implementation:
- For priority scheduling in scheduler: 
Select the thread with highest priority in ready queue (use list_max). 
When unblocking a thread, if this thread have higher priority than 
running thread then imediately yield the CPU (except the idle thread).

- For priority scheduling in semaphore: 
Select the thread with highest priority in semaphore waiters list to 
be unblocked.

- For priority in condition variables: 
Select the semaphore higest priority thread in its waiters list to be 
upped (this sema is up then the highest priority thread will be unblocked).

> Policy: A thread always have higher priority than its waiters
> Implementation:
- For priority donation: 
Whenever a current thread want to aqquire a lock, it first add itself to 
the holder waiters list, then check the holder's priority and donated 
if necessary. This design make sure that lock holder always have highest 
priority of lock's wating threads and thread can know which threads are 
wating for it (possibly from different locks). A thread also maintains its 
original priority.
Whenever a thread (call A) are going to release lock (or up a sema), 
it first remove from it's waiters all threads in sema waiters. Then a 
thread with highest priority (called B) is selected from sema waiters
and unblocked (and possibley will get the lock soon). When B accquired 
lock, it first checks A waiters. If A waiters have no thread with higher
priority than A original priority then A priority are set to its origin. 
Otherwise A priority is set to the highest priority in its waiters.
This design make sure that thread can lost donated priority when it
release lock but still have highest priority compared to its waiters
(i.e. from other locks) 

- For priority nest or chain donation:
A thread (called A) also know its waitee (called B), so whenever A was 
donated, A will check the priority of B (expect when B is NULL). If B 
have lower priority, A will donated its priority to B. B repeated this
process.

---- SYNCHRONIZATION ----

> Interrupt
Most of code in this section executed with interrupt disabled, so 
there is not much problem within sync. among interrupt handler and 
threads.

>> 


---- RATIONALE ----

>> Critique your design, pointing out advantages and disadvantages in
>> your design choices.

> Advantage: 
It's simple and straightfoward

> Disadvantage:
It's not yet cover all the possible scenario that can happens in a real
system. For example, when a thread release a lock, it will remove all
of thread in the semaphore waiters from its waiters but only select 
one thread to be unblocked, so how about other threads? Who gonna be 
their waitee now? (We can added some code to fix it).


                                 PRIORITY-SCHEDULING
                                 ===================

---- DATA STRUCTURES ----

>> Copy here the declaration of each new or changed `struct' or `struct'
>> member, global or static variable, `typedef', or enumeration.
>> Identify the purpose of each in 25 words or less.

Added to struct thread:
  /* Member for advanced scheduler */
  int nicess;                         /* Nice value */
  int recent_cpu;                     /* Recent CPU */

Added to thread.c:
/* Number of fractional bits in fixed point */
#define FRACT_BITS (14)
/* System load */
static int system_load;

---- ALGORITHMS ----

>> Briefly describe your implementation of advanced scheduling
Both the policy and how to implement are well and clearly describe 
in Pintos doccument. Here I just point out some critial in this 
impleentation:

> Fixed point implementation:
The fixed-point is embedded in the code, so there're some calculation
look tricky, e.g. with system load:
- Original formula:
system_load = 59/60 * system_load + 1/60 * ready_threads
- To fixed point:
59/60 * system_load --> 59*(1 << FRACT_BITS)/60 * system_load *(1 >> FRACT_BITS)
                    --> (59 * system_load) / 60
1/60 * ready_threads --> (1 << FRACT_BITS)/60 * ready_threads
The recent_cpu is similar.

> 

---- SYNCHRONIZATION ----

>> Interrupt
Most of code in this section executed with interrupt disabled, so 
there is not much problem within sync. among interrupt handler and 
threads.

>> 


---- RATIONALE ----

>> Critique your design, pointing out advantages and disadvantages in
>> your design choices.
