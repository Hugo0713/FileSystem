#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "block.h"
#include "common.h"
#include "fs.h"
#include "inode.h"
#include "mintest.h"

static void format() {
    cmd_login(1);
    cmd_f(1024, 63);
}

static int exist(char *name, int type) {
    entry *entries;
    int n;
    cmd_ls(&entries, &n);
    int found = 0;
    for (int i = 0; i < n; i++)
        if (strcmp(entries[i].name, name) == 0 && entries[i].type == type) {
            found = 1;
            break;
        }
    free(entries);
    return found;
}

mt_test(test_cmd_ls) {
    format();
    entry *entries;
    int n;
    cmd_ls(&entries, &n);
    mt_assert(n == 0);
    free(entries);
    
    cmd_mk("a", 0b1111);
    cmd_mk("b", 0b1111);
    cmd_mkdir("c", 0b1111);

    entry *entries2;
    int n2;
    cmd_ls(&entries2, &n2);
    mt_assert(n2 == 3);
    mt_assert(exist("a", T_FILE));
    mt_assert(exist("b", T_FILE));
    mt_assert(exist("c", T_DIR));
    free(entries2);
    return 0;
}

mt_test(test_cmd_mk) {
    format();
    int rc = cmd_mk("testfile", 0b1111);
    mt_assert(rc == E_SUCCESS);

    mt_assert(exist("testfile", T_FILE));

    return 0;
}

mt_test(test_cmd_mk_invalid) {
    format();
    int rc = cmd_mk("testfile", 0b1111);
    mt_assert(rc == E_SUCCESS);
    mt_assert(cmd_mk("testfile", 0b1111) == E_ERROR);
    return 0;
}

mt_test(test_cmd_mkdir) {
    format();
    mt_assert(cmd_mkdir("mydir", 0b1111) == E_SUCCESS);

    mt_assert(exist("mydir", T_DIR));

    mt_assert(cmd_cd("mydir") == E_SUCCESS);
    return 0;
}

mt_test(test_cmd_mkdir_invalid) {
    format();
    mt_assert(cmd_mkdir("mydir", 0b1111) == E_SUCCESS);
    mt_assert(cmd_mkdir("mydir", 0b1111) == E_ERROR);
    return 0;
}

mt_test(test_cmd_cd_absolute) {
    format();
    cmd_mkdir("mydir", 0b1111);
    mt_assert(cmd_cd("/mydir") == E_SUCCESS);
    mt_assert(cmd_cd("/") == E_SUCCESS);

    mt_assert(exist("mydir", T_DIR));

    return 0;
}

mt_test(test_cmd_cd_relative) {
    format();
    cmd_mkdir("mydir", 0b1111);
    mt_assert(cmd_cd("mydir") == E_SUCCESS);
    mt_assert(cmd_cd("..") == E_SUCCESS);

    mt_assert(exist("mydir", T_DIR));

    return 0;
}

mt_test(test_cmd_rm) {
    format();
    cmd_mk("victim", 0b1111);
    mt_assert(cmd_rm("victim") == E_SUCCESS);
    mt_assert(!exist("victim", T_FILE));
    mt_assert(cmd_rm("ghost") == E_ERROR);
    return 0;
}

mt_test(test_cmd_rmdir_with_files) {
    format();
    cmd_mkdir("mydir", 0b1111);
    cmd_cd("mydir");
    cmd_mk("file.txt", 0b1111);
    cmd_cd("..");
    mt_assert(cmd_rmdir("mydir") == E_ERROR);

    mt_assert(exist("mydir", T_DIR));

    return 0;
}

mt_test(test_file_lifecycle) {
    format();
    cmd_mk("data.txt", 0b1111);
    mt_assert(cmd_w("data.txt", 11, "hello world") == E_SUCCESS);

    uchar *buf = NULL;
    uint len;
    mt_assert(cmd_cat("data.txt", &buf, &len) == E_SUCCESS);
    mt_assert(memcmp(buf, "hello world", 11) == 0);
    free(buf);

    mt_assert(cmd_rm("data.txt") == E_SUCCESS);
    return 0;
}

mt_test(test_small_file_ops) {
    format();
    cmd_mk("small.txt", 0b1111);
    mt_assert(cmd_w("small.txt", 5, "hello") == E_SUCCESS);
    uchar *buf = NULL;
    uint len;
    mt_assert(cmd_cat("small.txt", &buf, &len) == E_SUCCESS);
    mt_assert(memcmp(buf, "hello", 5) == 0);
    free(buf);
    mt_assert(cmd_i("small.txt", 0, 5, "world") == E_SUCCESS);
    mt_assert(cmd_cat("small.txt", &buf, &len) == E_SUCCESS);
    mt_assert(memcmp(buf, "worldhello", 10) == 0);
    free(buf);
    mt_assert(cmd_d("small.txt", 3, 5) == E_SUCCESS);
    mt_assert(cmd_cat("small.txt", &buf, &len) == E_SUCCESS);
    mt_assert(memcmp(buf, "worlo", 5) == 0);
    free(buf);
    mt_assert(cmd_rm("small.txt") == E_SUCCESS);
    return 0;
}

static void generate_random_name(char *name, int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < length; i++) {
        name[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    name[length] = '\0';
}

mt_test(test_folder_tree_operations) {
    format();
    srand(time(NULL));

    char dir1[6], dir2[6], dir3[6];
    generate_random_name(dir1, 5);
    generate_random_name(dir2, 5);
    generate_random_name(dir3, 5);

    mt_assert(cmd_mkdir(dir1, 0b1111) == E_SUCCESS);
    mt_assert(exist(dir1, T_DIR));

    mt_assert(cmd_cd(dir1) == E_SUCCESS);
    mt_assert(cmd_mkdir(dir2, 0b1111) == E_SUCCESS);
    mt_assert(exist(dir2, T_DIR));

    mt_assert(cmd_cd(dir2) == E_SUCCESS);
    mt_assert(cmd_mkdir(dir3, 0b1111) == E_SUCCESS);
    mt_assert(exist(dir3, T_DIR));

    mt_assert(cmd_cd("/") == E_SUCCESS);

    char path[256];
    snprintf(path, sizeof(path), "/%s/%s/%s", dir1, dir2, dir3);
    mt_assert(cmd_cd(path) == E_SUCCESS);

    entry *entries;
    int n;
    cmd_ls(&entries, &n);
    mt_assert(n == 0);
    free(entries);

    mt_assert(cmd_cd("/") == E_SUCCESS);
    return 0;
}

mt_test(test_folder_tree_with_rm) {
    format();
    srand(time(NULL));

    char dirs[3][6], files[3][6];
    for (int i = 0; i < 3; i++) {
        generate_random_name(dirs[i], 5);
        mt_assert(cmd_mkdir(dirs[i], 0b1111) == E_SUCCESS);
        generate_random_name(files[i], 5);
        mt_assert(cmd_mk(files[i], 0b1111) == E_SUCCESS);
    }

    int idx_dir = rand() % 3;
    int idx_file = rand() % 3;

    mt_assert(cmd_rmdir(dirs[idx_dir]) == E_SUCCESS);
    mt_assert(!exist(dirs[idx_dir], T_DIR));

    mt_assert(cmd_rm(files[idx_file]) == E_SUCCESS);
    mt_assert(!exist(files[idx_file], T_FILE));

    for (int i = 0; i < 3; i++) {
        if (i != idx_dir) {
            mt_assert(exist(dirs[i], T_DIR));
        }
        if (i != idx_file) {
            mt_assert(exist(files[i], T_FILE));
        }
    }
    
    return 0;
}


void fs_tests() {
    mt_run_test(test_cmd_ls);
    mt_run_test(test_cmd_mk);
    mt_run_test(test_cmd_mk_invalid);
    mt_run_test(test_cmd_mkdir);
    mt_run_test(test_cmd_mkdir_invalid);
    mt_run_test(test_cmd_cd_absolute);
    mt_run_test(test_cmd_cd_relative);
    mt_run_test(test_cmd_rm);
    mt_run_test(test_cmd_rmdir_with_files);
    mt_run_test(test_file_lifecycle);
    mt_run_test(test_small_file_ops);
    mt_run_test(test_folder_tree_operations);
    mt_run_test(test_folder_tree_with_rm);
}
