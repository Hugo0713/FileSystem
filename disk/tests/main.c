#include <stdio.h>
#include <string.h>

#include "log.h"
#include "mintest.h"

int mt_tests_run = 0;
int mt_pass_count = 0;
int mt_fail_count = 0;

void disk_tests();

void all_tests() { mt_run_suite(disk_tests); }

FILE *log_file;

int main(int argc, char **argv)
{
    log_init("disk.log");
    mt_main(all_tests);
    log_close();
    return mt_fail_count;
}