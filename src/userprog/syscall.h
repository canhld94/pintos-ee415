/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
/* Process identifier. */
void exit (int status);
typedef int pid_t;
typedef int mmapid_t;
#define PID_ERROR ((pid_t) -1)

#endif /* userprog/syscall.h */
