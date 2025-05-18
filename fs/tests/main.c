#include <stdio.h>
#include <string.h>

#include "log.h"
#include "mintest.h"

int mt_tests_run = 0;
int mt_pass_count = 0;
int mt_fail_count = 0;

void block_tests();
void inode_tests();
void fs_tests();

void all_tests() {
    mt_run_suite(block_tests);
    mt_run_suite(inode_tests);
    mt_run_suite(fs_tests);
}

FILE *log_file;

int main(int argc, char **argv) {
    log_init("fs.log");
    void (*test)() = all_tests;
    if (argc > 1) {
        if (strcmp(argv[1], "block") == 0) {
            test = block_tests;
        } else if (strcmp(argv[1], "inode") == 0) {
            test = inode_tests;
        } else if (strcmp(argv[1], "fs") == 0) {
            test = fs_tests;
        }
    }
    mt_main(test);
    log_close();
    return mt_fail_count;
}