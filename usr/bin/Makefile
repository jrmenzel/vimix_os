#
# Build all userspace apps
#
ROOT := ../..
BUILD_DIR_SUFFIX := bin
DEPLOY_DIR_SUFFIX := bin
include $(ROOT)/usr/MakefileCommonUser.mk

USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I$(ROOT)/$d)
USER_SPACE_INC_PARAMS_HOST := $(foreach d, $(USER_SPACE_INC_HOST), -I$d)

KERNEL_SPACE_INC := ./include # public kernel API
KERNEL_SPACE_INC_PARAMS := $(foreach d, $(KERNEL_SPACE_INC), -I../../kernel/$d)

UPROGS_VIMIX_ONLY := init\
	fsinfo\
    grind\
    shutdown\
	mount\
	umount\
	meminfo\
	skill

# Some user space apps can be compiled on the host as well,
# e.g. as native Linux apps. This can be helpful for app
# development.
#
UPROGS_NAME_BOTH := cat\
	cp\
	date\
	echo\
	forktest\
	fortune\
	fsbench\
	grep\
	kill\
	ln\
	ls\
	mkdir\
	mknod\
	rm\
	rmdir\
	sh\
	sleep\
	stat\
	statvfs\
	time\
	wc\
	which\
	wumpus\
	xxd\
	zombie

UPROGS_NAME := $(UPROGS_VIMIX_ONLY) $(UPROGS_NAME_BOTH)

UPROGS := $(foreach a, $(UPROGS_NAME), $(B_DEPLOY_DIR)/$a)
PROGS_HOST := $(foreach a, $(UPROGS_NAME_BOTH), $(B_DEPLOY_DIR_HOST)/$a)

# keep intermediate .o files
.PRECIOUS: $(B_BUILD_DIR)/%.o $(B_BUILD_DIR_HOST)/%.o 

.PHONY: all directories clean usertests mkfs host

all: directories $(UPROGS) usertests mkfs #scripts

# binaries will end up in build/root_host/PATH_IN_VIMIX
host: directories $(PROGS_HOST) usertests_host mkfs_host #scripts_host # build supported apps for the host OS / architecture

###########################################################
# compiling apps for VIMIX
$(B_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

$(B_DEPLOY_DIR)/%: $(B_BUILD_DIR)/%.o $(LIB_PATH)/vimixutils.a $(LIB_PATH)/stdlib.a 
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T user-$(ARCH).ld $^ -o $@

$(LIB_PATH)/stdlib.a: 
	@$(MAKE) -C ../lib stdlib;

$(LIB_PATH)/vimixutils.a: 
	@$(MAKE) -C ../lib vimixutils;

###########################################################
# compiling apps for the host
$(B_BUILD_DIR_HOST)/%.o: %.c
	@printf "$(TASK_COLOR)Compile for host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $(USER_SPACE_INC_PARAMS_HOST) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

$(B_DEPLOY_DIR_HOST)/%: $(B_BUILD_DIR_HOST)/%.o $(LIB_PATH_HOST)/vimixutils.a
	@printf "$(TASK_COLOR)Link for host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $^ -o $@

$(LIB_PATH_HOST)/vimixutils.a: 
	@$(MAKE) -C ../lib vimixutils;

###########################################################

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)
	@mkdir -p $(B_BUILD_DIR_HOST)
	@mkdir -p $(B_DEPLOY_DIR)
	@mkdir -p $(B_DEPLOY_DIR_HOST)

clean: # delete build dir
	@$(MAKE) -C usertests clean;
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_BUILD_DIR_HOST)
	-@rm -rf $(B_DEPLOY_DIR)
	-@rm -rf $(B_DEPLOY_DIR_HOST)

###########################################################
# programs in sub dirs
#
usertests:
	@$(MAKE) -C usertests vimix;

usertests_host:
	@$(MAKE) -C usertests host;

mkfs:
	@$(MAKE) -C mkfs vimix;

mkfs_host:
	@$(MAKE) -C mkfs host;
