#
# Build all userspace apps
#
ROOT := ../..
include $(ROOT)/usr/MakefileCommonUser.mk

USER_SPACE_INC_PARAMS=$(foreach d, $(USER_SPACE_INC), -I$(ROOT)/$d)
USER_SPACE_INC_HOST_PARAMS=$(foreach d, $(USER_SPACE_INC_HOST), -I$d)

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
	grind\
	wc\
	zombie\
	ls\
	stat\
	shutdown\
	mknod\
	date\
	skill\
	sleep\
	rmdir

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
	sleep\
	rmdir

UPROGS=$(foreach a, $(UPROGS_NAME), $(B_DEPLOY_DIR)/$a)
PROGS_HOST=$(foreach a, $(UPROGS_NAME_HOST), $(B_DEPLOY_DIR_HOST)/$a)

# keep intermediate .o files
.PRECIOUS: $(B_BUILD_DIR)/%.o $(B_BUILD_DIR_HOST)/%.o 

.PHONY: all directories clean usertests host

all: directories $(UPROGS) usertests scripts

# binaries will end up in build/root_host/PATH_IN_VIMIX
host: directories $(PROGS_HOST) usertests_host scripts_host # build supported apps for the host OS / architecture

###########################################################
# compiling apps for VIMIX
$(B_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

$(B_DEPLOY_DIR)/%: $(B_BUILD_DIR)/%.o $(LIB_PATH)/stdlib.a
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T user-$(ARCH).ld $(LIB_PATH)/stdlib.a -o $@ $^

###########################################################
# compiling apps for the host
$(B_DEPLOY_DIR_HOST)/%: $(B_BUILD_DIR_HOST)/%.o
	@printf "$(TASK_COLOR)Link for host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $< -o $@

$(B_BUILD_DIR_HOST)/%.o: %.c
	@printf "$(TASK_COLOR)Compile for host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $(USER_SPACE_INC_HOST_PARAMS) -c $< -o $@

$(LIB_PATH)/stdlib.a: 
	$(MAKE) -C ../lib all;

###########################################################
scripts: # copy shell scripts
	cp *.sh $(B_DEPLOY_DIR)

scripts_host: # copy shell scripts host
	cp *.sh $(B_DEPLOY_DIR_HOST)

directories: # make temp build directories
	mkdir -p $(B_BUILD_DIR)
	mkdir -p $(B_BUILD_DIR_HOST)
	mkdir -p $(B_DEPLOY_DIR)
	mkdir -p $(B_DEPLOY_DIR_HOST)

clean: # delete build dir
	$(MAKE) -C usertests clean;
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_BUILD_DIR_HOST)
	-@rm -rf $(B_DEPLOY_DIR)
	-@rm -rf $(B_DEPLOY_DIR_HOST)

###########################################################
# programs in sub dirs
#
usertests:
	$(MAKE) -C usertests vimix;

usertests_host:
	$(MAKE) -C usertests host;
