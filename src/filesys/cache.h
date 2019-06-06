#ifndef _CACHE_H_
#define _CACHE_H_
#include "hash.h"
#include "stdio.h"
#include "stdlib.h"
#include "bitmap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/thread.h"

#define CACHE_MAGIC (-508)
#define CACHE_SIZE  (64)    /* Size of the page cache in sector (512B) */
#define PC_A        (0x1)   /* Access bit */
#define PC_D        (0x2)   /* Dirty bit */
#define PC_V        (0x4)   /* Valid bit */

/* RW lock, use in page cache and inode */
struct _rw_lock
{
  struct lock lock;           /* General lock */
  struct lock write_lock;     /* Write lock */
  uint32_t reader;            /* number of reader in lock */
};

void rwlock_init( struct _rw_lock *rw );

void rwlock_acquire_read_lock (struct _rw_lock *rw);

void rwlock_release_read_lock (struct _rw_lock *rw);

void rwlock_acquire_write_lock(struct _rw_lock *rw);

void rwlock_release_write_lock(struct _rw_lock *rw);


struct _block_sector         /* A block sector in disk cache */ 
{
    struct list_elem elem;    
    block_sector_t sector;  /* Corresponding sector on disk */
    uint32_t ref_count;     /* Number of process are refering to this block */
    uint32_t flags;         /* Flags */
    uint8_t *data;          /* Data of the sector */
    struct lock lock;       /* Accessed lock */
    struct _rw_lock rw;     /* r/w lock */
};

struct _disk_cache           /* The buffer cache object */
{
    struct list sector_list;      /* The hash table for cache */
    struct lock lock;       /* Global lock for the cache */
    uint32_t size;          /* Size of the cache in sector */
};
/* Init the page cache */
void disk_cache_init(void); 
/* Flush all page from cache to disk */
void disk_cache_flush_all(void);
/* Cached read, wrapper of block_read */
void cached_read(block_sector_t sector, void *data, uint32_t offs, uint32_t len);
/* Cached wrie, wrapper of block_write */
void cached_write(block_sector_t sector, void *data, uint32_t offs, uint32_t len);
#endif // !_CACHE_H_
