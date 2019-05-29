#include "filesys/cache.h"
#include "list.h"
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

uint8_t *disk_cache_search(block_sector_t sector, bool write)
{
    lock_acquire(&disk_cache->lock);
    struct list_elem *e = list_head(&disk_cache->sector_list);
    struct _block_sector *b;
    while((e = list_next(e)) != list_end(&disk_cache->sector_list))
    {
        b = list_entry(e, struct _block_sector, elem);
        if(b->sector == sector) break;
    }
    if(e == list_end(&disk_cache->sector_list))
    {
        lock_release(&disk_cache->lock);
        return NULL;
    }
    else
    {
        block_sector_set_accessed(b, true);
        if(write) block_sector_set_dirty(b, true);
        lock_release(&disk_cache->lock);
        return b->data;
    }
}

uint8_t *disk_cache_load(block_sector_t sector, bool write)
{
    struct list_elem *e;
    struct _block_sector *b;
    uint32_t i = 0, j = 0;
    lock_acquire(&disk_cache->lock);
    while(1)
    {
        e = list_pop_front(&disk_cache->sector_list);
        b = list_entry(e, struct _block_sector, elem);
        if(!block_sector_is_accessed(b))    /* If this sector is not accessed --> evict */
        {
            break;
        }
        else
        {
            i++;
            if(i < CACHE_SIZE)      /* Not enought one round, cnt. searching for not accessed sector */
            {
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
        block_write(fs_device, b->sector, b->data);
    }
    b->sector = sector;
    block_read(fs_device, sector, b->data);
    if(write)                       /* If write then set the block dirty and accessed */
    {
        block_sector_set_accessed(b, true);
        block_sector_set_dirty(b, true);
        list_push_back(&disk_cache->sector_list, e);
    }
    else
    {
        block_sector_set_accessed(b, true);
        block_sector_set_dirty(b, false);
        list_push_back(&disk_cache->sector_list, e);    
    }
    lock_release(&disk_cache->lock);
    return b->data;
}

bool block_sector_is_dirty(struct _block_sector *b)
{
    return b->flags & PC_D;
}

void block_sector_set_dirty(struct _block_sector *b, bool dirty)
{
    if (dirty)
    {
        b->flags = b->flags | PC_D;
    }
    else
    {
        b->flags = b->flags & ~PC_D;
    }
    
    
}  

bool block_sector_is_accessed(struct _block_sector *b)
{
    return b->flags & PC_A;
}

void block_sector_set_accessed(struct _block_sector *b, bool access)
{
    if (access)
    {
        b->flags = b->flags | PC_A;
    }
    else
    {
        b->flags = b->flags & ~PC_A;
    }
}

bool disk_cache_flush_all()
{
    lock_acquire(&disk_cache->lock);
    struct list_elem *e = list_head(&disk_cache->sector_list);
    struct _block_sector *b;
    while((e = list_next(e)) != list_end(&disk_cache->sector_list))
    {
        b = list_entry(e, struct _block_sector, elem);
        if(block_sector_is_dirty(b)) 
        {
            block_write(fs_device, b->sector, b->data);
            block_sector_set_accessed(b, false);
            block_sector_set_dirty(b, false);
        }
    }
    lock_release(&disk_cache->lock);
}
