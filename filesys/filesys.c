#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

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
    buffer_cache_end();
    free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size)
{
    block_sector_t inode_sector = 0;

    char name[NAME_MAX + 1];
    char *dirpath = NULL;
    char *workspace = NULL;
    struct dir *dir = NULL;
    bool success = false;

    if (path == NULL || *path == '\0') {
        goto done;
    }

    dirpath = calloc(1, strlen(path) + 1);
    workspace = calloc(1, strlen(path) + 1);

    if (!dirname(path, workspace, dirpath, name)) {
        goto done;
    }

    dir = dir_navigate(dirpath, workspace, false);

    /* struct dir *dir = dir_open_root (); */
    /* dir_close (dir); */
    /* dir = dir_open_root (); */
    success = (dir != NULL
                    && free_map_allocate (1, &inode_sector)
                    && inode_create (inode_sector, initial_size)
                    && dir_add (dir, name, inode_sector));
    if (!success && inode_sector != 0) 
        free_map_release (inode_sector, 1);

done:
    free(dirpath);
    free(workspace);
    dir_close (dir);

    return success;
}


void *
filesys_openi (const char *path, bool *isdir)
{
    struct inode *inode = NULL;
    char name[NAME_MAX + 1];
    char *dirpath = NULL;
    char *workspace = NULL;
    struct dir *dir = NULL;
    bool success = false;

    if (path == NULL || *path == '\0') {
        goto done;
    }

    dirpath = calloc(1, strlen(path) + 1);
    workspace = calloc(1, strlen(path) + 1);
    dirname(path, workspace, dirpath, name);


    dir = dir_navigate(dirpath, workspace, false);

    if (dir != NULL) {
        if (*name == '\0') {
            inode = dir_get_inode(dir);
            *isdir = true;
            goto done;
        }
        dir_lookup_and_check (dir, name, &inode, isdir);
    }

done:
    free(dirpath);
    free(workspace);
    dir_close (dir);

    if (*isdir) {
        return dir_open (inode);
    }

    return file_open (inode);
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *path)
{
    struct file *file;
    bool isdir = false;
    file = filesys_openi(path, &isdir);
    ASSERT(file == NULL || !isdir);
    return file;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *path)
{
    char name[NAME_MAX + 1];
    char *dirpath = NULL;
    char *workspace = NULL;
    struct dir *dir = NULL;
    bool success = false;

    if (path == NULL || *path == '\0') {
        goto done;
    }

    dirpath = calloc(1, strlen(path) + 1);
    workspace = calloc(1, strlen(path) + 1);
    dirname(path, workspace, dirpath, name);

    dir = dir_navigate(dirpath, workspace, false);

    if (dir == NULL) {
        goto done;
    }
    
    success = dir != NULL && dir_remove (dir, path);

done:
    free(dirpath);
    free(workspace);
    dir_close (dir);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}


bool
filesys_mkdir(const char *path)
{
    block_sector_t inode_sector = 0;
    char target[NAME_MAX + 1];
    char *dirpath = NULL;
    char *workspace = NULL;
    struct dir *dir = NULL;
    struct dir *parent = NULL;
    bool success = false;

    if (path == NULL || *path == '\0') {
        goto done;
    }

    dirpath = calloc(1, strlen(path) + 1);
    workspace = calloc(1, strlen(path) + 1);

    if (!dirname(path, workspace, dirpath, target)) {
        goto done;
    }

    dir = dir_navigate (dirpath, workspace, false);

    struct inode *ignore;

    if (dir == NULL || dir_lookup(dir, target, &ignore)) {
        dir_close(dir);
        goto done;
    }

    if (strcmp(target, ".") == 0 || strcmp(target, "..") == 0) {
        dir_close(dir);
        goto done;
    }

    success = (free_map_allocate (1, &inode_sector)
                    && dir_create(inode_sector, 16)
                    && dir_add_subdir (dir, target, inode_sector));

    if (!success && inode_sector != 0)
        free_map_release (inode_sector, 1);
    dir_close (dir);

    ASSERT(success);

    ASSERT((dir = dir_navigate(path, workspace, false)) != NULL);
    ASSERT((parent = dir_navigate(dirpath, workspace, false)) != NULL);
    ASSERT(dir_add_subdir (dir, ".", dir_get_inumber(dir)));
    ASSERT(dir_add_subdir (dir, "..", dir_get_inumber(parent)));
    dir_close (dir);
done:
    free(dirpath);
    free(workspace);

    return success;
}

