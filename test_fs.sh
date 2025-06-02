#!/bin/bash
# filepath: /mnt/x/VS code/2025 Spring/OS-design/Project3/Prj3_StudentID/test_integrated_fs.sh

# 文件系统集成测试脚本 - 使用原生 BDS, FS, FC
echo "=== File System Integration Test (BDS + FS + FC) ==="

# 配置参数
DISK_SERVER_PORT=8888
FS_SERVER_PORT=9999
SERVER_ADDR="localhost"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 测试结果统计
TOTAL_TESTS=0
PASSED_TESTS=0

# 进程ID存储
DISK_PID=""
FS_PID=""

# 测试函数 - 使用 FC 客户端
run_test() {
    local test_name="$1"
    local command="$2"
    local expected="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -e "${YELLOW}Testing: $test_name${NC}"
    echo "  Command: '$command'"
    
    # 使用 FC 客户端发送命令，移除换行符
    result=$(echo -e "$command" | timeout 10 fs/FC $FS_SERVER_PORT 2>/dev/null)
    
    # 显示完整输出用于调试
    echo "  Full output: '$result'"
    
    # 检查结果
    if [[ "$result" == *"$expected"* ]]; then
        echo -e "${GREEN}✓ PASS${NC}: $test_name"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ FAIL${NC}: $test_name"
        echo "  Expected: '$expected'"
        echo "  Got: '$result'"
    fi
    echo
}

# 编译项目函数
build_project() {
    echo -e "${BLUE}Building disk server (BDS)...${NC}"
    cd disk
    
    # 使用 Makefile 的 clean 目标
    if ! make clean > build.log 2>&1; then
        echo -e "${RED}Failed to clean disk project${NC}"
        cat build.log
        exit 1
    fi
    
    # 编译 BDS
    if ! make BDS >> build.log 2>&1; then
        echo -e "${RED}Failed to build BDS${NC}"
        cat build.log
        exit 1
    fi
    
    if [ ! -f BDS ]; then
        echo -e "${RED}BDS executable not found${NC}"
        exit 1
    fi
    
    cd ..
    
    echo -e "${BLUE}Building file system server (FS) and client (FC)...${NC}"
    cd fs
    
    # 使用 Makefile 的 clean 目标
    if ! make clean > build.log 2>&1; then
        echo -e "${RED}Failed to clean fs project${NC}"
        cat build.log
        exit 1
    fi
    
    # 编译 FS 和 FC
    if ! make FS FC >> build.log 2>&1; then
        echo -e "${RED}Failed to build FS and FC${NC}"
        cat build.log
        exit 1
    fi
    
    # 检查生成的可执行文件
    if [ ! -f FS ]; then
        echo -e "${RED}FS server not found${NC}"
        exit 1
    fi
    
    if [ ! -f FC ]; then
        echo -e "${RED}FC client not found${NC}"
        exit 1
    fi
    
    cd ..
    
    echo -e "${GREEN}Build completed successfully${NC}"
}

# 启动服务器函数
start_servers() {
    echo -e "${BLUE}Starting Disk Server (BDS) on port $DISK_SERVER_PORT...${NC}"
    cd disk
    
    # 确保磁盘文件存在
    if [ ! -f disk.img ]; then
        echo "Creating disk image..."
        dd if=/dev/zero of=disk.img bs=512 count=$((1024*63)) > /dev/null 2>&1
    fi
    
    # 启动 BDS: ./BDS <disk file> <cylinders> <sectors> <track delay> <port>
    echo "Starting BDS: ./BDS disk.img 1024 63 10 $DISK_SERVER_PORT"
    ./BDS disk.img 1024 63 10 $DISK_SERVER_PORT > disk_server.log 2>&1 &
    DISK_PID=$!
    echo "Disk Server (BDS) PID: $DISK_PID"
    cd ..
    
    sleep 3
    
    # 检查磁盘服务器是否正常启动
    if ! kill -0 $DISK_PID 2>/dev/null; then
        echo -e "${RED}Disk server (BDS) failed to start${NC}"
        echo "BDS log:"
        cat disk/disk_server.log
        exit 1
    fi
    
    echo -e "${BLUE}Starting File System Server (FS) on port $FS_SERVER_PORT...${NC}"
    cd fs
    # FS 根据代码应该接受: <disk_port> [fs_port]
    echo "Starting FS: ./FS $DISK_SERVER_PORT $FS_SERVER_PORT"
    ./FS $DISK_SERVER_PORT $FS_SERVER_PORT > fs_server.log 2>&1 &
    FS_PID=$!
    echo "File System Server (FS) PID: $FS_PID"
    cd ..
    
    sleep 3
    
    # 检查文件系统服务器是否正常启动
    if ! kill -0 $FS_PID 2>/dev/null; then
        echo -e "${RED}File system server (FS) failed to start${NC}"
        echo "FS server log:"
        cat fs/fs_server.log
        echo "BDS server log:"
        cat disk/disk_server.log
        exit 1
    fi
    
    echo -e "${GREEN}Servers started successfully!${NC}"
    echo "Disk Server (BDS) PID: $DISK_PID"
    echo "File System Server (FS) PID: $FS_PID"
    echo
}

# 停止服务器函数
stop_servers() {
    echo -e "${BLUE}Stopping servers...${NC}"
    if [ ! -z "$FS_PID" ]; then
        echo "Stopping FS server (PID: $FS_PID)"
        kill $FS_PID 2>/dev/null
        wait $FS_PID 2>/dev/null
    fi
    if [ ! -z "$DISK_PID" ]; then
        echo "Stopping BDS server (PID: $DISK_PID)"
        kill $DISK_PID 2>/dev/null
        wait $DISK_PID 2>/dev/null
    fi
    sleep 1
    echo -e "${GREEN}Servers stopped.${NC}"
}

# 测试客户端连接
test_connection() {
    echo -e "${YELLOW}Testing FC client connection to FS server...${NC}"
    
    # 使用简单的命令测试连接
    test_output=$(echo -n "ls" | timeout 10 fs/FC $FS_SERVER_PORT 2>&1)
    exit_code=$?
    
    echo "Connection test output: '$test_output'"
    echo "Connection test exit code: $exit_code"
    
    if [ $exit_code -eq 0 ] || [[ "$test_output" == *"Yes"* ]] || [[ "$test_output" == *"No"* ]]; then
        echo -e "${GREEN}✓ FC client connection successful${NC}"
        return 0
    else
        echo -e "${RED}✗ FC client connection failed${NC}"
        echo "Server logs:"
        echo "=== BDS Server Log ==="
        tail -20 disk/disk_server.log 2>/dev/null || echo "No BDS log"
        echo "=== FS Server Log ==="
        tail -20 fs/fs_server.log 2>/dev/null || echo "No FS log"
        return 1
    fi
}

# 主程序
main() {
    # 清理之前的进程
    echo "Cleaning up any existing processes..."
    pkill -f "BDS.*disk.img.*$DISK_SERVER_PORT" 2>/dev/null
    pkill -f "FS.*$DISK_SERVER_PORT.*$FS_SERVER_PORT" 2>/dev/null
    sleep 2
    
    # 编译项目
    build_project
    
    # 启动服务器
    start_servers
    
    # 测试连接
    if ! test_connection; then
        stop_servers
        exit 1
    fi
    
    # 开始测试
    echo "=== Running File System Tests with FC Client ==="
    echo
    
    # 基础测试
    run_test "Format file system" "f" "Yes"
    run_test "Login as user 0" "login 0" "Yes"
    run_test "List empty root directory" "ls" "Yes"
    
    # 文件操作测试
    run_test "Create file 'test.txt'" "mk test.txt" "Yes"
    run_test "Write data to file" "w test.txt 5 hello" "Yes"
    run_test "Read file contents" "cat test.txt" "hello"
    run_test "List directory with file" "ls" "test.txt"
    
    # 目录操作测试
    run_test "Create directory 'mydir'" "mkdir mydir" "Yes"
    run_test "Change to directory" "cd mydir" "Yes"
    run_test "List empty directory" "ls" "Yes"
    run_test "Go back to parent" "cd .." "Yes"
    
    # 文件编辑测试
    run_test "Insert data at position 0" "i test.txt 0 6 world " "Yes"
    run_test "Read modified file after insert" "cat test.txt" "world hello"
    run_test "Delete data from position 0" "d test.txt 0 6" "Yes"
    run_test "Read file after deletion" "cat test.txt" "hello"
    
    # 删除操作测试
    run_test "Remove file" "rm test.txt" "Yes"
    run_test "Remove directory" "rmdir mydir" "Yes"
    run_test "List empty directory again" "ls" "Yes"
    
    # 错误处理测试
    run_test "Access non-existent file" "cat nonexistent.txt" "No"
    run_test "Remove non-existent file" "rm nonexistent.txt" "No"
    run_test "Change to non-existent directory" "cd nonexistentdir" "No"
    
    # 停止服务器
    stop_servers
    
    # 显示测试结果
    echo "=== Test Results ==="
    echo "Total Tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$((TOTAL_TESTS - PASSED_TESTS))${NC}"
    
    if [ $PASSED_TESTS -eq $TOTAL_TESTS ]; then
        echo -e "${GREEN}All tests passed! 🎉${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed! 😞${NC}"
        echo "Check server logs for details:"
        echo "  - disk/disk_server.log"
        echo "  - fs/fs_server.log"
        exit 1
    fi
}

# 错误处理
trap 'stop_servers; exit 1' INT TERM

# 运行主程序
main "$@"