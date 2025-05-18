#include <string.h>
#include "inode.h"
#include "block.h"
#include "common.h"
#include "mintest.h"
#include "fs.h"
#include <time.h>
#include <stdlib.h>

inline static void format() {
    cmd_login(1);
    cmd_f(1024, 63);
}

mt_test(test_ialloc) {
    format();
    inode *ip = ialloc(T_FILE);
    mt_assert(ip != NULL);
    mt_assert(ip->type == T_FILE);
    mt_assert(ip->size == 0);
    mt_assert(ip->blocks == 0);
    iput(ip);

    ip = ialloc(T_DIR);
    mt_assert(ip != NULL);
    mt_assert(ip->type == T_DIR);
    iput(ip);
    return 0;
}

mt_test(test_iget) {
    format();
    inode *ip = ialloc(T_FILE);
    mt_assert(ip != NULL);
    uint inum = ip->inum;
    iput(ip);

    inode *retrieved = iget(inum);
    mt_assert(retrieved != NULL);
    mt_assert(retrieved->inum == inum);
    iput(retrieved);
    return 0;
}


mt_test(test_iupdate) {
    format();
    inode *ip = ialloc(T_FILE);
    mt_assert(ip != NULL);
    uint inum = ip->inum;
    ip->size = 1024;
    iupdate(ip);
    iput(ip);

    ip = iget(inum);
    mt_assert(ip->size == 1024);
    ip->blocks = 10;
    iupdate(ip);
    iput(ip);

    inode *retrieved = iget(inum);
    mt_assert(retrieved->size == 1024);
    mt_assert(retrieved->blocks == 10);
    iput(retrieved);
    return 0;
}

mt_test(test_writei) {
    format();
    inode *ip = ialloc(T_FILE);
    mt_assert(ip != NULL);

    // Write data to the inode
    uchar data[] = "Hello, World!";
    int bytes_written = writei(ip, data, 0, sizeof(data));
    mt_assert(bytes_written == sizeof(data));
    mt_assert(ip->size == sizeof(data));
    mt_assert(ip->blocks > 0);

    // Verify the written data
    uchar buf[sizeof(data)];
    int bytes_read = readi(ip, buf, 0, sizeof(data));
    mt_assert(bytes_read == sizeof(data));
    mt_assert(memcmp(data, buf, sizeof(data)) == 0);

    iput(ip);
    return 0;
}

mt_test(test_readi) {
    format();
    inode *ip = ialloc(T_FILE);
    mt_assert(ip != NULL);

    // Write data to the inode
    uchar data[] = "Test Read Functionality";
    writei(ip, data, 0, sizeof(data));

    // Read back the data
    uchar buf[sizeof(data)];
    int bytes_read = readi(ip, buf, 0, sizeof(data));
    mt_assert(bytes_read == sizeof(data));
    mt_assert(memcmp(data, buf, sizeof(data)) == 0);

    // Test reading beyond the file size
    uchar extra_buf[10];
    bytes_read = readi(ip, extra_buf, sizeof(data), sizeof(extra_buf));
    mt_assert(bytes_read == 0);  // No data should be read beyond EOF

    iput(ip);
    return 0;
}

mt_test(test_read_write_mixed) {
    format();
    inode *ip = ialloc(T_FILE);
    mt_assert(ip != NULL);

    // Write initial data to the inode
    uchar initial_data[] = "Initial Data";
    int bytes_written = writei(ip, initial_data, 0, sizeof(initial_data));
    mt_assert(bytes_written == sizeof(initial_data));
    mt_assert(ip->size == sizeof(initial_data));

    // Read back the initial data
    uchar read_buf[sizeof(initial_data)];
    int bytes_read = readi(ip, read_buf, 0, sizeof(initial_data));
    mt_assert(bytes_read == sizeof(initial_data));
    mt_assert(memcmp(initial_data, read_buf, sizeof(initial_data)) == 0);

    // Overwrite part of the data
    uchar overwrite_data[] = "Overwrite";
    bytes_written = writei(ip, overwrite_data, 8, sizeof(overwrite_data));
    mt_assert(bytes_written == sizeof(overwrite_data));
    mt_assert(ip->size == 8 + sizeof(overwrite_data));

    // Read back the mixed data
    uchar mixed_buf[sizeof(initial_data) + sizeof(overwrite_data)];
    bytes_read = readi(ip, mixed_buf, 0, ip->size);
    mt_assert(bytes_read == ip->size);

    uchar expected_data[] = "Initial Overwrite";
    mt_assert(memcmp(mixed_buf, expected_data, ip->size) == 0);

    iput(ip);
    return 0;
}


mt_test(test_random_binary_read_write) {
    format();
    inode *ip = ialloc(T_FILE);
    mt_assert(ip != NULL);

    // Generate random binary data larger than direct blocks
    const uint data_size = (NDIRECT + 5) * BSIZE;
    uchar *random_data = malloc(data_size);
    mt_assert(random_data != NULL);

    srand(time(NULL));
    for (uint i = 0; i < data_size; i++) {
        random_data[i] = rand() % 256;
    }

    int bytes_written = writei(ip, random_data, 0, data_size);
    mt_assert(bytes_written == data_size);
    mt_assert(ip->size == data_size);

    uchar *read_buf = malloc(data_size);
    mt_assert(read_buf != NULL);
    int bytes_read = readi(ip, read_buf, 0, data_size);
    mt_assert(bytes_read == data_size);

    mt_assert(memcmp(random_data, read_buf, data_size) == 0);

    // Write again with different offset
    int offset = data_size / 2;
    bytes_written = writei(ip, random_data, offset, data_size);
    mt_assert(bytes_written == data_size);
    mt_assert(ip->size == data_size + offset);
    free(read_buf);
    
    read_buf = malloc(ip->size);
    mt_assert(read_buf != NULL);
    bytes_read = readi(ip, read_buf, 0, ip->size);
    mt_assert(bytes_read == ip->size);

    mt_assert(memcmp(random_data, read_buf + offset, data_size) == 0);
    mt_assert(memcmp(random_data, read_buf, offset) == 0);

    free(random_data);
    free(read_buf);
    iput(ip);
    return 0;
}

void inode_tests() {
    mt_run_test(test_iget);
    mt_run_test(test_ialloc);
    mt_run_test(test_iupdate);
    mt_run_test(test_writei);
    mt_run_test(test_readi);
    mt_run_test(test_read_write_mixed);
    mt_run_test(test_random_binary_read_write);
}