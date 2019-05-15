/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
/* Process identifier. */
typedef int pid_t;
typedef int mmapid_t;
#define PID_ERROR ((pid_t) -1)

void exit (int status);
void munmap(mmapid_t mapping);

#endif /* userprog/syscall.h */
