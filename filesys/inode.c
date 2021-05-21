#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "off_t.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t pointers[125];             /* Pointers to another blocks. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  };

/* @colorados */
static block_sector_t
index_sector(struct inode_disk *idisk, int index, bool create, bool clear)
{
    ASSERT(idisk != NULL);
    ASSERT(!(create && clear)); // create NAND clear

    bool success;
    static char zeros[BLOCK_SECTOR_SIZE];
    static bool initzeros = false;

    if (create && !initzeros) {
        memset(zeros, 0, BLOCK_SECTOR_SIZE);
    }

    initzeros = true;

    block_sector_t sector;
    block_sector_t result;
    block_sector_t saux;
    block_sector_t ssaux;
    int iaux;
    int *idaux;

    if (index == 0) {
        if (clear) {
            idisk->start = 0;
        }

        if (idisk->start == 0 && create) {
            success = free_map_allocate(1, &sector);
            if (!success) {
                return -1;
            }
            buffer_cache_write(sector, zeros);
            idisk->start = sector;
        } else if (idisk->start == 0) {
            return -1;
        }

        return idisk->start;
    } else if (--index < 100) {
        if (clear) {
            idisk->pointers[index] = 0;
        }

        if (idisk->pointers[index] == 0 && create) {
            success = free_map_allocate(1, &sector);
            if (!success) {
                return -1;
            }

            buffer_cache_write(sector, zeros);
            idisk->pointers[index] = sector;
        } else if (idisk->pointers[index] == 0) {
            return -1;
        }

        return idisk->pointers[index];
    } else if (index < 3172) {
        iaux = index - 100;

        saux = idisk->pointers[100 + iaux/128];
        if (saux == 0 && create) {
            success = free_map_allocate(1, &sector);
            if (!success) {
                return -1;
            }
            buffer_cache_write(sector, zeros);

            idisk->pointers[100 + iaux/128] = sector;
            saux = sector;
        } else if (saux == 0) {
            return -1;
        }

        idaux = buffer_cache_connect(saux);

        if (clear) {
            idaux[iaux % 128] = 0;
        }

        if (idaux[iaux % 128] == 0 && create) {
            success = free_map_allocate(1, &sector);

            if (!success) {
                return -1;
            }

            buffer_cache_write(sector, zeros);

            idaux[iaux % 128] = sector;
        } else if (idaux[iaux % 128] == 0) {
            buffer_cache_logout(saux);
            return -1;
        }

        result = idaux[iaux % 128];
        buffer_cache_logout(saux);
        ASSERT(result != 0);

        return result;
    } else {
        iaux = index - 3172;
        saux = idisk->pointers[124];
        
        if (saux == 0 && create) {
            success = free_map_allocate(1, &sector);
            if (!success) {
                return -1;
            }
            buffer_cache_write(sector, zeros);

            idisk->pointers[124] = sector;
            saux = sector;
        } else if (saux == 0) {
            return -1;
        }

        idaux = buffer_cache_connect(saux);
        if (index < 3198) {
            if (clear) {
                idaux[iaux] = 0;
            }

            if (idaux[iaux] == 0 && create) {
                success = free_map_allocate(1, &sector);
                if (!success) {
                    buffer_cache_logout(saux);
                    return -1;
                }
                buffer_cache_write(sector, zeros);
                idaux[iaux] = sector;
            } else if (idaux[iaux] == 0) {
                buffer_cache_logout(saux);
                return -1;
            }

            result = idaux[iaux];
            buffer_cache_logout(saux);
            
            return result;
        } else if (index < 16254) {
            iaux = iaux - 26;
            ssaux = idaux[26 + iaux / 128]; // next block

            if (ssaux == 0 && create) {
                success = free_map_allocate(1, &sector);

                if (!success) {
                    buffer_cache_logout(saux);
                    return -1;
                }

                buffer_cache_write(sector, zeros);

                idaux[26 + iaux / 128] = sector;
                ssaux = sector;
            } else if (ssaux == 0) {
                buffer_cache_logout(saux);
                return -1;
            }

            buffer_cache_logout(saux);
            saux = ssaux;

            idaux = buffer_cache_connect(saux);
            
            if (clear) {
                idaux[iaux % 128] = 0;
            }

            if (idaux[iaux % 128] == 0 && create) {
                success = free_map_allocate(1, &sector);
                if (!success) {
                    buffer_cache_logout(saux);
                    return -1;
                }
                buffer_cache_write(sector, zeros);

                idaux[iaux % 128] = sector;
            } else if (idaux[iaux % 128] == 0) {
                buffer_cache_logout(saux);
                return -1;
            }

            result = idaux[iaux % 128];
            buffer_cache_logout(saux);

            return result;
        } else {
            buffer_cache_logout(saux);
            PANIC("[colorados]: maximum index");
        }
    }


    return -1;
}


static void
free_sectors(struct inode_disk *idisk, unsigned index_from)
{
    ASSERT(idisk != NULL);
    ASSERT(bytes_to_sectors(idisk->length) > index_from)

    int count = bytes_to_sectors(idisk->length) - bytes_to_sectors(index_from) - 1;

    count--;
    
    block_sector_t sector;
    block_sector_t saux;
    unsigned index;
    int *idaux;
    while (--count + 1) {
        index = index_from + count;
        sector = index_sector(idisk, index, false, true);
        if (sector != -1) {
            free_map_release(sector, 1);

            if (index < 3172) {
                index = index - 100;
                sector = idisk->pointers[100 + index/128];
                if ( index % 128 == 0 ) {
                    free_map_release(sector, 1);
                    idisk->pointers[100 + index/128] = 0;
                }
            } else {
                index = index - 3172;
                sector = idisk->pointers[124];
                
                if (index == 0) {
                    free_map_release(sector, 1);
                    idisk->pointers[124] = 0;
                    continue;
                }

                idaux = buffer_cache_connect(sector);

                if (index < 13082) {
                    index = index - 26;
                    saux = idaux[26 + index / 128]; // next block

                    if (index % 128 == 0) {
                        free_map_release(saux, 1);
                        idaux[26 + index / 128] = 0;
                        buffer_cache_logout(sector);
                        continue;
                    }
                }

                buffer_cache_logout(sector);
            }
        }
    }
}


static bool
expand_size(struct inode_disk *idisk, off_t pos)
{
    ASSERT(pos >= 0);

    if (pos < idisk->length) {
        return true;
    }

    int count;
    int index = idisk->length / BLOCK_SECTOR_SIZE;
    int sindex;
    block_sector_t sector;
    count = bytes_to_sectors(pos) - index;

    if (idisk->length == 0) {
        index = -1;
    }

    if (count == 0) {
        idisk->length = pos + 1;
        return true;
    }

    // grow content size
    index++;

    sindex = index;

    // TODO fill with zeros new sector
    while (count--) {
        if (index_sector(idisk, index++, true, false) == -1) {
            if (sindex + 1 < index) {
                free_sectors(idisk, sindex);
            }
            return false;
        }
    }
    idisk->length = pos + 1;
    
    return true;
}


block_sector_t
index_to_sector(const struct inode_disk *idisk, unsigned index, block_sector_t *next)
{
    block_sector_t result = -1;
    block_sector_t saux;
    int iaux;
    int *idaux;
    if (idisk->length >= index * BLOCK_SECTOR_SIZE) {
        // now, it's not continuos
        // result = idisk->start + index;

        /* if (index == 0) { */
            /* result = idisk->start; */
        /* } else if (--index < 100) { */
            /* result = idisk->pointers[index]; */
        /* } else if (index < 3172) { */
            /* iaux = index - 100; */
            /* saux = idisk->pointers[100 + iaux/128]; */
            /* idaux = buffer_cache_connect(saux); */
            /* result = idaux[iaux % 128]; */
            /* buffer_cache_logout(saux); */
        /* } else { */
            /* iaux = index - 3172; */
            /* saux = idisk->pointers[124]; */
            /* idaux = buffer_cache_connect(saux); */
            /* if (index < 3198) { */
                /* result = idaux[iaux]; */
                /* buffer_cache_logout(saux); */
            /* } else if (index < 16254) { */
                /* iaux = iaux - 26; */
                /* result = idaux[26 + iaux / 128]; // next block */
                /* buffer_cache_logout(saux); */
                /* saux = result; */
                /* idaux = buffer_cache_connect(saux); */
                /* result = idaux[iaux % 128]; */
                /* buffer_cache_logout(saux); */
            /* } else { */
                /* buffer_cache_logout(saux); */
            /* } */
        /* } */
        result = index_sector((struct inode_disk *) idisk, index, false, false);
    }

    if (next != NULL && result == -1) {
        *next = index_to_sector(idisk, index + 1, NULL);
    } else {
        *next = -1;
    }


    return result;
}
/* @colorados */

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos, block_sector_t *next)
{
    ASSERT (inode != NULL);
    struct inode_disk *idsk = buffer_cache_connect(inode->sector);
    block_sector_t result = -1;

    if (pos < idsk->length) {
        result = index_to_sector(idsk, pos / BLOCK_SECTOR_SIZE, next);
    }

    buffer_cache_logout(inode->sector);
    return result;
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

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
        /* size_t sectors = bytes_to_sectors (length); */
        /* disk_inode->length = length; */
        disk_inode->magic = INODE_MAGIC;

        if (length == 0 || (length > 0 && expand_size(disk_inode, length - 1))) {
            if (length == 0) {
                disk_inode->length = 0;
            }
            buffer_cache_write(sector, disk_inode);
            success = true;
        }

        /* if (free_map_allocate (sectors, &disk_inode->start))  */
            /* { */
            /* buffer_cache_write(sector, disk_inode); */
            /* if (sectors > 0)  */
                /* { */
                /* static char zeros[BLOCK_SECTOR_SIZE]; */
                /* size_t i; */

                /* for (i = 0; i < sectors; i++)  */
                    /* buffer_cache_write(disk_inode->start + i, zeros); */
                /* } */
            /* success = true;  */
            /* } */

        free (disk_inode);
    }
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
          struct inode_disk *idisk = buffer_cache_connect(inode->sector);
          /* free_map_release (idisk->start, */
                            /* bytes_to_sectors (idisk->length)); */
          free_sectors(idisk, 0);
          buffer_cache_logout(inode->sector);
          free_map_release (inode->sector, 1);
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
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  block_sector_t next_idx = -1;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, &next_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          if (next_idx != -1)
          {
            buffer_cache_async_fetch(next_idx);
          }
          buffer_cache_read(sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          if (next_idx != -1)
          {
            buffer_cache_async_fetch(next_idx);
          }
          buffer_cache_read(sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

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
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  struct inode_disk *idisk = buffer_cache_connect(inode->sector);
    
  if (offset + size - 1 > idisk->length) {
      ASSERT(expand_size(idisk, offset + size - 1));
  }

  buffer_cache_logout(inode->sector);

  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t next_idx = -1;
      block_sector_t sector_idx = byte_to_sector (inode, offset, &next_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          buffer_cache_write(sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left)
          {
            if (next_idx != -1)
            {
              buffer_cache_async_fetch(next_idx);
            }
            buffer_cache_read(sector_idx, bounce);
          }
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          /* block_write (fs_device, sector_idx, bounce); */
          buffer_cache_write(sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

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
    struct inode_disk *idisk = buffer_cache_connect(inode->sector);
    off_t length = idisk->length;
    buffer_cache_logout(inode->sector);
    return length;
}
