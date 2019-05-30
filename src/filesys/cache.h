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
#define CACHE_SIZE  (256)    /* Size of the page cache in sector (512B) */
#define PC_A        (0x1)   /* Access bit */
#define PC_D        (0x2)   /* Dirty bit */


struct _block_sector         /* A block sector in disk cache */ 
{
    struct list_elem elem;    
    block_sector_t sector;  /* Corresponding sector on disk */
    uint32_t flags;         /* Flags */
    uint8_t *data;          /* Data of the sector */
};

struct _disk_cache           /* The buffer cache object */
{
    struct list sector_list;      /* The hash table for cache */
    struct lock lock;       /* Global lock for the cache */
    uint32_t size;          /* Size of the cache in sector */
};
/* Init the page cache */
void disk_cache_init(void); 
/* Search for a particular disk sector in the page cache */
uint8_t *disk_cache_search(block_sector_t sector, bool write);
/* Add a sector to the page cache */
uint8_t *disk_cache_load(block_sector_t sector, bool write);
/* Remove a sector from page cache */
bool disk_cache_flush(struct _block_sector block_sector);
/* Flush all page from cache to disk */
void disk_cache_flush_all(void);
/* Return block sector is dirty or not */
bool block_sector_is_dirty(struct _block_sector *b);
/* Set a block sector dirty */
void block_sector_set_dirty(struct _block_sector *b, bool dirty);
/* Return block sector is accessed or not */
bool block_sector_is_accessed(struct _block_sector *b);
/* Set a block sector dirty or not */
void block_sector_set_accessed(struct _block_sector *b, bool access);
/* Destroy the page cache */
#endif // !_CACHE_H_