/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

struct execution
{
  /* data */
  char fn_copy[1024]; /* Copy of the filename, assume that max = 1024 char */
  struct lock ex_lock; /* Lock for the condvar */
  struct condition ex_cond; /* ˛Condvar to notify the parrent that child load successfuly or not */
  int load_status;
};

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
 struct execution *thread_args;
  
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  thread_args = palloc_get_page (0);
  lock_init(&thread_args->ex_lock);
  cond_init(&thread_args->ex_cond);
  if (thread_args->fn_copy == NULL || file_name == NULL)
    return TID_ERROR;
  strlcpy (thread_args->fn_copy, file_name, 1024);
  /* Create a new thread to execute FILE_NAME. */
  /* But before that, accquire the execution lock */
  DBG_MSG_USERPROG("[%s] accquire child execution lock of %s\n",thread_name(), file_name);
  lock_acquire(&thread_args->ex_lock);
  DBG_MSG_USERPROG("[%s] create child %s\n",thread_name(), file_name);
  tid = thread_create (file_name, PRI_DEFAULT, start_process, thread_args);
  if(tid == TID_ERROR)
  {
    DBG_MSG_USERPROG("[%s] child create error %s\n",thread_name(), file_name);
    lock_release(&thread_args->ex_lock); /* it's ok? */
    palloc_free_page (thread_args); 
    return tid;
  }
  /* Wait for the condition - exe success or not */
  DBG_MSG_USERPROG("[%s] wating for exec signal from child %s\n", thread_name(), file_name);
  cond_wait(&thread_args->ex_cond, &thread_args->ex_lock); 
  lock_release(&thread_args->ex_lock);
  DBG_MSG_USERPROG("[%s] get exec signal from child %s\n", thread_name(), file_name);
  if (thread_args->load_status == false)
    {
      DBG_MSG_USERPROG("[%s] child load executable file [%s] error\n",thread_name(), file_name);
      tid = TID_ERROR;
    }
  palloc_free_page (thread_args); 
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *thread_args_)
{
  /* Otherwise, run, accquire my own lock first */
  DBG_MSG_USERPROG("[%s] trying to get my internal lock \n", thread_name());
  lock_acquire(&thread_current()->internal_lock);
  DBG_MSG_USERPROG("[%s] get my internal lock\n", thread_name());
  page_table_init(thread_current());
  struct execution *thread_args = thread_args_;
  char *file_name = thread_args->fn_copy;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  DBG_MSG_USERPROG("[%s] loading userprog %s\n", thread_name(), file_name);
  success = load (file_name, &if_.eip, &if_.esp);
  /* Now we know that the exec is success or not, so signal the parrent */
  /* If load failed, quit. --> parrent will free args, relax */
  thread_args->load_status = success;
  DBG_MSG_USERPROG("[%s] signaling my parrent %s...\n", thread_name(), thread_current()->parrent->name);
  lock_acquire(&thread_args->ex_lock);
  cond_signal(&thread_args->ex_cond, &thread_args->ex_lock);
  lock_release(&thread_args->ex_lock);
  DBG_MSG_USERPROG("[%s] signaling OK...\n", thread_name());
  if (!success){
    thread_exit ();
  }
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
/* Ping-pong solution for wait:
   - When parrent create  */
int
process_wait (tid_t child_tid) 
{
  DBG_MSG_USERPROG("[%s] Wating for pid %d\n", thread_name(), child_tid);
  /* travel the child list */
  struct list_elem *e = list_begin(&thread_current()->childs);
  struct thread *t = NULL;
  int return_status;
  while(e != NULL) /* looking for child_tid in child list */
  {
    struct thread *t0 = list_entry(e, struct thread, child_elem);
    if(t0->tid == child_tid){
      DBG_MSG_USERPROG("[%s] found pid in childs %s\n", thread_name(), t0->name);
      t = t0;
      break;
    }
    e = e->next;
  }
  if(t == NULL) /* child_pid is invalid */
  {
    DBG_MSG_USERPROG("[%s] No valid pid found\n", thread_name());
    return_status = -1;
  }
  else 
  {
    DBG_MSG_USERPROG("[%s] try get child lock of %s\n", thread_name(), t->name);
    lock_acquire(&t->internal_lock);
    DBG_MSG_USERPROG("[%s] get child lock of %s\n", thread_name(), t->name);
    return_status = t->userprog_status;
    list_remove(&t->child_elem);
    DBG_MSG_USERPROG("[%s] release child parrent lock of %s\n", thread_name(), t->name);
    lock_release(&t->parrent_lock);
  }
  DBG_MSG_USERPROG("[%s] Child process return %d\n", thread_name(), return_status);
  return return_status;
  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  /* Free the swap table */
  swap_free(cur);
  /* Free the frame table */
  frame_table_free(cur);
  /* Free the supplemental table */
  page_table_destroy(thread_current());

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char **argv, int argc);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
static int userprog_parser(char *cmd, char **argv);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */

bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();
  /* TODO: Implement the parser here: seprate the file_name by 'blank space'
     The file_name can contains some opitons, e.g. 'ls -l' we need to break
     down the string into words and save to an array, e.g. 'ls', '-l' */
  char *argv[MAX_ARGV_LEN];
  char *cmd = palloc_get_page(0);
  strlcpy(cmd, file_name, MAX_CMD_LEN);
  DBG_MSG_USERPROG("[%s] parse %s\n", thread_name(), file_name);
  int argc = userprog_parser(cmd, argv);
  /* Open executable file. */
  DBG_MSG_USERPROG("[%s] open userprog %s\n", thread_name(), argv[0]);
  int attempt = 0;
  while(file == NULL && attempt < 5) /* Busy wait, no way now */
  {
  file = filesys_open (argv[0]);
  thread_sleep(10);
  attempt++;
  }
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", argv[0]);
      goto done; 
    }
  /* Deny writing to file */
  file_deny_write(file);
  thread_current()->my_elf = file;
  DBG_MSG_USERPROG("[%s] verify userprog %s\n", thread_name(), argv[0]);
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", argv[0]);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              DBG_MSG_VM("[VM: %s] loading segment %s at user page 0x%x, fsize of 0x%x and msize of 0x%x\n",thread_name(), writable?"data":"code", mem_page, page_offset + phdr.p_filesz, ROUND_UP(phdr.p_memsz, PGSIZE));
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                                 {
                                    DBG_MSG_USERPROG("[%s] segment load failed with %s\n",thread_name(), file_name);
                                    goto done;
                                 }
              // DBG_MSG_USERPROG("[%s] loading segment %s at upage 0x%x\n",thread_name(), writable?"data":"code", mem_page);
            }
          else
            {
              DBG_MSG_USERPROG("[%s] segment validate failed with %s\n",thread_name(), file_name);
              goto done;
            }
          break;
        }
    }

  /* Set up stack. */
  DBG_MSG_USERPROG("[%s] set up user stack\n", thread_name());
  if (!setup_stack (esp, argv, argc))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  palloc_free_page(cmd);
  // file_close (file);
  return success;
}

/* load() helpers. */
/* TODO: implement userprog_parser */
static int 
userprog_parser(char *cmd, char **argv)
{
  int cnt = 0; /* number of argv, at least 1 */
  char *token, *save_ptr;

   for (token = strtok_r (cmd, " ", &save_ptr); token != NULL;
        token = strtok_r (NULL, " ", &save_ptr))
        {
          argv[cnt] = token;
          cnt++;
        }
  argv[cnt] = NULL;
  return cnt;
}

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      if(page_read_bytes > 0) /* Load all needed page */
      {
        /* Get a page of memory. */
        uint8_t *kpage = frame_alloc();
        if (kpage == NULL)
        {
          DBG_MSG_VM("[%s - load_segment] cannot allocate page\n",thread_name());
          return false;
        }
        /* Load this page. */
        if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
          {
            frame_free(kpage);
            DBG_MSG_VM("[VM: %s - load_segment] cannot load segment page\n",thread_name());
            return false; 
          }
        memset (kpage + page_read_bytes, 0, page_zero_bytes);

        /* Add the page to the process's address space. */
        // DBG_MSG_VM("[ VM: %s - load segment] load to upage 0x%x\n",thread_name(), upage);
        if (!install_page (upage, kpage, writable)) 
          {
            frame_free(kpage);
            DBG_MSG_USERPROG("[%s - load_segment] cannot install new page\n",thread_name());
            return false; 
          }
      }
      else /* Marked in the supp table that allocate new page is enough */
      {
        // DBG_MSG_VM("[VM: %s - load_segment] insert segment page 0x%x to page table\n",thread_name(), upage);
        page_table_insert(thread_current(), upage, -1);
      }
            /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
    return true;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();
  frame_table_set(vtop(kpage), t, upage);
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
/* TODO: Correctly setup the stack, following the doccument */
static bool
setup_stack (void **esp, char **argv, int argc) 
{
  uint8_t *kpage;
  bool success = false;

  kpage =  frame_alloc(); /* kpage is a PHYSICAL page */
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
      {
        /* Pushing the argv value to the top of stack */
        uint8_t *virtual_esp = (uint8_t *) (PHYS_BASE); /* Virtual stack poiter */
        uint32_t argv_addr[argc + 1]; /* Address of argv in the stack */
        argv_addr[argc] = 0; /* The last argv should be NULL */
        int i;
        for(i = argc - 1; i >= 0; --i)
        {
          virtual_esp -= strlen(argv[i]) + 1; /* 1 for the NULL terminator */
          argv_addr[i] = (int) virtual_esp;
          strlcpy(virtual_esp, argv[i], MAX_ARGV_LEN); /* strlcpy alway returns the len of src */
        }
        /* Align the stack */
        while(((int) virtual_esp % 4) != 0)
        {
          virtual_esp--;
          *virtual_esp = 0;
        }
        /* Pushing the argv_addr to stack */
        for(i = argc; i >=0; --i)
        {
          virtual_esp -= 4;
          * (uint32_t *) virtual_esp = argv_addr[i];
        }
        /* Pushing argv itself to stack */
        virtual_esp -= 4;
        *(uint32_t *)virtual_esp = (int) (virtual_esp + 4);
        /* Pushing argc to stack */
        virtual_esp -= 4;
        *(uint32_t *)virtual_esp = argc;
        /* Pushing a NULL as fake return address */
        virtual_esp -= 4;
        *(uint32_t *)virtual_esp = 0;
        *esp = virtual_esp;
        // hex_dump((uint32_t)*esp, *esp, (PHYS_BASE - *esp), 1);
        /* Adding next page of stack to the page table*/
      }
      else
        frame_free(kpage);
    }
  return success;
}
