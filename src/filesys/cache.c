#include "filesys/cache.h"
#include "list.h"
#include "string.h"
#include "devices/block.h"
#include "threads/malloc.h"

struct _disk_cache *disk_cache;   /* The page cache object */
uint8_t *mem_cache;         /* The memory for the cache region */


void disk_cache_init()
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
        b->sector = CACHE_MAGIC;
        b->flags = 0;
        list_push_back(&disk_cache->sector_list, &b->elem);
    }
}

struct _block_sector *disk_cache_search(block_sector_t sector)
{
    lock_acquire(&disk_cache->lock);
    struct list_elem *e = list_head(&disk_cache->sector_list);
    struct _block_sector *b;
    while((e = list_next(e)) != list_end(&disk_cache->sector_list))
    {
        b = list_entry(e, struct _block_sector, elem);
        if(b->sector == sector) break;
    }
    lock_release(&disk_cache->lock);
    if(e == list_end(&disk_cache->sector_list))
    {
        return NULL;
    }
    else
    {
        return b;
    }
}

struct _block_sector *disk_cache_load(block_sector_t sector, bool write)
{
    struct list_elem *e;
    struct _block_sector *b;
    uint32_t i = 0, j = 0;
    while(1)
    {
        e = list_pop_front(&disk_cache->sector_list);
        b = list_entry(e, struct _block_sector, elem);
        if(!block_sector_is_accessed(b) && block_sector_is_valid(b))    /* If this sector is not accessed --> evict */
        {
            break;
        }
        else
        {
            i++;
            if(i < CACHE_SIZE)      /* Not enought one round, cnt. searching for not accessed sector */
            {
                if (!block_sector_is_dirty(b))
                    block_sector_set_accessed(b, false);    /* Give the sector a second chance */
                list_push_back(&disk_cache->sector_list, e);
            }
            else                    /* All block is accessed, now check dirty */
            {
                if(!block_sector_is_dirty(b)) /* This sector is not dirty --> evict */
                {
                    break;
                }
                else
                {
                    j++;
                    if(j < CACHE_SIZE)  /* Cnt. searching for not dirty sector */
                    {
                        list_push_back(&disk_cache->sector_list, e);
                    }
                    else                /* All sectors are dirty --> just evict one */
                    {
                        break;
                    }   
                }  
            }
        }
    }
    if(block_sector_is_dirty(b))    /* If b is dirty, then write back */
    {
        ASSERT(block_sector_is_accessed(b));
        // DBG_MSG_FS("[FS - %s] fflush dirty sector %d at it %d %d\n", thread_name(), b->sector, i, j);
        block_write(fs_device, b->sector, b->data);
    }
    lock_acquire(&disk_cache->lock);
    block_sector_set_valid(b, false);
    memset(b->data, 0, BLOCK_SECTOR_SIZE);
    b->sector = sector;
    if(!write)
        block_read(fs_device, b->sector, b->data);
    block_sector_set_accessed(b, false);
    block_sector_set_dirty(b, false);
    if(!write)
        block_sector_set_valid(b, true);
    list_push_back(&disk_cache->sector_list, e);    
    lock_release(&disk_cache->lock);
    return b;
}

void block_sector_load(struct _block_sector *b)
{
    lock_acquire(&disk_cache->lock);
    ASSERT(!block_sector_is_accessed(b));
    ASSERT(!block_sector_is_dirty(b));
    block_read(fs_device, b->sector, b->data);
    lock_release(&disk_cache->lock);
}

void block_sector_write(struct _block_sector *b, void *data, uint32_t offs, uint32_t len)
{
    lock_acquire(&disk_cache->lock);
    block_sector_set_valid(b, false);
    memcpy(b->data + offs, data, len);
    block_sector_set_accessed(b, true);
    block_sector_set_dirty(b, true);
    block_sector_set_valid(b, true);
    lock_release(&disk_cache->lock);
}

void block_sector_read(struct _block_sector *b, void *data, uint32_t offs, uint32_t len)
{
    lock_acquire(&disk_cache->lock);
    ASSERT(block_sector_is_valid(b));
    memcpy(data, b->data + offs, len);
    block_sector_set_accessed(b, true);
    lock_release(&disk_cache->lock);
}

bool block_sector_is_dirty(struct _block_sector *b)
{
    return (b->flags & PC_D) != 0;
}

void block_sector_set_dirty(struct _block_sector *b, bool dirty)
{
    if (dirty)
    {
        b->flags |= PC_D;
    }
    else
    {
        b->flags &= ~ (uint32_t) PC_D;
    }
}  

bool block_sector_is_accessed(struct _block_sector *b)
{
    return (b->flags & PC_A) != 0;
}

void block_sector_set_accessed(struct _block_sector *b, bool access)
{
    if (access)
    {
        b->flags |= PC_A;
    }
    else
    {
        b->flags &= ~ (uint32_t) PC_A;
    }
}

bool block_sector_is_valid(struct _block_sector *b)
{
    return (b->flags & PC_V) != 0;
}

void block_sector_set_valid(struct _block_sector *b, bool valid)
{
    if (valid)
    {
        b->flags |= PC_V;
    }
    else
    {
        b->flags &= ~ (uint32_t) PC_V;
    }
}


void disk_cache_flush_all()
{
    // DBG_MSG_FS("[FS - %s] fflush all dirty sector\n", thread_name());
    lock_acquire(&disk_cache->lock);
    struct list_elem *e = list_head(&disk_cache->sector_list);
    struct _block_sector *b;
    while((e = list_next(e)) != list_end(&disk_cache->sector_list))
    {
        b = list_entry(e, struct _block_sector, elem);
        if(block_sector_is_dirty(b)) 
        {
            ASSERT(block_sector_is_accessed(b));
            block_write(fs_device, b->sector, b->data);
            block_sector_set_accessed(b, false);
            block_sector_set_dirty(b, false);
        }
    }
    lock_release(&disk_cache->lock);
}
