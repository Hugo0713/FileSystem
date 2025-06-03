#include "user.h"
#include "fs.h"
#include "inode.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>

// 初始化用户系统
void init_user_system(void)
{
    // 检查用户信息inode是否存在
    inode *user_ip = iget(USER_INFO_INODE);
    if (user_ip == NULL || user_ip->type == T_UNUSED)
    {
        // 创建用户信息文件
        if (user_ip)
            iput(user_ip);

        user_ip = ialloc(T_FILE);
        if (user_ip == NULL)
        {
            Error("init_user_system: failed to allocate user info inode");
            return;
        }

        // 强制设置为指定inode号
        user_ip->inum = USER_INFO_INODE;
        user_ip->type = T_FILE;
        user_ip->mode = 0600; // 只有root可以读写
        user_ip->nlink = 1;
        user_ip->uid = ADMIN_UID;
        user_ip->size = 0;
        user_ip->dirty = 1;

        // 初始化用户数据
        user_info users[MAX_USERS];
        memset(users, 0, sizeof(users));

        // 创建管理员用户(UID=1)
        users[0].uid = ADMIN_UID;
        users[0].active = 1;
        users[0].is_admin = 1;
        users[0].home_dir_inum = 0; // 管理员使用根目录

        // 写入用户信息
        writei(user_ip, (uchar *)users, 0, sizeof(users));
        iupdate(user_ip);

        Log("init_user_system: created user system with admin user (UID=1)");
    }

    iput(user_ip);
}

// 创建新用户
// 创建新用户
int create_user(uint uid)
{
    if (uid == 0 || uid >= MAX_USERS)
    {
        Error("create_user: invalid UID %d", uid);
        return -1;
    }

    // 只有管理员可以创建用户
    if (current_uid != 0) // 修改：使用 UID 0 作为管理员
    {
        Error("create_user: only admin (UID=0) can create users");
        return -1;
    }

    // 检查用户是否已存在
    if (user_exists(uid))
    {
        Error("create_user: user %d already exists", uid);
        return -1;
    }

    // 创建用户主目录
    char dirname[MAXNAME];
    snprintf(dirname, MAXNAME, "user_%d", uid);

    // 保存当前上下文
    uint old_dir = current_dir;
    uint old_uid = current_uid;

    // 临时切换到根目录，保持管理员权限
    current_dir = 0; // 根目录
    // current_uid 保持为 0 (管理员)

    int mkdir_result = cmd_mkdir(dirname, 0755);

    if (mkdir_result != E_SUCCESS)
    {
        Error("create_user: failed to create home directory for user %d", uid);
        current_dir = old_dir;
        return -1;
    }

    // 获取新创建目录的inode号
    uint home_inum = find_entry_in_directory(0, dirname, T_DIR);
    if (home_inum == 0)
    {
        Error("create_user: failed to find created home directory");
        current_dir = old_dir;
        return -1;
    }

    // 修改目录所有者为新用户
    inode *home_ip = iget(home_inum);
    if (home_ip)
    {
        home_ip->uid = uid;   // 设置为新用户的UID
        home_ip->mode = 0755; // 确保权限正确
        home_ip->dirty = 1;
        iupdate(home_ip);
        iput(home_ip);
        Log("create_user: set home directory owner to uid %d", uid);
    }

    // 恢复原来的上下文
    current_dir = old_dir;

    // 读取并更新用户信息文件
    inode *user_ip = iget(USER_INFO_INODE);
    if (user_ip == NULL)
    {
        Error("create_user: failed to get user info inode");
        return -1;
    }

    user_info users[MAX_USERS];
    int bytes_read = readi(user_ip, (uchar *)users, 0, sizeof(users));

    // 找空位创建用户
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (!users[i].active)
        {
            users[i].uid = uid;
            users[i].active = 1;
            users[i].is_admin = 0;
            users[i].home_dir_inum = home_inum;

            // 写回用户信息
            user_ip->size = 0;
            writei(user_ip, (uchar *)users, 0, sizeof(users));
            iupdate(user_ip);
            iput(user_ip);

            Log("create_user: created user %d with home directory inode %d", uid, home_inum);
            return 0;
        }
    }

    Error("create_user: no space for new user");
    iput(user_ip);
    return -1;
}

// 检查用户是否存在
int user_exists(uint uid)
{
    if (uid >= MAX_USERS)
        return 0;

    inode *user_ip = iget(USER_INFO_INODE);
    if (user_ip == NULL)
        return 0;

    user_info users[MAX_USERS];
    int bytes_read = readi(user_ip, (uchar *)users, 0, sizeof(users));
    iput(user_ip);

    if (bytes_read != sizeof(users))
        return 0;

    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i].active && users[i].uid == uid)
        {
            return 1;
        }
    }
    return 0;
}

// 检查是否为管理员
int is_admin_user(uint uid)
{
    if (uid >= MAX_USERS)
        return 0;

    inode *user_ip = iget(USER_INFO_INODE);
    if (user_ip == NULL)
        return 0;

    user_info users[MAX_USERS];
    int bytes_read = readi(user_ip, (uchar *)users, 0, sizeof(users));
    iput(user_ip);

    if (bytes_read != sizeof(users))
        return 0;

    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i].active && users[i].uid == uid)
        {
            return users[i].is_admin;
        }
    }
    return 0;
}

// 获取用户信息
user_info *get_user_info(uint uid)
{
    static user_info user;

    if (uid >= MAX_USERS)
        return NULL;

    inode *user_ip = iget(USER_INFO_INODE);
    if (user_ip == NULL)
        return NULL;

    user_info users[MAX_USERS];
    int bytes_read = readi(user_ip, (uchar *)users, 0, sizeof(users));
    iput(user_ip);

    if (bytes_read != sizeof(users))
        return NULL;

    for (int i = 0; i < MAX_USERS; i++)
    {
        if (users[i].active && users[i].uid == uid)
        {
            user = users[i];
            return &user;
        }
    }
    return NULL;
}

// 检查文件权限
int check_file_permission(uint file_inum, uint uid, int operation)
{
    // 管理员拥有所有权限
    if (is_admin_user(uid))
        return 1;

    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
        return 0;

    short mode = file_ip->mode;
    uint owner_uid = file_ip->uid;

    iput(file_ip);

    // 检查所有者权限
    if (owner_uid == uid)
    {
        if (operation == PERM_READ && (mode & 0400))
            return 1;
        if (operation == PERM_WRITE && (mode & 0200))
            return 1;
    }

    // 检查其他用户权限
    if (operation == PERM_READ && (mode & 0004))
        return 1;
    if (operation == PERM_WRITE && (mode & 0002))
        return 1;

    return 0;
}