#include "bitmap.h"
#include "block.h"
#include "log.h"
#include <string.h>

// 获取位图的起始块和块数
static void get_bitmap_info(bitmap_type_t type, uint *start_block, uint *num_blocks, uint *max_items)
{
    switch (type)
    {
    case BITMAP_INODE:
        *start_block = sb.inodebmapstart;
        *num_blocks = sb.inodebmapblocks;
        *max_items = sb.ninodes;
        break;
    case BITMAP_BLOCK:
        *start_block = sb.bmapstart;
        *num_blocks = sb.bmapblocks;
        *max_items = sb.size;
        break;
    }
}

// 计算位图中的位置
typedef struct
{
    uint block_index; // 位图块索引
    uint byte_index;  // 字节索引
    uint bit_index;   // 位索引
} bitmap_pos_t;

static bitmap_pos_t get_bitmap_position(uint item_num)
{
    bitmap_pos_t pos;
    pos.block_index = item_num / (BSIZE * 8);
    pos.byte_index = (item_num % (BSIZE * 8)) / 8;
    pos.bit_index = item_num % 8;
    return pos;
}

// 检查位图中某项是否被使用
int bitmap_is_used(bitmap_type_t type, uint item_num)
{
    uint start_block, num_blocks, max_items;
    get_bitmap_info(type, &start_block, &num_blocks, &max_items);

    if (item_num >= max_items)
    {
        Error("bitmap_is_used: item %d out of range (max %d)", item_num, max_items);
        return -1;
    }

    bitmap_pos_t pos = get_bitmap_position(item_num);

    if (pos.block_index >= num_blocks)
    {
        Error("bitmap_is_used: block index %d out of range", pos.block_index);
        return -1;
    }

    uchar buf[BSIZE];
    read_block(start_block + pos.block_index, buf);

    return (buf[pos.byte_index] & (1 << pos.bit_index)) ? 1 : 0;
}

// 设置位图中某项的状态
int bitmap_set(bitmap_type_t type, uint item_num, int used)
{
    uint start_block, num_blocks, max_items;
    get_bitmap_info(type, &start_block, &num_blocks, &max_items);

    if (item_num >= max_items)
    {
        Error("bitmap_set: item %d out of range (max %d)", item_num, max_items);
        return -1;
    }

    bitmap_pos_t pos = get_bitmap_position(item_num);

    if (pos.block_index >= num_blocks)
    {
        Error("bitmap_set: block index %d out of range", pos.block_index);
        return -1;
    }

    uchar buf[BSIZE];
    read_block(start_block + pos.block_index, buf);

    if (used)
    {
        buf[pos.byte_index] |= (1 << pos.bit_index);
    }
    else
    {
        buf[pos.byte_index] &= ~(1 << pos.bit_index);
    }

    write_block(start_block + pos.block_index, buf);
    return 0;
}

// 查找第一个空闲项
uint bitmap_find_free(bitmap_type_t type)
{
    uint start_block, num_blocks, max_items;
    get_bitmap_info(type, &start_block, &num_blocks, &max_items);

    for (uint i = 0; i < num_blocks; i++)
    {
        uchar buf[BSIZE];
        read_block(start_block + i, buf);

        for (int j = 0; j < BSIZE; j++)
        {
            if (buf[j] != 0xFF)
            { // 这个字节中有空闲位
                for (int k = 0; k < 8; k++)
                {
                    if ((buf[j] & (1 << k)) == 0)
                    { // 找到空闲位
                        uint item_num = i * BSIZE * 8 + j * 8 + k;

                        if (item_num >= max_items)
                        {
                            return 0; // 超出范围，返回0表示未找到
                        }

                        return item_num;
                    }
                }
            }
        }
    }

    return 0; // 未找到空闲项
}

// 清空整个位图（设置为全0）
int bitmap_clear_all(bitmap_type_t type)
{
    uint start_block, num_blocks, max_items;
    get_bitmap_info(type, &start_block, &num_blocks, &max_items);

    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);

    for (uint i = 0; i < num_blocks; i++)
    {
        write_block(start_block + i, buf);
    }

    Log("bitmap_clear_all: cleared %s bitmap",
        type == BITMAP_INODE ? "inode" : "block");
    return 0;
}

// 标记系统使用的块为已分配
int bitmap_set_system_blocks_used(void)
{
    // 标记从块0到数据区开始之前的所有块为已使用
    for (uint i = 0; i < sb.datastart; i++)
    {
        if (bitmap_set(BITMAP_BLOCK, i, 1) < 0)
        {
            Error("bitmap_set_system_blocks_used: failed to mark block %d", i);
            return -1;
        }
        // else
        // {
        //     Log("bitmap_set_system_blocks_used: marked block %d as used", i);
        // }
    }

    Log("bitmap_set_system_blocks_used: marked %d system blocks as used", sb.datastart);
    return 0;
}