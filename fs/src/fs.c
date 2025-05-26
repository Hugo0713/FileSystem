#include "fs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"

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

    uint ninodes = size / 10;                 // inode数量约为总块数的1/10
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
    uchar buf[BSIZE];
    // 初始化数据块位图（全部设为空闲0）
    memset(buf, 0, BSIZE);
    for (uint i = 0; i < sb.bmapblocks; i++)
    {
        write_block(sb.bmapstart + i, buf);
    }

    // 标记系统使用的块为已分配
    for (uint i = 0; i < sb.datastart; i++)
    {
        uint bmap_block = sb.bmapstart + i / BPB;
        uint bmap_offset = (i % BPB) / 8;
        uint bmap_bit = i % 8;

        read_block(bmap_block, buf);
        buf[bmap_offset] |= (1 << bmap_bit);
        write_block(bmap_block, buf);
    }
    Log("Data bitmap initialized successfully");
}

void init_inode_bitmap()
{
    uchar buf[BSIZE];
    // 初始化inode位图（全部设为空闲）
    memset(buf, 0, BSIZE);
    for (uint i = 0; i < sb.inodebmapblocks; i++)
    {
        write_block(sb.inodebmapstart + i, buf);
    }

    // 初始化所有inode为未使用状态
    dinode empty_inode;
    memset(&empty_inode, 0, sizeof(empty_inode));
    empty_inode.type = T_UNUSED;

    // 将空inode写入一个块
    memset(buf, 0, BSIZE);
    for (int i = 0; i < BSIZE / sizeof(dinode); i++)
    {
        memcpy(buf + i * sizeof(dinode), &empty_inode, sizeof(dinode));
    }

    for (uint i = 0; i < sb.ninodes; i++)
    {
        write_block(sb.inodestart + i, buf);
    }

    Log("Inode bitmap and inode area initialized successfully");
}

void init_root_directory()
{
    uchar buf[BSIZE];

    // 创建根目录（inode 0）
    dinode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.type = T_DIR;
    root_inode.mode = 0755;              // 目录权限
    root_inode.nlink = 2;                // "." 和父目录的链接
    root_inode.uid = 0;                  // root用户
    root_inode.size = 2 * sizeof(entry); // "." 和 ".." 两个条目
    root_inode.blocks = 1;               // 分配一个数据块

    // 为根目录分配一个数据块
    uint root_data_block = sb.datastart; // 使用第一个数据块
    root_inode.addrs[0] = root_data_block;

    // 将根目录inode写入磁盘
    read_block(sb.inodestart, buf);
    memcpy(buf, &root_inode, sizeof(dinode));
    write_block(sb.inodestart, buf);

    // 标记根目录inode为已使用
    read_block(sb.inodebmapstart, buf);
    buf[0] |= 1; // 设置第0位
    write_block(sb.inodebmapstart, buf);

    // 标记根目录数据块为已使用
    uint bmap_block = sb.bmapstart + root_data_block / BPB;
    uint bmap_offset = (root_data_block % BPB) / 8;
    uint bmap_bit = root_data_block % 8;

    read_block(bmap_block, buf);
    buf[bmap_offset] |= (1 << bmap_bit);
    write_block(bmap_block, buf);

    // 初始化根目录内容
    entry root_entries[2];

    // "." 条目（指向自己）
    root_entries[0].inum = 0;
    root_entries[0].type = T_DIR;
    root_entries[0].size = 2 * sizeof(entry);
    root_entries[0].mode = 0755;
    root_entries[0].uid = 0;
    strcpy(root_entries[0].name, ".");

    // ".." 条目（根目录的父目录是自己）
    root_entries[1].inum = 0;
    root_entries[1].type = T_DIR;
    root_entries[1].size = 2 * sizeof(entry);
    root_entries[1].mode = 0755;
    root_entries[1].uid = 0;
    strcpy(root_entries[1].name, "..");

    // 将根目录条目写入数据块
    memset(buf, 0, BSIZE);
    memcpy(buf, root_entries, 2 * sizeof(entry));
    write_block(root_data_block, buf);

    Info("Root directory initialized successfully");
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
