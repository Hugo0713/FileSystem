# File System

## 项目简介

本项目是一个基于 C 语言实现的模拟xv6的文件系统，支持多客户端并发访问。

项目采用客户端-服务器架构，文件系统服务器通过 TCP 协议与磁盘服务器和多个客户端进行通信。

__主要特性:__
- 分层架构: 磁盘服务器 → 文件系统服务器 → 多客户端
- 多用户支持: 支持用户管理和权限控制
- 多客户端并发: 每个客户端维护独立的当前目录和用户状态(TODO)
- 完整的文件操作: 支持文件/目录的创建、删除、读写、权限管理等
- 缓存系统: 可配置的块缓存机制，提升 I/O 性能
- 连接管理: 可配置的多客户端连接管理，支持传统单用户模式
- 日志系统: 完整的操作日志记录和调试信息(TODO)

## 目录结构

```
FileSystem/ 
├── README.md       # 项目文档 
├── Makefile        # 主构建文件 
├── test_fs.sh      # 集成测试脚本 
├── run.sh          # 启动脚本
├── lib/  
│ ├── tcp_buffer.c  # TCP 缓冲区实现
│ ├── tcp_utils.c   # TCP 工具函数 
│ └── thpool.c      # 线程池实现 
├── include/         
│ ├── log.h         # 日志操作头文件 
│ ├── mintest.h     # 单元测试框架
│ ├── tcp_buffer.h  # TCP 缓冲区头文件
│ ├── tcp_utils.h   # TCP 工具函数头文件
│ └── thpool.h      # 线程池头文件  
├── disk/ # 磁盘服务器模块 
│ ├── Makefile      # 磁盘服务器构建文件 
│ ├── include/ 
│ │ └── disk.h      # 磁盘操作头文件
│ ├── src/ 
│ │ ├── disk.c      # 磁盘服务器核心实现 
│ │ ├── client.c    # 磁盘服务器客户端程序 
│ │ ├── sever.c     # 磁盘服务器主程序
│ │ └── main.c      # 本地磁盘服务器主程序 
│ ├── tests/ 
│   └── test_disk.c # 磁盘服务器测试 
├── fs/  
│ ├── Makefile      # 文件系统构建文件 
│ ├── include/ 
│ │ ├── bitmap.h        # 位图管理 
│ │ ├── block.h         # 块设备接口 
│ │ ├── common.h        # 文件系统公共定义 
│ │ ├── connection.h    # 连接管理 
│ │ ├── fs.h            # 文件系统主接口 
│ │ ├── fs_internal.h   # 文件系统内部结构 
│ │ ├── inode.h         # inode 管理 
│ │ ├── simple_cache.h  # 缓存系统 
│ │ └── user.h          # 用户管理 
│ ├── src/  
│ │ ├── server.c        # 文件系统服务器主程序 
│ │ ├── client.c        # 客户端程序 
│ │ ├── fs.c            # 文件系统核心实现 
│ │ ├── fs_format.c     # 文件系统格式化 
│ │ ├── fs_directory.c  # 目录操作实现 
│ │ ├── fs_utils.c      # 文件系统工具函数 
│ │ ├── connection.c    # 多客户端连接管理 
│ │ ├── user.c          # 用户管理实现 
│ │ ├── inode.c         # inode 操作实现 
│ │ ├── block.c         # 块设备操作 
│ │ ├── bitmap.c        # 位图操作实现 
│ │ ├── simple_cache.c  # 缓存系统实现 
│ │ └── main.c          # 单机版主程序（本地测试用） 
│ ├── tests/ 
│ │ ├── main.c          # 测试主程序 
│ │ ├── test_block.c    # 块操作测试 
│ │ ├── test_inode.c    # inode 测试 
│ │ └── test_fs.c       # 文件系统功能测试 
```

## 文件系统抽象层次

```
┌─────────────────────────────────────┐
│            客户端层 (Client)          
│     - 用户交互界面                   
│     - 命令解析和发送                 
│     - 响应处理和显示                 
└─────────────────────────────────────┘
                    │ TCP
                    ▼
┌─────────────────────────────────────┐
│         文件系统服务器层              
│     - 命令处理和路由                  
│     - 权限检查和用户管理              
│     - 连接管理和并发控制              
└─────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────┐
│         文件系统核心层                
│     - inode 管理                    
│     - 目录结构维护                   
│     - 文件操作实现                   
│     - 权限和元数据管理               
└─────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────┐
│           缓存层 (Cache)             
│     - 块缓存管理                     
│     - 轮询替换策略                   
│     - 脏页写回机制                   
└─────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────┐
│          块设备层 (Block)             
│     - 块读写操作                     
│     - 与磁盘服务器通信               
│     - 网络传输和协议处理             
└─────────────────────────────────────┘
                    │ TCP
                    ▼
┌─────────────────────────────────────┐
│          磁盘服务器层                
│     - 物理存储管理                   
│     - 块数据持久化                   
│     - 磁盘 I/O 操作                  
└─────────────────────────────────────┘
```


## 使用方法

我们需要先编译磁盘服务器和文件系统服务器，然后启动磁盘服务器和文件系统服务器，最后运行客户端。

### 需要编译的文件
- **disk目录**: BDS (Block Disk Server) - 磁盘服务器
- **fs目录**: FS (File System Server) - 文件系统服务器, FC (File System Client) - 文件系统客户端

### 使用脚本自动化运行（推荐）

#### 快速启动
```bash
# 一键构建、启动服务器并运行客户端
./run.sh run

# 或者使用简写
./run.sh r
```

#### 分步操作
```bash
# 仅构建项目
./run.sh build
./run.sh b

# 仅启动服务器（不启动客户端）
./run.sh start
./run.sh s

# 停止所有服务器
./run.sh stop

# 清理构建文件和停止服务器
./run.sh clean
./run.sh c
```
### 手动编译与运行
#### 编译
```bash
# 编译磁盘服务器
cd disk
make clean
make BDS
# 编译文件系统服务器和客户端
cd ../fs
make clean
make FS FC
cd ..
```
#### 运行
```bash
# 启动磁盘服务器
cd disk

./BDS <disk_file> <cylinders> <sectors_per_cylinder> <track_delay> <port>

e.g. ./BDS disk.img 1024 64 10 8888

# 启动文件系统服务器
cd ../fs
./FS <disk_port> [fs_port]

# 运行客户端
./FC <server_host> <fs_port>
```

### 参数配置
以下是可更改的参数配置：

```c
- 缓存：simple_cache.h
  - #define BLOCK_CACHE_SIZE 500    // 缓存块数量 (默认: 500)
  - #define CACHE_DISABLED 0        // 缓存开关 (0=启用, 1=禁用)
- 连接管理：connection.h
  - #define MAX_CONNECTIONS 10      // 最大连接数 (默认: 10)
  - #define SINGLE_USER_MODE 0       // 单用户模式开关 (0=多用户, 1=单用户)
- 用户管理：user.h
  - #define MAX_USERS 100            // 最大用户数 (默认: 100)
```

## 功能介绍

```bash
Available commands:
  f                    - Format file system
  mk <name>            - Create file
  mkdir <name>         - Create directory
  rm <name>            - Remove file
  rmdir <name>         - Remove directory
  cd <path>            - Change directory
  ls                   - List directory contents
  cat <name>           - Display file contents
  w <name> <len> <data> - Write to file
  i <name> <pos> <len> <data> - Insert into file
  d <name> <pos> <len> - Delete from file
  login <uid>          - Login as user
  adduser <uid>        - Add new user (admin only)
  pwd                  - Show current directory
  whoami               - Show current user
  e                    - Exit
  help                 - Show this help
```


## 版本历史

v1.0.0 (当前版本)
✅ 基础文件系统功能
✅ 多客户端支持
✅ 缓存系统
⏳ 日志系统

⭐ 如果这个项目对您有帮助，请给一个 Star！