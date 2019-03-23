
 	
                         +-----------------+
                         |      EE415      |
                         |    PROJECT 1    |
                         | DESIGN DOCUMENT |
                         +-----------------+

---- GROUP ----
Duc-Canh Le <20184167> <canhld@kaist.ac.kr>
Watermelon and Mun

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for
>> the TAs, or extra credit, please give them here.
Watermelon and Mun are my cats, they don't need credit but if you have
food you can bring it to them. They are always somewhere close to KI 
building.

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

> Policy: Sleep threads should not be scheduled until waking up
> Implemetation:
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
The alogrimth for tracking threads blocked ticks is O(N) with N
is number of sleep threads. There could be problematic if there are 
lots of thread sleeps. We need to travel the whole blocked_listed 
and it may be costly. If the interrupt handler takes to long then 
it will affect the system.



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
When running thread set its priority itself, chech whether it's still
the highest priority thread and yield if it's not.

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
wating for it (possibly from different locks).

Whenever a thread (call A) are going to release lock (or up a sema), 
it first remove from it's waiters all threads in sema waiters. Then a 
thread with highest priority (called B) is selected from sema waiters
and unblocked (and possibley will get the lock soon). When B accquired 
lock, it first checks A waiters. If A waiters have no thread with higher
priority than A original priority then A priority are set to its origin. 
Otherwise A priority is set to the highest priority in its waiters.
This design make sure that thread can lost donated priority when it
release lock but still have highest priority compared to its waiters
(i.e. from other locks).

When a thread set it new_priority itself, the new_thread is compared 
and replaced by highest priority in its waiters if neccessary.
A thread always maintains its original priority (i.e. not the donated
priority). 

- For priority nest or chain donation:
A thread (called A) also know its waitee (called B), so whenever A was 
donated, A will check the priority of B (expect when B is NULL). If B 
have lower priority, A will donated its priority to B. B repeated this
process.

---- SYNCHRONIZATION ----

> Interrupt
Most of code in this section executed with interrupt disabled and no
variable is sharing among interrupt handler and threads, so there is 
not much problem within sync. among interrupt handler and threads.

> Ohter sync. problem
- When to give up the donated priority?
It's much easier for thread to let the next lock holder control its 
priority instead of trying to do itself when release lock. For example,
first we came up with a solution where thread always set it priority
to origin before release lock, if it's need to be changed then the next
lock holder will do it. But it doesn't work because lock_release also
be called in many function (e.g. printf), so whenever printf is called
then the thread priority is set to the origin unexpectedly. 



---- RATIONALE ----

>> Critique your design, pointing out advantages and disadvantages in
>> your design choices.

> Advantage: 
It's simple and straightfoward

We used list_max <O(N)>, which is much better than list_sort <O(NlogN)>,
for select highest priority thread in a list

> Disadvantage:
It may lead to starvation when a low priority thread possibly be ignore
for very long time.

Sometime we need to travel the list <O(logN)>, this may be costly

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
  int recent_cpu;                     /* Recent CPU, sfi31_17 */

Added to thread.c:
/* Number of fractional bits in fixed point */
#define FRACT_BITS (14)
/* System load, sfi31_17 */
static int system_load;

---- ALGORITHMS ----

>> Briefly describe your implementation of advanced scheduling
Both the policy and how to implement are well and clearly describe 
in Pintos doccument and the lecture, we just need to follow it to finish
this part. Here I just point out some critial in this impleentation:

> Fixed point implementation:
The fixed-point is embedded in the code, so there're some calculations
look tricky, e.g. with system load:
- Original formula:
system_load = 59/60 * system_load + 1/60 * ready_threads
- To fixed point:
59/60 * system_load --> 59*(1 << FRACT_BITS)/60 * system_load *(1 >> FRACT_BITS)
                    --> (59 * system_load) / 60
1/60 * ready_threads --> (1 << FRACT_BITS)/60 * ready_threads
The recent_cpu is similar.
 
---- SYNCHRONIZATION ----

> Interrupt
Some thread attributes are shared between thread and the interrupt handle,
e.g. priority, cpu_recents. Therefore whenever access these variable in the
thread we disable the interrupt.


---- RATIONALE ----

>> Critique your design, pointing out advantages and disadvantages in
>> your design choices.

> Advantage (probaly the mlfqs advantage)
It's fair and practical (in the actual system where threads use both CPU 
and IO)

> Disadvantage (almost the mlfqs disadvantage)
The implementation is a little tricky (not easy to understand). We don't 
want to build the whole fixed-point arthimetic just for few operations.
Hopefuly, we don't need to use fixed-point in next projects.

Updating recent cpu and priority is O(N), so I'm not sure that mlfqs is 
suitabe in actual system.

We need to do many things in the interrupt handle, if we don't do it carefully
or there are to many threads I think the system may have some "unpredictable" behaviors due to missing timer ticks.
