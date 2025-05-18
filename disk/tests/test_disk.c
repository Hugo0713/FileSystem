#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "mintest.h"

inline static void setup_disk() { init_disk("test_disk.img", 10, 10, 0); }

mt_test(test_cmd_i)
{
    setup_disk();
    int ncyl, nsec;
    int result = cmd_i(&ncyl, &nsec);
    mt_assert(result == 0);
    mt_assert(ncyl == 10);
    mt_assert(nsec == 10);
    close_disk();
    return 0;
}

mt_test(test_cmd_wr)
{
    setup_disk();
    char write_buf[512];
    char read_buf[512];
    for (int i = 0; i < 512; i++)
    {
        write_buf[i] = 'A' + (i % 26);
    }
    write_buf[511] = '\0';

    int write_result = cmd_w(0, 0, 512, write_buf);
    mt_assert(write_result == 0);

    int read_result = cmd_r(0, 0, read_buf);
    mt_assert(read_result == 0);
    mt_assert(memcmp(write_buf, read_buf, 512) == 0);
    close_disk();
    return 0;
}

mt_test(test_wwr)
{
    setup_disk();
    char write_buf[512];
    char read_buf[512];

    for (int i = 0; i < 512; i++)
    {
        write_buf[i] = 'A' + ((i + 10) % 26);
    }
    write_buf[511] = '\0';

    int write_result = cmd_w(1, 1, 512, write_buf);
    mt_assert(write_result == 0);

    for (int i = 0; i < 512; i++)
    {
        write_buf[i] = 'A' + ((i + 5) % 26);
    }
    write_buf[511] = '\0';

    write_result = cmd_w(1, 1, 512, write_buf);
    mt_assert(write_result == 0);

    int read_result = cmd_r(1, 1, read_buf);
    mt_assert(read_result == 0);
    mt_assert(memcmp(write_buf, read_buf, 512) == 0);
    close_disk();
    return 0;
}

mt_test(test_w_partial)
{
    setup_disk();
    char data[512] = "Partial write test data hahaha";
    char read_buf[512];
    memset(data + 16, 0, sizeof(data) - 16);

    int write_result = cmd_w(2, 2, 16, data);
    mt_assert(write_result == 0);

    int read_result = cmd_r(2, 2, read_buf);
    mt_assert(read_result == 0);
    mt_assert(memcmp(data, read_buf, 512) == 0);
    close_disk();
    return 0;
}

mt_test(test_non_ascii)
{
    setup_disk();
    char write_buf[512];
    char read_buf[512];

    for (int i = 0; i < 512; i++)
    {
        write_buf[i] = 0xFF - (i % 256);
    }

    int write_result = cmd_w(3, 3, 512, write_buf);
    mt_assert(write_result == 0);

    int read_result = cmd_r(3, 3, read_buf);
    mt_assert(read_result == 0);
    mt_assert(memcmp(write_buf, read_buf, 512) == 0);
    close_disk();
    return 0;
}

mt_test(test_out_of_bounds)
{
    setup_disk();
    char buf[512];

    int read_result = cmd_r(10, 10, buf);
    mt_assert(read_result != 0);

    read_result = cmd_r(-1, -1, buf);
    mt_assert(read_result != 0);

    int write_result = cmd_w(10, 10, sizeof(buf), buf);
    mt_assert(write_result != 0);

    write_result = cmd_w(-1, -1, sizeof(buf), buf);
    mt_assert(write_result != 0);

    write_result = cmd_w(0, 0, 666, buf);
    mt_assert(write_result != 0);
    close_disk();
    return 0;
}

void disk_tests()
{
    mt_run_test(test_cmd_i);
    mt_run_test(test_cmd_wr);
    mt_run_test(test_wwr);
    mt_run_test(test_w_partial);
    mt_run_test(test_non_ascii);
    mt_run_test(test_out_of_bounds);
}
