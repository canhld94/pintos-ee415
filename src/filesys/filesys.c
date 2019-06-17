#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");
  DBG_MSG_FS("[FS - %s] Get file system block\n", thread_name());
  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{

 char new_file[128]; // an copy of dir
 strlcpy(new_file, name, 128);
 struct dir *workdir;
 struct inode *inode;
 bool success = true;
 if(new_file[0] == '/') /* Absolute path, need to go to root directory */
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
   for (token = strtok_r (new_file, "/", &save_ptr); token != NULL;
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
  block_sector_t inode_sector = 0;
  ASSERT(workdir != NULL);
  success =       success && (workdir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, 0)
                  && dir_add (workdir, hier[i], inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (workdir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{

 char new_file[128]; // an copy of dir
 strlcpy(new_file, name, 128);
 struct dir *workdir;
 struct inode *inode;
 bool success = true;
 if(new_file[0] == '/') /* Absolute path, need to go to root directory */
 {
   workdir = dir_open_root();
   if(new_file[1] == 0) return workdir;
 }
 else
 {
   workdir = dir_reopen(thread_current()->cur_dir);
   ASSERT(workdir != NULL);
 }
 /* Tokenize the string */
  int cnt = 0; /* number of argv, at least 1 */
  char *token, *save_ptr;
  char *hier[16]; // we allow maximum 16 level in the hierachy 
   for (token = strtok_r (new_file, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
        {
          // printf("%s\n", token);
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
      return NULL;
    dir_close(workdir);
    workdir = dir_open(inode);
  }
  ASSERT(workdir != NULL);
  struct inode *file_inode = NULL;
  success = success && dir_lookup (workdir, hier[i], &file_inode);
  if(!success) return NULL;
  dir_close (workdir);
  if(file_inode->data.flags)  /* Is directory */
  {
    return dir_open(file_inode);
  }
  else
  {
    return file_open (file_inode);    
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
 char new_file[128]; // an copy of dir
 strlcpy(new_file, name, 128);
 struct dir *workdir;
 struct inode *inode;
 bool success = true;
 if(new_file[0] == '/') /* Absolute path, need to go to root directory */
 {
   workdir = dir_open_root();
   if(new_file[1] == 0)
   {
     dir_close(workdir);
     return false;
   }
 }
 else
 {
   workdir = dir_reopen(thread_current()->cur_dir);
   ASSERT(workdir != NULL);
 }
 /* Tokenize the string */
  int cnt = 0; /* number of argv, at least 1 */
  char *token, *save_ptr;
  char *hier[16]; // we allow maximum 16 level in the hierachy 
   for (token = strtok_r (new_file, "/", &save_ptr); token != NULL;
        token = strtok_r (NULL, "/", &save_ptr))
        {
          // printf("%s\n", token);
          hier[cnt] = token;
          cnt++;
        }
  /* Move to desired working directory */
  int i = 0;
  for(i = 0; i < cnt - 1; i++)
  {
    /* Looking for the desired directory, or create if it's not exist */
    success = dir_lookup(workdir, hier[i], &inode);
    dir_close(workdir);
    if(!success)
      return success;
    workdir = dir_open(inode);
  }
  ASSERT(workdir != NULL);
  success = success && workdir != NULL && dir_remove (workdir, hier[i]);
  dir_close (workdir); 
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!root_dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
