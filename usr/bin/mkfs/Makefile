#
# Build vimixfs tools
#
ROOT := ../../..
BUILD_DIR_SUFFIX := bin/mkfs
DEPLOY_DIR_SUFFIX := bin
include $(ROOT)/usr/MakefileCommonUser.mk

USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I$(ROOT)/$d)
USER_SPACE_INC_PARAMS_HOST := $(foreach d, $(USER_SPACE_INC_HOST), -I$d)

.PHONY: all directories clean 

KERNEL_SPACE_INC := ./include # public kernel API
KERNEL_SPACE_INC_PARAMS := $(foreach d, $(KERNEL_SPACE_INC), -I../../../kernel/$d)

vimix: directories $(B_DEPLOY_DIR)/mkfs $(B_DEPLOY_DIR)/fsck.vimixfs

host: directories $(B_DEPLOY_DIR_HOST)/mkfs $(B_DEPLOY_DIR_HOST)/fsck.vimixfs

SHARED_O_FILES := $(B_BUILD_DIR)/libvimixfs.o
SHARED_O_FILES_HOST := $(B_BUILD_DIR_HOST)/libvimixfs.o

###########################################################
# compiling apps for VIMIX

$(B_DEPLOY_DIR)/mkfs: $(B_BUILD_DIR)/mkfs.o $(LIB_PATH)/stdlib.a $(LIB_PATH)/vimixutils.a $(SHARED_O_FILES)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T ../user-$(ARCH).ld -o $@ $^

$(B_DEPLOY_DIR)/fsck.vimixfs: $(B_BUILD_DIR)/fsck.vimixfs.o $(LIB_PATH)/stdlib.a $(LIB_PATH)/vimixutils.a $(SHARED_O_FILES)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T ../user-$(ARCH).ld -o $@ $^

$(B_BUILD_DIR)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

$(LIB_PATH)/stdlib.a: 
	@$(MAKE) -C ../../lib stdlib;

$(LIB_PATH)/vimixutils.a: 
	@$(MAKE) -C ../../lib vimixutils;

###########################################################
# compiling apps for the host

$(B_DEPLOY_DIR_HOST)/mkfs: $(B_BUILD_DIR_HOST)/mkfs.o $(LIB_PATH_HOST)/vimixutils.a $(SHARED_O_FILES_HOST)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -o $@ $^

$(B_DEPLOY_DIR_HOST)/fsck.vimixfs: $(B_BUILD_DIR_HOST)/fsck.vimixfs.o $(LIB_PATH_HOST)/vimixutils.a $(SHARED_O_FILES_HOST)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -o $@ $^

$(B_BUILD_DIR_HOST)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $(USER_SPACE_INC_PARAMS_HOST) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

$(LIB_PATH_HOST)/vimixutils.a: 
	@$(MAKE) -C ../../lib vimixutils;

###########################################################

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)
	@mkdir -p $(B_BUILD_DIR_HOST)

clean: # delete binary
	-@rm -rf $(B_DEPLOY_DIR)/mkfs
	-@rm -rf $(B_DEPLOY_DIR)/fsck.vimixfs
	-@rm -rf $(B_DEPLOY_DIR_HOST)/mkfs
	-@rm -rf $(B_DEPLOY_DIR_HOST)/fsck.vimixfs
