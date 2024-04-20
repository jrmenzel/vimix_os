#
# Build all userspace apps
#
include ../../MakefileCommon.mk
BUILD_DIR_ROOT=../../$(BUILD_DIR)
B_BUILD_DIR=$(BUILD_DIR_ROOT)/usr/bin
B_DEPLOY_DIR=$(BUILD_DIR_ROOT)/root

LIB_PATH=$(BUILD_DIR_ROOT)/usr/lib

UPROGS_NAME=\
	cat\
	echo\
	forktest\
	grep\
	init\
	kill\
	ln\
	ls\
	mkdir\
	rm\
	sh\
	stressfs\
	usertests\
	grind\
	wc\
	zombie\
	ls

UPROGS=$(foreach a, $(UPROGS_NAME), $(B_DEPLOY_DIR)/$a)

.PHONY: all directories clean

all: directories $(UPROGS)

$(B_BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) -c $< -o $@

$(B_DEPLOY_DIR)/%: $(B_BUILD_DIR)/%.o $(LIB_PATH)/stdlib.a
	$(LD) $(LDFLAGS) -T user.ld $(LIB_PATH)/stdlib.a -o $@ $^
	$(OBJDUMP) -S $@ > $(B_BUILD_DIR)/$*.S

# special case:
# usertest includes internal kernel API and needs more include search paths
#
# Include paths for the kernel: 
# !!! note the include path prefix ../kernel/ compared to the paths for the kernel build - those are internal kernel API !!!
#
KERNEL_SPACE_INC = ./arch/riscv # selected architecture
KERNEL_SPACE_INC += ./include # public kernel API
KERNEL_SPACE_INC += ../lib/std # mini C lib
KERNEL_SPACE_INC += ./
KERNEL_SPACE_INC_PARAMS = $(foreach d, $(KERNEL_SPACE_INC), -I../../kernel/$d)
#
$(B_BUILD_DIR)/usertests.o: usertests.c 
	$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

$(B_BUILD_DIR)/grind.o: grind.c 
	$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

$(B_BUILD_DIR)/mkfs.o: ../../tools/mkfs/mkfs.c
	$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

directories:
	mkdir -p $(B_BUILD_DIR)
	mkdir -p $(B_DEPLOY_DIR)

clean:
	-@rm -rf $(B_BUILD_DIR)
	mkdir -p $(B_BUILD_DIR)

