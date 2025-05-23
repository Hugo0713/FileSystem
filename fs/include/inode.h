#ifndef __INODE_H__
#define __INODE_H__

#include "common.h"

#define NDIRECT 10 // Direct blocks, you can change this value

#define APB (BSIZE / sizeof(uint))           // Address per block 128
#define MAXFILEB (NDIRECT + APB + APB * APB) // Maximum file size in blocks 16522
#define MAXFILE (MAXFILEB * BSIZE)           // Maximum file size in bytes (8MB)

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
    uint blocks;             // Number of blocks, may be larger than size
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
    uint blocks;  // Number of blocks, may be larger than size
    uint addrs[NDIRECT + 2];
} inode;

// You can change the size of MAXNAME
#define MAXNAME 18

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
