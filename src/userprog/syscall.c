/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#include "userprog/syscall.h"
#include <lib/stdio.h>
#include "lib/limits.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "devices/shutdown.h"
#include "lib/kernel/stdio.h"
#include <string.h>
#include <userprog/process.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "round.h"

static void syscall_handler (struct intr_frame *);
static void halt (void) NO_RETURN;
static pid_t exec (const char *file);
static int wait (pid_t);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);
static mmapid_t mmap(int fd, uint8_t *vaddr);
static bool chdir(const char *dir);
static bool mkdir(const char *dir);
static bool readdir(int id, char *dir);
static bool isdir(int fd);
static int inumber(int fd); 

#define TO_ARG(ESP, X) (*(int *)(ESP + 4*X))

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void exit (int status)
{
  DBG_MSG_FS("[%s] calls exit\n", thread_name());
  char thread_full_name[128];
  strlcpy(thread_full_name, thread_name(), 128);
  char *process_name, *save_prt;
  process_name = strtok_r(thread_full_name, " ", &save_prt);
  printf("%s: exit(%d)\n", process_name, status);
  thread_current()->userprog_status = status;
  thread_exit();
}


static void
syscall_handler (struct intr_frame *f) 
{
  // hex_dump((uint32_t)f->esp, f->esp, (PHYS_BASE - f->esp), 1);
  // intr_dump_frame (f);
  // debug_backtrace();
  char *esp = f->esp;
  uint32_t syscall = TO_ARG(esp,0), arg0 = TO_ARG(esp, 1), arg1 = TO_ARG(esp, 2), arg2 = TO_ARG(esp, 3);
  /* Check memory access */
  if(esp >= PHYS_BASE - 4)
  {
    // intr_dump_frame (f);
    exit(-1);
  }
  int ret_val;
  switch (syscall)
  {
      /* code */
    case SYS_HALT:                   /* Halt the operating system. */       
      halt();
      break;
    case SYS_EXIT:                  /* Terminate this process. */
      exit(arg0);
      break;
    case SYS_EXEC:                 /* Start another process. */
      ret_val = exec(arg0);
      break;
    case SYS_WAIT:                 /* Wait for a child process to die. */
      ret_val = wait(arg0);
      break;
    case SYS_CREATE:                /* Create a file. */
      ret_val = create(arg0, arg1);
      break;
    case SYS_REMOVE:                 /* Delete a file. */
      ret_val = remove(arg0);
      break;
    case SYS_OPEN:                   /* Open a file. */
      ret_val = open(arg0);
      break;
    case SYS_FILESIZE:               /* Obtain a file's size. */
      ret_val = filesize(arg0);
      break;
    case SYS_READ:                   /* Read from a file. */
      ret_val = read(arg0, arg1, arg2);
      break;
    case SYS_WRITE:                  /* Write to a file. */
      ret_val = write(arg0, arg1, arg2);
      break;
    case SYS_SEEK:                   /* Change position in a file. */         
      seek(arg0, arg1);
      break;
    case SYS_TELL:                   /* Report current position in a file. */
      ret_val = tell(arg0);
      break;
    case SYS_CLOSE:                  /* Close a file. */
      close(arg0);
      break;
    case SYS_MMAP:
      ret_val = mmap(arg0, arg1);    /* Map a file to a specific mem address */
      break;
    case SYS_MUNMAP:                 /* Unmap a mapped file */
      munmap(arg0);
      break;
    case SYS_CHDIR:                  /* Change curent directory of the process to dir  */
      ret_val = chdir(arg0);
      break;
    case SYS_MKDIR:                  /* Make a new directory named dir */ 
      ret_val = mkdir(arg0);
      break;
    case SYS_READDIR:                /* Read an directory entries */
      ret_val = readdir(arg0, arg1);
      break;
    case SYS_ISDIR:
      ret_val = isdir(arg0);         /* Return true if fd is directory, false otherwise*/
      break;
    case SYS_INUMBER:
      ret_val = inumber(arg0);       /* Return the inode number of fd */
      break;
    default:
      break;
  }
  f->eax = ret_val;
}

static void halt (void)
{
  DBG_MSG_USERPROG("[%s] calls halt\n", thread_name());
  shutdown_power_off();
}

static pid_t exec (const char *file)
{
    DBG_MSG_USERPROG("[%s] calls exec %s \n", thread_name(), file);
    if(file == NULL || !file)
    {
      exit(-1);
    }
    pid_t pid = process_execute(file);
    return pid;

}

static int wait (pid_t p)
{
  DBG_MSG_USERPROG("[%s] calls wait to %d\n", thread_name(), p);
  return process_wait(p);
}

static bool create (const char *file, unsigned initial_size)
{
  DBG_MSG_USERPROG("[%s] calls create %s\n", thread_name(), file);
  if(file == NULL || !*file)
  {
    exit(-1);
  }
  if(strlen(file) > UCHAR_MAX)
  {
    return 0;
  }
  /* Otherwise, just create */
  return filesys_create(file, initial_size);
}

static bool remove (const char *file)
{
  DBG_MSG_USERPROG("[%s] calls remove %s\n", thread_name(), file);
  if(file == NULL)
  {
    exit(-1);
  }
  return filesys_remove(file);
}

static int open (const char *file)
{
  DBG_MSG_USERPROG("[%s] calls open %s\n", thread_name(), file);
  if(file == NULL || !*file)
  {
    return -1;
  }
  struct file *tmp;
  uint32_t attemp = 0;
  while(attemp < 5)
  {
    tmp = filesys_open(file);   /* Open file */
    if(tmp != NULL) break;
    else 
    {
      thread_sleep(2);
      attemp++;
    }
  }
  if(tmp == NULL) return -1;  /* If failed, return -1 */
  DBG_MSG_FS("[%s] filesys_open %s success\n", thread_name(), file);
  /* If success find the first index that ofile[index] = NULL
     and return index + 2 */
  int i = 0;
  while(i < NOFILE && thread_current()->ofile[i].file != NULL)
  {
    i++;
  }
  if(i == NOFILE) return -1; /* excess number of opened file */
  thread_current()->ofile[i].file = tmp;
  DBG_MSG_USERPROG("[%s] open %s return %d\n", thread_name(), file, i + 2);
  return i + 2; 
}

static int filesize (int fd)
{
  if(fd < 0)
  {
    exit(-1);
  }
  switch (fd)
  {
    case STDIN_FILENO: /* stdin */
      exit(-1);
      break;
    case STDOUT_FILENO: /* stdout */
      exit(-1);
    default:
      return(file_length(thread_current()->ofile[fd - 2].file));
      break;
  }
}

static int read (int fd, void *buffer, unsigned length)
{
  if(buffer == NULL || fd < 0 || buffer >= PHYS_BASE)
  {
    exit(-1);
  }
  switch (fd)
  {
    case STDIN_FILENO: /* stdin */
      return strlen(buffer);
      break;
    case STDOUT_FILENO: /* stdout */
      exit(-1);
    default:
      DBG_MSG_USERPROG("[%s] calls read %d bytes from %d\n", thread_name(), length, fd);
      ASSERT(thread_current()->ofile[fd - 2].file != NULL);
      return file_read(thread_current()->ofile[fd - 2].file, buffer, length);
      break;
  }
}

static int write (int fd, const void *buffer, unsigned length)
{
  if(buffer == NULL || fd < 0 || buffer >= PHYS_BASE)
  {
    exit(-1);
  }
  switch (fd)
  {
    case STDIN_FILENO: /* stdin */
      exit(-1);
      break;
    case STDOUT_FILENO: /* stdout */
      putbuf(buffer, length);
      break;
    default:
      DBG_MSG_USERPROG("[%s] calls write %d bytes to %d\n", thread_name(), length, fd);
      return file_write(thread_current()->ofile[fd - 2].file, buffer, length);
      break;
  }
  return 0;   
}

static void seek (int fd, unsigned position)
{
  if(fd < 0)
  {
    exit(-1);
  }
  switch (fd)
  {
    case STDIN_FILENO: /* stdin */
      exit(-1);
      break;
    case STDOUT_FILENO: /* stdout */
      exit(-1);
    default:
      file_seek(thread_current()->ofile[fd - 2].file, position);
      break;
  }
}

static unsigned tell (int fd)
{
  if(fd < 0)
  {
    exit(-1);
  }
  switch (fd)
  {
    case STDIN_FILENO: /* stdin */
      exit(-1);
      break;
    case STDOUT_FILENO: /* stdout */
      exit(-1);
    default:
      return file_tell(thread_current()->ofile[fd - 2].file);
      break;
  }
}
static void close (int fd)
{
  if(fd < 0)
  {
    exit(-1);
  }
  switch (fd)
  {
    case STDIN_FILENO: /* stdin */
      exit(-1);
      break;
    case STDOUT_FILENO: /* stdout */
      exit(-1);
    default:
      DBG_MSG_USERPROG("[%s] calls close to %d\n", thread_name(), fd);
      if(thread_current()->ofile[fd - 2].file == NULL) /* Didn't open */
        return -1;
      file_close(thread_current()->ofile[fd - 2].file);
      thread_current()->ofile[fd - 2].file = NULL;
      break;
  }
}
static bool is_valid_mmap_vaddr(void *vaddr);
/*
map an opened file fd to the address vaddr
--> adding page at vaddr to the supplement page table
*/
static mmapid_t mmap(int fd, uint8_t *vaddr)
{
  /* Valid address */
  if(!is_valid_mmap_vaddr(vaddr))
    return -1;
  /* Valid file */
  if(fd <= STDOUT_FILENO || fd >= NOFILE || filesize(fd) == 0)
    return -1;
  /* Goto the file array */
  DBG_MSG_FS("[VM: %s] mapping file..\n", thread_name());
  struct openning_file *f = &thread_current()->ofile[fd - 2];
  if(f->file == NULL) return -1; /* No file */
  if(f->mmap_start != NULL) return -1; /* no remap now */
  f->mfile = file_reopen(f->file);
  f->mmap_start = vaddr;
  f->mmap_end = vaddr + ROUND_UP(filesize(fd), PGSIZE);
  /* Add the page to the supplement table; */
  DBG_MSG_FS("[VM: %s] adding mmap page from 0x%x to 0x%x to supp table..\n", thread_name(), f->mmap_start, f->mmap_end);
  uint8_t *a;
  for(a = f->mmap_start; a < f->mmap_end ; a += PGSIZE)
  {
    a = (uint32_t) a | PTE_AVL;
    if(page_table_insert(thread_current(), a, fd) != NULL) /* Overlap mapping */
    {
      /* Remove all page from supp table */
      struct page *p;
      while(a > f->mmap_start)
      {
        p = page_table_lookup(thread_current(), a);
        if(p) page_table_remove(thread_current(), p);
        a -= PGSIZE;
      }
      return -1;
    }
  }
  return fd;
}

void munmap(mmapid_t mapping)
{
  struct openning_file *f = &thread_current()->ofile[mapping - 2];
  ASSERT(f->mfile != NULL && f->mmap_start != NULL && f->mmap_end != NULL); // valid mmap
  uint8_t *a, *k;
  off_t file_offset = 0;
  for(a = f->mmap_start; a < f->mmap_end ; a += PGSIZE)
  {
    k = pagedir_get_page(thread_current()->pagedir, a);
    uint32_t written = PGSIZE;
    if(k == NULL)  /* not yet accessed */
    {
      struct page *p = page_table_lookup(thread_current(), a);
      ASSERT(p != NULL);
      page_table_remove(thread_current(), p);
    }
    else if(k != NULL && pagedir_is_dirty(thread_current()->pagedir, a)) // is dirty
    {
      if(file_offset + PGSIZE >= file_length(f->mfile)) /* We don't allow growing file via mmap */
      {
          written = file_length(f->mfile) - file_offset;
      }
      file_write_at(f->mfile, a, written, file_offset);
      DBG_MSG_FS("[VM:%s]write %d bytes to mmap file at 0x%x\n", thread_name(), written, a);
      pagedir_clear_page(thread_current()->pagedir, a);
      frame_free(k);
    }
    file_offset += written;
  }
  file_close(f->mfile);
  f->mfile = NULL;
  f->mmap_start = NULL;
  f->mmap_end = NULL;
}

static bool is_valid_mmap_vaddr(void *vaddr)
{
  /* Not NULL */
  if(vaddr == NULL)
    return false;
  /* Not a kernel address space */
  if(is_kernel_vaddr(vaddr)) 
    return false;
  /* Must be align */
  if((uint32_t) vaddr % PGSIZE != 0) 
    return false;
  /* Must not be mapped */
  if(pagedir_get_page(thread_current()->pagedir, vaddr) != NULL) 
    return false;
  /* Must not in the supp table */
  if(page_table_lookup(thread_current(), vaddr) != NULL)
    return false;
  /* Pass all */
  return true;
}

static bool chdir(const char *dir)
{

}

static bool mkdir(const char *dir)
{
  /*
  Implementation:
  If the string start with / --> absolute path --> chdir to root
  Else --> relative path --> keep process dir
  Tokenize the input string with '/'
  If the first word is '..' --> chdir to parrent dir --> how to know the parrent dir

  */
 if(dir == NULL || !*dir) // NULL pointer or empty string 
 {
   return false;
 }
 if(dir[0] == '/') /* Absolute path, need to go to root directory */
 {

 }
 /* Tokenize the string */
 
}

static bool readdir(int id, char *dir)
{
  
}

static bool isdir(int fd)
{

}

static int inumber(int fd)
{

}