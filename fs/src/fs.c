#include "fs.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"

void sbinit() {
    uchar buf[BSIZE];
    read_block(0, buf);
    memcpy(&sb, buf, sizeof(sb));
}

int cmd_f(int ncyl, int nsec) {
    return E_ERROR;
}

int cmd_mk(char *name, short mode) {
    return E_ERROR;
}

int cmd_mkdir(char *name, short mode) {
    return E_ERROR;
}

int cmd_rm(char *name) {
    return E_SUCCESS;
}

int cmd_cd(char *name) {
    return E_SUCCESS;
}

int cmd_rmdir(char *name) {
    return E_SUCCESS;
}
int cmd_ls(entry **entries, int *n) {
    return E_SUCCESS;
}

int cmd_cat(char *name, uchar **buf, uint *len) {
    return E_SUCCESS;
}

int cmd_w(char *name, uint len, const char *data) {
    return E_SUCCESS;
}

int cmd_i(char *name, uint pos, uint len, const char *data) {
    return E_SUCCESS;
}

int cmd_d(char *name, uint pos, uint len) {
    return E_SUCCESS;
}

int cmd_login(int auid) {
    return E_SUCCESS;
}
