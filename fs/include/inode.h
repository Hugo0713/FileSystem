#ifndef __INODE_H__
#define __INODE_H__

#include "common.h"

#define NDIRECT 10  // Direct blocks, you can change this value

#define MAXFILEB (NDIRECT + APB + APB * APB)

enum {
    T_DIR = 1,   // Directory
    T_FILE = 2,  // File
};

// You should add more fields
// the size of a dinode must divide BSIZE
typedef struct {
    ushort type;              // File type
    uint size;                // Size in bytes
    uint blocks;              // Number of blocks, may be larger than size
    uint addrs[NDIRECT + 2];  // Data block addresses,
                              // the last two are indirect blocks
    // ...
    // ...
    // Other fields can be added as needed
} dinode;

// inode in memory
// more useful fields can be added, e.g. reference count
typedef struct {
    uint inum;
    ushort type;
    uint size;
    uint blocks;
    uint addrs[NDIRECT + 2];
    // ...
    // ...
    // Other fields can be added as needed
} inode;

// You can change the size of MAXNAME
#define MAXNAME 12

// Get an inode by number (returns allocated inode or NULL)
// Don't forget to use iput()
inode *iget(uint inum);

// Free an inode (or decrement reference count)
void iput(inode *ip);

// Allocate a new inode of specified type (returns allocated inode or NULL)
// Don't forget to use iput()
inode *ialloc(short type);

// Update disk inode with memory inode contents
void iupdate(inode *ip);

// Read from an inode (returns bytes read or -1 on error)
int readi(inode *ip, uchar *dst, uint off, uint n);

// Write to an inode (returns bytes written or -1 on error)
int writei(inode *ip, uchar *src, uint off, uint n);

#endif
