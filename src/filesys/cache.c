#include "filesys/cache.h"
#include "list.h"
#include "string.h"
#include "devices/block.h"
#include "threads/malloc.h"

struct _disk_cache *disk_cache;   /* The page cache object */
uint8_t *mem_cache;         /* The memory for the cache region */

void rwlock_init( struct _rw_lock *rw )
{
  rw->reader = 0;
  lock_init(&rw->lock);
  lock_init(&rw->write_lock);
}

void rwlock_acquire_read_lock (struct _rw_lock *rw)
{
  lock_acquire(&rw->lock);
  rw->reader++;
  if(rw->reader == 1)       /* First reader hold write lock */
  {
    lock_acquire(&rw->write_lock);
  }
  lock_release(&rw->lock);
}

void rwlock_release_read_lock (struct _rw_lock *rw)
{
  lock_acquire(&rw->lock);
  rw->reader--;
  if(rw->reader == 0);     /* Last reader --> allow write */
  {
    lock_release(&rw->write_lock);
  }
  lock_release(&rw->lock);
}

void rwlock_acquire_write_lock(struct _rw_lock *rw)
{
  lock_acquire(&rw->write_lock);
}

void rwlock_release_write_lock(struct _rw_lock *rw)
{
  lock_release(&rw->write_lock);
}
/*
    Helper function for cached read and write
*/

static void ref_count_incr(struct _block_sector *b)
{
    lock_acquire(&b->lock);
    b->ref_count++;
    lock_release(&b->lock);
}

static void ref_count_decr(struct _block_sector *b)
{
    lock_acquire(&b->lock);
    b->ref_count--;
    lock_release(&b->lock);
}

static uint32_t ref_count_get(struct _block_sector *b)
{
    lock_acquire(&b->lock);
    uint32_t ref = b->ref_count;
    lock_release(&b->lock);
    return ref;
}
/* Emulate the circular queue of page cache */
static struct _block_sector *disk_cache_rotate();
/* Search for a particular disk sector in the page cache */
static struct _block_sector *disk_cache_search(block_sector_t sector);
/* Add a sector to the page cache */
static struct _block_sector *disk_cache_load(block_sector_t sector, bool write);
/* Bring a data from disk to page cache */
static void block_sector_load(struct _block_sector *b);
/* Remove a sector from page cache */
static void block_sector_flush(struct _block_sector *b, bool evict);
/* Return block sector is dirty or not */
static bool block_sector_is_dirty(struct _block_sector *b);
/* Set a block sector dirty */
static void block_sector_set_dirty(struct _block_sector *b, bool dirty);
/* Return block sector is valid or not */
static bool block_sector_is_valid(struct _block_sector *b);
/* Set a block sector dirty */
static void block_sector_set_valid(struct _block_sector *b, bool valid);
/* Return block sector is accessed or not */
static bool block_sector_is_accessed(struct _block_sector *b);
/* Set a block sector dirty or not */
static void block_sector_set_accessed(struct _block_sector *b, bool access);
/* Destroy the page cache */

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
        b->ref_count = 0;
        rwlock_init(&b->rw);
        lock_init(&b->lock);
        list_push_back(&disk_cache->sector_list, &b->elem);
    }
}

void cached_read(block_sector_t sector, void *data, uint32_t offset, uint32_t len)
{
    // check if the block is in the cache
    struct _block_sector *b = disk_cache_search(sector);
    if(b == NULL)
    {
      b = disk_cache_load(sector, false);
    }
    rwlock_acquire_read_lock(&b->rw);
    memcpy(data, b->data + offset, len);
    block_sector_set_accessed(b, true);
    rwlock_release_read_lock(&b->rw);
}


void cached_write(block_sector_t sector, void *data, uint32_t offset, uint32_t len)
{
    struct _block_sector *b = disk_cache_search(sector);
    if(b == NULL)
    {
      if (offset > 0 || len < BLOCK_SECTOR_SIZE)
      {
        b = disk_cache_load(sector, false);
      }
      else
      {
          b= disk_cache_load(sector, true);
      }
    }
    rwlock_acquire_read_lock(&b->rw);
    memcpy(b->data + offset, data, len);
    block_sector_set_accessed(b, true);
    block_sector_set_dirty(b, true);
    rwlock_release_read_lock(&b->rw);
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
            block_sector_flush(b, false);
        }
    }
    lock_release(&disk_cache->lock);
}

/* 
Implement of static function
*/

static struct _block_sector *disk_cache_rotate()
{
    lock_acquire(&disk_cache->lock);
    struct list_elem *e = list_pop_front(&disk_cache->sector_list);
    struct _block_sector *b = list_entry(e, struct _block_sector, elem);
    list_push_back(&disk_cache->sector_list, e);
    lock_release(&disk_cache->lock);
    return b;
}

static struct _block_sector *disk_cache_search(block_sector_t sector)
{
    lock_acquire(&disk_cache->lock);
    struct list_elem *e = list_head(&disk_cache->sector_list);
    struct _block_sector *b;
    while((e = list_next(e)) != list_end(&disk_cache->sector_list))
    {
        b = list_entry(e, struct _block_sector, elem);
        lock_acquire(&b->lock);
        if(b->sector == sector) 
        {
            lock_release(&b->lock);
            break;
        }
        else
        {
            lock_release(&b->lock);
        }
    }
    if(e == list_end(&disk_cache->sector_list))
    {
        lock_release(&disk_cache->lock);
        return NULL;
    }
    else
    {
        lock_release(&disk_cache->lock);
        return b;
    }
}

static struct _block_sector *disk_cache_load(block_sector_t sector, bool write)
{
    struct list_elem *e;
    struct _block_sector *b;
    uint32_t i = 0, j = 0;
    while(1)
    {
        /* Emulate */
        b = disk_cache_rotate();
        if(!block_sector_is_accessed(b))    /* If this sector is not accessed --> evict */
        {
            if(ref_count_get(b) == 0) break;
        }
        else
        {
            i++;
            if(i < CACHE_SIZE)      /* Not enought one round, cnt. searching for not accessed sector */
            {
                if (!block_sector_is_dirty(b))
                    block_sector_set_accessed(b, false);    /* Give the sector a second chance */
            }
            else                    /* All block is accessed, now check dirty */
            {
                if(!block_sector_is_dirty(b)) /* This sector is not dirty --> evict */
                {
                    if(ref_count_get(b) == 0) break;
                }
                else
                {
                    j++;
                    if(j > CACHE_SIZE)             /* All sectors are dirty --> just evict randomly one */
                    {
                        b = disk_cache_rotate();
                        if(ref_count_get(b) == 0) break;
                    }   
                }  
            }
        }
    }
    // DBG_MSG_FS("[FS - %s] evict sector %d for new sector %d at it %d %d\n", thread_name(), b->sector, sector, i, j);
    if(block_sector_is_dirty(b))    /* If b is dirty, then write back */
    {
        // DBG_MSG_FS("[FS - %s] fflush dirty sector %d for new sector %d at it %d %d\n", thread_name(), b->sector, sector, i, j);
        block_sector_flush(b, true);
    }
    rwlock_acquire_write_lock(&b->rw);
    memset(b->data, 0, BLOCK_SECTOR_SIZE);
    if(!write)
        block_read(fs_device, sector, b->data);
    block_sector_set_accessed(b, false);
    block_sector_set_dirty(b, false);
    lock_acquire(&b->lock);
    b->sector = sector;
    lock_release(&b->lock);
    rwlock_release_write_lock(&b->rw);
    return b;
}

static void block_sector_load(struct _block_sector *b)
{
    ASSERT(!block_sector_is_accessed(b));
    ASSERT(!block_sector_is_dirty(b));
    while (ref_count_get(b) > 0) 
    {
        printf("load yield()\n");
        thread_yield();
    }
    ref_count_incr(b);
    rwlock_acquire_write_lock(&b->rw);
    block_read(fs_device, b->sector, b->data);
    rwlock_release_write_lock(&b->rw);
    ref_count_decr(b);
}

static void block_sector_flush(struct _block_sector *b, bool evict)
{
    // ASSERT(block_sector_is_dirty(b));
    // ASSERT(block_sector_is_accessed(b));
    while (ref_count_get(b) > 0) 
    {
        printf("fflush yield()\n");
        thread_yield();
    }
    // ref_count_incr(b);
    rwlock_acquire_write_lock(&b->rw);
    block_sector_set_accessed(b, false);
    block_sector_set_dirty(b, false);
    block_write(fs_device, b->sector, b->data);
    if(evict) b->sector = -1;
    rwlock_release_write_lock(&b->rw);
    // ref_count_decr(b);
}

static bool block_sector_is_dirty(struct _block_sector *b)
{
    lock_acquire(&b->lock);
    bool status = ((b->flags & PC_D) != 0);
    lock_release(&b->lock);
    return status;
}

static void block_sector_set_dirty(struct _block_sector *b, bool dirty)
{
    lock_acquire(&b->lock);
    if (dirty)
    {
        b->flags |= PC_D;
    }
    else
    {
        b->flags &= ~ (uint32_t) PC_D;
    }
    lock_release(&b->lock);
}  

static bool block_sector_is_accessed(struct _block_sector *b)
{
    lock_acquire(&b->lock);
    bool status = ((b->flags & PC_A) != 0);
    lock_release(&b->lock);
    return status;
}

static void block_sector_set_accessed(struct _block_sector *b, bool access)
{
    lock_acquire(&b->lock);
    if (access)
    {
        b->flags |= PC_A;
    }
    else
    {
        b->flags &= ~ (uint32_t) PC_A;
    }
    lock_release(&b->lock);
}

static bool block_sector_is_valid(struct _block_sector *b)
{
    lock_acquire(&b->lock);
    bool status = ((b->flags & PC_V) != 0);
    lock_release(&b->lock);
    return status;
}

static void block_sector_set_valid(struct _block_sector *b, bool valid)
{
    lock_acquire(&b->lock);
    if (valid)
    {
        b->flags |= PC_V;
    }
    else
    {
        b->flags &= ~ (uint32_t) PC_V;
    }
    lock_release(&b->lock);
}