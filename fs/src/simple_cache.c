#include "simple_cache.h"
#include "log.h"
#include <string.h>

// 声明原始的磁盘操作函数
extern void raw_read_block(int blockno, uchar *buf);
extern void raw_write_block(int blockno, uchar *buf);

// 全局块缓存数组
static block_cache_entry_t block_cache[BLOCK_CACHE_SIZE];
static int cache_initialized = 0;
static int next_slot = 0;  // 简单的轮询指针

// 初始化块缓存
void cache_init(void)
{
    if (cache_initialized)
    {
        return;
    }

    // 清空缓存
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++)
    {
        block_cache[i].blockno = 0;
        block_cache[i].valid = 0;
        block_cache[i].dirty = 0;
        memset(block_cache[i].data, 0, BSIZE);
    }

    next_slot = 0;
    cache_initialized = 1;
    Log("Block cache initialized with %d slots", BLOCK_CACHE_SIZE);
}

// 在缓存中查找块
static int find_block_in_cache(uint blockno)
{
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++)
    {
        if (block_cache[i].valid && block_cache[i].blockno == blockno)
        {
            return i;
        }
    }
    return -1; // 未找到
}

// 获取空闲的缓存槽位（简单轮询）
static int get_free_cache_slot(void)
{
    // 先查找无效的槽位
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++)
    {
        if (!block_cache[i].valid)
        {
            return i;
        }
    }

    // 所有槽位都被占用，使用轮询替换
    int slot = next_slot;
    next_slot = (next_slot + 1) % BLOCK_CACHE_SIZE;
    
    // 如果被替换的块是脏的，先写回磁盘
    if (block_cache[slot].dirty)
    {
        raw_write_block(block_cache[slot].blockno, block_cache[slot].data);
        block_cache[slot].dirty = 0;
    }
    
    // 清空槽位
    block_cache[slot].valid = 0;
    return slot;
}

// 缓存版本的读块
void cached_read_block(int blockno, uchar *buf)
{
#if CACHE_DISABLED
    raw_read_block(blockno, buf);
    return;
#endif

    if (!cache_initialized)
    {
        cache_init();
    }

    // 在缓存中查找
    int slot = find_block_in_cache(blockno);
    if (slot >= 0)
    {
        // 缓存命中
        memcpy(buf, block_cache[slot].data, BSIZE);
        return;
    }

    // 缓存未命中 - 从磁盘读取
    raw_read_block(blockno, buf);

    // 将块添加到缓存
    slot = get_free_cache_slot();
    block_cache[slot].blockno = blockno;
    memcpy(block_cache[slot].data, buf, BSIZE);
    block_cache[slot].valid = 1;
    block_cache[slot].dirty = 0;
}

// 缓存版本的写块
void cached_write_block(int blockno, uchar *buf)
{
#if CACHE_DISABLED
    raw_write_block(blockno, buf);
    return;
#endif

    if (!cache_initialized)
    {
        cache_init();
    }

    // 在缓存中查找
    int slot = find_block_in_cache(blockno);
    if (slot >= 0)
    {
        // 缓存命中 - 更新缓存数据
        memcpy(block_cache[slot].data, buf, BSIZE);
        block_cache[slot].dirty = 1;  // 标记为脏
        return;
    }

    // 缓存未命中 - 添加到缓存并标记为脏
    slot = get_free_cache_slot();
    block_cache[slot].blockno = blockno;
    memcpy(block_cache[slot].data, buf, BSIZE);
    block_cache[slot].valid = 1;
    block_cache[slot].dirty = 1;
}

// 刷新所有脏块到磁盘
void cache_flush(void)
{
#if CACHE_DISABLED
    return;
#endif
    if (!cache_initialized)
    {
        return;
    }

    for (int i = 0; i < BLOCK_CACHE_SIZE; i++)
    {
        if (block_cache[i].valid && block_cache[i].dirty)
        {
            raw_write_block(block_cache[i].blockno, block_cache[i].data);
            block_cache[i].dirty = 0;
        }
    }
}