#include "userprog/exception.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "round.h"
#include <inttypes.h>
#include <stdio.h>
#include "string.h"
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "process.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
   /* Whatever the reason for being killed is, wait return -1 */
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      // printf ("%s: dying due to interrupt %#04x (%s).\n",
      //         thread_name (), f->vec_no, intr_name (f->vec_no));
      // intr_dump_frame (f);
      exit(-1);
      // asm();

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      // intr_dump_frame (f);
      // PANIC ("Kernel bug - unexpected interrupt in kernel"); 
      ASSERT(is_not_initial(thread_current()));
      exit(-1);

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));
//   DBG_MSG_USERPROG("[%s] call page fault at 0x%x\n", thread_name(), fault_addr);
  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  uint8_t *vpage = (uint8_t *) ROUND_DOWN((uint32_t) fault_addr, PGSIZE);
  if(fault_addr == NULL || vpage >= PHYS_BASE) /* Illegal address */
  {
      kill(f);
  }
  if(write && !not_present) /* Write violation */
  {
     kill(f);
  }
  struct page *p = page_table_lookup(thread_current(), vpage);
  if(p != NULL)
  {
      uint8_t *kpage = frame_alloc(vpage);
      if(kpage == NULL)
      {
         DBG_MSG_VM("[VM: %s] full of memory, kill\n", thread_name());
         kill(f);
      }
      if(p->aux == -1) /* load new page from elf --> do nothing */
      {
         // DBG_MSG_VM("[VM: %s] load 0x%x from elf %d\n", thread_name(), p->vaddr, p->aux);
         install_page(vpage, kpage, 1);
      }
      else /* Swap page or mmap page, detect by the bit 9-11 in page */
      {
         if((uint32_t) p->vaddr & PTE_AVL) // mmap page
         {
            install_page(vpage, kpage, 1);
            DBG_MSG_VM("[VM: %s] load 0x%x from mmap %d\n", thread_name(), p->vaddr, p->aux);
            struct openning_file *f = &thread_current()->ofile[(uint32_t) p->aux - 2];
            uint32_t file_offset = vpage - f->mmap_start;
            // read from file
            frame_table_set_restricted(vtop(kpage), -1);
            off_t read_bytes = file_read_at(f->mfile, vpage, PGSIZE, file_offset);
            frame_table_set_restricted(vtop(kpage), 0);
            // if(read_bytes < PGSIZE)
            // {
            //    memset(vpage + read_bytes, vpage + PGSIZE, 0);
            // }
         }
         else
         {
            DBG_MSG_VM("[VM: %s] load 0x%x from swap %d at pf %d\n", thread_name(), p->vaddr, (uint32_t) p->aux >> 12, page_fault_cnt);
            /* Mark the frame that's being swapped in */
            frame_table_set_restricted(vtop(kpage), -1);
            swap_in((uint32_t) p->aux >> PGBITS, vtop(kpage));
            frame_table_set_restricted(vtop(kpage), 0);
            /* Mark the frame that's it's nomal */
            install_page(vpage, kpage, 1);
         }
      }
      page_table_remove(thread_current(), p);
      // lock_release(&thread_current()->page_mgm->lock);
      pagedir_set_dirty(thread_current()->pagedir, vpage, 0);
      pagedir_set_accessed(thread_current()->pagedir, vpage, 0);
      if(!user) /* Kernel frame, lock it */
      {
         frame_table_set_restricted(vtop(kpage), -1);
      }
      goto done;   
  }
  else if ((fault_addr - f->esp < PGSIZE && f->esp - fault_addr < PGSIZE) ) /* Stack growth */
  {
      DBG_MSG_VM("[VM: %s] stack growth\n", thread_name());
      while (vpage < PHYS_BASE)
      {
         if(pagedir_get_page(thread_current()->pagedir ,vpage) == NULL)
         {
            uint8_t *kpage = frame_alloc(vpage);
            if(kpage == NULL || !install_page(vpage, kpage, 1))
            {
               DBG_MSG_VM("[VM: %s] full of memory, kill\n", thread_name());
               kill(f);
            }
         }
         pagedir_set_dirty(thread_current()->pagedir, vpage, 0);
         pagedir_set_accessed(thread_current()->pagedir, vpage, 0);
         vpage += PGSIZE;
      }
      goto done;
  }
  else
  {
      DBG_MSG_VM("[VM: %s] call page fault at 0x%x at pf %d\n", thread_name(), fault_addr, page_fault_cnt);  
      // intr_dump_frame(f);
      // PANIC("PGF");
      kill(f);
  }

  done:
   return;
}

