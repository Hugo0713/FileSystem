#ifndef BITMAP_H
#define BITMAP_H

#include "fs.h"

// 位图类型
typedef enum
{
    BITMAP_INODE,
    BITMAP_BLOCK
} bitmap_type_t;

// 基本位图操作
int bitmap_is_used(bitmap_type_t type, uint item_num);
int bitmap_set(bitmap_type_t type, uint item_num, int used);
uint bitmap_find_free(bitmap_type_t type);
uint bitmap_count_used(bitmap_type_t type);
int bitmap_set_range(bitmap_type_t type, uint start_item, uint count, int used);

// 便捷的包装函数
static inline int inode_bitmap_is_used(uint inum)
{
    return bitmap_is_used(BITMAP_INODE, inum);
}
static inline int inode_bitmap_set_used(uint inum)
{
    return bitmap_set(BITMAP_INODE, inum, 1);
}
static inline int inode_bitmap_set_free(uint inum)
{
    return bitmap_set(BITMAP_INODE, inum, 0);
}
static inline uint inode_bitmap_find_free(void)
{
    return bitmap_find_free(BITMAP_INODE);
}


static inline int block_bitmap_is_used(uint bno)
{
    return bitmap_is_used(BITMAP_BLOCK, bno);
}
static inline int block_bitmap_set_used(uint bno)
{
    return bitmap_set(BITMAP_BLOCK, bno, 1);
}
static inline int block_bitmap_set_free(uint bno)
{
    return bitmap_set(BITMAP_BLOCK, bno, 0);
}
static inline uint block_bitmap_find_free(void)
{
    return bitmap_find_free(BITMAP_BLOCK);
}

// 批量操作函数
int bitmap_clear_all(bitmap_type_t type);
int bitmap_set_system_blocks_used(void);

#endif // BITMAP_H