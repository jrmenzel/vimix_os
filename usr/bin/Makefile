#
# Build all userspace apps
#
include ../../MakefileCommon.mk
BUILD_DIR_ROOT=../../$(BUILD_DIR)
B_BUILD_DIR=$(BUILD_DIR_ROOT)/usr/bin
B_DEPLOY_DIR=$(BUILD_DIR_ROOT)/root/usr/bin
B_DEPLOY_DIR_HOST=$(BUILD_DIR_ROOT)/root_host/usr/bin

LIB_PATH=$(BUILD_DIR_ROOT)/usr/lib

UPROGS_NAME=\
	init\
	cat\
	echo\
	forktest\
	grep\
	kill\
	ln\
	ls\
	mkdir\
	rm\
	sh\
	usertests\
	grind\
	wc\
	zombie\
	ls\
	stat\
	shutdown\
	mknod\
	date\
	skill\
	sleep

# Some user space apps can be compiled on the host as well,
# e.g. as native Linux apps. This can be helpful for app
# development.
#
UPROGS_NAME_HOST=\
	cat\
	echo\
	forktest\
	grep\
	kill\
	ln\
	ls\
	mkdir\
	rm\
	sh\
	wc\
	zombie\
	stat\
	date\
	skill\
	sleep

UPROGS=$(foreach a, $(UPROGS_NAME), $(B_DEPLOY_DIR)/$a)
PROGS_HOST=$(foreach a, $(UPROGS_NAME_HOST), $(B_DEPLOY_DIR_HOST)/$a)

.PHONY: all directories clean

all: directories $(UPROGS) scripts

$(B_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -I. $(USER_SPACE_INC_PARAMS) -c $< -o $@

$(B_DEPLOY_DIR)/%: $(B_BUILD_DIR)/%.o $(LIB_PATH)/stdlib.a
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T user.ld $(LIB_PATH)/stdlib.a -o $@ $^

$(LIB_PATH)/stdlib.a: 
	$(MAKE) -C ../lib all;

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
	@printf "$(TASK_COLOR)Compile with extra flags: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -DMEMORY_SIZE=$(MEMORY_SIZE) -I. $(USER_SPACE_INC_PARAMS) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

scripts: # copy shell scripts
	cp *.sh $(B_DEPLOY_DIR)

directories: # make temp build directories
	mkdir -p $(B_BUILD_DIR)
	mkdir -p $(B_DEPLOY_DIR)
	mkdir -p $(B_DEPLOY_DIR_HOST)

clean: # delete build dir
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_DEPLOY_DIR)
	-@rm -rf $(B_DEPLOY_DIR_HOST)

# binaries will end up in build/root_host/PATH_IN_VIMIX
host: directories $(PROGS_HOST) # build supported apps for the host OS / architecture

$(B_DEPLOY_DIR_HOST)/%: %.c
	@printf "$(TASK_COLOR)Compile for host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -I/usr/include $< -o $@
