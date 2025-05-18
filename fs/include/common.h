#ifndef __COMMAN_H__
#define __COMMAN_H__

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

// block size in bytes
#define BSIZE 512

// bits per block
#define BPB (BSIZE * 8)

// block of free map containing bit for block b
#define BBLOCK(b) ((b) / BPB + sb.bmapstart)

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

// error codes
enum {
    E_SUCCESS = 0,
    E_ERROR = 1,
    E_NOT_LOGGED_IN = 2,
    E_NOT_FORMATTED = 3,
    E_PERMISSION_DENIED = 4,
};

#endif
