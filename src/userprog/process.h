/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
/* Maximum command line length is 4KB */
#define MAX_CMD_LEN 4096
/* Maximum length of each argument is 128B */
#define MAX_ARGV_LEN 128

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (void *upage, void *kpage, bool writable);


#endif /* userprog/process.h */
