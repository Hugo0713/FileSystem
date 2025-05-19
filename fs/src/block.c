#include "block.h"

#include <string.h>

#include "common.h"
#include "log.h"

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
    uchar buf[BSIZE];
    uint bno = 0;
    int i = 0;

    while (i < sb.bmapblocks)
    {
        read_block(sb.bmapstart + i, buf);
        for (int j = 0; j < BSIZE; j++)
        {
            if (buf[j] != 0xFF)
            {
                // Find the first free block
                for (int k = 0; k < 8; k++)
                {
                    if ((buf[j] & (1 << k)) == 0)
                    {
                        buf[j] |= (1 << k);
                        bno = i * BSIZE * 8 + j * 8 + k;
                        write_block(sb.bmapstart + i, buf);
                        return bno;
                    }
                }
            }
        }
        i++;
    }
    Warn("Out of blocks");
    return 0;
}

void free_block(uint bno)
{
    uchar buf[BSIZE];
    uint blockno = bno / (BSIZE * 8);
    uint bitno = bno % (BSIZE * 8);
    read_block(sb.bmapstart + blockno, buf);
    buf[bitno / 8] &= ~(1 << (bitno % 8));
    write_block(sb.bmapstart + blockno, buf);
}

void get_disk_info(int *ncyl, int *nsec)
{
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
