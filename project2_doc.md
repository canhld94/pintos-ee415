		     +--------------------------+
       	     |	   EE 415 / PD 511     	|
		     | PROJECT 2: USER PROGRAMS	|
		     | 	   DESIGN DOCUMENT     	|
		     +--------------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Duc-Canh Le <canhld@kaist.ac.kr>

>> Fill in your GitLab repository address.

https://gitlab.com/canhld94/pintos-ee415

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.
I use two days token

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.
Operating System Concept (8th), Chapter 3: Process Concept, p.106

			   ARGUMENT PASSING
			   ================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

---- ALGORITHMS ----

>> A2: Briefly describe how you implemented argument parsing.  How do
>> you arrange for the elements of argv[] to be in the right order?
>> How do you avoid overflowing the stack page?
1. Parse the command with strtok_r() and save to argv (char **) and argc
2. Setup the stack:
Push the argv value to the stack and save their address in an array argv_addr[];
Align if neccessary
Push argv_addr to the stack
Push argc to the stack
Push NULL to the stack as a fake return address
The maximum length of the command is set to a reasonable value to avoid stack
overflow (1024 characters)

---- RATIONALE ----

>> A3: Why does Pintos implement strtok_r() but not strtok()?
According to the manual, strtok cannot parse two string simutanously,
therefore an interrupt during calling strtok may potentaly break the
user commands.
>> A4: In Pintos, the kernel separates commands into a executable name
>> and arguments.  In Unix-like systems, the shell does this
>> separation.  Identify at least two advantages of the Unix approach.
Flexible: Shell can be written in higher language such as C++, Python 
which is better support for string manipulation
Efficiency: Kernel could be free to do other important task (scheduling,
error handling), especially when the command lengh is long.

			     SYSTEM CALLS
			     ============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

Addition member in thread structure:
    uint32_t *pagedir;                  <!-- Page directory -->
    struct thread *parrent;             <!-- Parrent of this process -->
    struct list childs;                 <!-- Childs of this process --> 
    struct list_elem child_elem;        <!-- List element for child -->
    struct file *ofile[NOFILE];			<!-- Array of file pointer -->
    struct lock internal_lock;          <!-- My own lock --> 
    struct lock parrent_lock;           <!-- My parrent lock  -->
    int userprog_status;				<!-- User program return status -->
    struct file *my_elf;				<!-- Poiter to user executatble file -->

New struct to pass argument to child thread:
	struct execution
	{
	char fn_copy[1024]; 		<!-- Copy of the filename, assume that max = 1024 char -->
	struct lock ex_lock; 		<!-- Lock for the condvar -->
	struct condition ex_cond;	<!-- Condvar to notify the parrent that child load successfuly or not  -->
	int load_status;			<!-- Status of loading execution file -->
	};
>> B2: Describe how file descriptors are associated with open files.
>> Are file descriptors unique within the entire OS or just within a
>> single process?
Each process maintains an array of file poiter (that are initilized to 
NULL at the time process is created). When open is called, the fist not
NULL poiter in the array is choosen to store file and the file descriptor
is the index of the pointer in the array. This FD is unique within a single
process.

---- ALGORITHMS ----

>> B3: Describe your code for reading and writing user data from the
>> kernel.
There are two type of reading user data from the kernel:
1. Read the data value from user stack
--> Staightforward
2. Read the poiter to data from user stack and dereference this pointer
If this pointer point to unvalid address (NULL or kernel virtual address
space), we can simply kill the thread by calling exit(-1).
If this poiter point to valid address in user address space but the memory
has not been allocated, the pagefault will be call and we can kill the 
process in pagefaul handler with exit(-1).
>> B4: Suppose a system call causes a full page (4,096 bytes) of data
>> to be copied from user space into the kernel.  What is the least
>> and the greatest possible number of inspections of the page table
>> (e.g. calls to pagedir_get_page()) that might result?  What about
>> for a system call that only copies 2 bytes of data?  Is there room
>> for improvement in these numbers, and how much?
Full page copy: at least 1, at most 2
2 byte copy: at least 0, at most 1
>> B5: Briefly describe your implementation of the "wait" system call
>> and how it interacts with process termination.
Each process has two lock: internal_lock and parrent_lock. When parrent 
create a child, parrent will accquire child's parrent_lock and child will
accquire its internal lock. When child process exits, it will release its
internal_lock and try to accquire parrent_lock before destroy its thread
entry.
Parrent wait for child by trying to accquire childs internal_lock. After 
getting childs internal lock, parrent can read child status and then release 
childs parrent_lock, allow childs to be destroyed.
If parrent is terminated, it will travel its child list and release all of 
childs parrent_lock, so its child can exit successfuly.
If child terminates before parrent calls wait, it will be block by trying 
accquire parent_lock and cannot be destroyed. When parrent calls wait then
child will accquire parrent_lock and exit normally.
>> B6: Any access to user program memory at a user-specified address
>> can fail due to a bad pointer value.  Such accesses must cause the
>> process to be terminated.  System calls are fraught with such
>> accesses, e.g. a "write" system call requires reading the system
>> call number from the user stack, then each of the call's three
>> arguments, then an arbitrary amount of user memory, and any of
>> these can fail at any point.  This poses a design and
>> error-handling problem: how do you best avoid obscuring the primary
>> function of code in a morass of error-handling?  Furthermore, when
>> an error is detected, how do you ensure that all temporarily
>> allocated resources (locks, buffers, etc.) are freed?  In a few
>> paragraphs, describe the strategy or strategies you adopted for
>> managing these issues.  Give an example.
The error handling is called only when an error is raised. That mean we don't
completely check the valid of user pointer, but if this pointer is invalid then
it will raise the error and we will kill this process to avoid kernel panic.

All resource is freed in the function thread_exit that every thread calls on 
exit, whatever the reason for exiting (being killed, finish its execution, detected
invalid pointer). 
The resource includes:
- Buffers
The page directory of the process is destroyed, free all the physical frames
- Opened file:
B travel its fd array and close any FD with file is not NULL
- Child lock:
B travel its child list and release all child parrent_locks

---- SYNCHRONIZATION ----

>> B7: The "exec" system call returns -1 if loading the new executable
>> fails, so it cannot return before the new executable has completed
>> loading.  How does your code ensure this?  How is the load
>> success/failure status passed back to the thread that calls "exec"?
The arguments that parrent pass to thread_create() in process_execute.c
includes a lock, a condition vatiable and a status. After calling thread_create,
parrent will wait for signal from child. When child loads the execcutable
file, the result is pass to status, then child signals to wake up its parrent.

>> B8: Consider parent process P with child process C.  How do you
>> ensure proper synchronization and avoid race conditions when P
>> calls wait(C) before C exits?  After C exits?  How do you ensure
>> that all resources are freed in each case?  How about when P
>> terminates without waiting, before C exits?  After C exits?  Are
>> there any special cases?
When B is parrent of C, then B holds C's parrent_lock and C hold its
internal_lock.
- If B calls wait before C exits:
B will try to accquire C's internal_lockand be blocked until C calls 
thread_exit() and release its internal_lock. B can read C's status and 
then release C's parrent_lock, allow C to be destroyed
- If B calls wait after C exits:
B will accquire C's internal_lock imeidiately, read C status, and then 
release C's parrent_lock, allow C to be destroyed
- If B terminates without waiting:
B will travel its child and release all parrent_locks, so all of its child 
can exit normally.

All resource is freed in the function thread_exit that every thread calls on exit.
The resource includes:
- Buffers
The page directory of the process is destroyed, free all the physical frames
- Opened file:
B travel its fd array and close any FD with file is not NULL
- Child lock:
B travel its child list and release all child parrent_locks

---- RATIONALE ----

>> B9: Why did you choose to implement access to user memory from the
>> kernel in the way that you did?
It's more reasonable than trying to validate the address passed to 
kernel, and it can take the advantage of MMU.
>> B10: What advantages or disadvantages can you see to your design
>> for file descriptors?
Advantage: it's simple, easy to handle opened file
Disadvantage: 
- The number of opened file is limited
- It may take too much space in kernel stack
- It's hard to handle file charatersitic: r/w/x

>> B11: The default tid_t to pid_t mapping is the identity mapping.
>> If you changed it, what advantages are there to your approach?
I didn't change it, but I think it can handle multi-thread processes

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
