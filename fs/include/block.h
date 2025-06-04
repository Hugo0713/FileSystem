#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "common.h"

typedef struct
{
    uint magic;     // Magic number, used to identify the file system (0xf0f03410)
    uint size;      // Size in blocks

    uint bmapstart; // Block number of first free bitmap block -> disk block
    uint bmapblocks; // Number of blocks used for free bitmap

    uint inodebmapstart; // Block number of first inode bitmap block
    uint inodebmapblocks; // Number of blocks used for inode bitmap

    uint inodestart; // Block number of first inode(root inode) block
    uint ninodes;    // Total number of inodes

    uint logstart;   // Start block of log
    uint nlog;       // Number of log blocks

    uint datastart;  // Block number of first data block
    uint ndatablocks; // Total number of data blocks
} superblock; // 48 bytes

// sb is defined in block.c
extern superblock sb;

// RAMDISK
extern uchar ramdisk[MAXBLOCK];

void zero_block(uint bno); 
uint allocate_block(); 
void free_block(uint bno); 

void get_disk_info(int *ncyl, int *nsec);
void raw_read_block(int blockno, uchar *buf);
void raw_write_block(int blockno, uchar *buf);
void read_block(int blockno, uchar *buf);
void write_block(int blockno, uchar *buf);

void init_block_bitmap();
int init_disk_connection(const char *host, int port);
void cleanup_disk_connection();
void block_to_cyl_sec(int blockno, int *cyl, int *sec);

#endif