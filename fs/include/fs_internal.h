#ifndef FS_INTERNAL_H
#define FS_INTERNAL_H

#include "fs.h"

// 全局变量声明（在 fs.c 中定义，其他模块使用）
extern uint current_dir;
extern uint current_uid;

// 内部函数声明
// fs_format.c
void init_sb(int size);
void init_root_directory();
int init_directory_entries(uint dir_inum, uint parent_inum, uint data_block, short mode);

// fs_directory.c
uint find_entry_in_directory(uint dir_inum, char *name, short entry_type);
int collect_directory_entries(uint dir_inum, entry *entries_array, uint max_entries, uint *count);
int add_entry_to_directory(uint dir_inum, char *filename, uint file_inum, short file_type, short file_mode);
int remove_entry_from_directory(uint dir_inum, char *filename);
int is_directory_empty(uint dir_inum);


// fs_utils.c
void free_file_blocks(inode *ip);
uint resolve_absolute_path(char *path);
// 辅助搜索函数
uint search_directory_block(uint block_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count);
uint search_indirect_block(uint indirect_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count);
uint search_double_indirect_block(uint double_indirect_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count);
// 便捷函数
uint find_file_in_directory(uint dir_inum, char *filename);
uint find_file_only(uint dir_inum, char *filename);
uint find_directory_only(uint dir_inum, char *dirname);

#endif // FS_INTERNAL_H