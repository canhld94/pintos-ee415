#ifndef _SWAP_H_
#define _SWAP_H_
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct _swap
{
    struct block *block_sw;
    void **sw_table;
    struct lock lock;
};

void swap_init();

void swap_out(uint8_t *pframe, uint32_t swap_index, struct thread *t);

void swap_in(uint32_t swap_index, uint8_t *pframe);

int swap_alloc();

void swap_free(struct thread *t);

void swap_destroy();

#endif // !_SWAP_H_
