#ifndef  _FRAME_H_
#define _FRAME_H_
#include "threads/palloc.h"
#include "threads/synch.h"

struct _frame_table
{
    struct thread *thread;
    uint8_t *page;
};

struct _frame
{
    struct lock lock;
    struct _frame_table *frame_table;
    uint32_t total_frames;
    uint32_t user_frames;
};


/*
Implement of physical frame table, the entry format is same as page table 
Frame table should be global
Similary, the swap table should be global
Only the supp table is local (thread scope)
*/
/* Frame table entries.

   31                                 12 11                     0
   +------------------------------------+------------------------+
   |               Thread Address       |         Flags          |
   +------------------------------------+------------------------+

   The PD Address point to the Page directrory that reference the addr
   The important flags are listed below.
   When a PDE or PTE is not "present", the other flags are
   ignored.
   A PDE or PTE that is initialized to 0 will be interpreted as
   "not present", which is just fine. */

void frame_init();
void *frame_alloc();
void frame_free(void *);
void frame_table_get(uint8_t *pframe, struct thread **t, uint8_t **page);
void frame_table_set(uint8_t *pframe, struct thread *t, uint8_t *page);
void frame_table_free(struct thread *t);
uint8_t *frame_table_get_pframe(uint32_t index);
void frame_destroy();
void frame_table_dump();



#endif // ! _FRAME_H_#define _FRAME_H_


