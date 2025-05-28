#ifndef __FS_H__
#define __FS_H__

#include "common.h"
#include "inode.h"

// used for cmd_ls and dirent
typedef struct
{
    uint inum;   // inode number
    uint size;   // size in bytes
    ushort type; // file type: 0: unused, 1: directory, 2: file
    ushort mode; // file mode
    ushort uid;  // user id
    char name[MAXNAME];
} entry; // 32 bytes, 16 entries per block

void sbinit();

void init_sb(int size);        // Initialize superblock with given size
void init_data_bitmap();       // Initialize data block bitmap
void init_inode_bitmap();      // Initialize inode bitmap
void init_root_directory();    // Initialize root directory

int cmd_f(int ncyl, int nsec); // Format the filesystem

uint find_file_in_directory(uint dir_inum, char *filename);           // Find file in directory
int search_directory_block_for_name(uint block_addr, char *filename); // Search for a file name in a directory block
int search_indirect_block_for_name(uint indirect_addr, char *filename);
int search_double_indirect_block_for_name(uint double_indirect_addr, char *filename);
int add_entry_to_directory(uint dir_inum, char *filename, uint file_inum, short file_type); // Add a new entry to a directory

int cmd_mk(char *name, short mode);                                                         // Create a new file
int cmd_mkdir(char *name, short mode);                                                      // Create a new directory
int cmd_rm(char *name);
int cmd_rmdir(char *name);

int cmd_cd(char *name);
int cmd_ls(entry **entries, int *n);

int cmd_cat(char *name, uchar **buf, uint *len);
int cmd_w(char *name, uint len, const char *data);
int cmd_i(char *name, uint pos, uint len, const char *data);
int cmd_d(char *name, uint pos, uint len);

int cmd_login(int auid);

#endif