#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Inode index block parametter */
#define DIRECT_BLOCK 12
#define SECTORS_PER_BLOCK  (BLOCK_SECTOR_SIZE / 4)
#define DIRECT_LIMIT    (DIRECT_BLOCK * BLOCK_SECTOR_SIZE)  /* 6KB */
#define IDIRECT_LIMIT   (DIRECT_LIMIT + BLOCK_SECTOR_SIZE * SECTORS_PER_BLOCK)  /* 70KB */
#define DIDIRECT_LIMIT  (IDIRECT_LIMIT + BLOCK_SECTOR_SIZE * SECTORS_PER_BLOCK * SECTORS_PER_BLOCK) /* 8262KB */

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

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
/*
  TODO: reimplement this function: convert from byte to sector
*/
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

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
};

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos >= inode->data.length)
  {
    return -1;  
  }
  /* Is it in the direct block */
  if (pos < DIRECT_LIMIT)
  {
    // printf("%d\n", pos);
    return inode->data.dblock[pos/BLOCK_SECTOR_SIZE];
  }
  /* Is it in the indirect block */
  else if (pos >= DIRECT_LIMIT && pos < IDIRECT_LIMIT)
  {
    /* Index of the sector in the indirect block */
    uint32_t offs_b = pos/BLOCK_SECTOR_SIZE - DIRECT_BLOCK;
    /* Read the indirect block to memory */
    block_sector_t* *buf = calloc(1, BLOCK_SECTOR_SIZE);
    cached_read(inode->data.iblock, buf, 0, BLOCK_SECTOR_SIZE);
    /* Read the desired entry */
    uint32_t sector = buf[offs_b];
    // printf("%d\n", sector);
    free(buf);
    return sector;
  }
  /* Is it in the doubly indirect block */
  else if (pos >= IDIRECT_LIMIT)
  {
    /* Index of the sector in the doubly indirect block */
    uint32_t offs_b = pos/BLOCK_SECTOR_SIZE - DIRECT_BLOCK - SECTORS_PER_BLOCK;
    /* Index of the indirect block */
    uint32_t offs_d = offs_b/SECTORS_PER_BLOCK;
    /* Index in the indirect block */
    uint32_t off_i = offs_b - offs_d*SECTORS_PER_BLOCK;
    /* Read first level  */
    block_sector_t *buf = malloc(BLOCK_SECTOR_SIZE);
    cached_read(inode->data.diblock, buf, 0, BLOCK_SECTOR_SIZE);
    /* Read the second level block */
    cached_read(buf[offs_d], buf, 0, BLOCK_SECTOR_SIZE);
    /* Read the desired entry */
    uint32_t sector = buf[off_i];
    free(buf);
    return sector;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

void map_sector_to_inode(block_sector_t *sectors_idx, 
                         block_sector_t **iblock,
                         block_sector_t ***diblock,
                         block_sector_t ***diblock_i,
                         size_t sectors, 
                         struct inode_disk *disk_inode)
{
  /* 
  Map the sector index to the inode structure 
  Fitst 12 sector to direct block
  Next 128 sectors --> need to allocate new sector (iblock)
  Nex 128*128 block --> 
  */
  uint32_t index = 0;
  /* Map the sectors array to direct block */
  for(; index < sectors; index++)
  {
    if(index >= DIRECT_BLOCK) break;  /* Excess direct block */
    else
    {
      disk_inode->dblock[index] = sectors_idx[index];
    }
  }
  // printf("%d\n", index);
  if(index == DIRECT_BLOCK) /* Need to use indirect block */
  {
    free_map_allocate(1, &disk_inode->iblock);
    /* Alocate the indirect block */
    *iblock = calloc(1, BLOCK_SECTOR_SIZE);
    ASSERT(*iblock != NULL);
    /* Map the sectors array to indirect block */
    for(; index < sectors; index++)
    {
      if(index >= DIRECT_BLOCK + SECTORS_PER_BLOCK) break;  /* Excess indirect block */
      else
      {
        (*iblock)[index - DIRECT_BLOCK] = sectors_idx[index];
      }
    }
  }
  // printf("%d\n", index);
  if(index == DIRECT_BLOCK + SECTORS_PER_BLOCK) /* Need to use doubly indirect block */
  {
    free_map_allocate(1, &disk_inode->diblock);
    /* Allocate the double indirect block */
    *diblock = calloc(1, BLOCK_SECTOR_SIZE);
    *diblock_i = calloc(1, BLOCK_SECTOR_SIZE);
    /* Map the sector array to indirect block */
    uint32_t sect, off, i, j;
    for(i = 0; i < SECTORS_PER_BLOCK; i++)
    {
      if(index == sectors) break;
      free_map_allocate(1, &(*diblock_i)[i]);
      // printf("dii %d\n", (*diblock_i)[i]);
      (*diblock)[i] = calloc(1,BLOCK_SECTOR_SIZE);
      for(j = 0; j < SECTORS_PER_BLOCK; j++)
      {
        (*diblock)[i][j] = sectors_idx[index++];
        // printf("di %d\n", (*diblock)[i][j]);
        if(index == sectors) break;
      }
    }
  }
  // printf("%d\n", index);
  // printf("%x %x\n", *iblock, *diblock);
}

/* 
Allocate cnt free sector, not neccessarily consecutive
Return the sector index to the array
*/
bool sectors_allocate(size_t cnt, block_sector_t *arr)
{
  uint32_t i;
  for(i = 0; i < cnt; i++)
  {
    if(!free_map_allocate(1, &arr[i]))
      return false;
  }
  return true;
}

void sectors_release(struct inode_disk *disk_inode)
{
  uint32_t sectors = bytes_to_sectors(disk_inode->length);
  uint32_t index;
  /* Free direct block */
  for(index = 0; index < sectors; index++)
  {
    if(index >= DIRECT_BLOCK) break;  /* Excess direct block */
    else
    {
      free_map_release(disk_inode->dblock[index], 1);
    }
  }
  if(index == DIRECT_BLOCK) /* Need to free indirect block */
  {
    /* Load the indirect block */
    block_sector_t *iblock = calloc(1, BLOCK_SECTOR_SIZE);
    cached_read(disk_inode->iblock, iblock, 0, BLOCK_SECTOR_SIZE);
    // ASSET(iblock != NULL);
    /* Map the sectors array to indirect block */
    for(; index < sectors; index++)
    {
      if(index >= DIRECT_BLOCK + SECTORS_PER_BLOCK) break;  /* Excess indirect block */
      else
      {
        free_map_release(iblock[index - DIRECT_BLOCK], 1);
      }
    }
    free_map_release(disk_inode->iblock, 1);
    free(iblock);
  }
  if(index == DIRECT_BLOCK + SECTORS_PER_BLOCK) /* Need to free doubly indirect block */
  {
    /* Load the doubly indirect block */
    block_sector_t **diblock = calloc(1, BLOCK_SECTOR_SIZE);
    cached_read(disk_inode->diblock, diblock, 0, BLOCK_SECTOR_SIZE);
    /* Map the sector array to indirect block */
    uint32_t sect, off, i, j;
    for(i = 0; i < SECTORS_PER_BLOCK; i++)
    {
      if(index == sectors) break;
      diblock[i] = malloc(BLOCK_SECTOR_SIZE);
      cached_read(diblock[i], diblock[i], 0, BLOCK_SECTOR_SIZE);
      for(j = 0; j < SECTORS_PER_BLOCK; j++)
      {
        free_map_release(diblock[i][j], 1);
        index++;
        if(index == sectors) break;
      }
      free(diblock[i]);
    }
    free(diblock);
  }
}
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0 && length < DIDIRECT_LIMIT);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    size_t sectors = bytes_to_sectors (length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    if(sectors == 0) goto re;
    DBG_MSG_FS("[FS - %s] create new inode with len = %d\n", thread_name(), sectors);
    block_sector_t *sectors_idx = malloc(sectors * sizeof(block_sector_t));
    ASSERT(sectors_idx != NULL);
    if (sectors_allocate (sectors, sectors_idx)) 
    {
      int i;
      // for (i = 0; i < sectors; i++)
      // {
      //     DBG_MSG_FS("%d ", sectors_idx[i]);
      // }
      // DBG_MSG_FS("\n");
      block_sector_t *iblock = NULL;
      block_sector_t **diblock = NULL;
      block_sector_t *diblock_i = NULL;
      map_sector_to_inode(sectors_idx, &iblock, &diblock, &diblock_i, sectors, disk_inode);
      // printf("%x %x\n", iblock, diblock);
      /* Write the inode to disk */
      DBG_MSG_FS("[FS - %s] writing new direct block\n", thread_name());
      cached_write (sector, disk_inode, 0, BLOCK_SECTOR_SIZE);
      /* Write the indirect block */
      if(iblock != NULL)
      {
        DBG_MSG_FS("[FS - %s] writing new indirect block\n", thread_name());
        // for (i = 0; i < SECTORS_PER_BLOCK; i++)
        // {
        //     DBG_MSG_FS("%d ", iblock[i]);
        // }
        // DBG_MSG_FS("\n");
        cached_write(disk_inode->iblock, iblock, 0, BLOCK_SECTOR_SIZE);
        free(iblock);
      }
      if(diblock != NULL)
      {
        DBG_MSG_FS("[FS - %s] writing new doubly indirect block\n", thread_name());
        cached_write(disk_inode->diblock, diblock_i, 0, BLOCK_SECTOR_SIZE);
        uint32_t i;
        for(i = 0; i < SECTORS_PER_BLOCK; i++)
        {
          if(diblock[i] != NULL)
          {
            DBG_MSG_FS("[FS - %s] writing new doubly indirect entry %d\n", thread_name(), diblock_i[i]);
            cached_write(diblock_i[i], diblock[i], 0, BLOCK_SECTOR_SIZE);
            free(diblock[i]);
          }
        }
        free(diblock);
        free(diblock_i);
      }
      if (sectors > 0) 
        {
          static char zeros[BLOCK_SECTOR_SIZE];
          size_t i;
          
          for (i = 0; i < sectors; i++) 
            cached_write(sectors_idx[i], zeros, 0, BLOCK_SECTOR_SIZE);
        }
      success = true; 
    }
    free(sectors_idx);
    free(disk_inode);
  }
  re:
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  rwlock_init(&inode->rw);
  cached_read (inode->sector, &inode->data, 0, BLOCK_SECTOR_SIZE);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}


/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          sectors_release (&inode->data);        
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
/*
  Read file --> read the inode --> locate the disk block --> o to the disk block

*/
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  rwlock_acquire_read_lock(&inode->rw);
  while (size > 0) 
  {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length (inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    DBG_MSG_FS("[FS - %s] read sector %d size %d\n", thread_name(), sector_idx, size);
    cached_read(sector_idx, buffer + bytes_read, sector_ofs, chunk_size);
    
    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }
  rwlock_release_read_lock(&inode->rw);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;
  rwlock_acquire_write_lock(&inode->rw);
  while (size > 0) 
  {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length (inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    // DBG_MSG_FS("[FS - %s] write to sector %d size %d\n", thread_name(), sector_idx, chunk_size);
    cached_write(sector_idx, buffer + bytes_written, sector_ofs, chunk_size);
    
    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }
  rwlock_release_write_lock(&inode->rw);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
