#include "userprog/syscall.h"
#include <lib/stdio.h>
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
static void exit (int status) NO_RETURN;
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
  // hex_dump((uint32_t)f->esp, f->esp, (PHYS_BASE - f->esp), 1); 
  // printf("System call number %d\n", *(int*)f->esp);
  // return;
  char *esp = f->esp;
  // char *base = (char *) f->esp_dummy;
  int arg0 = TO_ARG(esp, 1), arg1 = TO_ARG(esp, 2), arg2 = TO_ARG(esp, 3);
  int ret_val;
  switch (* (int *)esp)
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
  shutdown_power_off();
}
static void exit (int status)
{
  DBG_MSG("[%s] exit call\n", thread_name());
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
    DBG_MSG("[%s] executing %s ...\n", thread_name(), file);
    pid_t pid = process_execute(file);
    return pid;

}
static int wait (pid_t p)
{
  return process_wait(p);
}
static bool create (const char *file, unsigned initial_size)
{

}
static bool remove (const char *file)
{

}
static int open (const char *file)
{

}
static int filesize (int fd)
{

}
static int read (int fd, void *buffer, unsigned length)
{

}
static int write (int fd, const void *buffer, unsigned length)
{
  // printf("this is write to %d with 0x%x and len %d\n", fd, buffer, length);
  switch (fd)
  {
    case STDIN_FILENO: /* stdin */
      break;
    case STDOUT_FILENO: /* stdout */
      putbuf(buffer, length);
    default:
      break;
  }
  return 0;   
}
static void seek (int fd, unsigned position)
{

}
static unsigned tell (int fd)
{

}
static void close (int fd)
{

}