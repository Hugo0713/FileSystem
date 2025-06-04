#include "fs.h"
#include "fs_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"
#include "bitmap.h"
#include "user.h"

uint current_dir = 0;                 // 当前目录的 inode 编号
uint current_uid = 0;                 // 当前用户的 UID
static char current_path[1024] = "/"; // 当前路径

void sbinit(int ncyl_, int nsec_)
{
    uchar buf[BSIZE];
    read_block(0, buf);
    memcpy(&sb, buf, sizeof(sb));
    cmd_f(ncyl_, nsec_);
}
char *get_current_path(void)
{
    return current_path;
}
void update_current_path(const char *path)
{
    if (path[0] == '/')
    {
        strcpy(current_path, path);
    }
    else if (strcmp(path, "..") == 0)
    {
        char *last_slash = strrchr(current_path, '/');
        if (last_slash && last_slash != current_path)
        {
            *last_slash = '\0';
        }
        else
        {
            strcpy(current_path, "/");
        }
    }
    else if (strcmp(path, ".") != 0)
    {
        if (strcmp(current_path, "/") != 0)
        {
            strcat(current_path, "/");
        }
        strcat(current_path, path);
    }
}

int cmd_f(int ncyl, int nsec)
{
    if (current_uid != ADMIN_UID)
    {
        Error("cmd_f: only admin (UID=0) can format file system");
        return E_ERROR;
    }
    int size = ncyl * nsec;
    init_sb(size);         // 初始化超级块
    init_block_bitmap();   // 初始化数据块位图
    init_inode_system();   // 初始化inode位图和inode区域
    init_root_directory(); // 初始化根目录
    init_user_system();    // 初始化用户系统

    // 初始化日志区域（清零）
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    for (uint i = 0; i < sb.nlog; i++)
    {
        write_block(sb.logstart + i, buf);
    }

    Log("File system formatted successfully");
    Log("Total blocks: %d, Data blocks: %d, Inodes: %d", sb.size, sb.ndatablocks, sb.ninodes);
    return E_SUCCESS;
}

int cmd_mk(char *name, short mode)
{
    // 检查当前目录写权限
    if (!check_file_permission(current_dir, current_uid, PERM_WRITE))
    {
        Error("cmd_mk: no permission to create file in current directory");
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_mk: invalid filename");
        return E_ERROR;
    }
    if (strlen(name) >= MAXNAME)
    {
        Error("cmd_mk: filename too long (max %d characters)", MAXNAME - 1);
        return E_ERROR;
    }
    Log("cmd_mk: creating file '%s' with mode %o", name, mode);

    // 检查文件是否已存在
    uint existing_inum = find_file_only(current_dir, name);
    if (existing_inum != 0)
    {
        Error("cmd_mk: entry '%s' already exists", name);
        return E_ERROR;
    }

    // 分配新的 inode
    inode *ip = ialloc(T_FILE);
    if (ip == NULL)
    {
        Error("cmd_mk: failed to allocate inode for file '%s'", name);
        return E_ERROR;
    }

    // 设置文件属性
    ip->type = T_FILE;
    ip->mode = mode;
    ip->nlink = 1;
    ip->uid = current_uid;
    ip->size = 0;
    ip->dirty = 1;

    iupdate(ip);

    // 在当前目录中添加文件条目
    int result = add_entry_to_directory(current_dir, name, ip->inum, T_FILE, mode);
    if (result < 0)
    {
        Error("cmd_mk: failed to add file to directory");
        iput(ip);
        return E_ERROR;
    }

    Log("cmd_mk: successfully created file '%s' with inode %d", name, ip->inum);
    iput(ip);
    return E_SUCCESS;
}

int cmd_mkdir(char *name, short mode)
{
    if (!check_file_permission(current_dir, current_uid, PERM_WRITE))
    {
        Error("cmd_mkdir: no permission to create directory");
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_mkdir: invalid directory name");
        return E_ERROR;
    }
    if (strlen(name) >= MAXNAME)
    {
        Error("cmd_mkdir: directory name too long (max %d characters)", MAXNAME - 1);
        return E_ERROR;
    }
    Log("cmd_mkdir: creating directory '%s' with mode %o", name, mode);

    // 检查目录是否已存在
    uint existing_inum = find_directory_only(current_dir, name);
    if (existing_inum != 0)
    {
        Error("cmd_mkdir: entry '%s' already exists", name);
        return E_ERROR;
    }

    // 分配新的 inode
    inode *ip = ialloc(T_DIR);
    if (ip == NULL)
    {
        Error("cmd_mkdir: failed to allocate inode for directory '%s'", name);
        return E_ERROR;
    }

    // 设置目录属性
    ip->type = T_DIR;
    ip->mode = mode;
    ip->nlink = 2; // "." 和父目录中的条目
    ip->uid = current_uid;
    ip->size = 2 * sizeof(entry); // "." 和 ".." 条目
    ip->dirty = 1;

    // 为新目录分配数据块
    uint dir_data_block = allocate_block();
    if (dir_data_block == 0)
    {
        Error("cmd_mkdir: no free blocks available for directory '%s'", name);
        iput(ip);
        return E_ERROR;
    }
    ip->addrs[0] = dir_data_block;

    iupdate(ip);

    // 初始化目录内容
    if (init_directory_entries(ip->inum, current_dir, dir_data_block, mode) < 0)
    {
        Error("cmd_mkdir: failed to initialize directory entries");
        free_block(dir_data_block);
        iput(ip);
        return E_ERROR;
    }

    // 在当前目录中添加新目录的条目
    int result = add_entry_to_directory(current_dir, name, ip->inum, T_DIR, mode);
    if (result < 0)
    {
        Error("cmd_mkdir: failed to add directory to parent");
        free_block(dir_data_block);
        iput(ip);
        return E_ERROR;
    }

    // 增加父目录的链接数（新增了一个子目录）
    inode *parent_ip = iget(current_dir);
    if (parent_ip != NULL && parent_ip->type == T_DIR)
    {
        parent_ip->nlink++;
        parent_ip->dirty = 1;
        iupdate(parent_ip);
        iput(parent_ip);
    }

    Log("cmd_mkdir: successfully created directory '%s' with inode %d", name, ip->inum);
    iput(ip);
    return E_SUCCESS;
}

int cmd_rm(char *name)
{
    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_rm: file '%s' not found", name);
        return E_ERROR;
    }
    // 检查文件写权限（删除需要写权限）
    if (!check_file_permission(file_inum, current_uid, PERM_WRITE))
    {
        Error("cmd_rm: no permission to delete file '%s'", name);
        return E_ERROR;
    }
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_rm: invalid filename");
        return E_ERROR;
    }
    Log("cmd_rm: removing file '%s'", name);

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_rm: failed to get file inode %d", file_inum);
        return E_ERROR;
    }
    if (file_ip->type != T_FILE)
    {
        Error("cmd_rm: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 检查文件是否有其他硬链接
    if (file_ip->nlink > 1)
    { // 只是减少链接数，不删除实际数据
        file_ip->nlink--;
        file_ip->dirty = 1;
        iupdate(file_ip);
        Log("cmd_rm: decreased link count for file '%s' to %d", name, file_ip->nlink);
    }
    else // 最后一个链接，删除文件数据
    {
        Log("cmd_rm: freeing file data for '%s'", name);

        // 释放文件的所有数据块
        free_file_blocks(file_ip);
        // 标记 inode 为未使用
        file_ip->type = T_UNUSED;
        file_ip->dirty = 1;
        iupdate(file_ip);
        // 释放 inode
        free_inode_in_bitmap(file_inum);
    }

    iput(file_ip);

    // 从父目录中删除条目
    int result = remove_entry_from_directory(current_dir, name);
    if (result < 0)
    {
        Error("cmd_rm: failed to remove file from directory");
        return E_ERROR;
    }
    Log("cmd_rm: successfully removed file '%s'", name);
    return E_SUCCESS;
}

int cmd_rmdir(char *name)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_rmdir: invalid directory name");
        return E_ERROR;
    }
    // 不允许删除特殊目录
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        Error("cmd_rmdir: cannot remove '.' or '..' directory");
        return E_ERROR;
    }
    Log("cmd_rmdir: removing directory '%s'", name);

    // 在当前目录中查找目录
    uint dir_inum = find_entry_in_directory(current_dir, name, T_DIR);
    if (dir_inum == 0)
    {
        Error("cmd_rmdir: directory '%s' not found", name);
        return E_ERROR;
    }

    // 不允许删除根目录
    if (dir_inum == 0)
    {
        Error("cmd_rmdir: cannot remove root directory");
        return E_ERROR;
    }

    // 检查目录是否为空
    int empty_result = is_directory_empty(dir_inum);
    if (empty_result < 0)
    {
        Error("cmd_rmdir: failed to check if directory is empty");
        return E_ERROR;
    }
    if (empty_result == 0)
    {
        Error("cmd_rmdir: directory '%s' is not empty", name);
        return E_ERROR;
    }

    // 获取目录 inode
    inode *dir_ip = iget(dir_inum);
    if (dir_ip == NULL)
    {
        Error("cmd_rmdir: failed to get directory inode %d", dir_inum);
        return E_ERROR;
    }
    if (dir_ip->type != T_DIR)
    {
        Error("cmd_rmdir: '%s' is not a directory", name);
        iput(dir_ip);
        return E_ERROR;
    }
    Log("cmd_rmdir: freeing directory data for '%s'", name);

    // 释放目录的所有数据块
    free_file_blocks(dir_ip);

    // 标记 inode 为未使用
    dir_ip->type = T_UNUSED;
    dir_ip->dirty = 1;
    iupdate(dir_ip);

    // 释放 inode
    free_inode_blocks(dir_ip);
    iput(dir_ip);

    // 从父目录中删除条目
    int result = remove_entry_from_directory(current_dir, name);
    if (result < 0)
    {
        Error("cmd_rmdir: failed to remove directory from parent");
        return E_ERROR;
    }

    // 更新父目录的链接数（因为删除了一个子目录）
    inode *parent_ip = iget(current_dir);
    if (parent_ip != NULL && parent_ip->type == T_DIR)
    {
        if (parent_ip->nlink > 0)
        {
            parent_ip->nlink--;
            parent_ip->dirty = 1;
            iupdate(parent_ip);
        }
        iput(parent_ip);
    }

    Log("cmd_rmdir: successfully removed directory '%s'", name);
    return E_SUCCESS;
}

int cmd_cd(char *path)
{
    if (path == NULL)
    {
        Error("cmd_cd: invalid path");
        return E_ERROR;
    }

    uint target_inum;
    if (path[0] == '/') // 绝对路径：从根目录开始解析
    {
        if (strcmp(path, "/") == 0)
        {
            target_inum = 0;           // 根目录
            current_dir = target_inum; // 更新当前目录为根目录
            return E_SUCCESS;
        }
        else
        {
            target_inum = resolve_absolute_path(path);
        }
    }
    else // 相对路径：从当前目录开始解析
    {
        if (strcmp(path, "..") == 0)
        {
            target_inum = find_entry_in_directory(current_dir, "..", T_DIR);
        }
        else
        {
            target_inum = find_entry_in_directory(current_dir, path, T_DIR);
            if (target_inum == 0)
            {
                Error("cmd_cd: directory '%s' not found in current directory", path);
                return E_ERROR;
            }
        }
    }

    // 验证目标是目录
    inode *target_ip = iget(target_inum);
    if (target_ip == NULL || target_ip->type != T_DIR)
    {
        Error("cmd_cd: '%s' is not a directory", path);
        if (target_ip)
            iput(target_ip);
        return E_ERROR;
    }
    iput(target_ip);

    // 更新当前目录
    uint old_dir = current_dir;
    current_dir = target_inum;
    update_current_path(path);

    Log("cmd_cd: changed directory from %d to %d (%s)", old_dir, target_inum, path);
    return E_SUCCESS;
}
int cmd_ls(entry **entries, int *n)
{
    if (entries == NULL || n == NULL)
    {
        Error("cmd_ls: invalid output parameters");
        return E_ERROR;
    }
    Log("cmd_ls: listing directory %d", current_dir);

    // 获取当前目录 inode
    inode *dir_ip = iget(current_dir);
    if (dir_ip == NULL)
    {
        Error("cmd_ls: failed to get current directory inode %d", current_dir);
        return E_ERROR;
    }
    if (dir_ip->type != T_DIR)
    {
        Error("cmd_ls: current inode %d is not a directory", current_dir);
        iput(dir_ip);
        return E_ERROR;
    }

    // 检查目录是否为空
    if (dir_ip->size == 0)
    {
        Log("cmd_ls: directory is empty");
        *entries = NULL;
        *n = 0;
        iput(dir_ip);
        return E_SUCCESS; // 空目录返回成功
    }
    iput(dir_ip); // 释放 dir_ip

    // 使用较大的临时数组存储所有可能的条目
    uint max_possible_entries = MAXFILEB * (BSIZE / sizeof(entry));
    entry *temp_entries = malloc(max_possible_entries * sizeof(entry));
    if (temp_entries == NULL)
    {
        Error("cmd_ls: failed to allocate memory for temp entries");
        return E_ERROR;
    }

    uint valid_count = 0;
    // 收集目录中的所有条目
    int result = collect_directory_entries(current_dir, temp_entries, max_possible_entries, &valid_count);
    if (result < 0)
    {
        Error("cmd_ls: failed to collect directory entries");
        free(temp_entries);
        return E_ERROR;
    }

    // 过滤掉 "." 和 ".." 条目
    uint filtered_count = 0;
    entry *filtered_entries = malloc(valid_count * sizeof(entry));
    if (filtered_entries == NULL && valid_count > 0)
    {
        Error("cmd_ls: failed to allocate filtered entries array");
        free(temp_entries);
        return E_ERROR;
    }

    for (uint i = 0; i < valid_count; i++)
    {
        // 跳过 "." 和 ".." 条目
        if (strcmp(temp_entries[i].name, ".") != 0 &&
            strcmp(temp_entries[i].name, "..") != 0)
        {
            if (filtered_entries != NULL)
            {
                filtered_entries[filtered_count] = temp_entries[i];
                // 获取 inode 并填充详细信息
                inode *file_ip = iget(temp_entries[i].inum);
                if (file_ip != NULL)
                {
                    filtered_entries[filtered_count].size = file_ip->size;
                    filtered_entries[filtered_count].mode = file_ip->mode;
                    filtered_entries[filtered_count].uid = file_ip->uid;
                    filtered_entries[filtered_count].type = file_ip->type;
                    iput(file_ip);
                }
                else
                {
                    filtered_entries[filtered_count].size = 0;
                    filtered_entries[filtered_count].mode = 0644;
                    filtered_entries[filtered_count].uid = 0;
                    filtered_entries[filtered_count].type = T_FILE;
                }
            }
            filtered_count++;
        }
    }

    free(temp_entries);

    // 根据过滤后的条目数分配最终结果数组
    if (filtered_count > 0)
    {
        entry *result_entries = malloc(filtered_count * sizeof(entry));
        if (result_entries == NULL)
        {
            Error("cmd_ls: failed to allocate result array");
            if (filtered_entries)
                free(filtered_entries);
            return E_ERROR;
        }
        memcpy(result_entries, filtered_entries, filtered_count * sizeof(entry));
        *entries = result_entries;
    }
    else
    {
        *entries = NULL;
    }

    if (filtered_entries)
    {
        free(filtered_entries);
    }
    *n = (int)filtered_count;

    Log("cmd_ls: found %d entries in directory %d", filtered_count, current_dir);
    return E_SUCCESS;
}

int cmd_cat(char *name, uchar **buf, uint *len)
{
    if (name == NULL || strlen(name) == 0 || buf == NULL || len == NULL)
    {
        Error("cmd_cat: invalid parameters");
        return E_ERROR;
    }
    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_cat: file '%s' not found", name);
        return E_ERROR;
    }
    // 检查读权限
    if (!check_file_permission(file_inum, current_uid, PERM_READ))
    {
        Error("cmd_cat: no permission to read file '%s'", name);
        return E_ERROR;
    }
    Log("cmd_cat: reading file '%s'", name);

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_cat: failed to get file inode %d", file_inum);
        return E_ERROR;
    }
    if (file_ip->type != T_FILE)
    {
        Error("cmd_cat: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 如果文件为空
    if (file_ip->size == 0)
    {
        Log("cmd_cat: file '%s' is empty", name);
        *buf = NULL;
        *len = 0;
        iput(file_ip);
        return E_SUCCESS;
    }

    // 分配缓冲区读取文件内容
    uchar *file_buf = malloc(file_ip->size);
    if (file_buf == NULL)
    {
        Error("cmd_cat: failed to allocate buffer for file content (size: %d)", file_ip->size);
        iput(file_ip);
        return E_ERROR;
    }

    // 读取文件内容
    int bytes_read = readi(file_ip, file_buf, 0, file_ip->size);
    if (bytes_read != file_ip->size)
    {
        Error("cmd_cat: failed to read file content (expected %d, got %d)", file_ip->size, bytes_read);
        free(file_buf);
        iput(file_ip);
        return E_ERROR;
    }

    *buf = file_buf;
    *len = file_ip->size;

    int file_size = file_ip->size;
    iput(file_ip);
    Log("cmd_cat: successfully read %d bytes from file '%s'", file_size, name);
    return E_SUCCESS;
}

int cmd_w(char *name, uint len, const char *data)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_w: invalid filename");
        return E_ERROR;
    }
    if (len > 0 && data == NULL)
    {
        Error("cmd_w: invalid data pointer");
        return E_ERROR;
    }

    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_w: file '%s' not found", name);
        return E_ERROR;
    }
    // 检查写权限
    if (!check_file_permission(file_inum, current_uid, PERM_WRITE))
    {
        Error("cmd_w: no permission to write file '%s'", name);
        return E_ERROR;
    }
    Log("cmd_w: writing %d bytes to file '%s'", len, name);

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_w: failed to get file inode %d", file_inum);
        return E_ERROR;
    }

    if (file_ip->type != T_FILE)
    {
        Error("cmd_w: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 清空文件内容（从偏移量0开始写入）
    file_ip->size = 0;
    file_ip->dirty = 1;

    // 如果有数据要写入
    if (len > 0 && data != NULL)
    {
        int bytes_written = writei(file_ip, (uchar *)data, 0, len);
        if (bytes_written != len)
        {
            Error("cmd_w: failed to write data (expected %d, wrote %d)", len, bytes_written);
            iput(file_ip);
            return E_ERROR;
        }
    }

    iput(file_ip);
    Log("cmd_w: successfully wrote %d bytes to file '%s'", len, name);
    return E_SUCCESS;
}

int cmd_i(char *name, uint pos, uint len, const char *data)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_i: invalid filename");
        return E_ERROR;
    }
    if (len > 0 && data == NULL)
    {
        Error("cmd_i: invalid data pointer");
        return E_ERROR;
    }
    Log("cmd_i: inserting %d bytes to file '%s' at position %d", len, name, pos);

    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_i: file '%s' not found", name);
        return E_ERROR;
    }

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_i: failed to get file inode %d", file_inum);
        return E_ERROR;
    }
    if (file_ip->type != T_FILE)
    {
        Error("cmd_i: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 检查插入位置是否有效
    if (pos > file_ip->size)
    {
        Error("cmd_i: insert position %d exceeds file size %d", pos, file_ip->size);
        iput(file_ip);
        return E_ERROR;
    }
    if (len == 0) // 如果没有数据要插入
    {
        Log("cmd_i: no data to insert");
        iput(file_ip);
        return E_SUCCESS;
    }

    // 读取原文件内容
    uchar *original_data = NULL;
    uint original_size = file_ip->size;
    if (original_size > 0)
    {
        original_data = malloc(original_size);

        int bytes_read = readi(file_ip, original_data, 0, original_size);
        if (bytes_read != original_size)
        {
            Error("cmd_i: failed to read original file content");
            free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }
    // 清空文件
    file_ip->size = 0;
    file_ip->dirty = 1;

    // 写入插入位置之前的数据
    if (pos > 0)
    {
        int bytes_written = writei(file_ip, original_data, 0, pos);
        if (bytes_written != pos)
        {
            Error("cmd_i: failed to write pre-insert data");
            if (original_data)
                free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    // 写入新数据
    int bytes_written = writei(file_ip, (uchar *)data, pos, len);
    if (bytes_written != len)
    {
        Error("cmd_i: failed to write insert data");
        if (original_data)
            free(original_data);
        iput(file_ip);
        return E_ERROR;
    }

    // 写入插入位置之后的数据
    if (pos < original_size)
    {
        bytes_written = writei(file_ip, original_data + pos, pos + len, original_size - pos);
        if (bytes_written != (original_size - pos))
        {
            Error("cmd_i: failed to write post-insert data");
            if (original_data)
                free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    if (original_data)
        free(original_data);
    iput(file_ip);
    Log("cmd_i: successfully inserted %d bytes to file '%s' at position %d", len, name, pos);
    return E_SUCCESS;
}

int cmd_d(char *name, uint pos, uint len)
{
    if (name == NULL || strlen(name) == 0)
    {
        Error("cmd_d: invalid filename");
        return E_ERROR;
    }
    Log("cmd_d: deleting %d bytes from file '%s' at position %d", len, name, pos);

    // 在当前目录中查找文件
    uint file_inum = find_entry_in_directory(current_dir, name, T_FILE);
    if (file_inum == 0)
    {
        Error("cmd_d: file '%s' not found", name);
        return E_ERROR;
    }

    // 获取文件 inode
    inode *file_ip = iget(file_inum);
    if (file_ip == NULL)
    {
        Error("cmd_d: failed to get file inode %d", file_inum);
        return E_ERROR;
    }
    if (file_ip->type != T_FILE)
    {
        Error("cmd_d: '%s' is not a file", name);
        iput(file_ip);
        return E_ERROR;
    }

    // 检查删除位置和长度是否有效
    if (pos >= file_ip->size)
    {
        Error("cmd_d: delete position %d exceeds file size %d", pos, file_ip->size);
        iput(file_ip);
        return E_ERROR;
    }
    // 如果没有数据要删除
    if (len == 0)
    {
        Log("cmd_d: no data to delete");
        iput(file_ip);
        return E_SUCCESS;
    }

    // 调整删除长度，不能超过文件剩余长度
    uint actual_delete_len = len;
    if (pos + len > file_ip->size)
    {
        actual_delete_len = file_ip->size - pos;
        Log("cmd_d: adjusted delete length to %d", actual_delete_len);
    }

    // 读取原文件内容
    uint original_size = file_ip->size;
    uchar *original_data = malloc(original_size);
    if (original_data == NULL)
    {
        Error("cmd_d: failed to allocate buffer for original data");
        iput(file_ip);
        return E_ERROR;
    }

    int bytes_read = readi(file_ip, original_data, 0, original_size);
    if (bytes_read != original_size)
    {
        Error("cmd_d: failed to read original file content");
        free(original_data);
        iput(file_ip);
        return E_ERROR;
    }

    // 清空文件
    file_ip->size = 0;
    file_ip->dirty = 1;

    // 写入删除位置之前的数据
    if (pos > 0)
    {
        int bytes_written = writei(file_ip, original_data, 0, pos);
        if (bytes_written != pos)
        {
            Error("cmd_d: failed to write pre-delete data");
            free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    // 写入删除位置之后的数据
    uint remaining_start = pos + actual_delete_len;
    if (remaining_start < original_size)
    {
        uint remaining_len = original_size - remaining_start;
        int bytes_written = writei(file_ip, original_data + remaining_start, pos, remaining_len);
        if (bytes_written != remaining_len)
        {
            Error("cmd_d: failed to write post-delete data");
            free(original_data);
            iput(file_ip);
            return E_ERROR;
        }
    }

    free(original_data);
    iput(file_ip);
    Log("cmd_d: successfully deleted %d bytes from file '%s' at position %d", actual_delete_len, name, pos);
    return E_SUCCESS;
}

int cmd_login(int auid)
{
    if (auid < 0 || auid >= MAX_USERS)
    {
        Error("cmd_login: invalid user ID %d", auid);
        return E_ERROR;
    }
    if (!user_exists(auid))
    {
        Error("cmd_login: user %d does not exist", auid);
        return E_ERROR;
    }

    current_uid = auid;
    current_dir = 0; // 登录后默认进入根目录
    strcpy(current_path, "/");
    Log("cmd_login: logged in as user %d", auid);
    return E_SUCCESS;
}

int cmd_adduser(int uid)
{
    if (!is_admin_user(current_uid)) // 检查当前用户是否为管理员
    {
        Error("cmd_adduser: only admin can add users");
        return E_ERROR;
    }
    if (uid <= 0 || uid >= MAX_USERS) // 检查用户ID有效性
    {
        Error("cmd_adduser: invalid user ID %d", uid);
        return E_ERROR;
    }
    if (user_exists(uid)) // 检查用户是否已存在
    {
        Error("cmd_adduser: user %d already exists", uid);
        return E_ERROR;
    }
    if (create_user(uid) != 0) // 创建用户
    {
        Error("cmd_adduser: failed to create user %d", uid);
        return E_ERROR;
    }
    Log("cmd_adduser: successfully created user %d", uid);
    return E_SUCCESS;
}
