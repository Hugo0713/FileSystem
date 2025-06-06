#include "connection.h"
#include "log.h"
#include "fs.h"
#include <string.h>

static connection_info connections[MAX_CONNECTIONS];
static int system_initialized = 0;

// 初始化连接系统
void init_connection_system(void)
{
#if CONNECTION_DISABLED
    Log("Connection management is disabled");
    return;
#endif
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
#if CONNECTION_DISABLED
    return;
#endif
    if (!system_initialized)
    {
        init_connection_system();
    }

    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS)
    {
        connections[connection_id].active = 1;
        connections[connection_id].current_dir = 0; // 根目录
        connections[connection_id].current_uid = 0; // 管理员
        strcpy(connections[connection_id].current_path, "/");
        Log("Initialized connection %d", connection_id);
    }
}

// 清理连接
void cleanup_connection(int connection_id)
{
#if CONNECTION_DISABLED
    return;
#endif
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
#if CONNECTION_DISABLED
    extern uint current_dir;
    return current_dir; // 如果连接管理被禁用，直接返回当前目录
#endif
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
#if CONNECTION_DISABLED
    extern uint current_uid;
    return current_uid; // 如果连接管理被禁用，直接返回当前用户ID
#endif
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        return connections[connection_id].current_uid;
    }
    return 0; // 默认返回管理员
}

// 获取连接的当前路径
char *get_connection_path(int connection_id)
{
#if CONNECTION_DISABLED
    return get_current_path(); // 如果连接管理被禁用，直接返回当前路径
#endif
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
#if CONNECTION_DISABLED
    // 如果连接管理被禁用，设置全局变量
    extern uint current_dir;
    current_dir = dir;
    return;
#endif
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        connections[connection_id].current_dir = dir;
    }
}

// 设置连接的当前用户
void set_connection_uid(int connection_id, uint uid)
{
#if CONNECTION_DISABLED
    // 如果连接管理被禁用，设置全局变量
    extern uint current_uid;
    current_uid = uid;
    return;
#endif
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        connections[connection_id].current_uid = uid;
    }
}

// 设置连接的当前路径
void set_connection_path(int connection_id, const char *path)
{
#if CONNECTION_DISABLED
    // 如果连接管理被禁用，设置全局路径
    update_current_path(path);
    return;
#endif
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
#if CONNECTION_DISABLED
    // 如果连接管理被禁用，设置全局变量
    extern uint current_dir, current_uid;
    current_dir = dir;
    current_uid = uid;
    return;
#endif
    if (connection_id >= 0 && connection_id < MAX_CONNECTIONS &&
        connections[connection_id].active)
    {
        connections[connection_id].current_dir = dir;
        connections[connection_id].current_uid = uid;
    }
}
