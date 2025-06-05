EXES = FS FS_local FC test_fs 

BUILD_DIR = build

FS_OBJS = src/server.o \
	src/block.o \
	src/fs.o \
	src/fs_format.o \
	src/fs_directory.o \
	src/fs_utils.o \
	src/inode.o \
	src/bitmap.o \
	src/user.o \
	src/simple_cache.o \
	src/connection.o 

FS_local_OBJS = src/main.o \
	src/block.o \
	src/fs.o \
	src/fs_format.o \
	src/fs_directory.o \
	src/fs_utils.o \
	src/inode.o \
	src/bitmap.o \
	src/user.o \
	src/simple_cache.o \
	src/connection.o 

FC_OBJS = src/client.o

test_fs_OBJS = tests/main.o \
	src/block.o \
	src/fs.o \
	src/fs_format.o \
	src/fs_directory.o \
	src/fs_utils.o \
	src/inode.o \
	src/bitmap.o \
	src/user.o \
	src/simple_cache.o \
	src/connection.o \
	tests/test_block.o \
	tests/test_fs.o \
	tests/test_inode.o

# Add $(BUILD_DIR) to the beginning of each object file path
$(foreach exe,$(EXES), \
    $(eval $(exe)_OBJS := $$(addprefix $$(BUILD_DIR)/,$$($(exe)_OBJS))))

LIB_SRCS = ../lib/tcp_buffer.c ../lib/tcp_utils.c ../lib/thpool.c
# Replace .. with $(BUILD_DIR)
LIB_OBJS = $(LIB_SRCS:../lib/%.c=$(BUILD_DIR)/lib/%.o)

CC ?= gcc
CFLAGS += -Wall -MMD -Iinclude -I../include
LDFLAGS += -lpthread

DEBUG ?= 1
ifeq ($(DEBUG),1)
CFLAGS += -fsanitize=address -g
endif

all: $(EXES)

define make-target
$1: $(LIB_OBJS) $$($1_OBJS)
	$$(CC) $$(CFLAGS) -o $$@ $$^ $$(LDFLAGS)
endef

$(foreach exe,$(EXES),$(eval $(call make-target,$(exe))))

run: FS_local
	sudo sysctl vm.mmap_rnd_bits=28
	./FS_local

test: test_fs
	sudo sysctl vm.mmap_rnd_bits=28
	./test_fs

# 添加单独测试目标
test-block: test_fs
	sudo sysctl vm.mmap_rnd_bits=28
	./test_fs block

test-inode: test_fs
	sudo sysctl vm.mmap_rnd_bits=28
	./test_fs inode

test-fs: test_fs
	sudo sysctl vm.mmap_rnd_bits=28
	./test_fs fs

# rules to build object files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# rules to build library object files
$(BUILD_DIR)/lib/%.o: ../lib/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(EXES) *.log

OBJS = $(foreach exe,$(EXES),$($(exe)_OBJS)) $(LIB_OBJS)
DEPS = $(OBJS:.o=.d)
-include $(DEPS)

.PHONY: all clean run test test-block test-inode test-fs
