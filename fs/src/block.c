#include "block.h"

#include <string.h>

#include "common.h"
#include "log.h"
#include "bitmap.h"

superblock sb;
uchar ramdisk[MAXBLOCK];

void zero_block(uint bno)
{
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    write_block(bno, buf);
}

uint allocate_block()
{
    uint bno = block_bitmap_find_free();
    if (bno == -1)
    {
        Error("alloc_block: no free blocks available");
        return 0;
    }

    if (block_bitmap_set_used(bno) < 0)
    {
        Error("alloc_block: failed to mark block %d as used", bno);
        return 0;
    }

    // 清零新分配的块
    zero_block(bno);

    Log("alloc_block: allocated block %d", bno);
    return bno;
}

void free_block(uint bno)
{
    if (bno < sb.datastart || bno >= sb.size)
    {
        Error("free_block: blockno %d out of range", bno);
        return;
    }
    // 检查是否已经空闲
    int used = block_bitmap_is_used(bno);
    if (used < 0)
    {
        Error("free_block: invalid block number %d", bno);
        return;
    }
    if (!used)
    {
        Warn("free_block: block %d already free", bno);
        return;
    }

    // 标记为空闲
    if (block_bitmap_set_free(bno) < 0)
    {
        Error("free_block: failed to free block %d", bno);
        return;
    }

    zero_block(bno); // 清零块内容
    Log("free_block: block %d freed", bno);
}

void get_disk_info(int *ncyl, int *nsec)
{
    *ncyl = NCYL;
    *nsec = NSEC;
}

void read_block(int blockno, uchar *buf)
{
    if (blockno < 0 || blockno >= sb.size)
    {
        Error("read_block: blockno %d out of range", blockno);
        return;
    }
    // Read the block from disk
    memcpy(buf, ramdisk + blockno * BSIZE, BSIZE);
}

void write_block(int blockno, uchar *buf)
{
    if (blockno < 0 || blockno >= sb.size)
    {
        Error("write_block: blockno %d out of range", blockno);
        return;
    }
    // Write the block to disk
    memcpy(ramdisk + blockno * BSIZE, buf, BSIZE);
}
