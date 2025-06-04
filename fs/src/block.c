#include "block.h"
#include "simple_cache.h"

#include <string.h>

#include "common.h"
#include "log.h"
#include "bitmap.h"
#include "tcp_utils.h"

superblock sb;
// uchar ramdisk[MAXBLOCK];
static tcp_client disk_client = NULL;

// 磁盘信息
extern int ncyl, nsec;

int init_disk_connection(const char *host, int port)
{
    disk_client = client_init(host, port);
    if (disk_client == NULL)
    {
        Error("init_disk_connection: failed to connect to disk server at %s:%d", host, port);
        return -1;
    }
    Log("Disk connection initialized successfully to %s:%d", host, port);
    return 0;
}

void cleanup_disk_connection()
{
    if (disk_client)
    {
        client_destroy(disk_client);
        disk_client = NULL;
        Log("Disk connection closed");
    }
}

void zero_block(uint bno)
{
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    write_block(bno, buf);
}

uint allocate_block()
{
    uint bno = block_bitmap_find_free();
    if (bno == 0)
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

void get_disk_info(int *ncyl_, int *nsec_)
{
    if (!disk_client)
    {
        Error("Disk client not initialized");
        return;
    }

    // 发送 "I" 命令获取磁盘信息
    char cmd[] = "I";
    client_send(disk_client, cmd, strlen(cmd) + 1);

    char response[256];
    int n = client_recv(disk_client, response, sizeof(response));
    response[n] = '\0';

    // 解析响应："ncyl nsec"
    if (sscanf(response, "%d %d", ncyl_, nsec_) != 2)
    {
        Error("Failed to parse disk info: %s", response);
    }

    Log("Got disk info: %d cylinders, %d sectors", ncyl, nsec);
}

// 将块号转换为柱面/扇区的函数
void block_to_cyl_sec(int blockno, int *cyl, int *sec)
{
    *cyl = blockno / nsec;
    *sec = blockno % nsec;
}

void raw_read_block(int blockno, uchar *buf)
{
    if (!disk_client)
    {
        Error("Disk client not initialized");
        return;
    }

    int cyl, sec;
    block_to_cyl_sec(blockno, &cyl, &sec);

    // 发送读命令 "R cyl sec"
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "R %d %d", cyl, sec);
    client_send(disk_client, cmd, strlen(cmd) + 1);

    char response[1024]; 
    int n = client_recv(disk_client, response, sizeof(response));

    // 检查响应格式
    if (n > 3 && strncmp(response, "Yes", 3) == 0)
    {
        // 数据在"Yes"之后
        int data_size = n - 4; // 减去"Yes\0"
        if (data_size >= BSIZE)
        {
            memcpy(buf, response + 4, BSIZE);
        }
        else
        {
            memcpy(buf, response + 4, data_size);
            memset(buf + data_size, 0, BSIZE - data_size);
        }
        // Log("read_block: successfully read block %d", blockno);
    }
    else
    {
        Error("read_block: failed for block %d, response: %s", blockno, response);
        memset(buf, 0, BSIZE);
    }
}

void raw_write_block(int blockno, uchar *buf)
{
    if (!disk_client)
    {
        Error("Disk client not initialized");
        return;
    }

    int cyl, sec;
    block_to_cyl_sec(blockno, &cyl, &sec);

    // 发送写命令 "W cyl sec len data"
    char cmd[1024];
    int header_len = snprintf(cmd, sizeof(cmd), "W %d %d %d ", cyl, sec, BSIZE);
    memcpy(cmd + header_len, buf, BSIZE);

    client_send(disk_client, cmd, header_len + BSIZE);

    char response[256];
    int n = client_recv(disk_client, response, sizeof(response));
    response[n] = '\0';

    if (strncmp(response, "Yes", 3) == 0)
    {
        //Log("write_block: successfully wrote block %d", blockno);
    }
    else
    {
        Error("write_block: failed for block %d, response: %s", blockno, response);
    }
}

// 修改 read_block 函数使用缓存
void read_block(int blockno, uchar *buf)
{
    cached_read_block(blockno, buf);
}

// 修改 write_block 函数使用缓存
void write_block(int blockno, uchar *buf)
{
    cached_write_block(blockno, buf);
}

void init_block_bitmap()
{
    // 清空所有数据块位图
    if (bitmap_clear_all(BITMAP_BLOCK) < 0)
    {
        Error("init_block_bitmap: failed to clear block bitmap");
        return;
    }
    // 标记系统块为已使用
    if (bitmap_set_system_blocks_used() < 0)
    {
        Error("init_block_bitmap: failed to mark system blocks");
        return;
    }
    Log("Block bitmap initialized successfully");
}