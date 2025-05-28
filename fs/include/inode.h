#ifndef __INODE_H__
#define __INODE_H__

#include "common.h"

enum
{
    T_UNUSED = 0, // Unused
    T_DIR = 1,    // Directory
    T_FILE = 2,   // File
};

// You should add more fields
// the size of a dinode must divide BSIZE
typedef struct
{
    ushort type;             // File type
    ushort mode;             // File mode
    ushort nlink;            // Number of links
    ushort uid;              // User ID
    uint size;               // Size in bytes
    ushort dirty;              // Dirty flag, 1 if inode is modified
    ushort blocks;          // Number of blocks allocated (for file size)
    uint addrs[NDIRECT + 2]; // Data block addresses, the last two are indirect blocks
} dinode; // 64 bytes, 8 dinodes for each blocks

// inode in memory
// more useful fields can be added, e.g. reference count
typedef struct
{
    uint inum;    // Inode number
    ushort type;  // File type
    ushort mode;  // File mode
    ushort nlink; // Number of links
    ushort uid;   // User ID
    uint size;    // Size in bytes
    ushort dirty;   // Dirty flag, 1 if inode is modified
    ushort blocks; // Number of blocks allocated (for file size)
    uint addrs[NDIRECT + 2];
} inode;

// Get an inode by number (returns allocated inode or NULL)
// Don't forget to use iput()
inode *iget(uint inum);

void free_inode_blocks(inode *ip);
void free_inode_in_bitmap(uint inum);
void clear_disk_inode(uint inum);
// Free an inode (or decrement reference count)
void iput(inode *ip);

void init_inode(inode *ip, uint inum, short type);
int mark_inode_used(uint inum);
// Allocate a new inode of specified type (returns allocated inode or NULL)
// Don't forget to use iput()
inode *ialloc(short type);

// Update disk inode with memory inode contents
void iupdate(inode *ip);

uint bmap(inode *ip, uint bn); // Get the block number for a given block index
// Read from an inode (returns bytes read or -1 on error)
int readi(inode *ip, uchar *dst, uint off, uint n);

// Write to an inode (returns bytes written or -1 on error)
int writei(inode *ip, uchar *src, uint off, uint n);

void init_inode_system(); // Initialize the inode system
#endif
