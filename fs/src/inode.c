#include "inode.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "log.h"
#include "bitmap.h"

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
    if (inode_bitmap_set_free(inum) < 0)
    {
        Error("free_inode_in_bitmap: failed to free inode %d", inum);
    }
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
    return inode_bitmap_find_free();
}

static int mark_inode_used(uint inum)
{
    int used = inode_bitmap_is_used(inum);
    if (used < 0)
    {
        Error("mark_inode_used: invalid inode number %d", inum);
        return -1;
    }
    if (used)
    {
        Error("mark_inode_used: inode %d already in use", inum);
        return -1;
    }
    // 标记为已使用
    return inode_bitmap_set_used(inum);
}

inode *ialloc(short type)
{
    uint inum = find_free_inode();
    if (inum == 0)
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

// 根据偏移量获取对应的块号(考虑一级间接块和二级间接块分配的逻辑块号)
static uint bmap(inode *ip, uint bn)
{ // bn: 0 ~ MAXFILEB - 1
    uint addr;
    uint *indirect_block;
    uchar buf[BSIZE];

    // 直接块
    if (bn < NDIRECT)
    {
        if ((addr = ip->addrs[bn]) == 0)
        {
            ip->addrs[bn] = addr = alloc_block();
            if (addr == 0)
            {
                return 0;
            }
            ip->dirty = 1;
        }
        return addr;
    }

    bn -= NDIRECT; // 跳过直接块部分
    // 一级间接块
    if (bn < APB)
    {
        // 分配间接块（如果需要）
        addr = ip->addrs[NDIRECT];
        if (addr == 0)
        {
            ip->addrs[NDIRECT] = addr = alloc_block();
            if (addr == 0)
            {
                return 0;
            }
            ip->dirty = 1;
        }

        // 读取间接块
        read_block(addr, buf);
        indirect_block = (uint *)buf; // 一级间接块存储的地址数组

        // 分配数据块（如果需要）
        if ((addr = indirect_block[bn]) == 0)
        {
            indirect_block[bn] = addr = alloc_block();
            if (addr == 0)
            {
                return 0;
            }
            write_block(ip->addrs[NDIRECT], buf); // 更新一级间接块
        }
        return addr;
    }

    bn -= APB; // 跳过一级间接块部分
    // 二级间接块
    if (bn < APB * APB)
    {
        // 分配二级间接块（如果需要）
        if ((addr = ip->addrs[NDIRECT + 1]) == 0)
        {
            ip->addrs[NDIRECT + 1] = addr = alloc_block();
            if (addr == 0)
            {
                return 0;
            }
            ip->dirty = 1;
        }

        // 读取二级间接块
        read_block(addr, buf);
        indirect_block = (uint *)buf;

        // 分配一级间接块（如果需要）
        if ((addr = indirect_block[bn / APB]) == 0)
        {
            indirect_block[bn / APB] = addr = alloc_block();
            if (addr == 0)
            {
                return 0;
            }
            write_block(ip->addrs[NDIRECT + 1], buf);
        }

        // 读取一级间接块
        read_block(addr, buf);
        indirect_block = (uint *)buf;

        // 分配数据块（如果需要）
        if ((addr = indirect_block[bn % APB]) == 0)
        {
            indirect_block[bn % APB] = addr = alloc_block();
            if (addr == 0)
            {
                return 0;
            }
            write_block(ip->addrs[NDIRECT + 1], buf);
        }
        return addr;
    }

    // 超出文件大小限制
    Error("bmap: block number %d out of range", bn);
    return 0;
}

int readi(inode *ip, uchar *dst, uint off, uint n)
{
    uint total, bytes_this_iteration; // 总共读取的字节数, 本次读取的字节数
    uint target_block, block_offset;  // 目标块号和块内偏移
    uchar buf[BSIZE];

    if (ip == NULL || dst == NULL)
    {
        Error("readi: invalid parameters");
        return -1;
    }
    if (off > ip->size)
    {
        Error("readi: offset %d beyond file size %d", off, ip->size);
        return 0;
    }

    // 调整读取字节数，不能超出文件大小
    if (off + n > ip->size)
    {
        n = ip->size - off;
        Error("readi: adjusting read size to %d bytes", n);
    }

    Log("readi: reading %d bytes from inode %d at offset %d", n, ip->inum, off);

    for (total = 0; total < n; total += bytes_this_iteration, off += bytes_this_iteration, dst += bytes_this_iteration)
    {
        // 计算当前读取位置对应的块号和块内偏移
        target_block = off / BSIZE;
        block_offset = off % BSIZE;

        // 获取物理块号
        uint block_addr = bmap(ip, target_block);
        if (block_addr == 0)
        {
            Error("readi: failed to get block address for block %d", target_block);
            break;
        }

        // 读取块数据
        read_block(block_addr, buf);
        // 计算本次读取的字节数
        bytes_this_iteration = BSIZE - block_offset;
        if (bytes_this_iteration > n - total)
        {
            bytes_this_iteration = n - total;
        }
        memcpy(dst, buf + block_offset, bytes_this_iteration);
    }

    Log("readi: successfully read %d bytes from inode %d", total, ip->inum);
    return total;
}

int writei(inode *ip, uchar *src, uint off, uint n)
{
    uint total, bytes_this_iteration; // 总共写入的字节数, 本次写入的字节数
    uint target_block, block_offset;  // 目标块号和块内偏移
    uchar buf[BSIZE];

    // 参数检查
    if (ip == NULL || src == NULL)
    {
        Error("writei: invalid parameters");
        return -1;
    }
    uint max_size = MAXFILE;
    if (off + n > max_size)
    {
        Error("writei: write would exceed maximum file size");
        return -1;
    }
    Log("writei: writing %d bytes to inode %d at offset %d", n, ip->inum, off);

    for (total = 0; total < n; total += bytes_this_iteration, off += bytes_this_iteration, src += bytes_this_iteration)
    {
        // 计算当前写入位置对应的块号和块内偏移
        target_block = off / BSIZE;
        block_offset = off % BSIZE;
        // 获取物理块号（如果不存在会自动分配）
        uint block_addr = bmap(ip, target_block);
        if (block_addr == 0)
        {
            Error("writei: failed to allocate block for block %d", target_block);
            break;
        }
        // 计算本次写入的字节数
        bytes_this_iteration = BSIZE - block_offset;
        if (bytes_this_iteration > n - total)
        {
            bytes_this_iteration = n - total;
        }

        // 如果不是整块写入，需要先读取现有数据
        if (block_offset > 0 || bytes_this_iteration < BSIZE)
        {
            read_block(block_addr, buf);
        }
        memcpy(buf + block_offset, src, bytes_this_iteration);
        write_block(block_addr, buf);
    }

    // 更新文件大小
    uint new_size = off; // 最后一次写入的偏移量
    if (new_size > ip->size)
    {
        ip->size = new_size;
        ip->dirty = 1;
    }

    ip->dirty = 1;
    Log("writei: successfully wrote %d bytes to inode %d", total, ip->inum);
    iupdate(ip);
    return total;
}
