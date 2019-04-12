#include "userprog/syscall.h"
#include <lib/stdio.h>
#include "lib/limits.h"
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "lib/kernel/stdio.h"
#include <string.h>
#include <userprog/process.h>

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

#define TO_ARG(ESP, X) (*(int *)(ESP + 4*X))

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  char *esp = f->esp;
  int syscall = TO_ARG(esp,0), arg0 = TO_ARG(esp, 1), arg1 = TO_ARG(esp, 2), arg2 = TO_ARG(esp, 3);
  /* Check memory access */
  if(esp >= PHYS_BASE || arg0 >= PHYS_BASE || arg1 >= PHYS_BASE || arg2 >= PHYS_BASE)
  {
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
    case SYS_WRITE:                  /* Write to a file. */
      ret_val = write (arg0, arg1, arg2);
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
    default:
      break;
  }
  f->eax = ret_val;
}

static void halt (void)
{
  DBG_MSG("[%s] calls halt\n", thread_name());
  shutdown_power_off();
}
void exit (int status)
{
  DBG_MSG("[%s] calls exit\n", thread_name());
  char thread_full_name[128];
  strlcpy(thread_full_name, thread_name(), 128);
  char *process_name, *save_prt;
  process_name = strtok_r(thread_full_name, " ", &save_prt);
  printf("%s: exit(%d)\n", process_name, status);
  thread_current()->userprog_status = status;
  thread_exit();
}
static pid_t exec (const char *file)
{
    DBG_MSG("[%s] calls exec %s \n", thread_name(), file);
    if(file == NULL)
    {
      exit(-1);
    }
    pid_t pid = process_execute(file);
    return pid;

}
static int wait (pid_t p)
{
  DBG_MSG("[%s] calls wait to %d\n", thread_name(), p);
  return process_wait(p);
}
static bool create (const char *file, unsigned initial_size)
{
  DBG_MSG("[%s] calls open %s\n", thread_name(), file);
  if(file == NULL || !*file)
  {
    exit(-1);
  }
  if(strlen(file) > UCHAR_MAX)
  {
    return 0;
  }

}
static bool remove (const char *file)
{
  DBG_MSG("[%s] calls remove %s\n", thread_name(), file);
  if(file == NULL)
  {
    exit(-1);
  }
}
static int open (const char *file)
{
  DBG_MSG("[%s] calls open %s\n", thread_name(), file);
  if(file == NULL || !*file)
  {
    return -1;
  }
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
      break;
  }
}
static int read (int fd, void *buffer, unsigned length)
{
  DBG_MSG("[%s] calls read %d bytes from %d\n", thread_name(), length, fd);
  if(buffer == NULL || fd < 0)
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
      break;
  }
}
static int write (int fd, const void *buffer, unsigned length)
{
  DBG_MSG("[%s] calls write %d bytes to %d\n", thread_name(), length, fd);
  if(buffer == NULL || fd < 0)
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
      break;
  }
}