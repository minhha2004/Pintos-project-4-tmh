#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>

#include "filesys/off_t.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0 /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1 /* Root directory file inode sector. */

/** #Project 4: File System - define types */
#define FILE_TYPE 0
#define DIR_TYPE  1
#define LINK_TYPE 2

/* Disk used for file system. */
extern struct disk *filesys_disk;

void filesys_init(bool format);
void filesys_done(void);
bool filesys_create(const char *name, off_t initial_size);
struct file *filesys_open(const char *name);
bool filesys_remove(const char *name);

#ifdef FILESYS
/** #Project 4: File System */
struct dir *parse_path(char *, char *);
bool filesys_chdir(const char *dir_name);
bool filesys_mkdir(const char *dir_name);
#endif

#endif /* filesys/filesys.h */
