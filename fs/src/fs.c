#include "fs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"
#include "bitmap.h"

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

// 初始化数据块位图
void init_data_bitmap()
{
    if (bitmap_clear_all(BITMAP_BLOCK) < 0)
    {
        Error("init_data_bitmap: failed to clear data bitmap");
        return;
    }

    if (bitmap_set_system_blocks_used() < 0)
    {
        Error("init_data_bitmap: failed to mark system blocks");
        return;
    }

    Log("Data bitmap initialized successfully using bitmap functions");
}

// 初始化 inode 位图和 inode 区域
void init_inode_bitmap()
{
    if (bitmap_clear_all(BITMAP_INODE) < 0)
    {
        Error("init_inode_bitmap: failed to clear inode bitmap");
        return;
    }

    // 初始化所有 inode 为未使用状态
    dinode empty_inode;
    memset(&empty_inode, 0, sizeof(empty_inode));
    empty_inode.type = T_UNUSED;

    // 创建包含空 inode 的缓冲区
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    for (int i = 0; i < BSIZE / sizeof(dinode); i++)
    {
        memcpy(buf + i * sizeof(dinode), &empty_inode, sizeof(dinode));
    }

    // 计算需要多少个块来存储所有 inode
    uint inodes_per_block = BSIZE / sizeof(dinode);
    uint inode_blocks = (sb.ninodes + inodes_per_block - 1) / inodes_per_block;

    // 写入所有 inode 块
    for (uint i = 0; i < inode_blocks; i++)
    {
        write_block(sb.inodestart + i, buf);
    }
    Log("Inode bitmap and inode area initialized successfully using bitmap functions");
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
    uchar buf[BSIZE];
    // 创建根目录（inode 0）
    dinode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.type = T_DIR;
    root_inode.mode = 0755;
    root_inode.nlink = 2;                // 根目录至少有两个链接（. 和 ..）
    root_inode.uid = 0;                  // 假设 UID 为 0（root 用户）
    root_inode.size = 2 * sizeof(entry); // 根目录包含两个条目（. 和 ..）
    root_inode.dirty = 1;

    // 为根目录分配一个数据块
    uint root_data_block = block_bitmap_find_free();
    if (root_data_block == 0)
    {
        Error("init_root_directory: no free blocks available for root directory");
        return;
    }
    root_inode.addrs[0] = root_data_block;

    // 将根目录 inode 写入磁盘
    read_block(sb.inodestart, buf);
    memcpy(buf, &root_inode, sizeof(dinode));
    write_block(sb.inodestart, buf);

    // 使用位图函数标记根目录 inode 为已使用
    if (inode_bitmap_set_used(0) < 0)
    {
        Error("init_root_directory: failed to mark root inode as used");
        return;
    }

    // 使用位图函数标记根目录数据块为已使用
    if (block_bitmap_set_used(root_data_block) < 0)
    {
        Error("init_root_directory: failed to mark root data block as used");
        return;
    }

    // 初始化根目录内容（根目录的父目录是自己）
    if (init_directory_entries(0, 0, root_data_block, 0755) < 0)
    {
        Error("init_root_directory: failed to initialize directory entries");
        return;
    }
    Log("Root directory initialized successfully using bitmap functions");
}

int cmd_f(int ncyl, int nsec)
{
    int size = ncyl * nsec;
    init_sb(size);         // 初始化超级块
    init_data_bitmap();    // 初始化数据块位图
    init_inode_bitmap();   // 初始化inode位图和inode区域
    init_root_directory(); // 初始化根目录

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

// 在目录中查找指定类型的文件/目录
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
                found_inum = search_directory_block(dir_ip->addrs[addr_index], name, entry_type);
            }
        }
        else if (addr_index == NDIRECT) // 一级间接块
        {
            if (dir_ip->addrs[NDIRECT] != 0)
            {
                found_inum = search_indirect_block(dir_ip->addrs[NDIRECT], name, entry_type);
            }
        }
        else // 二级间接块
        {
            if (dir_ip->addrs[NDIRECT + 1] != 0)
            {
                found_inum = search_double_indirect_block(dir_ip->addrs[NDIRECT + 1], name, entry_type);
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

// 在单个目录数据块中搜索指定条目
uint search_directory_block(uint block_addr, char *name, short entry_type)
{
    uchar buf[BSIZE];
    read_block(block_addr, buf);

    uint offset = 0;
    while (offset + sizeof(entry) <= BSIZE)
    {
        entry *current_entry = (entry *)(buf + offset);
        // 检查条目是否有效且名字匹配
        if (current_entry->inum != 0 && strcmp(current_entry->name, name) == 0)
        {
            // 检查类型匹配（如果指定了类型）
            if (entry_type == -1 || current_entry->type == entry_type)
            {
                Log("search_directory_block: found '%s' (inode %d, type %d)", name, current_entry->inum, current_entry->type);
                return current_entry->inum;
            }
        }
        offset += sizeof(entry);
    }
    return 0;
}

// 在一级间接块中搜索指定条目
uint search_indirect_block(uint indirect_addr, char *name, short entry_type)
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
            uint found_inum = search_directory_block(block_addrs[i], name, entry_type);
            if (found_inum != 0)
            {
                return found_inum;
            }
        }
    }
    return 0;
}

// 在二级间接块中搜索指定条目
uint search_double_indirect_block(uint double_indirect_addr, char *name, short entry_type)
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
            uint found_inum = search_indirect_block(level1_addrs[i], name, entry_type);
            if (found_inum != 0)
            {
                return found_inum;
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
    uint dir_data_block = block_bitmap_find_free();
    if (dir_data_block == 0)
    {
        Error("cmd_mkdir: no free blocks available for directory '%s'", name);
        iput(ip);
        return E_ERROR;
    }
    ip->addrs[0] = dir_data_block;

    // 标记数据块为已使用
    if (block_bitmap_set_used(dir_data_block) < 0)
    {
        Error("cmd_mkdir: failed to mark data block as used");
        iput(ip);
        return E_ERROR;
    }

    iupdate(ip);

    // 初始化目录内容
    if (init_directory_entries(ip->inum, current_dir, dir_data_block, mode) < 0)
    {
        Error("cmd_mkdir: failed to initialize directory entries");
        block_bitmap_set_free(dir_data_block);
        iput(ip);
        return E_ERROR;
    }

    // 在当前目录中添加新目录的条目
    int result = add_entry_to_directory(current_dir, name, ip->inum, T_DIR, mode);
    if (result < 0)
    {
        Error("cmd_mkdir: failed to add directory to parent");
        block_bitmap_set_free(dir_data_block);
        iput(ip);
        return E_ERROR;
    }

    Log("cmd_mkdir: successfully created directory '%s' with inode %d", name, ip->inum);
    iput(ip);
    return E_SUCCESS;
}

int cmd_rm(char *name)
{
    return E_SUCCESS;
}
int cmd_rmdir(char *name)
{
    return E_SUCCESS;
}

int cmd_cd(char *name)
{
    return E_SUCCESS;
}
int cmd_ls(entry **entries, int *n)
{
    return E_SUCCESS;
}

int cmd_cat(char *name, uchar **buf, uint *len)
{
    return E_SUCCESS;
}

int cmd_w(char *name, uint len, const char *data)
{
    return E_SUCCESS;
}

int cmd_i(char *name, uint pos, uint len, const char *data)
{
    return E_SUCCESS;
}

int cmd_d(char *name, uint pos, uint len)
{
    return E_SUCCESS;
}

int cmd_login(int auid)
{
    if (auid < 0 || auid > 65535)
    {
        Error("cmd_login: invalid user ID %d", auid);
        return E_ERROR;
    }

    current_uid = auid;
    Log("cmd_login: logged in as user %d", auid);
    return E_SUCCESS;
}
