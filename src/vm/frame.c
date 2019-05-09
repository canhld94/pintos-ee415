#include "frame.h"
#include "stdio.h"
#include "stdlib.h"
#include "round.h"
#include "bitmap.h"
#include "random.h"
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
static uint8_t *frame_to_be_evicted();


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
void frame_table_get(uint8_t *pframe, struct thread **t, uint8_t **upage)
{
    uint32_t index =  (uint32_t) pframe / PGSIZE - (1024*1024)/PGSIZE - frame.total_frames/2 - 1;
    *t = frame.frame_table[index].thread;
    *upage = frame.frame_table[index].page;
}

void frame_table_set(uint8_t *pframe, struct thread *t, uint8_t *page)
{
    uint32_t index =  (uint32_t) pframe / PGSIZE - (1024*1024)/PGSIZE - frame.total_frames/2 - 1;
    // DBG_MSG_VM("[VM: %s] adding new pages to frame table at %d value 0x%x\n", thread_name(), index, page);
    lock_acquire(&frame.lock);
    frame.frame_table[index].thread = t;
    frame.frame_table[index].page = page;
    lock_release(&frame.lock);
}

uint8_t *frame_table_get_pframe(uint32_t index)
{
    return (uint8_t *) ((index + (1024*1024)/PGSIZE + frame.total_frames/2 + 1)*PGSIZE);
}

/*
    Allocate one frame from user pool and add it to frame table
*/
void *frame_alloc()
{
    /* Obtain one page from user pool */
    uint8_t *kpage = palloc_get_page(PAL_USER|PAL_ZERO);
    if(kpage != NULL)
    {
    /* Update the frame table entry at pframe */
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
        uint8_t *e_frame = frame_to_be_evicted();
        // DBG_MSG_VM("[VM: %s] swap frame 0x%x to swap slot %d\n", thread_name(), e_frame, swap_page);
        // DBG_MSG_VM("[VM: %s] evicted: %s, 0x%x, 0x%x, %d\n", thread_name(), t->name, upage, e_frame, swap_page);
        swap_out(e_frame, swap_page);
        intr_disable();
        struct thread *t; 
        uint8_t *upage;
        frame_table_get(e_frame, &t, &upage);
        /* TODO: Update pagedir table */
        pagedir_clear_page(t->pagedir, upage);
        /* TODO: Update supp table */
        page_table_insert(t, upage, swap_page);
        intr_enable();
        /* Update frame table and pagedir --> done with install_page */
        return ptov(e_frame);
    }
}

void frame_table_free(struct thread *t)
{
    int i;
    lock_acquire(&frame.lock);
    for(i = 0; i < frame.user_frames; i++)
    {
        if(frame.frame_table[i].thread == t)
        {
            // DBG_MSG_VM("[VM: %s] free frame entry 0x%x\n", thread_name(), frame_table_get_pframe(i));
            frame.frame_table[i].thread = NULL;
            frame.frame_table[i].page = NULL;
        }
    }
    lock_release(&frame.lock);
    
}

void frame_free(void *kpage)
{    
    /* Free the kernel page */
    palloc_free_page(kpage);
    /* Remove the page entry from the frame table */
    frame_table_set(vtop(kpage), NULL, NULL);
}

void frame_destroy()
{
    int npage = ROUND_UP(init_ram_pages, PGSIZE/sizeof(uint32_t));
    // frame_table_dump();
    palloc_free_multiple(frame.frame_table, npage * sizeof(struct _frame_table) / PGSIZE);
}


static uint8_t *frame_to_be_evicted()
{
    static uint32_t i = 0;
    i += 11;
    random_init(i%INT32_MAX);
    uint32_t f = random_ulong() % (frame.user_frames - 1);
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