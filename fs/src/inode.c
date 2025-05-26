#include "inode.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "log.h"

inode *iget(uint inum)
{
    if (inum >= sb.ninodes)
    {
        Error("iget: inum out of range");
        return NULL;
    }
    // 计算包含该inode的磁盘块号
    uint block_num = sb.inodestart + inum / (BSIZE / sizeof(dinode));
    uint offset = inum % (BSIZE / sizeof(dinode));

    // 读取包含该inode的磁盘块
    uchar buf[BSIZE];
    read_block(block_num, buf);

    // 获取该inode在块中的位置
    dinode *disk_inode = (dinode *)buf + offset;

    // 检查inode是否有效
    if (disk_inode->type == T_UNUSED)
    {
        Error("iget: inode %d is unused", inum);
        return NULL;
    }

    inode *ip = (inode *)malloc(sizeof(inode));
    if (ip == NULL)
    {
        Error("iget: malloc failed");
        return NULL;
    }

    // 将dinode的内容复制到内存inode中
    ip->inum = inum;
    ip->type = disk_inode->type;
    ip->mode = disk_inode->mode;
    ip->nlink = disk_inode->nlink;
    ip->uid = disk_inode->uid;
    ip->size = disk_inode->size;
    ip->dirty = disk_inode->dirty;

    // 复制地址数组
    for (int i = 0; i < NDIRECT + 2; i++)
    {
        ip->addrs[i] = disk_inode->addrs[i];
    }

    Log("iget: loaded inode %d (type=%d, size=%d)", inum, ip->type, ip->size);
    return ip;
}

void free_inode_blocks(inode *ip)
{
    // 释放直接块
    for (int i = 0; i < NDIRECT; i++)
    {
        if (ip->addrs[i] != 0)
        {
            free_block(ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    // 释放一级间接块
    if (ip->addrs[NDIRECT] != 0)
    {
        uchar buf[BSIZE];
        read_block(ip->addrs[NDIRECT], buf);
        uint *addrs = (uint *)buf;

        for (int i = 0; i < APB; i++)
        {
            if (addrs[i] != 0)
            {
                free_block(addrs[i]);
            }
        }
        free_block(ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    // 释放二级间接块
    if (ip->addrs[NDIRECT + 1] != 0)
    {
        uchar buf[BSIZE];
        read_block(ip->addrs[NDIRECT + 1], buf);
        uint *level1_addrs = (uint *)buf;

        for (int i = 0; i < APB; i++)
        {
            if (level1_addrs[i] != 0)
            {
                uchar buf2[BSIZE];
                read_block(level1_addrs[i], buf2);
                uint *level2_addrs = (uint *)buf2;

                for (int j = 0; j < APB; j++)
                {
                    if (level2_addrs[j] != 0)
                    {
                        free_block(level2_addrs[j]);
                    }
                }
                free_block(level1_addrs[i]);
            }
        }
        free_block(ip->addrs[NDIRECT + 1]);
        ip->addrs[NDIRECT + 1] = 0;
    }
    ip->size = 0;
}

void free_inode_in_bitmap(uint inum)
{
    uint bmap_block = sb.inodebmapstart + inum / (BSIZE * 8);
    uint bmap_offset = (inum % (BSIZE * 8)) / 8;
    uint bmap_bit = inum % 8;

    uchar buf[BSIZE];
    read_block(bmap_block, buf);
    buf[bmap_offset] &= ~(1 << bmap_bit);
    write_block(bmap_block, buf);
}

void clear_disk_inode(uint inum)
{
    uint block_num = sb.inodestart + inum / (BSIZE / sizeof(dinode));
    uint offset = inum % (BSIZE / sizeof(dinode));

    uchar buf[BSIZE];
    read_block(block_num, buf);

    dinode *disk_inode = (dinode *)buf + offset;
    memset(disk_inode, 0, sizeof(dinode));
    disk_inode->type = T_UNUSED;

    write_block(block_num, buf);
}

void iput(inode *ip)
{
    if (ip == NULL)
    {
        return;
    }

    // 如果inode被修改过，写回磁盘
    if (ip->dirty)
    {
        iupdate(ip);
    }

    // 检查是否需要释放inode和相关资源
    if (ip->nlink == 0)
    {
        free_inode_blocks(ip);          // 释放文件的所有数据块
        free_inode_in_bitmap(ip->inum); // 在inode位图中标记该inode为空闲
        clear_disk_inode(ip->inum);     // 清零磁盘上的inode
    }
    // 释放内存
    free(ip);
    Log("iput: inode %d released", ip->inum);
}

void init_inode(inode *ip, uint inum, short type)
{
    if (ip == NULL)
    {
        Error("init_inode: null inode pointer");
        return;
    }

    // 清零整个结构
    memset(ip, 0, sizeof(inode));

    // 设置基本信息
    ip->inum = inum;
    ip->type = type;
    ip->uid = 0; // 可以根据需要设置为当前用户ID
    ip->size = 0;
    ip->dirty = 1; // 标记为脏，需要写回磁盘

    switch (type)
    {
    case T_FILE:
        ip->mode = 0644; // rw-r--r--
        ip->nlink = 1;   // 新文件默认有一个硬链接
        break;

    case T_DIR:
        ip->mode = 0755; // rwxr-xr-x
        ip->nlink = 2;   // 目录至少有两个链接（. 和 父目录中的条目）
        break;
    }

    Log("init_inode: initialized inode %d (type=%d, mode=%o, nlink=%d)",
        inum, type, ip->mode, ip->nlink);
}

static uint find_free_inode()
{
    // 遍历所有inode位图块，寻找空闲inode
    for (uint i = 0; i < sb.inodebmapblocks; i++)
    {
        uchar buf[BSIZE];
        read_block(sb.inodebmapstart + i, buf);

        // 在当前位图块中查找空闲位
        for (int j = 0; j < BSIZE; j++)
        {
            if (buf[j] != 0xFF) // 这个字节中有空闲位
            {
                // 找到第一个空闲位
                for (int k = 0; k < 8; k++)
                {
                    if ((buf[j] & (1 << k)) == 0) // 找到空闲位
                    {
                        // 计算inode编号
                        uint inum = i * BSIZE * 8 + j * 8 + k;

                        // 检查是否超出inode总数
                        if (inum >= sb.ninodes)
                        {
                            return -1; // 表示未找到
                        }

                        return inum;
                    }
                }
            }
        }
    }
    return -1;
}

static int mark_inode_used(uint inum)
{
    uint i = inum / (BSIZE * 8);
    uint j = (inum % (BSIZE * 8)) / 8;
    uint k = inum % 8;

    if (i >= sb.inodebmapblocks)
    {
        Error("mark_inode_used: invalid inode number %d", inum);
        return -1;
    }

    uchar buf[BSIZE];
    read_block(sb.inodebmapstart + i, buf);

    // 检查是否已经被使用
    if (buf[j] & (1 << k))
    {
        Error("mark_inode_used: inode %d already in use", inum);
        return -1;
    }

    // 标记为已使用
    buf[j] |= (1 << k);
    write_block(sb.inodebmapstart + i, buf);
    return 0;
}

inode *ialloc(short type)
{
    uint inum = find_free_inode();
    if (inum == -1)
    {
        Error("ialloc: no free inodes available");
        return NULL;
    }

    if (mark_inode_used(inum) < 0)
    {
        Error("ialloc: failed to mark inode %d as used", inum);
        return NULL;
    }

    clear_disk_inode(inum);                     // 清零磁盘上的inode
    inode *ip = (inode *)malloc(sizeof(inode)); // 分配内存inode
    init_inode(ip, inum, type);                 // 初始化内存inode
    iupdate(ip);                                // 写回磁盘

    Info("ialloc: successfully allocated inode %d (type=%d)", inum, type);
    return ip;
}

void iupdate(inode *ip)
{
    if (ip == NULL)
    {
        Error("iupdate: null inode");
        return;
    }

    // 计算包含该inode的磁盘块号
    uint block_num = sb.inodestart + ip->inum / (BSIZE / sizeof(dinode));
    uint offset = ip->inum % (BSIZE / sizeof(dinode));

    // 读取磁盘块
    uchar buf[BSIZE];
    read_block(block_num, buf);

    // 定位到具体的dinode
    dinode *disk_inode = (dinode *)buf + offset;

    // 将内存inode的数据复制到dinode
    disk_inode->type = ip->type;
    disk_inode->mode = ip->mode;
    disk_inode->nlink = ip->nlink;
    disk_inode->uid = ip->uid;
    disk_inode->size = ip->size;
    disk_inode->dirty = ip->dirty = 0; // 清除脏标志

    // 复制地址数组
    for (int i = 0; i < NDIRECT + 2; i++)
    {
        disk_inode->addrs[i] = ip->addrs[i];
    }

    // 写回磁盘
    write_block(block_num, buf);
    Log("iupdate: updated inode %d to disk", ip->inum);
}

int readi(inode *ip, uchar *dst, uint off, uint n)
{
    return n;
}

int writei(inode *ip, uchar *src, uint off, uint n)
{
    iupdate(ip);
    return n;
}
