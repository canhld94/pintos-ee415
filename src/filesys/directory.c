#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"


/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
root_dir_create (block_sector_t sector, size_t entry_cnt)
{
  bool success = inode_create (sector, 0, 1);
  struct dir *rootdir = dir_open(inode_open(sector));
  success = success && dir_add(rootdir, ".", sector);  /* This directory */ 
  success = success && dir_add(rootdir, "..",sector); /* Parrent directory */
  dir_close(rootdir);
  return success;
}

/* 
  reimplementation of dir_create 
  Input: name of the directory
  Output:
    - create new directory in the current working process directory
    - adding new directory's entry to the current process directory data
*/

bool dir_create(const char *name)
{
 char new_dir[128]; // an copy of dir
 strlcpy(new_dir, name, 128);
 struct dir *workdir;
 struct inode *inode;
 bool success = true;
 if(new_dir[0] == '/') /* Absolute path, need to go to root directory */
 {
   workdir = dir_open_root();
 }
 else
 {
   workdir = dir_reopen(thread_current()->cur_dir);
 }
 /* Tokenize the string */
  int cnt = 0; /* number of argv, at least 1 */
  char *token, *save_ptr;
  char *hier[16]; // we allow maximum 16 level in the hierachy 
   for (token = strtok_r (new_dir, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
        {
          hier[cnt] = token;
          cnt++;
        }
  /* Move to desired working directory */
  int i = 0;
  for(i = 0; i < cnt - 1; i++)
  {
    /* Looking for the desired directory, or create if it's not exist */
    success = dir_lookup(workdir, hier[i], &inode);
    if(!success)
      return success;
    dir_close(workdir);
    workdir = dir_open(inode);
  }
  /* Create new dir here */
  /* Found a new inode */
  block_sector_t inode_sector = 0;
  ASSERT(workdir != NULL);
  success = success && (workdir != NULL
                  && free_map_allocate (1, &inode_sector) /* Allocate new sector */
                  && inode_create (inode_sector, 0, 1)    /* Create inode at the sector */
                  && dir_add (workdir, hier[i], inode_sector));    /* Add this directory to the working directory */
  if (!success) 
  {
    if(inode_sector != 0)
      free_map_release (inode_sector, 1);
    dir_close(workdir);
    return success;
  }
  /* Open the newly created dir */
  // printf("%d\n", inode_sector);
  struct dir *newdir = dir_open(inode_open(inode_sector));
  /* Add . and .. to the directory */
  success = success && dir_add(newdir, ".", inode_sector);  /* This directory */ 
  success = success && dir_add(newdir, "..", workdir->inode->sector); /* Parrent directory */
  /* Close work dir */
  dir_close(workdir);
  dir_close(newdir);
  return success;
}

struct dir *open_dir(const char *name)
{
 char new_dir[128]; // an copy of dir
 strlcpy(new_dir, name, 128);
 struct dir *workdir;
 struct inode *inode;
 bool success = true;
 if(new_dir[0] == '/') /* Absolute path, need to go to root directory */
 {
   workdir = dir_open_root();
 }
 else
 {
   workdir = dir_reopen(thread_current()->cur_dir);
 }
 /* Tokenize the string */
  int cnt = 0; /* number of argv, at least 1 */
  char *token, *save_ptr;
  char *hier[16]; // we allow maximum 16 level in the hierachy 
   for (token = strtok_r (new_dir, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
        {
          hier[cnt] = token;
          cnt++;
        }
  /* Move to desired working directory */
  int i = 0;
  for(i = 0; i < cnt; i++)
  {
    /* Looking for the desired directory, or create if it's not exist */
    success = dir_lookup(workdir, hier[i], &inode);
    dir_close(workdir);
    if(!success)
    {
      return NULL;
    }
    workdir = dir_open(inode);
  }
  // ASSERT(workdir != NULL);
  return workdir;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      ASSERT(inode->data.flags);
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  ASSERT(dir != NULL);
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

static bool
dir_isempty(const struct dir *dir) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  // printf("%d\n", dir->inode->sector);
  for (ofs = 40; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e) 
  {
    if (e.in_use) 
    {
      // printf("%s\n", e.name);
      return false;
    }
  }
  return true;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  // printf("%d\n", dir->inode->sector);
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
  {
    if (e.in_use && !strcmp (name, e.name)) 
    {
      if (ep != NULL)
        *ep = e;
      if (ofsp != NULL)
        *ofsp = ofs;
      return true;
    }
  }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);
    // printf("%d\n", dir->inode->sector);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;
  // printf("%d %d\n", dir->inode->sector, (*inode)->sector);
  return *inode != NULL;
}

/*
  Search dir for an entry with name, if the entry exist return
  If not then create the entry
*/

// bool 
// dir_lookup_and_create(struct dir *dir, const char *name, struct inode **inode)
// {
//   bool success = dir_lookup(dir, name, inode);
//   if(!success) /* Need to create new dir */
//   {
//     success = dir_create(dir, name);
//     success = success && dir_lookup(dir, name, inode);
//   } 
//   return success;
// }
/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; ofs <= inode_length(dir->inode); ofs += sizeof e) 
  {
    // printf("%d: %d full\n", inode_sector, ofs);
    inode_read_at (dir->inode, &e, sizeof e, ofs);
    if (!e.in_use)
      break;
  }

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;
  if(e.inode_sector == ROOT_DIR_SECTOR 
    || e.inode_sector == thread_current()->cur_dir->inode->sector)
    goto done;
  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL || (inode->data.flags && inode->open_cnt > 1))
    goto done;
  /* Check if it's dir and dir entry > 0 */

  if(inode->data.flags)
  {
    struct dir *thisdir = dir_open(inode);
    if(!dir_isempty(thisdir))
    {
      dir_close(thisdir);
      return success;
    }
  }
  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  // printf("%d\n", inode->sector);
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
  if(dir->pos < 40) dir->pos = 40;
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          // printf("%s\n", e.name);
          return true;
        } 
    }
  return false;
}
