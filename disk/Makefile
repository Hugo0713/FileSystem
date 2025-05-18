EXES = BDS BDS_local BDC test_bd

BUILD_DIR = build

BDS_OBJS = src/server.o \
	src/disk.o

BDS_local_OBJS = src/main.o \
	src/disk.o

BDC_OBJS = src/client.o

test_bd_OBJS = tests/main.o \
	src/disk.o \
	tests/test_disk.o

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

run: BDS_local
	sudo sysctl vm.mmap_rnd_bits=28
	./BDS_local disk.img 1024 63 10

test: test_bd
	sudo sysctl vm.mmap_rnd_bits=28
	./test_bd

# rules to build object files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# rules to build library object files
$(BUILD_DIR)/lib/%.o: ../lib/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(EXES)

OBJS = $(foreach exe,$(EXES),$($(exe)_OBJS)) $(LIB_OBJS)
DEPS = $(OBJS:.o=.d)
-include $(DEPS)

.PHONY: all clean run test
