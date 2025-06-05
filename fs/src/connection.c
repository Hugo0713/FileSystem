#include "connection.h"
#include "log.h"
#include <string.h>

static connection_info connections[MAX_CONNECTIONS];
static int system_initialized = 0;

// 初始化连接系统
void init_connection_system(void)
{
    if (system_initialized)
        return;

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        connections[i].active = 0;
        connections[i].current_dir = 0;
        connections[i].current_uid = 0;
        strcpy(connections[i].current_path, "/");
    }
    system_initialized = 1;
    Log("Connection system initialized");
}

// 初始化单个连接
void init_connection(int connection_id)
{
    if (!system_initialized)
    {
        init_connection_system();
    }

    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS)
    {
        connections[connection_id].active = 1;
        connections[connection_id].current_dir = 0; // 根目录
        connections[connection_id].current_uid = 0; // 匿名用户
        strcpy(connections[connection_id].current_path, "/");
        Log("Initialized connection %d", connection_id);
    }
}

// 清理连接
void cleanup_connection(int connection_id)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS)
    {
        connections[connection_id].active = 0;
        connections[connection_id].current_dir = 0;
        connections[connection_id].current_uid = 0;
        strcpy(connections[connection_id].current_path, "/");
        Log("Cleaned up connection %d", connection_id);
    }
}

// 获取连接的当前目录
uint get_connection_dir(int connection_id)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        return connections[connection_id].current_dir;
    }
    return 0; // 默认返回根目录
}

// 获取连接的当前用户
uint get_connection_uid(int connection_id)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        return connections[connection_id].current_uid;
    }
    return 0; // 默认返回匿名用户
}

// 获取连接的当前路径
char *get_connection_path(int connection_id)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        return connections[connection_id].current_path;
    }
    return "/"; // 默认返回根路径
}

// 设置连接的当前目录
void set_connection_dir(int connection_id, uint dir)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        connections[connection_id].current_dir = dir;
    }
}

// 设置连接的当前用户
void set_connection_uid(int connection_id, uint uid)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        connections[connection_id].current_uid = uid;
    }
}

// 设置连接的当前路径
void set_connection_path(int connection_id, const char *path)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active && path)
    {
        strncpy(connections[connection_id].current_path, path, 1023);
        connections[connection_id].current_path[1023] = '\0';
    }
}

// 设置连接状态
void set_connection_state(int connection_id, uint dir, uint uid)
{
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        connections[connection_id].current_dir = dir;
        connections[connection_id].current_uid = uid;
    }
}