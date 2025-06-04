#ifndef __FS_H__
#define __FS_H__

#include "common.h"
#include "inode.h"

// used for cmd_ls and dirent
typedef struct
{
    uint inum;   // inode number
    uint size;   // size in bytes
    ushort type; // file type: 0: unused, 1: directory, 2: file
    ushort mode; // file mode
    ushort uid;  // user id
    char name[MAXNAME];
} entry; // 32 bytes, 16 entries per block

// 公共函数声明（给 server.c 等外部模块使用）
void sbinit(int ncyl_, int nsec_);
char *get_current_path(void);
void update_current_path(const char *path);

// 主要命令接口
int cmd_f(int ncyl, int nsec);
int cmd_mk(char *name, short mode);
int cmd_mkdir(char *name, short mode);
int cmd_rm(char *name);
int cmd_rmdir(char *name);
int cmd_cd(char *path);
int cmd_ls(entry **entries, int *n);
int cmd_cat(char *name, uchar **buf, uint *len);
int cmd_w(char *name, uint len, const char *data);
int cmd_i(char *name, uint pos, uint len, const char *data);
int cmd_d(char *name, uint pos, uint len);
int cmd_login(int auid);
int cmd_adduser(int uid);

#endif