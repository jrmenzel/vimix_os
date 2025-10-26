#
# Build all userspace apps
#
ROOT := ../..
BUILD_DIR_SUFFIX := bin
DEPLOY_DIR_SUFFIX := bin
include $(ROOT)/usr/MakefileCommonUser.mk

# Some user space apps can be compiled on the host as well,
# e.g. as native Linux apps. This can be helpful for app
# development.
#
UPROGS_NAME := cat\
	cp\
	chmod\
	chgrp\
	chown\
	date\
	echo\
	forktest\
	fortune\
	fsbench\
	grep\
	id\
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

ifneq ($(TARGET), host)

UPROGS_NAME += init\
	fsinfo\
    grind\
	login\
    shutdown\
	mount\
	umount\
	meminfo\
	skill\
	su

endif

UPROGS := $(foreach a, $(UPROGS_NAME), $(B_DEPLOY_DIR)/$a)

# keep intermediate .o files
.PRECIOUS: $(B_BUILD_DIR)/%.o 

.PHONY: all directories clean usertests mkfs

all: directories $(UPROGS) usertests mkfs 

###########################################################
# compiling apps for VIMIX
$(B_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

$(B_DEPLOY_DIR)/%: $(B_BUILD_DIR)/%.o $(COMMON_LIBS)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LINKER)  $(LDFLAGS) $^ -o $@

###########################################################

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)
	@mkdir -p $(B_DEPLOY_DIR)

clean: # delete build dir
	@$(MAKE) -C usertests clean;
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_DEPLOY_DIR)

###########################################################
# programs in sub dirs
#
usertests:
	@$(MAKE) TARGET=$(TARGET) -C usertests all;

mkfs:
	@$(MAKE) TARGET=$(TARGET) -C mkfs all;
