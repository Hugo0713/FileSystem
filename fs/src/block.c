#include "block.h"

#include <string.h>

#include "common.h"
#include "log.h"

superblock sb;

void zero_block(uint bno) {
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    write_block(bno, buf);
}

uint allocate_block() {
    Warn("Out of blocks");
    return 0;
}

void free_block(uint bno) {

}

void get_disk_info(int *ncyl, int *nsec) {

}

void read_block(int blockno, uchar *buf) {

}

void write_block(int blockno, uchar *buf) {

}
