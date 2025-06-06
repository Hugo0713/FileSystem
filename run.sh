#!/bin/bash
# filepath: /mnt/x/VS code/2025 Spring/OS-design/Project3/Prj3_StudentID/debug.sh

# 简化版文件系统调试脚本
echo "=== FS Debug Script ==="

# 配置
DISK_PORT=8888
FS_PORT=9999
SERVER_ADDR="localhost"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 进程ID
DISK_PID=""
FS_PID=""

# 编译
build() {
    echo -e "${BLUE}Building...${NC}"
    
    # 编译 BDS
    cd disk
    if ! make clean > /dev/null 2>&1 || ! make BDS > /dev/null 2>&1; then
        echo -e "${RED}✗ BDS build failed${NC}"
        return 1
    fi
    echo -e "${GREEN}✓ BDS built${NC}"
    cd ..
    
    # 编译 FS 和 FC
    cd fs
    if ! make clean > /dev/null 2>&1 || ! make FS FC > /dev/null 2>&1; then
        echo -e "${RED}✗ FS/FC build failed${NC}"
        return 1
    fi
    echo -e "${GREEN}✓ FS/FC built${NC}"
    cd ..
    
    echo -e "${GREEN}Build complete${NC}"
}

# 清理端口
cleanup_ports() {
    echo -e "${YELLOW}Cleaning ports...${NC}"
    
    # 杀死占用端口的进程
    pkill -f "BDS.*$DISK_PORT" 2>/dev/null
    pkill -f "FS.*$FS_PORT" 2>/dev/null
    
    # 强制清理端口
    local disk_pids=$(lsof -ti:$DISK_PORT 2>/dev/null)
    local fs_pids=$(lsof -ti:$FS_PORT 2>/dev/null)
    
    [ ! -z "$disk_pids" ] && echo $disk_pids | xargs kill -9 2>/dev/null
    [ ! -z "$fs_pids" ] && echo $fs_pids | xargs kill -9 2>/dev/null
    
    sleep 1
}

# 启动服务器
start() {
    echo -e "${BLUE}Starting servers...${NC}"
    
    # 清理端口
    cleanup_ports
    
    # 检查端口
    if lsof -i:$DISK_PORT >/dev/null 2>&1; then
        echo -e "${RED}✗ Port $DISK_PORT still in use${NC}"
        return 1
    fi
    if lsof -i:$FS_PORT >/dev/null 2>&1; then
        echo -e "${RED}✗ Port $FS_PORT still in use${NC}"
        return 1
    fi
    
    # 创建磁盘镜像
    cd disk
    [ ! -f disk.img ] && dd if=/dev/zero of=disk.img bs=512 count=64512 >/dev/null 2>&1
    
    # 启动 BDS
    ./BDS disk.img 1024 63 10 $DISK_PORT > disk.log 2>&1 &
    DISK_PID=$!
    cd ..
    sleep 2
    
    if ! kill -0 $DISK_PID 2>/dev/null; then
        echo -e "${RED}✗ BDS failed to start${NC}"
        cat disk/disk.log
        return 1
    fi
    echo -e "${GREEN}✓ BDS started (PID: $DISK_PID)${NC}"
    
    # 启动 FS
    cd fs
    ./FS $DISK_PORT $FS_PORT > fs.log 2>&1 &
    FS_PID=$!
    cd ..
    sleep 2
    
    if ! kill -0 $FS_PID 2>/dev/null; then
        echo -e "${RED}✗ FS failed to start${NC}"
        cat fs/fs.log
        return 1
    fi
    echo -e "${GREEN}✓ FS started (PID: $FS_PID)${NC}"
    
    echo -e "${GREEN}Servers ready!${NC}"
}

# 停止服务器
stop() {
    echo -e "${YELLOW}Stopping servers...${NC}"
    
    [ ! -z "$FS_PID" ] && kill $FS_PID 2>/dev/null && echo -e "${GREEN}✓ FS stopped${NC}"
    [ ! -z "$DISK_PID" ] && kill $DISK_PID 2>/dev/null && echo -e "${GREEN}✓ BDS stopped${NC}"
    
    cleanup_ports
}

# 清理
clean() {
    echo -e "${YELLOW}Cleaning...${NC}"
    
    stop
    
    cd disk && make clean >/dev/null 2>&1 && cd ..
    cd fs && make clean >/dev/null 2>&1 && cd ..
    
    rm -f disk/disk.log fs/fs.log
    
    echo -e "${GREEN}✓ Cleaned${NC}"
}

# 运行并启动客户端
run() {
    if build && start; then
        echo
        echo -e "${YELLOW}=== Starting FC Client ===${NC}"
        echo -e "${BLUE}Connecting to $SERVER_ADDR:$FS_PORT${NC}"
        echo -e "${YELLOW}Servers will stop when client exits${NC}"
        echo
        
        # 设置退出时自动停止服务器
        trap 'echo; echo -e "${YELLOW}Client exited, stopping servers...${NC}"; clean; exit 0' EXIT
        
        # 启动 FC 客户端
        cd fs
        ./FC $SERVER_ADDR $FS_PORT
        cd ..
    fi
}

# 主程序
case "${1:-run}" in
    "build"|"b")
        build
        ;;
    "start"|"s")
        start
        echo -e "${BLUE}Connect with: ./fs/FC $SERVER_ADDR $FS_PORT${NC}"
        ;;
    "stop")
        stop
        ;;
    "clean"|"c")
        clean
        ;;
    "run"|"r"|"")
        run
        ;;
    *)
        echo "Usage: $0 {build|start|stop|clean|run}"
        echo "  build/b  - Compile BDS, FS, FC"
        echo "  start/s  - Start servers only"
        echo "  stop     - Stop servers"  
        echo "  clean/c  - Clean and stop"
        echo "  run/r    - Build, start servers and run FC client (default)"
        ;;
esac

# 只有非 run 命令才设置 EXIT trap
if [[ "${1:-run}" != "run" && "${1:-run}" != "r" && "${1:-run}" != "" ]]; then
    trap 'clean' EXIT
fi