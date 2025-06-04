#include "fs_internal.h"
#include <string.h>
#include <stdlib.h>
#include "block.h"
#include "log.h"
#include "bitmap.h"
#include "user.h"

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
