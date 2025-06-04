#include "fs_internal.h"
#include <string.h>
#include <stdlib.h>
#include "block.h"
#include "log.h"
#include "bitmap.h"
#include "user.h"

// 在单个目录数据块中搜索/收集条目
uint search_directory_block(uint block_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count)
{
    uchar buf[BSIZE];
    read_block(block_addr, buf);

    uint offset = 0;
    while (offset + sizeof(entry) <= BSIZE)
    {
        entry *current_entry = (entry *)(buf + offset);
        // 检查条目是否有效
        if (current_entry->inum != 0)
        {
            if (name != NULL) // 查找模式（name != NULL）
            {
                if (strcmp(current_entry->name, name) == 0)
                { // 检查类型匹配（如果指定了类型）
                    if (entry_type == -1 || current_entry->type == entry_type)
                    {
                        Log("search_directory_block: found '%s' (inode %d, type %d)", name, current_entry->inum, current_entry->type);
                        return current_entry->inum;
                    }
                }
            }
            // 收集模式（entries_array != NULL）
            else if (entries_array != NULL && current_count != NULL && *current_count < max_entries)
            {
                memcpy(&entries_array[*current_count], current_entry, sizeof(entry));
                (*current_count)++;
                Log("search_directory_block: collected entry '%s' (inode %d, type %d)", current_entry->name, current_entry->inum, current_entry->type);
            }
        }
        offset += sizeof(entry);
    }
    return 0; // 查找模式下未找到
}

// 在一级间接块中搜索/收集条目
uint search_indirect_block(uint indirect_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count)
{
    if (indirect_addr == 0)
    {
        return 0;
    }

    uchar buf[BSIZE];
    read_block(indirect_addr, buf);
    uint *block_addrs = (uint *)buf;

    for (int i = 0; i < APB; i++)
    {
        if (block_addrs[i] != 0)
        {
            if (name != NULL) // 查找模式
            {
                uint found_inum = search_directory_block(block_addrs[i], name, entry_type, NULL, 0, NULL);
                if (found_inum != 0)
                {
                    return found_inum;
                }
            }
            // 收集模式
            else if (entries_array != NULL && current_count != NULL && *current_count < max_entries)
            {
                search_directory_block(block_addrs[i], NULL, -1, entries_array, max_entries, current_count);
            }
        }
    }
    return 0;
}

// 在二级间接块中搜索/收集条目
uint search_double_indirect_block(uint double_indirect_addr, char *name, short entry_type, entry *entries_array, uint max_entries, uint *current_count)
{
    if (double_indirect_addr == 0)
    {
        return 0;
    }

    uchar buf[BSIZE];
    read_block(double_indirect_addr, buf);
    uint *level1_addrs = (uint *)buf;
    for (int i = 0; i < APB; i++)
    {
        if (level1_addrs[i] != 0)
        {
            if (name != NULL) // 查找模式
            {
                uint found_inum = search_indirect_block(level1_addrs[i], name, entry_type, NULL, 0, NULL);
                if (found_inum != 0)
                {
                    return found_inum;
                }
            }
            // 收集模式
            else if (entries_array != NULL && current_count != NULL && *current_count < max_entries)
            {
                search_indirect_block(level1_addrs[i], NULL, -1, entries_array, max_entries, current_count);
            }
        }
    }
    return 0;
}

// 兼容性函数：查找任意类型的文件/目录
uint find_file_in_directory(uint dir_inum, char *filename)
{
    return find_entry_in_directory(dir_inum, filename, -1); // -1 表示任意类型
}

// 便捷函数：只查找文件
uint find_file_only(uint dir_inum, char *filename)
{
    return find_entry_in_directory(dir_inum, filename, T_FILE);
}

// 便捷函数：只查找目录
uint find_directory_only(uint dir_inum, char *dirname)
{
    return find_entry_in_directory(dir_inum, dirname, T_DIR);
}

// 释放文件的所有数据块
void free_file_blocks(inode *ip)
{
    if (ip == NULL)
    {
        return;
    }
    Log("free_file_blocks: freeing blocks for inode %d", ip->inum);

    free_inode_blocks(ip);

    ip->size = 0;
    ip->blocks = 0;
    ip->dirty = 1;
    iupdate(ip);
}

// 解析绝对路径，返回目标目录的 inode 号
uint resolve_absolute_path(char *path)
{
    if (path == NULL || path[0] != '/')
    {
        Error("resolve_absolute_path: invalid absolute path");
        return 0;
    }

    path++; // 跳过开头的 '/'

    uint current_inum = 0; // 从根目录开始
    char *path_copy = malloc(strlen(path) + 1);
    strcpy(path_copy, path);

    // 使用 strtok 分割路径
    char *token = strtok(path_copy, "/");

    while (token != NULL)
    {
        Log("resolve_absolute_path: looking for '%s' in directory %d", token, current_inum);

        // 在当前目录中查找该组件
        uint found_inum = find_entry_in_directory(current_inum, token, T_DIR);
        if (found_inum == 0)
        {
            Error("resolve_absolute_path: directory '%s' not found in path", token);
            free(path_copy);
            return 0;
        }

        // 验证找到的是目录
        inode *ip = iget(found_inum);
        if (ip == NULL || ip->type != T_DIR)
        {
            Error("resolve_absolute_path: '%s' is not a directory", token);
            if (ip)
                iput(ip);
            free(path_copy);
            return 0;
        }
        iput(ip);

        current_inum = found_inum;
        token = strtok(NULL, "/");
    }

    free(path_copy);
    Log("resolve_absolute_path: resolved to inode %d", current_inum);
    return current_inum;
}
