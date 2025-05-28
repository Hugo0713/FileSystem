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

extern uint current_dir; // Current directory inode number
extern uint current_uid; // Current user ID

void sbinit();

void init_sb(int size);        // Initialize superblock with given size
int init_directory_entries(uint dir_inum, uint parent_inum, uint data_block, short mode); // Initialize directory entries
void init_root_directory();    // Initialize root directory

int cmd_f(int ncyl, int nsec); // Format the filesystem

uint search_directory_block(uint block_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count);
uint search_indirect_block(uint indirect_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count);
uint search_double_indirect_block(uint double_indirect_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count);
int collect_directory_entries(uint dir_inum, entry *entries_array, uint max_entries, uint *count);


uint find_entry_in_directory(uint dir_inum, char *name, short entry_type);
uint find_file_in_directory(uint dir_inum, char *filename); // Find a file in a directory
uint find_file_only(uint dir_inum, char *filename); // Find a file only
uint find_directory_only(uint dir_inum, char *dirname); // Find a directory only
int add_entry_to_directory(uint dir_inum, char *filename, uint file_inum, short file_type, short file_mode); // Add an entry to a directory

int cmd_mk(char *name, short mode);                                                         // Create a new file
int cmd_mkdir(char *name, short mode);                                                      // Create a new directory

int remove_entry_from_directory(uint dir_inum, char *filename);
int is_directory_empty(uint dir_inum);
void free_file_blocks(inode *ip);

int cmd_rm(char *name);
int cmd_rmdir(char *name);

uint resolve_absolute_path(char *path);
uint get_parent_directory(uint dir_inum);

int cmd_cd(char *name);
int cmd_ls(entry **entries, int *n);

int cmd_cat(char *name, uchar **buf, uint *len);
int cmd_w(char *name, uint len, const char *data);
int cmd_i(char *name, uint pos, uint len, const char *data);
int cmd_d(char *name, uint pos, uint len);

int cmd_login(int auid);

#endif