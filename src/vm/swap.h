#ifndef _SWAP_H_
#define _SWAP_H_
#include "devices/block.h"
#include "threads/synch.h"

struct _swap
{
    struct block *block_sw;
    uint32_t **sw_table;
    struct lock lock;
};

void swap_init();

void swap_out(uint32_t *pframe, uint32_t swap_index);

void swap_in(uint32_t swap_index, uint32_t *pframe);

int swap_alloc();

void swap_free(uint32_t);

void swap_destroy();

#endif // !_SWAP_H_
