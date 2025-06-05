#ifndef USER_H
#define USER_H

#include "common.h"

#define MAX_USERS 256
#define USER_INFO_INODE 1  // 用户信息存储在inode 1
#define ADMIN_UID 0        // 管理员用户ID
#define MAX_CONNECTIONS 10 // 最大连接用户数

// 简化的用户信息结构
typedef struct
{
    uint uid;           // 用户ID
    short active;       // 是否激活 (1=激活, 0=未使用)
    short is_admin;     // 是否为管理员 (1=管理员, 0=普通用户)
    uint home_dir_inum; // 用户主目录inode编号
} user_info;

// 用户系统函数
void init_user_system(void);
int create_user(uint uid);
int user_exists(uint uid);
int is_admin_user(uint uid);
user_info *get_user_info(uint uid);

// 权限检查函数
int check_file_permission(uint file_inum, uint uid, int operation);

// 权限操作常量
#define PERM_READ 1
#define PERM_WRITE 2

#endif