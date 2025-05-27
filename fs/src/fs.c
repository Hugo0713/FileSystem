#include "fs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"
#include "bitmap.h"

superblock sb;

void sbinit()
{
    uchar buf[BSIZE];
    read_block(0, buf);
    memcpy(&sb, buf, sizeof(sb));
}

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

void init_root_directory()
{
    uchar buf[BSIZE];
    // 创建根目录（inode 0）
    dinode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.type = T_DIR;
    root_inode.mode = 0755;
    root_inode.nlink = 2; // 根目录至少有两个链接（. 和 ..）
    root_inode.uid = 0;
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

    // 初始化根目录内容
    entry root_entries[2];
    // "." 条目（指向自己）
    root_entries[0].inum = 0;
    root_entries[0].type = T_DIR;
    root_entries[0].size = BSIZE;
    root_entries[0].mode = 0755;
    root_entries[0].uid = 0;
    strcpy(root_entries[0].name, ".");

    // ".." 条目（根目录的父目录是自己）
    root_entries[1].inum = 0;
    root_entries[1].type = T_DIR;
    root_entries[1].size = BSIZE;
    root_entries[1].mode = 0755;
    root_entries[1].uid = 0;
    strcpy(root_entries[1].name, "..");

    // 将根目录条目写入数据块
    memset(buf, 0, BSIZE);
    memcpy(buf, root_entries, 2 * sizeof(entry));
    write_block(root_data_block, buf);
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
    Log("Total blocks: %d, Data blocks: %d, Inodes: %d",
        sb.size, sb.ndatablocks, sb.ninodes);
    return E_SUCCESS;
}

int cmd_mk(char *name, short mode)
{
    return E_ERROR;
}

int cmd_mkdir(char *name, short mode)
{
    return E_ERROR;
}

int cmd_rm(char *name)
{
    return E_SUCCESS;
}

int cmd_cd(char *name)
{
    return E_SUCCESS;
}

int cmd_rmdir(char *name)
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
    return E_SUCCESS;
}
