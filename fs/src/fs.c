#include "fs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"
#include "bitmap.h"
#include "user.h"

uint current_dir = 0; // 当前目录的 inode 编号
uint current_uid = 0; // 当前用户的 UID

void sbinit()
{
    uchar buf[BSIZE];
    read_block(0, buf);
    memcpy(&sb, buf, sizeof(sb));
}

// 初始化超级块
void init_sb(int size)
{
    if (size <= 0 || size > MAXBLOCK)
    {
        Error("Invalid disk size: %d blocks", size);
        return;
    }

    uint bmapstart = 1;                           // 位图从第1块开始（第0块是超级块）
    uint bmapblocks = size / BPB + 1;             // 数据块位图块数
    uint inodebmapstart = bmapstart + bmapblocks; // inode位图从数据块位图之后开始

    uint ninodes = size / RATE;               // inode数量约为总块数的1/RATE
    uint inodebmapblocks = ninodes / BPB + 1; // inode位图块数
    uint inodestart = inodebmapstart + inodebmapblocks;

    uint logstart = inodestart + inodebmapblocks; // 日志从inode位图之后开始
    uint nlog = LOGS;                             // 日志块数量
    uint datastart = logstart + nlog;             // 数据块从日志之后开始
    uint ndatablocks = size - datastart;

    // 初始化超级块
    memset(&sb, 0, sizeof(sb));
    sb.magic = MAGIC;
    sb.size = size;
    sb.bmapstart = bmapstart;
    sb.bmapblocks = bmapblocks;
    sb.inodebmapstart = inodebmapstart;
    sb.inodebmapblocks = inodebmapblocks;
    sb.inodestart = inodestart;
    sb.ninodes = ninodes;
    sb.logstart = logstart;
    sb.nlog = nlog;
    sb.datastart = datastart;
    sb.ndatablocks = ndatablocks;

    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(0, buf);
    Log("Superblock initialized successfully");
}

// 初始化目录内容（创建 "." 和 ".." 条目）
int init_directory_entries(uint dir_inum, uint parent_inum, uint data_block, short mode)
{
    entry dir_entries[2];

    // "." 条目（指向自己）
    memset(&dir_entries[0], 0, sizeof(entry));
    dir_entries[0].inum = dir_inum;
    dir_entries[0].type = T_DIR;
    dir_entries[0].size = 2 * sizeof(entry);
    dir_entries[0].mode = mode;
    dir_entries[0].uid = current_uid;
    strcpy(dir_entries[0].name, ".");

    // ".." 条目（指向父目录）
    memset(&dir_entries[1], 0, sizeof(entry));
    dir_entries[1].inum = parent_inum;
    dir_entries[1].type = T_DIR;
    dir_entries[1].size = 0; // 父目录大小由父目录管理
    dir_entries[1].mode = mode;
    dir_entries[1].uid = current_uid;
    strcpy(dir_entries[1].name, "..");

    // 将目录条目写入数据块
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    memcpy(buf, dir_entries, 2 * sizeof(entry));
    write_block(data_block, buf);

    Log("init_directory_entries: initialized directory %d with parent %d", dir_inum, parent_inum);
    return 0;
}

// 初始化根目录
void init_root_directory()
{
    // 创建根目录 inode
    inode *root_ip = ialloc(T_DIR);
    if (root_ip == NULL)
    {
        Error("init_root_directory: failed to allocate root inode");
        return;
    }

    // 强制设置为 inode 0（根目录特殊情况）
    root_ip->type = T_DIR;
    root_ip->inum = 0;
    root_ip->mode = 0755;
    root_ip->nlink = 2;         // 根目录至少有两个链接（"." 和 ".."）
    root_ip->uid = current_uid; // 设置为当前用户 ID
    root_ip->size = 2 * sizeof(entry);
    root_ip->blocks = 1; // 根目录至少有一个数据块
    root_ip->dirty = 1;  // 标记为脏，需要写回磁盘

    // 为根目录分配数据块 - 通过 allocate_block
    uint root_data_block = allocate_block();
    if (root_data_block == 0)
    {
        Error("init_root_directory: failed to allocate data block");
        iput(root_ip);
        return;
    }
    root_ip->addrs[0] = root_data_block;

    iupdate(root_ip);

    // 初始化目录内容
    init_directory_entries(0, 0, root_data_block, 0755);
    current_dir = 0; // 确保每次初始化当前目录为根目录

    iput(root_ip);
    Log("Root directory initialized successfully");
}

int cmd_f(int ncyl, int nsec)
{
    int size = ncyl * nsec;
    init_sb(size);         // 初始化超级块
    init_block_bitmap();   // 初始化数据块位图
    init_inode_system();   // 初始化inode位图和inode区域
    init_root_directory(); // 初始化根目录
    init_user_system();    // 初始化用户系统

    // 初始化日志区域（清零）
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    for (uint i = 0; i < sb.nlog; i++)
    {
        write_block(sb.logstart + i, buf);
    }

    Log("File system formatted successfully");
    Log("Total blocks: %d, Data blocks: %d, Inodes: %d", sb.size, sb.ndatablocks, sb.ninodes);
    return E_SUCCESS;
}

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

int cmd_mk(char *name, short mode)
{
    // 参数检查
    // 检查登录状态
    if (current_uid == 0)
    {
        Error("cmd_mk: please login first");
        return E_ERROR;
    }
    // 检查当前目录写权限
    if (!check_file_permission(current_dir, current_uid, PERM_WRITE))
    {
        Error("cmd_mk: no permission to create file in current directory");
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_mk: invalid filename");
        return E_ERROR;
    }
    if (strlen(name) >= MAXNAME)
    {
        Error("cmd_mk: filename too long (max %d characters)", MAXNAME - 1);
        return E_ERROR;
    }
    Log("cmd_mk: creating file '%s' with mode %o", name, mode);

    // 检查文件是否已存在
    uint existing_inum = find_file_only(current_dir, name);
    if (existing_inum != 0)
    {
        Error("cmd_mk: entry '%s' already exists", name);
        return E_ERROR;
    }

    // 分配新的 inode
    inode *ip = ialloc(T_FILE);
    if (ip == NULL)
    {
        Error("cmd_mk: failed to allocate inode for file '%s'", name);
        return E_ERROR;
    }

    // 设置文件属性
    ip->type = T_FILE;
    ip->mode = mode;
    ip->nlink = 1;
    ip->uid = current_uid;
    ip->size = 0;
    ip->dirty = 1;

    iupdate(ip);

    // 在当前目录中添加文件条目
    int result = add_entry_to_directory(current_dir, name, ip->inum, T_FILE, mode);
    if (result < 0)
    {
        Error("cmd_mk: failed to add file to directory");
        iput(ip);
        return E_ERROR;
    }

    Log("cmd_mk: successfully created file '%s' with inode %d", name, ip->inum);
    iput(ip);
    return E_SUCCESS;
}

int cmd_mkdir(char *name, short mode)
{
    if (current_uid == 0)
    {
        Error("cmd_mkdir: please login first");
        return E_ERROR;
    }
    if (!check_file_permission(current_dir, current_uid, PERM_WRITE))
    {
        Error("cmd_mkdir: no permission to create directory");
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_mkdir: invalid directory name");
        return E_ERROR;
    }
    if (strlen(name) >= MAXNAME)
    {
        Error("cmd_mkdir: directory name too long (max %d characters)", MAXNAME - 1);
        return E_ERROR;
    }
    Log("cmd_mkdir: creating directory '%s' with mode %o", name, mode);

    // 检查目录是否已存在
    uint existing_inum = find_directory_only(current_dir, name);
    if (existing_inum != 0)
    {
        Error("cmd_mkdir: entry '%s' already exists", name);
        return E_ERROR;
    }

    // 分配新的 inode
    inode *ip = ialloc(T_DIR);
    if (ip == NULL)
    {
        Error("cmd_mkdir: failed to allocate inode for directory '%s'", name);
        return E_ERROR;
    }

    // 设置目录属性
    ip->type = T_DIR;
    ip->mode = mode;
    ip->nlink = 2; // "." 和父目录中的条目
    ip->uid = current_uid;
    ip->size = 2 * sizeof(entry); // "." 和 ".." 条目
    ip->dirty = 1;

    // 为新目录分配数据块
    uint dir_data_block = allocate_block();
    if (dir_data_block == 0)
    {
        Error("cmd_mkdir: no free blocks available for directory '%s'", name);
        iput(ip);
        return E_ERROR;
    }
    ip->addrs[0] = dir_data_block;

    iupdate(ip);

    // 初始化目录内容
    if (init_directory_entries(ip->inum, current_dir, dir_data_block, mode) < 0)
    {
        Error("cmd_mkdir: failed to initialize directory entries");
        free_block(dir_data_block);
        iput(ip);
        return E_ERROR;
    }

    // 在当前目录中添加新目录的条目
    int result = add_entry_to_directory(current_dir, name, ip->inum, T_DIR, mode);
    if (result < 0)
    {
        Error("cmd_mkdir: failed to add directory to parent");
        free_block(dir_data_block);
        iput(ip);
        return E_ERROR;
    }

    // 增加父目录的链接数（新增了一个子目录）
    inode *parent_ip = iget(current_dir);
    if (parent_ip != NULL && parent_ip->type == T_DIR)
    {
        parent_ip->nlink++;
        parent_ip->dirty = 1;
        iupdate(parent_ip);
        iput(parent_ip);
    }

    Log("cmd_mkdir: successfully created directory '%s' with inode %d", name, ip->inum);
    iput(ip);
    return E_SUCCESS;
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

int cmd_rm(char *name)
{
    if (current_uid == 0)
    {
        Error("cmd_rm: please login first");
        return E_ERROR;
    }
    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_rm: file '%s' not found", name);
        return E_ERROR;
    }
    // 检查文件写权限（删除需要写权限）
    if (!check_file_permission(file_inum, current_uid, PERM_WRITE))
    {
        Error("cmd_rm: no permission to delete file '%s'", name);
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_rm: invalid filename");
        return E_ERROR;
    }
    Log("cmd_rm: removing file '%s'", name);

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_rm: failed to get file inode %d", file_inum);
        return E_ERROR;
    }

    if (file_ip->type != T_FILE)
    {
        Error("cmd_rm: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 检查文件是否有其他硬链接
    if (file_ip->nlink > 1)
    { // 只是减少链接数，不删除实际数据
        file_ip->nlink--;
        file_ip->dirty = 1;
        iupdate(file_ip);
        Log("cmd_rm: decreased link count for file '%s' to %d", name, file_ip->nlink);
    }
    else // 最后一个链接，删除文件数据
    {
        Log("cmd_rm: freeing file data for '%s'", name);

        // 释放文件的所有数据块
        free_file_blocks(file_ip);
        // 标记 inode 为未使用
        file_ip->type = T_UNUSED;
        file_ip->dirty = 1;
        iupdate(file_ip);

        // 释放 inode
        free_inode_in_bitmap(file_inum);
    }

    iput(file_ip);

    // 从父目录中删除条目
    int result = remove_entry_from_directory(current_dir, name);
    if (result < 0)
    {
        Error("cmd_rm: failed to remove file from directory");
        return E_ERROR;
    }
    Log("cmd_rm: successfully removed file '%s'", name);
    return E_SUCCESS;
}

int cmd_rmdir(char *name)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_rmdir: invalid directory name");
        return E_ERROR;
    }
    // 不允许删除特殊目录
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        Error("cmd_rmdir: cannot remove '.' or '..' directory");
        return E_ERROR;
    }
    Log("cmd_rmdir: removing directory '%s'", name);

    // 在当前目录中查找目录
    uint dir_inum = find_entry_in_directory(current_dir, name, T_DIR);
    if (dir_inum == 0)
    {
        Error("cmd_rmdir: directory '%s' not found", name);
        return E_ERROR;
    }

    // 不允许删除根目录
    if (dir_inum == 0)
    {
        Error("cmd_rmdir: cannot remove root directory");
        return E_ERROR;
    }

    // 检查目录是否为空
    int empty_result = is_directory_empty(dir_inum);
    if (empty_result < 0)
    {
        Error("cmd_rmdir: failed to check if directory is empty");
        return E_ERROR;
    }
    if (empty_result == 0)
    {
        Error("cmd_rmdir: directory '%s' is not empty", name);
        return E_ERROR;
    }

    // 获取目录 inode
    inode *dir_ip = iget(dir_inum);
    if (dir_ip == NULL)
    {
        Error("cmd_rmdir: failed to get directory inode %d", dir_inum);
        return E_ERROR;
    }
    if (dir_ip->type != T_DIR)
    {
        Error("cmd_rmdir: '%s' is not a directory", name);
        iput(dir_ip);
        return E_ERROR;
    }
    Log("cmd_rmdir: freeing directory data for '%s'", name);

    // 释放目录的所有数据块
    free_file_blocks(dir_ip);

    // 标记 inode 为未使用
    dir_ip->type = T_UNUSED;
    dir_ip->dirty = 1;
    iupdate(dir_ip);

    // 释放 inode
    free_inode_blocks(dir_ip);
    iput(dir_ip);

    // 从父目录中删除条目
    int result = remove_entry_from_directory(current_dir, name);
    if (result < 0)
    {
        Error("cmd_rmdir: failed to remove directory from parent");
        return E_ERROR;
    }

    // 更新父目录的链接数（因为删除了一个子目录）
    inode *parent_ip = iget(current_dir);
    if (parent_ip != NULL && parent_ip->type == T_DIR)
    {
        if (parent_ip->nlink > 0)
        {
            parent_ip->nlink--;
            parent_ip->dirty = 1;
            iupdate(parent_ip);
        }
        iput(parent_ip);
    }

    Log("cmd_rmdir: successfully removed directory '%s'", name);
    return E_SUCCESS;
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

int cmd_cd(char *path)
{
    if (path == NULL)
    {
        Error("cmd_cd: invalid path");
        return E_ERROR;
    }

    uint target_inum;
    if (path[0] == '/') // 绝对路径：从根目录开始解析
    {
        if (strcmp(path, "/") == 0)
        {
            target_inum = 0;           // 根目录
            current_dir = target_inum; // 更新当前目录为根目录
            return E_SUCCESS;
        }
        else
        {
            target_inum = resolve_absolute_path(path);
        }
    }
    else // 相对路径：从当前目录开始解析
    {
        if (strcmp(path, "..") == 0)
        {
            target_inum = find_entry_in_directory(current_dir, "..", T_DIR);
        }
        else
        {
            target_inum = find_entry_in_directory(current_dir, path, T_DIR);
            if (target_inum == 0)
            {
                Error("cmd_cd: directory '%s' not found in current directory", path);
                return E_ERROR;
            }
        }
    }

    // 验证目标是目录
    inode *target_ip = iget(target_inum);
    if (target_ip == NULL || target_ip->type != T_DIR)
    {
        Error("cmd_cd: '%s' is not a directory", path);
        if (target_ip)
            iput(target_ip);
        return E_ERROR;
    }
    iput(target_ip);

    // 更新当前目录
    uint old_dir = current_dir;
    current_dir = target_inum;

    Log("cmd_cd: changed directory from %d to %d (%s)", old_dir, target_inum, path);
    return E_SUCCESS;
}
int cmd_ls(entry **entries, int *n)
{
    if (entries == NULL || n == NULL)
    {
        Error("cmd_ls: invalid output parameters");
        return E_ERROR;
    }

    Log("cmd_ls: listing directory %d", current_dir);

    // 获取当前目录 inode
    inode *dir_ip = iget(current_dir);
    if (dir_ip == NULL)
    {
        Error("cmd_ls: failed to get current directory inode %d", current_dir);
        return E_ERROR;
    }
    if (dir_ip->type != T_DIR)
    {
        Error("cmd_ls: current inode %d is not a directory", current_dir);
        iput(dir_ip);
        return E_ERROR;
    }

    // 检查目录是否为空
    if (dir_ip->size == 0)
    {
        Log("cmd_ls: directory is empty");
        *entries = NULL;
        *n = 0;
        iput(dir_ip);
        return E_SUCCESS; // 空目录返回成功
    }

    iput(dir_ip); // 释放 dir_ip

    // 使用较大的临时数组存储所有可能的条目
    uint max_possible_entries = MAXFILEB * (BSIZE / sizeof(entry));
    entry *temp_entries = malloc(max_possible_entries * sizeof(entry));
    if (temp_entries == NULL)
    {
        Error("cmd_ls: failed to allocate memory for temp entries");
        return E_ERROR;
    }

    uint valid_count = 0;
    // 收集目录中的所有条目
    int result = collect_directory_entries(current_dir, temp_entries, max_possible_entries, &valid_count);
    if (result < 0)
    {
        Error("cmd_ls: failed to collect directory entries");
        free(temp_entries);
        return E_ERROR;
    }

    // 过滤掉 "." 和 ".." 条目（如果测试期望不显示它们）
    uint filtered_count = 0;
    entry *filtered_entries = malloc(valid_count * sizeof(entry));
    if (filtered_entries == NULL && valid_count > 0)
    {
        Error("cmd_ls: failed to allocate filtered entries array");
        free(temp_entries);
        return E_ERROR;
    }

    for (uint i = 0; i < valid_count; i++)
    {
        // 跳过 "." 和 ".." 条目（根据测试需求）
        if (strcmp(temp_entries[i].name, ".") != 0 &&
            strcmp(temp_entries[i].name, "..") != 0)
        {
            if (filtered_entries != NULL)
            {
                filtered_entries[filtered_count] = temp_entries[i];
            }
            filtered_count++;
        }
    }

    free(temp_entries);

    // 根据过滤后的条目数分配最终结果数组
    if (filtered_count > 0)
    {
        entry *result_entries = malloc(filtered_count * sizeof(entry));
        if (result_entries == NULL)
        {
            Error("cmd_ls: failed to allocate result array");
            if (filtered_entries)
                free(filtered_entries);
            return E_ERROR;
        }
        memcpy(result_entries, filtered_entries, filtered_count * sizeof(entry));
        *entries = result_entries;
    }
    else
    {
        *entries = NULL;
    }

    if (filtered_entries)
    {
        free(filtered_entries);
    }
    *n = (int)filtered_count;

    Log("cmd_ls: found %d entries in directory %d", filtered_count, current_dir);
    return E_SUCCESS;
}

int cmd_cat(char *name, uchar **buf, uint *len)
{
    if (current_uid == 0)
    {
        Error("cmd_cat: please login first");
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0 || buf == NULL || len == NULL)
    {
        Error("cmd_cat: invalid parameters");
        return E_ERROR;
    }
    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_cat: file '%s' not found", name);
        return E_ERROR;
    }
    // 检查读权限
    if (!check_file_permission(file_inum, current_uid, PERM_READ))
    {
        Error("cmd_cat: no permission to read file '%s'", name);
        return E_ERROR;
    }
    Log("cmd_cat: reading file '%s'", name);

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_cat: failed to get file inode %d", file_inum);
        return E_ERROR;
    }
    if (file_ip->type != T_FILE)
    {
        Error("cmd_cat: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 如果文件为空
    if (file_ip->size == 0)
    {
        Log("cmd_cat: file '%s' is empty", name);
        *buf = NULL;
        *len = 0;
        iput(file_ip);
        return E_SUCCESS;
    }

    // 分配缓冲区读取文件内容
    uchar *file_buf = malloc(file_ip->size);
    if (file_buf == NULL)
    {
        Error("cmd_cat: failed to allocate buffer for file content (size: %d)", file_ip->size);
        iput(file_ip);
        return E_ERROR;
    }

    // 读取文件内容
    int bytes_read = readi(file_ip, file_buf, 0, file_ip->size);
    if (bytes_read != file_ip->size)
    {
        Error("cmd_cat: failed to read file content (expected %d, got %d)", file_ip->size, bytes_read);
        free(file_buf);
        iput(file_ip);
        return E_ERROR;
    }

    *buf = file_buf;
    *len = file_ip->size;

    int file_size = file_ip->size;
    iput(file_ip);
    Log("cmd_cat: successfully read %d bytes from file '%s'", file_size, name);
    return E_SUCCESS;
}

int cmd_w(char *name, uint len, const char *data)
{
    if (current_uid == 0)
    {
        Error("cmd_w: please login first");
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_w: invalid filename");
        return E_ERROR;
    }
    if (len > 0 && data == NULL)
    {
        Error("cmd_w: invalid data pointer");
        return E_ERROR;
    }

    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_w: file '%s' not found", name);
        return E_ERROR;
    }
    // 检查写权限
    if (!check_file_permission(file_inum, current_uid, PERM_WRITE))
    {
        Error("cmd_w: no permission to write file '%s'", name);
        return E_ERROR;
    }
    Log("cmd_w: writing %d bytes to file '%s'", len, name);

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_w: failed to get file inode %d", file_inum);
        return E_ERROR;
    }

    if (file_ip->type != T_FILE)
    {
        Error("cmd_w: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 清空文件内容（从偏移量0开始写入）
    file_ip->size = 0;
    file_ip->dirty = 1;

    // 如果有数据要写入
    if (len > 0 && data != NULL)
    {
        int bytes_written = writei(file_ip, (uchar *)data, 0, len);
        if (bytes_written != len)
        {
            Error("cmd_w: failed to write data (expected %d, wrote %d)", len, bytes_written);
            iput(file_ip);
            return E_ERROR;
        }
    }

    iput(file_ip);
    Log("cmd_w: successfully wrote %d bytes to file '%s'", len, name);
    return E_SUCCESS;
}

int cmd_i(char *name, uint pos, uint len, const char *data)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_i: invalid filename");
        return E_ERROR;
    }
    if (len > 0 && data == NULL)
    {
        Error("cmd_i: invalid data pointer");
        return E_ERROR;
    }
    Log("cmd_i: inserting %d bytes to file '%s' at position %d", len, name, pos);

    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_i: file '%s' not found", name);
        return E_ERROR;
    }

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_i: failed to get file inode %d", file_inum);
        return E_ERROR;
    }
    if (file_ip->type != T_FILE)
    {
        Error("cmd_i: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 检查插入位置是否有效
    if (pos > file_ip->size)
    {
        Error("cmd_i: insert position %d exceeds file size %d", pos, file_ip->size);
        iput(file_ip);
        return E_ERROR;
    }
    if (len == 0) // 如果没有数据要插入
    {
        Log("cmd_i: no data to insert");
        iput(file_ip);
        return E_SUCCESS;
    }

    // 读取原文件内容
    uchar *original_data = NULL;
    uint original_size = file_ip->size;
    if (original_size > 0)
    {
        original_data = malloc(original_size);

        int bytes_read = readi(file_ip, original_data, 0, original_size);
        if (bytes_read != original_size)
        {
            Error("cmd_i: failed to read original file content");
            free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }
    // 清空文件
    file_ip->size = 0;
    file_ip->dirty = 1;

    // 写入插入位置之前的数据
    if (pos > 0)
    {
        int bytes_written = writei(file_ip, original_data, 0, pos);
        if (bytes_written != pos)
        {
            Error("cmd_i: failed to write pre-insert data");
            if (original_data)
                free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    // 写入新数据
    int bytes_written = writei(file_ip, (uchar *)data, pos, len);
    if (bytes_written != len)
    {
        Error("cmd_i: failed to write insert data");
        if (original_data)
            free(original_data);
        iput(file_ip);
        return E_ERROR;
    }

    // 写入插入位置之后的数据
    if (pos < original_size)
    {
        bytes_written = writei(file_ip, original_data + pos, pos + len, original_size - pos);
        if (bytes_written != (original_size - pos))
        {
            Error("cmd_i: failed to write post-insert data");
            if (original_data)
                free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    if (original_data)
        free(original_data);
    iput(file_ip);
    Log("cmd_i: successfully inserted %d bytes to file '%s' at position %d", len, name, pos);
    return E_SUCCESS;
}

int cmd_d(char *name, uint pos, uint len)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_d: invalid filename");
        return E_ERROR;
    }
    Log("cmd_d: deleting %d bytes from file '%s' at position %d", len, name, pos);

    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_d: file '%s' not found", name);
        return E_ERROR;
    }

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_d: failed to get file inode %d", file_inum);
        return E_ERROR;
    }
    if (file_ip->type != T_FILE)
    {
        Error("cmd_d: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 检查删除位置和长度是否有效
    if (pos >= file_ip->size)
    {
        Error("cmd_d: delete position %d exceeds file size %d", pos, file_ip->size);
        iput(file_ip);
        return E_ERROR;
    }
    // 如果没有数据要删除
    if (len == 0)
    {
        Log("cmd_d: no data to delete");
        iput(file_ip);
        return E_SUCCESS;
    }

    // 调整删除长度，不能超过文件剩余长度
    uint actual_delete_len = len;
    if (pos + len > file_ip->size)
    {
        actual_delete_len = file_ip->size - pos;
        Log("cmd_d: adjusted delete length to %d", actual_delete_len);
    }

    // 读取原文件内容
    uint original_size = file_ip->size;
    uchar *original_data = malloc(original_size);
    if (original_data == NULL)
    {
        Error("cmd_d: failed to allocate buffer for original data");
        iput(file_ip);
        return E_ERROR;
    }

    int bytes_read = readi(file_ip, original_data, 0, original_size);
    if (bytes_read != original_size)
    {
        Error("cmd_d: failed to read original file content");
        free(original_data);
        iput(file_ip);
        return E_ERROR;
    }

    // 清空文件
    file_ip->size = 0;
    file_ip->dirty = 1;

    // 写入删除位置之前的数据
    if (pos > 0)
    {
        int bytes_written = writei(file_ip, original_data, 0, pos);
        if (bytes_written != pos)
        {
            Error("cmd_d: failed to write pre-delete data");
            free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    // 写入删除位置之后的数据
    uint remaining_start = pos + actual_delete_len;
    if (remaining_start < original_size)
    {
        uint remaining_len = original_size - remaining_start;
        int bytes_written = writei(file_ip, original_data + remaining_start, pos, remaining_len);
        if (bytes_written != remaining_len)
        {
            Error("cmd_d: failed to write post-delete data");
            free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    free(original_data);
    iput(file_ip);
    Log("cmd_d: successfully deleted %d bytes from file '%s' at position %d", actual_delete_len, name, pos);
    return E_SUCCESS;
}

int cmd_login(int auid)
{
    if (auid < 1 || auid >= MAX_USERS)
    {
        Error("cmd_login: invalid user ID %d", auid);
        return E_ERROR;
    }
    if (!user_exists(auid))
    {
        Error("cmd_login: user %d does not exist", auid);
        return E_ERROR;
    }

    current_uid = auid;
    current_dir = 0; // 登录后默认进入根目录
    Log("cmd_login: logged in as user %d", auid);
    return E_SUCCESS;
}

int cmd_adduser(int uid)
{
    // 检查当前用户是否为管理员
    if (!is_admin_user(current_uid))
    {
        Error("cmd_adduser: only admin can add users");
        return E_ERROR;
    }

    // 检查用户ID有效性
    if (uid <= 0 || uid >= MAX_USERS)
    {
        Error("cmd_adduser: invalid user ID %d", uid);
        return E_ERROR;
    }

    // 检查用户是否已存在
    if (user_exists(uid))
    {
        Error("cmd_adduser: user %d already exists", uid);
        return E_ERROR;
    }

    // 创建用户
    if (create_user(uid) != 0)
    {
        Error("cmd_adduser: failed to create user %d", uid);
        return E_ERROR;
    }

    Log("cmd_adduser: successfully created user %d", uid);
    return E_SUCCESS;
}