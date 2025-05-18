#include <string.h>

#include "block.h"
#include "common.h"
#include "mintest.h"

int nmeta;

void mock_format() {
    sb.size = 2048;  // 2048 blocks
    int nbitmap = (sb.size / BPB) + 1;
    nmeta = nbitmap + 7;  // some first blocks for metadata

    sb.bmapstart = 1;
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    for (int i = 0; i < sb.size; i += BPB) write_block(BBLOCK(i), buf);  // initialize bitmap blocks

    for (int i = 0; i < nmeta; i += BPB) {
        memset(buf, 0, BSIZE);
        for (int j = 0; j < BPB; j++)
            if (i + j < nmeta) buf[j / 8] |= 1 << (j % 8);  // mark as used
        write_block(BBLOCK(i), buf);
    }
}

mt_test(test_read_write_block) {
    uchar write_buf[BSIZE], read_buf[BSIZE];
    for (int i = 0; i < BSIZE; i++) {
        write_buf[i] = (uchar)i;
    }

    write_block(0, write_buf);
    read_block(0, read_buf);

    for (int i = 0; i < BSIZE; i++) {
        mt_assert(write_buf[i] == read_buf[i]);
    }
    return 0;
}

mt_test(test_zero_block) {
    uchar buf[BSIZE];
    memset(buf, 0xFF, BSIZE);
    write_block(0, buf);

    zero_block(0);
    read_block(0, buf);

    for (int i = 0; i < BSIZE; i++) {
        mt_assert(buf[i] == 0);
    }
    return 0;
}

mt_test(test_allocate_block) {
    mock_format();
    uint bno = allocate_block();
    mt_assert(bno == nmeta);  // the first free block

    uchar buf[BSIZE];
    read_block(bno, buf);

    for (int i = 0; i < BSIZE; i++) {
        mt_assert(buf[i] == 0);  // the block should be zeroed
    }

    // check if the block is marked as used in the bitmap
    read_block(BBLOCK(bno), buf);
    int i = bno % BPB;
    int m = 1 << (i % 8);
    mt_assert((buf[i / 8] & m) != 0);  // the block should be marked as used
    return 0;
}

mt_test(test_allocate_block_all) {
    mock_format();
    for (int i = 0; i < sb.size - nmeta; i++) {
        uint bno = allocate_block();
        mt_assert(bno != 0);
    }
    uint bno = allocate_block();
    mt_assert(bno == 0);  // no more blocks available
    return 0;
}

mt_test(test_free_block) {
    mock_format();
    uint bno = allocate_block();
    mt_assert(bno != 0);

    free_block(bno);

    uchar buf[BSIZE];
    read_block(BBLOCK(bno), buf);

    int i = bno % BPB;
    int m = 1 << (i % 8);
    mt_assert((buf[i / 8] & m) == 0);
    return 0;
}

void block_tests() {
    mt_run_test(test_read_write_block);
    mt_run_test(test_zero_block);
    mt_run_test(test_allocate_block);
    mt_run_test(test_allocate_block_all);
    mt_run_test(test_free_block);
}
