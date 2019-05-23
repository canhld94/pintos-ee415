       	       	    +---------------------------+
       	       	    |	   EE 415 / PD 511		|
		    		| PROJECT 3: VIRTUAL MEMORY	|
		    		|	   DESIGN DOCUMENT		|
		    		+---------------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Duc Canh Le <canhld@kaist.ac.kr>

>> Fill in your GitLab repository address.
https://github.com/canhld94/pintos-ee415

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.
Here we use page table mean Pintos original page table.
>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.

			PAGE TABLE MANAGEMENT
			=====================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
Page in supplemental page table:
	struct page
	{
		struct hash_elem hash_elem;
		uint8_t *vaddr; 				<!-- Virtual address -->
		uint8_t *aux; 					<!-- Aux data, depend on segment of vaddr -->
	};

Supplemental table in the thread structure:
	struct page_mgm						<!-- Page management system -->
	{
	struct lock lock;					<!-- Lock -->
	struct hash *page_table;			<!-- Supplemental table -->
	};

Adding in the thread structure:
	struct page_mgm *page_mgm;		

---- ALGORITHMS ----

>> A2: In a few paragraphs, describe your code for locating the frame,
>> if any, that contains the data of a given page.
If the page is in the page table, we simply use pagedir_get_page to 
get the corresponding kernel virtual address and then translate to 
physical address to get the frame.
If the page is not in the page table (that cause page fault), we will 
look for that page in the supplemental table. If it's in the supplemental 
table, then we need to allocate new frame and load the data (that can be 
from swap or file system depend on the data of the entry in supplemental 
table) to this frame. 
The frame allocation may lead to frame eviction if the memory is full.

>> A3: How does your code coordinate accessed and dirty bits between
>> kernel and user virtual addresses that alias a single frame, or
>> alternatively how do you avoid the issue?
The access bit an dirty bit is only meaningful when the page is in the 
page table. Therefore, for a page in user virtual address, we just need 
to check in the page table. For the page in the kernel virtual address, 
we first need to translate it to physical address and then locate which 
thread and which page it was associated with in the frame table, then 
we can check the dirty or accessed bit in the page table of the thread.

---- SYNCHRONIZATION ----

>> A4: When two user processes both need a new frame at the same time,
>> how are races avoided?
We use different lock for each component of the VM subsytem: frame table, 
swap table. If two process both need new frame but there're room for them 
in the free page pool, then we don't need to worry about race condition. If 
there are no page for both of them, we need to evict some frame. The eviction
process require accquiring all the lock of VMs sub system, therefore there is 
only one process can do the eviction at a time.

---- RATIONALE ----

>> A5: Why did you choose the data structure(s) that you did for
>> representing virtual-to-physical mappings?
All page table should support fast lookup, however we choose different implementation 
for each table:
For the supplemental page table, we use hash table because the supplemental table is
per thread, so we dont want to store information of all page in the table because it 
will waiste a lot of memory. Hash table allow fast looking up and reasonable page size 
(only needed pages are added to the table and will be removed when it's reloaded)
For the frame table and swap table, we simply use fixed array and use the frame (or swap
slot) number itself as the index for looking up in the array. This allow very fast looking
up. Even we need to store the informaton of all frame (or swap slot), it's acceptable 
because there is only one frame table and one swap table in the system.

		       PAGING TO AND FROM DISK
		       =======================

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
Frame table and frame:
	struct _frame_table 			<!-- Frame table -->
	{
		struct thread *thread;		<!-- Thread that hold the frame -->
		uint8_t *page;				<!-- Corresponding page -->
		void *aux;					<!-- Auxilary data -->
	};

	struct _frame							<!-- Frame object -->
	{
		struct lock lock;					<!-- Lock -->
		struct _frame_table *frame_table;	<!-- Frame table -->
		uint32_t total_frames;				<!-- Total frame in the system -->
		uint32_t user_frames;				<!-- Total availble frame for user -->
	};

Swap:
	struct _swap
	{
		struct block *block_sw;		<!-- Disk block used for swap -->
		void **sw_table;			<!-- The swap table -->
		struct lock lock;			<!-- Lock -->
	};

Addding variable:
	struct _frame frame;
	struct _swap swap;

---- ALGORITHMS ----

>> B2: When a frame is required but none is free, some frame must be
>> evicted.  Describe your code for choosing a frame to evict.
Because of a bad design at the starting, we just randomly select a 
frame for eviction. Our swap table structure have no information about 
the previous thread and page that occupy the swap slot, and the supplemental 
table also remove the page after swapping in, therefore even we know that the 
page is not dirty, we don't know where is it in the swap region. I.e we always 
need to re-write a page during the eviction process, so the dirty bit is 
not make sense. 
For the accessed bit, we obseve that all of the page access bit will be 1 very soon 
after the program started, so we cannot avoid evicting an accessed frame.

>> B3: When a process P obtains a frame that was previously used by a
>> process Q, how do you adjust the page table (and any other data
>> structures) to reflect the frame Q no longer has?
Clear the entry of frame in the page table and add the corresponding page
to the supplemental table of Q. The swap table also had been updated before
the frame was swapped out.

>> B4: Explain your heuristic for deciding whether a page fault for an
>> invalid virtual address should cause the stack to be extended into
>> the page that faulted.
If the fault address is beyond the stack pointer, then it's stack growth.
Or if the fault address is below the stack pointer but close enough (i.e 
within a page) to the stack pointer, we also consider it as stack growth.

---- SYNCHRONIZATION ----

>> B5: Explain the basics of your VM synchronization design.  In
>> particular, explain how it prevents deadlock.  (Refer to the
>> textbook for an explanation of the necessary conditions for
>> deadlock.)


>> B6: A page fault in process P can cause another process Q's frame
>> to be evicted.  How do you ensure that Q cannot access or modify
>> the page during the eviction process?  How do you avoid a race
>> between P evicting Q's frame and Q faulting the page back in?
If a frame of Q is selected to be evicted, its corresponding page in 
the page table will be clear (with pagedir_cler_page) and added to the 
supplemental table before the frame is actually swapped out.
Here we use seprate locks for frame table and swap table. When P do
the eviction, P mus accquire both of these locks. If Q wants to faulting 
the page back it (swap), it must accquire the swap locks and thus, cannot
be done if the eviction process has not yet finish.

>> B7: Suppose a page fault in process P causes a page to be read from
>> the file system or swap.  How do you ensure that a second process Q
>> cannot interfere by e.g. attempting to evict the frame while it is
>> still being read in?
The entries in the frame table including an restriction flag that will be 
set during its data is reading from swap of mmap file. The frame with 
restriction flag enabled will not be considered as candidate for evicting

>> B8: Explain how you handle access to paged-out pages that occur
>> during system calls.  Do you use page faults to bring in pages (as
>> in user programs), or do you have a mechanism for "locking" frames
>> into physical memory, or do you use some other design?  How do you
>> gracefully handle attempted accesses to invalid virtual addresses?
We just use the pagefault for bringing back the page. In addition, if 
we detect that the pagefault cause by the kernel then this page will 
be access soon (usually we read and write the whole page), therefore we
also mark this page as restricted (i.e cannot be evicted).
If the fault address is NULL or beyond the phys_base, or we detected the
process is trying to write to RO page, we simply kill the process. If the 
page of fault address cannot be found in the supplemental page table and the 
fault address itslelf doesn't denote stack growth, we also kill the process.

---- RATIONALE ----

>> B9: A single lock for the whole VM system would make
>> synchronization easy, but limit parallelism.  On the other hand,
>> using many locks complicates synchronization and raises the
>> possibility for deadlock but allows for high parallelism.  Explain
>> where your design falls along this continuum and why you chose to
>> design it this way.
We chose using many lock for the efficient: for example, a thread P is swapping
page in must accquire the swap lock. On the other hand, a thread Q that load new
page from executable file (or mmap file or any other case that don't access the
swap table) just need to accquire the frame lock. If we have just one global lock 
then these two process cannot run concurrently.

			 MEMORY MAPPED FILES
			 ===================

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
New struct:
	<!-- An opened file in the process --> 
	struct openning_file
	{
	struct file *file;		<!-- Pointer to file -->
	struct file *mfile;		<!-- Pointer to the file when mapping is created -->
	uint8_t *mmap_start;	<!-- Start address of mapping -->
	uint8_t *mmap_end;		<!-- End address of mapping -->
};

Added in the thread struct:
    struct openning_file *ofile; 	<!-- The file handler (and mmap handler as well) array -->

---- ALGORITHMS ----

>> C2: Describe how memory mapped files integrate into your virtual
>> memory subsystem.  Explain how the page fault and eviction
>> processes differ between swap pages and other pages.
We create a mapping by adding all mapped pages to the supplemental table
and load it when pagefault.
Both swap page and mapping page is in the supplemental page table and it 
will be load if page fault happens. The entry of supplemental page table
include a pointer to page and a pointer of auxilary data. The auxilary data
varies depend on the type of page, if it's mmap page, the auxilary data is 
the mapping id and if it's swap page, the auxilary data is the swap slot. 
We use bit 9-11 of the auxilary data for marking type of page (if the system
grows, we may need a dedicated member for flag but for demonstration purpose 
then using these two bits is enough, the limit is 4G of swap and 1024 mappings).


>> C3: Explain how you determine whether a new file mapping overlaps
>> any existing segment.
We create a mapping by adding all mapped page to the supplemental table
and load it when pagefault. Therefore if a new file mapping has a page that 
has already been in the supplemental table, this is an overlapped mapping.

---- RATIONALE ----

>> C4: Mappings created with "mmap" have similar semantics to those of
>> data demand-paged from executables, except that "mmap" mappings are
>> written back to their original files, not to swap.  This implies
>> that much of their implementation can be shared.  Explain why your
>> implementation either does or does not share much of the code for
>> the two situations.
We actually shared the mechanism: both pages that need to be loaded from 
executable and pages that were mapped are added to the supplemental table, 
and the page fault handler will check some bits to know whether a page 
should be loaded from executable file or from mmap file.
However, there are some different between them:
1.For executablt file, we pre-loaded all code page and initilized data page,
therefore when pagefault happens, we don't actually "load" a new page from 
executable file but just allocate a new frame (with the flag PAL_ZEROS). On 
the other hand we did load a new page from mmap file.
2.For mmap file, we need to write back the dirty page to the mmap file

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
