#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "tcp_utils.h"

// 简化的客户端状态 - 不缓存路径
typedef struct
{
    int logged_in;     // 是否已登录
    int uid;           // 用户ID
    char username[32]; // 用户名
} client_state;

// 静默获取当前路径
char* get_current_path_quietly(tcp_client client)
{
    static char path[256] = "/"; // 静态变量保存路径
    
    // 发送 pwd 命令
    client_send(client, "pwd", 4);
    
    // 接收响应
    char response[4096];
    int n = client_recv(client, response, sizeof(response));
    response[n] = '\0';
    
    // 解析路径
    if (strncmp(response, "Yes", 3) == 0 && strlen(response) > 3)
    {
        // "Yes /path" 格式
        char *path_start = response + 4; // 跳过 "Yes "
        char *newline = strchr(path_start, '\n');
        if (newline) *newline = '\0';
        
        strncpy(path, path_start, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    // 如果获取失败，保持原有路径
    return path;
}

// 动态提示符 - 每次都获取当前路径
void print_prompt(tcp_client client, client_state *state)
{
    if (state->logged_in)
    {
        char *path = get_current_path_quietly(client);
        printf("%s@fs:%s$ ", state->username, path);
    }
    else
    {
        printf("fs@guest:/$ ");
    }
    fflush(stdout);
}

// 本地命令
int handle_local_command(const char *cmd, client_state *state)
{
    if (strcmp(cmd, "help") == 0)
    {
        printf("Available commands:\n");
        printf("  f                    - Format file system\n");
        printf("  mk <name>            - Create file\n");
        printf("  mkdir <name>         - Create directory\n");
        printf("  rm <name>            - Remove file\n");
        printf("  rmdir <name>         - Remove directory\n");
        printf("  cd <path>            - Change directory\n");
        printf("  ls                   - List directory contents\n");
        printf("  cat <name>           - Display file contents\n");
        printf("  w <name> <len> <data> - Write to file\n");
        printf("  i <name> <pos> <len> <data> - Insert into file\n");
        printf("  d <name> <pos> <len> - Delete from file\n");
        printf("  login <uid>          - Login as user\n");
        printf("  adduser <uid>        - Add new user (admin only)\n");
        printf("  pwd                  - Show current directory\n");
        printf("  whoami               - Show current user\n");
        printf("  e                    - Exit\n");
        printf("  help                 - Show this help\n");
        return 1;
    }

    if (strcmp(cmd, "clear") == 0)
    {
        system("clear");
        return 1;
    }
    
    return 0;
}

// 响应处理
void handle_response(char *response, char *last_cmd, client_state *state)
{
    if (strncmp(response, "Yes", 3) == 0)
    {
        printf("YES\n%s\n", response+3);
        // 处理登录状态更新
        if (strncmp(last_cmd, "login ", 6) == 0)
        {
            int uid = atoi(last_cmd + 6);
            state->logged_in = 1;
            state->uid = uid;
            if (uid == 0)
            {
                strcpy(state->username, "admin");
            }
            else
            {
                snprintf(state->username, sizeof(state->username), "user%d", uid);
            }
        }
    }
    else if (strncmp(response, "No", 2) == 0)
    {
        // 显示错误信息
        if (strlen(response) > 2 && response[2] == ' ')
        {
            printf("Error: %s\n", response + 3);
        }
        else
        {
            printf("Error: Command failed\n");
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <ServerAddr> <FSPort>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_addr = argv[1];
    int fs_port = atoi(argv[2]);

    // 初始化客户端状态 
    client_state state = {0};
    strcpy(state.username, "guest");

    printf("Connecting to file system server at %s:%d\n", server_addr, fs_port);

    tcp_client client = client_init(server_addr, fs_port);
    if (client == NULL)
    {
        fprintf(stderr, "Failed to connect to file system server\n");
        exit(EXIT_FAILURE);
    }

    printf("Connected successfully!\n");
    printf("Type 'help' for available commands or 'e' to exit.\n\n");

    char buf[4096];
    char last_cmd[256];

    while (1)
    {
        // 每次都从服务器获取当前路径
        print_prompt(client, &state);
        
        if (!fgets(buf, sizeof(buf), stdin))
        {
            break;
        }

        // 去除换行符
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
        {
            buf[len - 1] = '\0';
        }
        if (strlen(buf) == 0)
        {
            continue;
        }

        // 处理本地命令
        if (handle_local_command(buf, &state))
        {
            continue;
        }

        // 保存命令并发送到服务器
        strncpy(last_cmd, buf, sizeof(last_cmd) - 1);
        last_cmd[sizeof(last_cmd) - 1] = '\0';

        client_send(client, buf, strlen(buf) + 1);

        // 接收并处理响应
        int n = client_recv(client, buf, sizeof(buf));
        buf[n] = '\0';

        handle_response(buf, last_cmd, &state);

        // 检查退出条件
        if (strncmp(buf, "Bye!", 4) == 0)
        {
            break;
        }
    }

    client_destroy(client);
    printf("\nDisconnected from server.\n");
    return 0;
}