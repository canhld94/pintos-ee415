#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "filesys/cache.h"
#include "threads/synch.h"
struct bitmap;

/* Inode index parametter */
#define DIRECT_BLOCK 12
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
/*
  TODO: expand the inode disk structure to support extensible file
        use index blocks:
        block_sector_t direct_block[12]
        block_sector_t doubly_indirect_block;
        Each doubly indirect block can hold 512/4 = 128 sectors
        --> total file size 512*128 = 64 KB
*/
struct inode_disk
{
  // block_sector_t start;                               /* First data sector */
  block_sector_t dblock[DIRECT_BLOCK];                /* Direct data sector. */
  block_sector_t iblock;                              /* Inditect block */
  block_sector_t diblock;                             /* Duobly indirect block */
  off_t length;                                       /* File size in bytes. */
  uint32_t flags;                                     /* Flags */
  unsigned magic;                                     /* Magic number. */
  uint32_t unused[111];                               /* Not used. */
};

/* In-memory inode. */
/*
  TODO: Adding inode lock
        Inode lock --> read lock and write lock
*/
struct inode 
{
  struct list_elem elem;              /* Element in inode list. */
  block_sector_t sector;              /* Sector number of disk location. */
  int open_cnt;                       /* Number of openers. */
  bool removed;                       /* True if deleted, false otherwise. */
  int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  struct _rw_lock rw;
  struct inode_disk data;             /* Inode content. */
  struct lock lock;
};


// helper
// void map_sector_to_inode(block_sector_t *, block_sector_t *, block_sector_t **, size_t, struct inode_disk *);
// bool sectors_allocate(size_t cnt, block_sector_t *arr);
// void sectors_release(struct inode_disk *);

void inode_init (void);
bool inode_create (block_sector_t, off_t, uint32_t isdir);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
void inode_length_set (struct inode *inode, uint32_t newlen);

#endif /* filesys/inode.h */
