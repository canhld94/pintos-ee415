/* Runs 4 child-linear processes at once. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "stdio.h"

#define CHILD_CNT 4

void
test_main (void)
{
  pid_t children[CHILD_CNT];
  int i;

  for (i = 0; i < CHILD_CNT; i++) {
    char name[16];
    snprintf(name, 16, "%s%d", "child-linear ", i);
    CHECK ((children[i] = exec (name)) != -1,
           "exec \"child-linear\"");
  }


  for (i = 0; i < CHILD_CNT; i++) 
    CHECK (wait (children[i]) == 0x42, "wait for child %d", i);
}
