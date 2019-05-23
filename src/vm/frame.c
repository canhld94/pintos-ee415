/*  
  Duc-Canh Le <canhld@kaist.ac.kr>
  Network and Computing Laboratory
*/

#include "frame.h"
#include "stdio.h"
#include "stdlib.h"
#include "round.h"
#include "bitmap.h"
#include "random.h"
#include "string.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "swap.h"
#include "page.h"
#include "threads/interrupt.h"
struct _frame frame;
extern struct _swap swap;
static uint8_t *frame_to_be_evicted();
static bool frame_table_is_restricted(uint8_t *pframe);

/*
    Init the frame table
*/

void frame_init()
{
    lock_init(&frame.lock);
    uint32_t npage = ROUND_UP(init_ram_pages, PGSIZE/sizeof(uint32_t));
    frame.frame_table = palloc_get_multiple(0|PAL_ZERO,  npage * sizeof(struct _frame_table) / PGSIZE);
    uint8_t *free_start = ptov (1024 * 1024);
    uint8_t *free_end = ptov (init_ram_pages * PGSIZE);
    size_t free_pages = (free_end - free_start) / PGSIZE;
    frame.total_frames = free_pages;
    size_t bm_pages = DIV_ROUND_UP (bitmap_buf_size (frame.total_frames/2), PGSIZE);
    frame.user_frames = frame.total_frames/2 - bm_pages;
    DBG_MSG_VM("[VM: %s] frame table init with %d frames and %d entries\n", thread_name(), npage, frame.user_frames);
}

/* Frame table manipulation*/
void frame_table_get(uint8_t *pframe, struct thread **t, uint8_t **upage, bool lock)
{
    uint32_t index =  (uint32_t) pframe / PGSIZE - (1024*1024)/PGSIZE - frame.total_frames/2 - 1;
    if(lock) lock_acquire(&frame.lock);
    *t = frame.frame_table[index].thread;
    *upage = frame.frame_table[index].page;
    if(lock) lock_release(&frame.lock);
}

void frame_table_set(uint8_t *pframe, struct thread *t, uint8_t *page, bool lock)
{
    uint32_t index =  (uint32_t) pframe / PGSIZE - (1024*1024)/PGSIZE - frame.total_frames/2 - 1;
    // DBG_MSG_VM("[VM: %s] adding new pages to frame table at %d value 0x%x\n", thread_name(), index, page);
    if(lock) lock_acquire(&frame.lock);
    frame.frame_table[index].thread = t;
    frame.frame_table[index].page = page;
    if(lock) lock_release(&frame.lock);
}

uint8_t *frame_table_get_pframe(uint32_t index)
{
    return (uint8_t *) ((index + (1024*1024)/PGSIZE + frame.total_frames/2 + 1)*PGSIZE);
}

/*
    Allocate one frame from user pool and add it to frame table
*/
void *frame_alloc(void * vpage)
{
    lock_acquire(&frame.lock);
    /* Obtain one page from user pool */
    uint8_t *kpage = palloc_get_page(PAL_USER|PAL_ZERO);
    if(kpage != NULL)
    {
    /* Update the frame table entry at pframe */
        frame_table_set(vtop(kpage), thread_current(), vpage, false);
        lock_release(&frame.lock);
        // DBG_MSG_VM("[VM: %s] new page allocated for 0x%x \n", thread_name(), vpage);
        return kpage;
    }
    else
    {
        /* Allocate one swap page */
        uint32_t swap_page = swap_alloc();
        if(swap_page == -1) /* Even swap is full */
        {
            return NULL;
        }
        /* 
        Evic one frame 
        BUG: Should we update the frame first or swap first?
        */
        struct thread *t; 
        uint8_t *upage;
        lock_acquire(&swap.lock);
        uint8_t *pframe;
        do 
        {
         pframe = frame_to_be_evicted();
        } while (frame_table_is_restricted(pframe));
        frame_table_get(pframe, &t, &upage, false);
        ASSERT(upage != NULL && t != NULL);
        DBG_MSG_VM("[VM: %s] swap page 0x%x to swap slot %d entries %s\n", thread_name(), upage, swap_page, t->name);
        /* TODO: Update supp table */
        while(page_table_insert(t, upage, swap_page << PGBITS) != NULL) 
        {
            DBG_MSG_VM("[VM: %s] spinning \n", thread_name());
            lock_release(&frame.lock);
            lock_release(&swap.lock);
            random_init(thread_current());
            thread_sleep(random_ulong() % 10);
            DBG_MSG_VM("[VM: %s] spinning middle \n", thread_name());
            lock_acquire(&frame.lock);
            lock_acquire(&swap.lock);
            DBG_MSG_VM("[VM: %s] spinning end \n", thread_name());
        }
        // DBG_MSG_VM("[VM: %s] swap frame 0x%x to swap slot %d entries %s\n", thread_name(), pframe, swap_page, t->name);
        /* TODO: Update pagedir table */
        if(t->pagedir) pagedir_clear_page(t->pagedir, upage);
        /* Update frame table and pagedir */
        frame_table_set(pframe, thread_current(), vpage, false);
        swap_out(pframe, swap_page, t);
        lock_release(&swap.lock);
        lock_release(&frame.lock);
        memset(ptov(pframe), 0, PGSIZE);
        // DBG_MSG_VM("[VM: %s] swap page 0x%x to swap slot %d entries %s\n", thread_name(), upage, swap_page, t->name);
        return ptov(pframe);
    }
}

void frame_table_free(struct thread *t)
{
    int i;
    // lock_acquire(&frame.lock);
    for(i = 0; i < frame.user_frames; i++)
    {
        if(frame.frame_table[i].thread == t)
        {
            // DBG_MSG_VM("[VM: %s] free frame entry 0x%x\n", thread_name(), frame_table_get_pframe(i));
            frame.frame_table[i].thread = NULL;
            frame.frame_table[i].page = NULL;
            frame.frame_table[i].aux = NULL;
        }
    }
    // lock_release(&frame.lock);
}

void frame_free(void *kpage)
{    
    lock_acquire(&frame.lock);
    /* Free the kernel page */
    palloc_free_page(kpage);
    /* Remove the page entry from the frame table */
    frame_table_set(vtop(kpage), NULL, NULL, false);
    lock_release(&frame.lock);
}

void frame_destroy()
{
    int npage = ROUND_UP(init_ram_pages, PGSIZE/sizeof(uint32_t));
    // frame_table_dump();
    palloc_free_multiple(frame.frame_table, npage * sizeof(struct _frame_table) / PGSIZE);
}

void frame_table_set_restricted(uint8_t *pframe, void *access)
{
    uint32_t index =  (uint32_t) pframe / PGSIZE - (1024*1024)/PGSIZE - frame.total_frames/2 - 1;
    lock_acquire(&frame.lock);
    frame.frame_table[index].aux = access;
    lock_release(&frame.lock);
}
static bool frame_table_is_restricted(uint8_t *pframe)
{
    void *access;
    uint32_t index =  (uint32_t) pframe / PGSIZE - (1024*1024)/PGSIZE - frame.total_frames/2 - 1;
    // lock_acquire(&frame.lock);
    access = frame.frame_table[index].aux;
    // lock_release(&frame.lock);
    if(access == -1) 
    {
        return true;
    }
    else return false;
}

static uint8_t *frame_to_be_evicted()
{
    static uint32_t i = 0;
    uint32_t f;
    gen:
    i += 1;
    random_init(i);
    f = random_ulong() % (frame.user_frames - 1);
    // if(pagedir_is_dirty(frame.frame_table[f].thread->pagedir, frame.frame_table[f].page))
    // {
    //     goto gen;
    // }
    return frame_table_get_pframe(f);
}

void frame_table_dump()
{
    int i;
    for(i = 0 ; i < frame.user_frames; ++i)
    {
        printf("Frame-table[%d] %s 0x%x\n", i, frame.frame_table[i].thread->name, frame.frame_table[i].page);        
    }
}