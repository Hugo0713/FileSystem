#include "fs_internal.h"
#include <string.h>
#include <stdlib.h>
#include "block.h"
#include "log.h"
#include "bitmap.h"
#include "user.h"

// 在目录中查找指定名称的条目
uint find_entry_in_directory(uint dir_inum, char *name, short entry_type)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("find_entry_in_directory: invalid name");
        return 0;
    }

    // 获取目录 inode
    inode *dir_ip = iget(dir_inum);
    if (dir_ip == NULL || dir_ip->type != T_DIR)
    {
        Error("find_entry_in_directory: invalid directory inode %d", dir_inum);
        if (dir_ip)
            iput(dir_ip);
        return 0;
    }

    Log("find_entry_in_directory: searching for '%s' (type %d) in directory %d", name, entry_type, dir_inum);

    uint found_inum = 0;

    // 遍历目录的所有地址块
    for (int addr_index = 0; addr_index < NDIRECT + 2 && found_inum == 0; addr_index++)
    {
        if (addr_index < NDIRECT) // 直接块
        {
            if (dir_ip->addrs[addr_index] != 0)
            {
                found_inum = search_directory_block(dir_ip->addrs[addr_index], name, entry_type, NULL, 0, NULL);
            }
        }
        else if (addr_index == NDIRECT) // 一级间接块
        {
            if (dir_ip->addrs[NDIRECT] != 0)
            {
                found_inum = search_indirect_block(dir_ip->addrs[NDIRECT], name, entry_type, NULL, 0, NULL);
            }
        }
        else // 二级间接块
        {
            if (dir_ip->addrs[NDIRECT + 1] != 0)
            {
                found_inum = search_double_indirect_block(dir_ip->addrs[NDIRECT + 1], name, entry_type, NULL, 0, NULL);
            }
        }
    }

    iput(dir_ip);
    if (found_inum != 0)
    {
        Log("find_entry_in_directory: found '%s' (inode %d) in directory %d", name, found_inum, dir_inum);
    }
    else
    {
        Log("find_entry_in_directory: '%s' not found in directory %d", name, dir_inum);
    }
    return found_inum;
}

// 收集目录中的所有条目
int collect_directory_entries(uint dir_inum, entry *entries_array, uint max_entries, uint *count)
{
    if (entries_array == NULL || count == NULL)
    {
        Error("collect_directory_entries: invalid parameters");
        return -1;
    }

    // 获取目录 inode
    inode *dir_ip = iget(dir_inum);
    if (dir_ip == NULL || dir_ip->type != T_DIR)
    {
        Error("collect_directory_entries: invalid directory inode %d", dir_inum);
        if (dir_ip)
            iput(dir_ip);
        return -1;
    }
    Log("collect_directory_entries: collecting entries from directory %d", dir_inum);

    *count = 0;
    // 遍历目录的所有地址块
    for (int addr_index = 0; addr_index < NDIRECT + 2 && *count < max_entries; addr_index++)
    {
        if (addr_index < NDIRECT) // 直接块
        {
            if (dir_ip->addrs[addr_index] != 0)
            {
                search_directory_block(dir_ip->addrs[addr_index], NULL, -1, entries_array, max_entries, count);
            }
        }
        else if (addr_index == NDIRECT) // 一级间接块
        {
            if (dir_ip->addrs[NDIRECT] != 0)
            {
                search_indirect_block(dir_ip->addrs[NDIRECT], NULL, -1, entries_array, max_entries, count);
            }
        }
        else // 二级间接块
        {
            if (dir_ip->addrs[NDIRECT + 1] != 0)
            {
                search_double_indirect_block(dir_ip->addrs[NDIRECT + 1], NULL, -1, entries_array, max_entries, count);
            }
        }
    }
    iput(dir_ip);
    Log("collect_directory_entries: collected %d entries from directory %d", *count, dir_inum);
    return 0;
}

// 在目录中添加新条目
int add_entry_to_directory(uint dir_inum, char *filename, uint file_inum, short file_type, short file_mode)
{
    // 读取目录 inode
    inode *dir_ip = iget(dir_inum);
    if (dir_ip == NULL || dir_ip->type != T_DIR)
    {
        Error("add_entry_to_directory: invalid directory inode %d", dir_inum);
        return -1;
    }

    // 创建新的目录条目
    entry new_entry;
    memset(&new_entry, 0, sizeof(entry));
    new_entry.inum = file_inum;
    new_entry.type = file_type;
    new_entry.mode = file_mode;
    new_entry.uid = current_uid;
    new_entry.size = 0;
    strncpy(new_entry.name, filename, MAXNAME - 1);
    new_entry.name[MAXNAME - 1] = '\0';

    // 将新条目写入目录末尾
    int bytes_written = writei(dir_ip, (uchar *)&new_entry, dir_ip->size, sizeof(entry));
    if (bytes_written != sizeof(entry))
    {
        Error("add_entry_to_directory: failed to write directory entry");
        iput(dir_ip);
        return -1;
    }

    iput(dir_ip); // iput中已有iupdate机制

    Log("add_entry_to_directory: added '%s' (inode %d) to directory %d",
        filename, file_inum, dir_inum);
    return 0;
}

// 从目录中删除指定条目
int remove_entry_from_directory(uint dir_inum, char *filename)
{
    if (filename == NULL || strlen(filename) == 0)
    {
        Error("remove_entry_from_directory: invalid filename");
        return -1;
    }

    // 收集所有条目
    uint max_entries = MAXFILEB * (BSIZE / sizeof(entry));
    entry *entries = malloc(max_entries * sizeof(entry));

    uint count = 0;
    if (collect_directory_entries(dir_inum, entries, max_entries, &count) < 0)
    {
        Error("remove_entry_from_directory: failed to collect entries");
        free(entries);
        return -1;
    }

    // 查找要删除的条目
    int found_index = -1;
    for (uint i = 0; i < count; i++)
    {
        if (strcmp(entries[i].name, filename) == 0)
        {
            found_index = i;
            break;
        }
    }
    if (found_index == -1)
    {
        Log("remove_entry_from_directory: entry '%s' not found", filename);
        free(entries);
        return -1;
    }

    // 获取目录 inode
    inode *dir_ip = iget(dir_inum);
    if (dir_ip == NULL)
    {
        Error("remove_entry_from_directory: failed to get directory inode");
        free(entries);
        return -1;
    }
    // 清空目录
    free_file_blocks(dir_ip);
    // 重新写入所有条目（除了被删除的）
    for (uint i = 0, write_index = 0; i < count; i++)
    {
        if (i != found_index) // 跳过要删除的条目
        {
            int bytes_written = writei(dir_ip, (uchar *)&entries[i], write_index * sizeof(entry), sizeof(entry));
            if (bytes_written != sizeof(entry))
            {
                Error("remove_entry_from_directory: failed to write entry");
                free(entries);
                iput(dir_ip);
                return -1;
            }
            write_index++;
        }
    }

    free(entries);
    iput(dir_ip);
    Log("remove_entry_from_directory: successfully removed '%s'", filename);
    return 0;
}

// 检查目录是否为空（除了 "." 和 ".." 条目）
int is_directory_empty(uint dir_inum)
{
    // 收集所有条目
    uint max_entries = MAXFILEB * (BSIZE / sizeof(entry));
    entry *entries = malloc(max_entries * sizeof(entry));
    if (entries == NULL)
    {
        Error("is_directory_empty: failed to allocate memory");
        return -1;
    }

    uint count = 0;
    int result = collect_directory_entries(dir_inum, entries, max_entries, &count);
    if (result < 0)
    {
        Error("is_directory_empty: failed to collect directory entries");
        free(entries);
        return -1;
    }

    // 计算除了 "." 和 ".." 之外的条目数量
    uint valid_entries = 0;
    for (uint i = 0; i < count; i++)
    {
        if (strcmp(entries[i].name, ".") != 0 && strcmp(entries[i].name, "..") != 0)
        {
            valid_entries++;
        }
    }
    free(entries);
    return (valid_entries == 0) ? 1 : 0; // 1表示空目录，0表示非空
}
