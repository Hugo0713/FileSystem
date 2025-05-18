#include "inode.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "log.h"

inode *iget(uint inum) {
    return NULL;
}

void iput(inode *ip) { free(ip); }

inode *ialloc(short type) {
    Error("ialloc: no inodes");
    return NULL;
}

void iupdate(inode *ip) {

}

int readi(inode *ip, uchar *dst, uint off, uint n) {
    return n;
}

int writei(inode *ip, uchar *src, uint off, uint n) {
    iupdate(ip);
    return n;
}
