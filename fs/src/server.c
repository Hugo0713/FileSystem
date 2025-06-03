#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "log.h"
#include "tcp_utils.h"
#include "block.h"
#include "common.h"
#include "fs.h"
#include "user.h"

int ncyl, nsec;

// 将权限模式转换为字符串
static char *mode_to_string(short mode)
{
    static char mode_str[10];
    mode_str[0] = (mode & 0400) ? 'r' : '-'; // 所有者读
    mode_str[1] = (mode & 0200) ? 'w' : '-'; // 所有者写
    mode_str[2] = (mode & 0100) ? 'x' : '-'; // 所有者执行
    mode_str[3] = (mode & 0040) ? 'r' : '-'; // 组读
    mode_str[4] = (mode & 0020) ? 'w' : '-'; // 组写
    mode_str[5] = (mode & 0010) ? 'x' : '-'; // 组执行
    mode_str[6] = (mode & 0004) ? 'r' : '-'; // 其他读
    mode_str[7] = (mode & 0002) ? 'w' : '-'; // 其他写
    mode_str[8] = (mode & 0001) ? 'x' : '-'; // 其他执行
    mode_str[9] = '\0';
    return mode_str;
}

// 获取文件类型字符
static char get_file_type_char(short type)
{
    switch (type)
    {
    case T_DIR:
        return 'd';
    case T_FILE:
        return '-';
    default:
        return '?';
    }
}

int handle_f(tcp_buffer *wb, char *args, int len)
{
    if (cmd_f(ncyl, nsec) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Format success");
    }
    else
    {
        reply_with_no(wb, "Failed to format", strlen("Failed to format"));
        Warn("Failed to format");
    }
    return 0;
}

int handle_mk(tcp_buffer *wb, char *args, int len)
{
    char *name = args;
    short mode = 0644;
    if (cmd_mk(name, mode) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Create file success: %s", name);
    }
    else
    {
        reply_with_no(wb, "Failed to create file", strlen("Failed to create file"));
        Warn("Failed to create file: %s", name);
    }
    return 0;
}

int handle_mkdir(tcp_buffer *wb, char *args, int len)
{
    char *name = args;
    short mode = 0755;
    if (cmd_mkdir(name, mode) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Create directory success: %s", name);
    }
    else
    {
        reply_with_no(wb, "Failed to create directory", strlen("Failed to create directory"));
        Warn("Failed to create directory: %s", name);
    }
    return 0;
}

int handle_rm(tcp_buffer *wb, char *args, int len)
{
    char *name = args;
    if (cmd_rm(name) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Remove file success: %s", name);
    }
    else
    {
        reply_with_no(wb, "Failed to remove file", strlen("Failed to remove file"));
        Warn("Failed to remove file: %s", name);
    }
    return 0;
}

int handle_cd(tcp_buffer *wb, char *args, int len)
{
    char *name = args;
    if (cmd_cd(name) == E_SUCCESS)
    {
        char *current_path = get_current_path();
        char response[512];
        snprintf(response, sizeof(response), "Changed to %s", current_path);
        reply_with_yes(wb, response, strlen(response));
        Log("Change directory success: %s", name);
    }
    else
    {
        reply_with_no(wb, "Failed to change directory", strlen("Failed to change directory"));
        Warn("Failed to change directory: %s", name);
    }
    return 0;
}

int handle_rmdir(tcp_buffer *wb, char *args, int len)
{
    char *name = args;
    if (cmd_rmdir(name) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Remove directory success: %s", name);
    }
    else
    {
        reply_with_no(wb, "Failed to remove directory", strlen("Failed to remove directory"));
        Warn("Failed to remove directory: %s", name);
    }
    return 0;
}

int handle_ls(tcp_buffer *wb, char *args, int len)
{
    entry *entries = NULL;
    int n = 0;

    if (cmd_ls(&entries, &n) != E_SUCCESS)
    {
        reply_with_no(wb, "Failed to list files", strlen("Failed to list files"));
        Warn("Failed to list files");
        return 0;
    }

    if (n == 0)
    {
        reply_with_yes(wb, "", 0);
        Log("Directory is empty");
        return 0;
    }

    // 构建文件列表字符串
    char list_data[8192] = "";

    // 添加表头
    strcat(list_data, "Permissions    UID  Size  Name\n");
    strcat(list_data, "-------------------------------------\n");

    // 详细格式输出
    for (int i = 0; i < n; i++)
    {
        char entry_info[512];
        char type_char = get_file_type_char(entries[i].type);
        char *mode_str = mode_to_string(entries[i].mode);

        snprintf(entry_info, sizeof(entry_info),
                 "%c%s %3d %8u %s\n",
                 type_char,        // 文件类型
                 mode_str,         // 权限
                 entries[i].uid,   // 所有者UID
                 entries[i].size,  // 文件大小
                 entries[i].name); // 文件名

        strcat(list_data, entry_info);
    }

    reply_with_yes(wb, list_data, strlen(list_data));
    Log("List files success, %d entries ", n);

    if (entries)
        free(entries);
    return 0;
}

int handle_cat(tcp_buffer *wb, char *args, int len)
{
    char *name = args;
    uchar *buf = NULL;
    uint file_len;

    if (cmd_cat(name, &buf, &file_len) == E_SUCCESS)
    {
        reply_with_yes(wb, (char *)buf, file_len);
        Log("Read file success: %s, length: %d", name, file_len);
        free(buf);
    }
    else
    {
        reply_with_no(wb, "Failed to read file", strlen("Failed to read file"));
        Warn("Failed to read file: %s", name);
    }
    return 0;
}

int handle_w(tcp_buffer *wb, char *args, int len)
{
    char *name = strtok(args, " ");
    char *len_str = strtok(NULL, " ");
    char *data = strtok(NULL, " ");

    if (!name || !len_str || !data)
    {
        reply_with_no(wb, "Invalid arguments for write", strlen("Invalid arguments for write"));
        Warn("Invalid arguments for write");
        return 0;
    }

    uint data_len = atoi(len_str);
    if (cmd_w(name, data_len, data) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Write file success: %s, length: %d", name, data_len);
    }
    else
    {
        reply_with_no(wb, "Failed to write file", strlen("Failed to write file"));
        Warn("Failed to write file: %s", name);
    }
    return 0;
}

int handle_i(tcp_buffer *wb, char *args, int len)
{
    char *name = strtok(args, " ");
    char *pos_str = strtok(NULL, " ");
    char *len_str = strtok(NULL, " ");
    char *data = strtok(NULL, "");

    if (!name || !pos_str || !len_str || !data)
    {
        reply_with_no(wb, "Invalid arguments for insert", strlen("Invalid arguments for insert"));
        Warn("Invalid arguments for insert");
        return 0;
    }

    uint pos = atoi(pos_str);
    uint data_len = atoi(len_str);

    if (cmd_i(name, pos, data_len, data) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Insert data success: %s, pos: %d, length: %d", name, pos, data_len);
    }
    else
    {
        reply_with_no(wb, "Failed to insert data", strlen("Failed to insert data"));
        Warn("Failed to insert data: %s", name);
    }
    return 0;
}

int handle_d(tcp_buffer *wb, char *args, int len)
{
    char *name = strtok(args, " ");
    char *pos_str = strtok(NULL, " ");
    char *len_str = strtok(NULL, " ");

    if (!name || !pos_str || !len_str)
    {
        reply_with_no(wb, "Invalid arguments for delete", strlen("Invalid arguments for delete"));
        Warn("Invalid arguments for delete");
        return 0;
    }

    uint pos = atoi(pos_str);
    uint data_len = atoi(len_str);

    if (cmd_d(name, pos, data_len) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Delete data success: %s, pos: %d, length: %d", name, pos, data_len);
    }
    else
    {
        reply_with_no(wb, "Failed to delete data", strlen("Failed to delete data"));
        Warn("Failed to delete data: %s", name);
    }
    return 0;
}

int handle_e(tcp_buffer *wb, char *args, int len)
{
    const char *msg = "Bye!";
    reply(wb, msg, strlen(msg) + 1);
    Log("Exit");
    return -1;
}

int handle_login(tcp_buffer *wb, char *args, int len)
{
    int uid = atoi(args);
    if (cmd_login(uid) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Login success: uid %d", uid);
    }
    else
    {
        reply_with_no(wb, "Failed to login", strlen("Failed to login"));
        Warn("Failed to login: uid %d", uid);
    }
    return 0;
}

int handle_adduser(tcp_buffer *wb, char *args, int len)
{
    // 检查参数是否有效
    if (!args || len <= 0)
    {
        reply_with_no(wb, "Invalid arguments for adduser", strlen("Invalid arguments for adduser"));
        Warn("Invalid arguments for adduser");
        return 0;
    }
    // 解析用户ID
    int uid = atoi(args);
    if (uid < 1 || uid >= MAX_USERS)
    {
        reply_with_no(wb, "Invalid user ID", strlen("Invalid user ID"));
        Warn("Invalid user ID: %d", uid);
        return 0;
    }

    if (cmd_adduser(uid) == E_SUCCESS)
    {
        reply_with_yes(wb, NULL, 0);
        Log("Add user success: uid %d", uid);
    }
    else
    {
        reply_with_no(wb, "Failed to create user", strlen("Failed to create user"));
        Warn("Failed to create user: uid %d", uid);
    }
    return 0;
}

int handle_pwd(tcp_buffer *wb, char *args, int len)
{
    char *current_path = get_current_path();
    if (current_path)
    {
        reply_with_yes(wb, current_path, strlen(current_path));
        Log("Current directory: %s", current_path);
    }
    else
    {
        reply_with_no(wb, "Failed to get current directory", strlen("Failed to get current directory"));
        Warn("Failed to get current directory");
    }
    return 0;
}

static struct
{
    const char *name;
    int (*handler)(tcp_buffer *, char *, int);
} cmd_table[] = {
    {"f", handle_f},
    {"mk", handle_mk},
    {"mkdir", handle_mkdir},
    {"rm", handle_rm},
    {"cd", handle_cd},
    {"rmdir", handle_rmdir},
    {"ls", handle_ls},
    {"cat", handle_cat},
    {"w", handle_w},
    {"i", handle_i},
    {"d", handle_d},
    {"e", handle_e},
    {"login", handle_login},
    {"adduser", handle_adduser},
    {"pwd", handle_pwd}};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

void on_connection(int id)
{
    Log("Client %d connected", id);
}

int on_recv(int id, tcp_buffer *wb, char *msg, int len)
{
    // char *p = strtok(msg, " \r\n");
    char *newline = strchr(msg, '\n');
    if (newline)
        *newline = '\0';
    newline = strchr(msg, '\r');
    if (newline)
        *newline = '\0';

    char *p = strtok(msg, " ");
    int ret = 1;
    for (int i = 0; i < NCMD; i++)
        if (p && strcmp(p, cmd_table[i].name) == 0)
        {
            ret = cmd_table[i].handler(wb, p + strlen(p) + 1, len - strlen(p) - 1);
            break;
        }
    if (ret == 1)
    {
        static char unk[] = "Unknown command";
        buffer_append(wb, unk, sizeof(unk));
    }
    if (ret < 0)
    {
        return -1;
    }
    return 0;
}

void cleanup(int id)
{
    Log("Client %d disconnected", id);
}

FILE *log_file;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <disk_port> [fs_port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int disk_port = atoi(argv[1]);
    int fs_port = argc > 2 ? atoi(argv[2]) : 666;

    log_init("fs.log");

    assert(BSIZE % sizeof(dinode) == 0);

    // 连接到磁盘服务器
    if (init_disk_connection("localhost", disk_port) < 0)
    {
        Error("Failed to connect to disk server");
        exit(EXIT_FAILURE);
    }

    // 获取磁盘信息并初始化文件系统
    get_disk_info(&ncyl, &nsec);
    sbinit(ncyl, nsec);

    Log("File system server starting on port %d, connected to disk server on port %d", fs_port, disk_port);

    // 启动TCP服务器
    tcp_server server = server_init(fs_port, 1, on_connection, on_recv, cleanup);
    server_run(server);

    // never reached
    cleanup_disk_connection();
    log_close();
}