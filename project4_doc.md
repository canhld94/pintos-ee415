       	       	     +-------------------------+
       	       	     |	   EE 415 / PD 511     |
		     | PROJECT 4: FILE SYSTEMS |
		     |	   DESIGN DOCUMENT     |
		     +-------------------------+

---- GROUP ----

>> Fill in the names and email addresses of your group members.

Duc Canh Le <canhld@kaist.ac.kr>

>> Fill in your GitLab repository address.

https://github.com/canhld94/pintos-ee415

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.

		     INDEXED AND EXTENSIBLE FILES
		     ============================

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
New inode disk struct with indexed block
struct inode_disk
{
  block_sector_t dblock[DIRECT_BLOCK];                /* Direct data sector. */
  block_sector_t iblock;                              /* Inditect block */
  block_sector_t diblock;                             /* Duobly indirect block */
  off_t length;                                       /* File size in bytes. */
  uint32_t flags;                                     /* Flags */
  unsigned magic;                                     /* Magic number. */
  uint32_t unused[111];                               /* Not used. */
};

Adding read-write lock to inode
struct inode 
{
  struct list_elem elem;              /* Element in inode list. */
  block_sector_t sector;              /* Sector number of disk location. */
  int open_cnt;                       /* Number of openers. */
  bool removed;                       /* True if deleted, false otherwise. */
  int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  struct _rw_lock rw;
  struct inode_disk data;             /* Inode content. */
};

Read write lock
struct _rw_lock
{
  struct lock lock;           /* General lock */
  struct lock write_lock;     /* Write lock */
  uint32_t reader;            /* number of reader in lock */
};


>> A2: What is the maximum size of a file supported by your inode
>> structure?  Show your work.
512x12 + 512/4x512 + 512/4x512/4x512 = 8262 KB

---- SYNCHRONIZATION ----

>> A3: Explain how your code avoids a race if two processes attempt to
>> extend a file at the same time.
The inode is protected by read/write lock. The read lock allow multiple 
processes read that inode at the same time but the write lock is exclusive 
lock, therefore only one process with write lock can modify the inode at 
a time
>> A4: Suppose processes A and B both have file F open, both
>> positioned at end-of-file.  If A reads and B writes F at the same
>> time, A may read all, part, or none of what B writes.  However, A
>> may not read data other than what B writes, e.g. if B writes
>> nonzero data, A is not allowed to see all zeros.  Explain how your
>> code avoids this race.
In the read/write lock, the first reader must accquire write lock. Therefore, 
if A want to read an inode, A must prevent any write to inode, or must wait if 
there is another process writing to the inode. I.e. either A read none of B write 
or A read all of B write, so A cannot read data other than B writes.
>> A5: Explain how your synchronization design provides "fairness".
>> File access is "fair" if readers cannot indefinitely block writers
>> or vice versa.  That is, many processes reading from a file cannot
>> prevent forever another process from writing the file, and many
>> processes writing to a file cannot prevent another process forever
>> from reading the file.

---- RATIONALE ----

>> A6: Is your inode structure a multilevel index?  If so, why did you
>> choose this particular combination of direct, indirect, and doubly
>> indirect blocks?  If not, why did you choose an alternative inode
>> structure, and what advantages and disadvantages does your
>> structure have, compared to a multilevel index?
Yes. We use 12 direct blocks, 1 indirect block and 1 doubly indirect block. We 
use this combination in the manner that most of file will falls in direct 
blocks and indirect block, that reduce the disk access to look up the block. 
The doubly indirect block is neccessary to support large file (up to 8262KB,
e.g. larger than file system capacity)
			    SUBDIRECTORIES
			    ==============

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
Directory structure
struct dir 
{
    struct inode *inode;                /* Backing store. */
    struct dir *parent;                 /* parrent of this directory */
    off_t pos;                          /* Current position. */
};

---- ALGORITHMS ----

>> B2: Describe your code for traversing a user-specified path.  How
>> do traversals of absolute and relative paths differ?
Firstly we check the path to see it's absolute path or relative path. Absolute path 
start with '/' and we need to open the working directory at root directory. For 
relative path, we open the working directory at process current directory. Then we 
tokenize the path with '/' and then 
---- SYNCHRONIZATION ----

>> B4: How do you prevent races on directory entries?  For example,
>> only one of two simultaneous attempts to remove a single file
>> should succeed, as should only one of two simultaneous attempts to
>> create a file with the same name, and so on.

>> B5: Does your implementation allow a directory to be removed if it
>> is open by a process or if it is in use as a process's current
>> working directory?  If so, what happens to that process's future
>> file system operations?  If not, how do you prevent it?

---- RATIONALE ----

>> B6: Explain why you chose to represent the current directory of a
>> process the way you did.

			     BUFFER CACHE
			     ============

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.
RW lock, use in page cache and inode
struct _rw_lock
{
  struct lock lock;           <!-- General lock -->
  struct lock write_lock;     <!-- Write lock -->
  uint32_t reader;            <!-- number of reader in lock -->
};

A block sector in disk cache 
struct _block_sector 
{
    struct list_elem elem;  <!-- List element for block sector in list -->
    block_sector_t sector;  <!-- Corresponding sector on disk -->
    uint32_t ref_count;     <!-- Number of process are refering to this block -->
    uint32_t flags;         <!-- Flags -->
    uint8_t *data;          <!-- Data of the sector -->
    struct lock lock;       <!-- Accessed lock -->
    struct _rw_lock rw;     <!-- r/w lock -->
};

The buffer cache object
struct _disk_cache
{
    struct list sector_list;      <!-- The list for cache -->
    struct lock lock;       <!-- Global lock for the cache -->
    uint32_t size;          <!-- Size of the cache in sector -->
};

---- ALGORITHMS ----

>> C2: Describe how your cache replacement algorithm chooses a cache
>> block to evict.
We use second chance algorthim: The clock hand start at 0, if we need to 
evict a page, we first check the page at the clock hand. If the reference bit 
is 0, we evict this page. Otherwise we set the page to 1 and advance the clock hand. 
The process is repeated untill we found a non-refered page. 
>> C3: Describe your implementation of write-behind.
Create a specific thread for write behind and schedule it every, say, 
100 ticks. This thread will scan the buffer and write all dirty blocks
to filesystem.
>> C4: Describe your implementation of read-ahead.
Similarly, there is a specific thread for read ahead. When a process finishes
loading a block, it will schedule this read ahead thread for loading near by
blocks to the page cache
---- SYNCHRONIZATION ----

>> C5: When one process is actively reading or writing data in a
>> buffer cache block, how are other processes prevented from evicting
>> that block?
Each page has its own reference counter. Each read/write to this page first 
increase the ref. count and decrease it when read/write is done. While running the 
clock algorithm, if the target page has reference counter > 0, we don't evict this page but search for other block. 

>> C6: During the eviction of a block from the cache, how are other
>> processes prevented from attempting to access the block?
Assume that we need to evict the block N to load new block M. Then in the worst case
we need to write the contents of block N to disk and load the block M. We use an
exclusive lock in each page to protect its metadata (which is different from read/
write lock to protect the actual data). During evicting the page (say, caching
block N), we must acquire the exclusive lock of the page. Therefore, other process
cannot access the metadata of this page and therefore, it cannot found that block N 
is caching in the page and therefore, we can safety write the content of N to 
disk. 
---- RATIONALE ----

>> C7: Describe a file workload likely to benefit from buffer caching,
>> and workloads likely to benefit from read-ahead and write-behind.
The buffer cache may be useful when:
1. Working with a lot of files in the same directory: Then we must access the 
directory's data block many times for searching its contents.
2. Working on update a large file: Then we must access the inode of this file very 
frequently to update the metadata and searching for the data block.

The read-ahead is most benefit when we access many consecutive block, e.g. updating 
a very large file sequentially.
The write behind is most benefit when we make a lot of small update on a block, i.e.
update the inode metadata, then many update is batched to single update.

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
>> students in future quarters?

>> Any other comments?
