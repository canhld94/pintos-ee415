#include "swap.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "stdlib.h"
#include "stdio.h"
#include "round.h"
#include "frame.h"
#include "threads/thread.h"
struct _swap swap;
extern struct _frame frame;

void swap_init()
{   /* Init the lock */
    lock_init(&swap.lock);
    /* Get the swap block */
    swap.block_sw = block_get_role(BLOCK_SWAP);
    if(swap.block_sw != NULL)
    {
        /* Get swap size in bytes*/
        int swap_size = ROUND_UP(block_size(swap.block_sw) * BLOCK_SECTOR_SIZE, PGSIZE)/PGSIZE;
        DBG_MSG_VM("[VM: %s] swap table init with %d pages\n", thread_name(), swap_size);
        /* We maintain swap table as an array, the index is the block sector index
           and the entry is the corresponding physical frame
           Don't care about conner case now */
        swap.sw_table = palloc_get_multiple(0|PAL_ZERO, swap_size * sizeof(uint32_t) / PGSIZE);
    }
}

/*
    Evic a frame at *pframe and write it to sector 
*/
void swap_out(uint32_t *pframe, uint32_t swap_index)
{
    /* Write evicted frame to sector */
    int write = 0, sector = swap_index * PGSIZE / BLOCK_SECTOR_SIZE;
    while(write < PGSIZE){
        block_write(swap.block_sw, sector, ptov(pframe) + write);
        write += BLOCK_SECTOR_SIZE;
        sector++;
    }
    /* Update the swap table */
    struct thread *t;
    uint32_t *upage;
    frame_table_get(pframe, &t, &upage);
    lock_acquire(&swap.lock);
    swap.sw_table[swap_index] = t;
    lock_release(&swap.lock);
    return true;
}

/*
    Bring a swap page at swap_index to *pframe 
*/
void swap_in(uint32_t swap_index, uint32_t *pframe)
{
    /* Read the target frame to sector */
    int read = 0, sector = swap_index * PGSIZE / BLOCK_SECTOR_SIZE;
    while(read < PGSIZE){
        block_read(swap.block_sw, sector, ptov(pframe) + read);
        read += BLOCK_SECTOR_SIZE;
        sector++;
    }
    /* Update the swap table */
    lock_acquire(&swap.lock);
    swap.sw_table[swap_index] = NULL;
    lock_release(&swap.lock);
}

int swap_alloc()
{
    int i = 0;
    int swap_pages = block_size(swap.block_sw) * BLOCK_SECTOR_SIZE / PGSIZE;
    for(i = 0; i < swap_pages; i++)
    {
        if(swap.sw_table[i] == NULL) break;
    }
    if(i < swap_pages)
    {
        lock_acquire(&swap.lock);
        swap.sw_table[i] = thread_current();
        lock_release(&swap.lock);
        return i;
    }
    else
    {
        return -1;
    }
}
/* Swap free, use when free resouce when a process is teardown*/
void swap_free(uint32_t index) 
{
    ASSERT(swap.sw_table[index] != NULL);
    lock_acquire(&swap.lock);
    swap.sw_table[index] = NULL;
    lock_release(&swap.lock);
}

void swap_destroy()
{
    int swap_size = ROUND_UP(block_size(swap.block_sw) * BLOCK_SECTOR_SIZE, PGSIZE)/PGSIZE;
    if(swap_size)
    {
        palloc_free_multiple(swap.sw_table, swap_size);
    }
}