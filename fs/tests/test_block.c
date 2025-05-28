#include <string.h>

#include "block.h"
#include "common.h"
#include "mintest.h"
#include "fs.h"
#include "bitmap.h"
#include "log.h"

int nmeta;

// void mock_format()
// {
//     sb.size = 2048; // 2048 blocks
//     int nbitmap = (sb.size / BPB) + 1;
//     nmeta = nbitmap + 7; // some first blocks for metadata

//     sb.bmapstart = 1;
//     uchar buf[BSIZE];
//     memset(buf, 0, BSIZE);
//     for (int i = 0; i < sb.size; i += BPB)
//         write_block(BBLOCK(i), buf); // initialize bitmap blocks

//     for (int i = 0; i < nmeta; i += BPB)
//     {
//         memset(buf, 0, BSIZE);
//         for (int j = 0; j < BPB; j++)
//             if (i + j < nmeta)
//                 buf[j / 8] |= 1 << (j % 8); // mark as used
//         write_block(BBLOCK(i), buf);
//     }
// }

void mock_format()
{
    int size = 1024;
    init_sb(size);         // 初始化超级块
    init_block_bitmap();   // 初始化数据块位图
    init_inode_system();   // 初始化inode位图和inode区域
    //init_root_directory(); // 初始化根目录

    // 初始化日志区域（清零）
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    for (uint i = 0; i < sb.nlog; i++)
    {
        write_block(sb.logstart + i, buf);
    }

    Log("File system formatted successfully");
    Log("Total blocks: %d, Data blocks: %d, Inodes: %d", sb.size, sb.ndatablocks, sb.ninodes);
    // Calculate the number of metadata blocks
    nmeta = sb.datastart;
    Log("Mock format completed. Total metadata blocks: %d", nmeta);
}

mt_test(test_read_write_block)
{
    uchar write_buf[BSIZE], read_buf[BSIZE];
    for (int i = 0; i < BSIZE; i++)
    {
        write_buf[i] = (uchar)i;
    }

    write_block(0, write_buf);
    read_block(0, read_buf);

    for (int i = 0; i < BSIZE; i++)
    {
        mt_assert(write_buf[i] == read_buf[i]);
    }
    return 0;
}

mt_test(test_zero_block)
{
    uchar buf[BSIZE];
    memset(buf, 0xFF, BSIZE);
    write_block(0, buf);

    zero_block(0);
    read_block(0, buf);

    for (int i = 0; i < BSIZE; i++)
    {
        mt_assert(buf[i] == 0);
    }
    return 0;
}

mt_test(test_allocate_block)
{
    mock_format();
    Log("Block datastart used: %d", bitmap_is_used(BITMAP_BLOCK, nmeta));
    uint bno = allocate_block();
    Log("Allocated block number: %u\n", bno);
    mt_assert(bno == nmeta); // the first free block

    uchar buf[BSIZE];
    read_block(bno, buf);

    for (int i = 0; i < BSIZE; i++)
    {
        mt_assert(buf[i] == 0); // the block should be zeroed
    }

    // check if the block is marked as used in the bitmap
    read_block(BBLOCK(bno), buf);
    int i = bno % BPB;
    int m = 1 << (i % 8);
    mt_assert((buf[i / 8] & m) != 0); // the block should be marked as used
    return 0;
}

mt_test(test_allocate_block_all)
{
    mock_format();
    for (int i = 0; i < sb.size - nmeta; i++)
    {
        uint bno = allocate_block();
        mt_assert(bno != 0);
    }
    uint bno = allocate_block();
    mt_assert(bno == 0); // no more blocks available
    return 0;
}

mt_test(test_free_block)
{
    mock_format();
    uint bno = allocate_block();
    mt_assert(bno != 0);

    free_block(bno);

    uchar buf[BSIZE];
    read_block(BBLOCK(bno), buf);

    int i = bno % BPB;
    int m = 1 << (i % 8);
    mt_assert((buf[i / 8] & m) == 0);
    return 0;
}

void block_tests()
{
    mt_run_test(test_read_write_block);
    mt_run_test(test_zero_block);
    mt_run_test(test_allocate_block);
    mt_run_test(test_allocate_block_all);
    mt_run_test(test_free_block);
}
