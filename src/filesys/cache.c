#include "cache.h"
#include "list.h"
#include "devices/block.h"
#include "threads/malloc.h"

struct _disk_cache *disk_cache;   /* The page cache object */
uint8_t *mem_cache;         /* The memory for the cache region */


void page_cache_init()
{
    mem_cache = malloc(CACHE_SIZE * BLOCK_SECTOR_SIZE);
    disk_cache = malloc(sizeof(struct _disk_cache));
    list_init(&disk_cache->sector_list);
    lock_init(&disk_cache->lock);
    // disk_cache->size = 0;
    /* Fill all the sector list with dump sector */
    uint32_t i;
    for (i = 0; i < CACHE_SIZE; i++)
    {
        struct _block_sector *b = malloc(sizeof(struct _block_sector));
        b->data = mem_cache + i*BLOCK_SECTOR_SIZE;
        b->sector = -1;
        b->flags = 0;
        list_push_back(&disk_cache->sector_list, &b->elem);
    }
    
}

uint8_t *page_cache_search(block_sector_t sector)
{
    struct _block_sector *b;
    struct lish_elem *e = list_head(&disk_cache->sector_list);
    while((e = list_next(e)) != list_end(&disk_cache->sector_list))
    {
        b = list_entry(e, struct _block_sector, elem);
        if(b->sector == sector) break;
    }
    if(e == list_end(&disk_cache->sector_list))
    {
        return NULL;
    }
    else
    {
        return b->data;
    }
}

bool page_cache_load(block_sector_t sector)
{
    struct list_elem *e;
    struct _block_sector *b;
    uint32_t i = 0;
    while(1)
    {
        e = list_pop_front(&disk_cache->sector_list);
        b = list_entry(e, struct _block_sector, elem);
        if(!sector_is_dirty(b))
        {
            break;
        }
        else
        {
            list_push_back(&disk_cache->sector_list, e);
        }
    }
    
}
