#ifndef SIMPLE_CACHE_H
#define SIMPLE_CACHE_H

#include "common.h"
#include "block.h"

// 块缓存配置
#define BLOCK_CACHE_SIZE 500 
#define CACHE_DISABLED 0    // 是否禁用缓存

// 缓存项
typedef struct block_cache_entry
{
    uint blockno;      // 块号
    uchar data[BSIZE]; // 块数据
    int valid;         // 是否有效 (0: 无效, 1: 有效)
    int dirty;         // 是否脏数据 (0: 干净, 1: 脏)
} block_cache_entry_t;

// 函数声明
void cache_init(void);
void cached_read_block(int blockno, uchar *buf);
void cached_write_block(int blockno, uchar *buf);
void cache_flush(void);

#endif