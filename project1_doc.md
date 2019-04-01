			+--------------------+
			|   EE 415 / PD 511  |
			| PROJECT 1: THREADS |
			|   DESIGN DOCUMENT  |
			+--------------------+
				   
---- GROUP ----

>> Fill in the names and email addresses of your group members.
Duc-Canh Le <canhld@kaist.ac.kr>

>> Fill in your GitLab repository address.
https://gitlab.com/canhld94/pintos-ee415

---- PRELIMINARIES ----
>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.

I use one day token

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.
None.

			     ALARM CLOCK
			     ===========

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

Added to struct thread:
    /* Member for alarm-clock */
    int64_t blocked_ticks;              /* Number of ticks that thread is blocked*/

Added global variable in thread.c
    static struct list blocked_list;    /* List of thread that is blocked by timer_sleep */


---- ALGORITHMS ----

>> A2: Briefly describe what happens in a call to timer_sleep(),
>> including the effects of the timer interrupt handler.

Put the thread into block mode, set the blocked_ticks to ticks and add the 
thread to the blocked_list. Everytime recieve timer ticks, travel the 
blocked_list and decrease the blocked_ticks of threads in the list. Any
threat that blocked_ticks reach 0 is removed from blocked_list and unblocked.

>> A3: What steps are taken to minimize the amount of time spent in
>> the timer interrupt handler?

The blocked_threads list logically is not neccessary, we can use the all_list.
But if we have lots of thread in the system then traveling the all_list
is costly and may take long time in interrupt handler.

---- SYNCHRONIZATION ----

>> A4: How are race conditions avoided when multiple threads call
>> timer_sleep() simultaneously?

In the scope of the project, we assume that there are only 1 CPU. Therefore
threads cannot "truely" call timer_sleep() simutaneously. In case of interrupt
come when a thread is calling timer_sleep, it's also fine as we present in the 
next question.

>> A5: How are race conditions avoided when a timer interrupt occurs
>> during a call to timer_sleep()?

Thread_blocked and blocked_ticks are elements of thread but can also need to
be accessed in interrupt handler as well. So, when thread want to access them, it 
disable interrupt first. If interrupt happens before then it's nothing to worry.


---- RATIONALE ----

>> A6: Why did you choose this design?  In what ways is it superior to
>> another design you considered?

> Advantage 
This design is simple but efficient because the sleep threads are not 
scheduled. 

> Disadvantage
The alogrimth for tracking threads blocked ticks is O(N) with N
is number of sleep threads. There could be problematic if there are 
lots of thread sleeps. However it's still better than using
all_list (which I had actually did before comming to this solution).

			 PRIORITY SCHEDULING
			 ===================

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

Added to struct thread:
  /* Member for priority schedule */
  int non_donated_priority;           /* Thread original priority */
  struct list_elem wait_elem;         /* List elemen for waiting list */
  struct list waiters;                /* List of threads wating for this thread */ 
  struct thread *waitee;              /* Thread that this thread is waiting for */


>> B2: Explain the data structure used to track priority donation.
>> Use ASCII art to diagram a nested donation.  (Alternately, submit a
>> .png file.)

Waiters is a list of thread wating for current threads. So the current thread
priority is logicaly the maximum priority in this list (including itself).

      Head --> waiter 0 --> waiter 1 --> ... --> waiter N --> Tail

Waitee is the pointer to thread that current thread are waiting (if there is)

    thread A0 --> A waitee (A1) --> A1 waitee (A2) --> ... --> NULL


---- ALGORITHMS ----

>> B3: How do you ensure that the highest priority thread waiting for
>> a lock, semaphore, or condition variable wakes up first?

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

>> B4: Describe the sequence of events when a call to lock_acquire()
>> causes a priority donation.  How is nested donation handled?

- For priority donation: 
Whenever a current thread want to aqquire a lock, it first add itself to 
the holder waiters list, then check the holder's priority and donated 
if necessary. This design make sure that lock holder always have highest 
priority of lock's wating threads and thread can know which threads are 
wating for it (possibly from different locks).
- For priority nest or chain donation:
A thread (called A) also know its waitee (called B), so whenever A was 
donated, A will check the priority of B (expect when B is NULL). If B 
have lower priority, A will donated its priority to B. B repeated this
process.

>> B5: Describe the sequence of events when lock_release() is called
>> on a lock that a higher-priority thread is waiting for.

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
- Note: A thread always maintains its original priority (i.e. not the donated priority). 

---- SYNCHRONIZATION ----

>> B6: Describe a potential race in thread_set_priority() and explain
>> how your implementation avoids it.  Can you use a lock to avoid
>> this race?

When a thread set it new_priority itself, the new_thread is compared 
and replaced by highest priority in its waiters if neccessary.
A thread always maintains its original priority (i.e. not the donated
priority). However, its priority can also be set by another threads, 
so we disable interrupts durring thread_set_priority.

---- RATIONALE ----

>> B7: Why did you choose this design?  In what ways is it superior to
>> another design you considered?

This design is easy to simple, although we need to add some of threads
attribute. But I think it's acceptable (and neccessary).

> Advantage: 
The implementation is simple and straightfoward

We used list_max <O(N)>, which is much better than list_sort <O(NlogN)>,
for select highest priority thread in a list

> Disadvantage:
It may lead to starvation when a low priority thread possibly be ignore
for very long time.

Sometime we need to travel the list <O(N)>, this may be costly. We are
working with interrup disabled so we want to minimize the time as much as
possible.

It's not yet cover all the possible scenario that can happens in a real
system. For example, when a thread release a lock, it will remove all
of thread in the semaphore waiters from its waiters but only select 
one thread to be unblocked, so how about other threads? Who gonna be 
their waitee now? (We can added some code to fix it).

			  ADVANCED SCHEDULER
			  ==================

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

Added to struct thread:
  /* Member for advanced scheduler */
  int nicess;                         /* Nice value */
  int recent_cpu;                     /* Recent CPU, sfi31_17 */

Added to thread.c:
/* Number of fractional bits in fixed point */
#define FRACT_BITS (14)
/* System load, sfi31_17 */
static int system_load;
/* # of timer ticks in the system */
static long long system_ticks; 

---- ALGORITHMS ----

>> C2: Suppose threads A, B, and C have nice values 0, 1, and 2.  Each
>> has a recent_cpu value of 0.  Fill in the table below showing the
>> scheduling decision and the priority and recent_cpu values for each
>> thread after each given number of timer ticks:

timer  recent_cpu    priority   thread
ticks   A   B   C   A   B   C   to run
-----  --  --  --  --  --  --   ------
 0		0   0	0  63  61  59   A    
 4		4	0	0  62  61  59   A
 8      8	0	0  62  61  59   A
12     12   0   0  61  61  59   B
16     12   4   0  61  62  59   B
20     12   8   0  61  61  59   A     
24	   16   8   0  60  61  59   B
28     16   12  0  59  60  59   B
32     16   16  0  59  59  59   C
36     16   16  4  59  59  60   C

>> C3: Did any ambiguities in the scheduler specification make values
>> in the table uncertain?  If so, what rule did you use to resolve
>> them?  Does this match the behavior of your scheduler?

Yes, sometime some threads have same priority. We just execute those threads 
in round robin manner. 
It's match, because in thread_yield we push back current
thread to back of ready list. And the list_max alway select the first maximum 
prority thread from this list (because our list_less function use <, not <=).

>> C4: How is the way you divided the cost of scheduling between code
>> inside and outside interrupt context likely to affect performance?

Actually most of the code was implemented in thread_ticks, which is the interrupt
handler, so we just try to optimize the code in this function:
   Using >> and << when its possible to replce * and / (not sure it's gonna good 
   within current CPU technology).
   Using list_max <O(N)> instead of list_short <at least O(NlogN)>
It's just unavoidable to modify thread_ticks (that is, the algorithm).

---- RATIONALE ----

>> C5: Briefly critique your design, pointing out advantages and
>> disadvantages in your design choices.  If you were to have extra
>> time to work on this part of the project, how might you choose to
>> refine or improve your design?

> Advantage (probaly the mlfqs advantage)
It's fair and practical (in the actual system where threads use both CPU 
and IO)

> Disadvantage (almost the mlfqs disadvantage)

Updating recent cpu and priority is O(N), so I'm not sure that mlfqs is 
suitabe in actual system (with many threads).

We need to do many things in the interrupt handle, if we don't do it carefully
or there are to many threads I think the system may have some "unpredictable" 
behaviors due to missing timer ticks.

>> C6: The assignment explains arithmetic for fixed-point math in
>> detail, but it leaves it open to you to implement it.  Why did you
>> decide to implement it the way you did?  If you created an
>> abstraction layer for fixed-point math, that is, an abstract data
>> type and/or a set of functions or macros to manipulate fixed-point
>> numbers, why did you do so?  If not, why not?

The fixed-point is embedded in the code, so there're some calculations
look tricky, e.g. with system load:
- Original formula:
system_load = 59/60 * system_load + 1/60 * ready_threads
- To fixed point:
59/60 * system_load  --> 59*(1 << FRACT_BITS)/60 * system_load *(1 >> FRACT_BITS)
                     --> (59 * system_load) / 60
1/60 * ready_threads --> (1 << FRACT_BITS)/60 * ready_threads

The implementation is a little tricky (not easy to understand). We don't 
want to build the whole fixed-point arthimetic just for few operations.
Hopefuly, we don't need to use fixed-point in next projects.

			   SURVEY QUESTIONS
			   ================

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

>> In your opinion, was this assignment, or any one of the three problems
>> in it, too easy or too hard?  Did it take too long or too little time?

>> Did you find that working on a particular part of the assignment gave
>> you greater insight into some aspect of OS design?

>> Is there some particular fact or hint we should give students in
>> future quarters to help them solve the problems?  Conversely, did you
>> find any of our guidance to be misleading?

>> Do you have any suggestions for the TAs to more effectively assist
>> students, either for future quarters or the remaining projects?

>> Any other comments?
