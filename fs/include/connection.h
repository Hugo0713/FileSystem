#ifndef CONNECTION_H
#define CONNECTION_H

#include "common.h"

#define MAX_CONNECTIONS 16

// 连接状态结构
typedef struct
{
    int active;              // 连接是否活跃
    uint current_dir;        // 当前目录inode编号
    uint current_uid;        // 当前用户ID
    char current_path[1024]; // 当前路径字符串
} connection_info;

// 连接管理函数
void init_connection_system(void);
void init_connection(int connection_id);
void cleanup_connection(int connection_id);

// 连接状态访问函数
uint get_connection_dir(int connection_id);
uint get_connection_uid(int connection_id);
char *get_connection_path(int connection_id);

// 连接状态设置函数
void set_connection_dir(int connection_id, uint dir);
void set_connection_uid(int connection_id, uint uid);
void set_connection_path(int connection_id, const char *path);
void set_connection_state(int connection_id, uint dir, uint uid);

#endif