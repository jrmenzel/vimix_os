#
# Build xv6fs tools
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

vimix: directories $(B_DEPLOY_DIR)/mkfs $(B_DEPLOY_DIR)/fsck.xv6fs

host: directories $(B_DEPLOY_DIR_HOST)/mkfs $(B_DEPLOY_DIR_HOST)/fsck.xv6fs

###########################################################
# compiling apps for VIMIX

$(B_DEPLOY_DIR)/%: $(B_BUILD_DIR)/%.o $(LIB_PATH)/stdlib.a
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T ../user-$(ARCH).ld -o $@ $^

$(B_BUILD_DIR)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

###########################################################
# compiling apps for the host

$(B_DEPLOY_DIR_HOST)/%: $(B_BUILD_DIR_HOST)/%.o
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -o $@ $^

$(B_BUILD_DIR_HOST)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $(USER_SPACE_INC_PARAMS_HOST) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

###########################################################

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)
	@mkdir -p $(B_BUILD_DIR_HOST)

clean: # delete binary
	-@rm -rf $(B_DEPLOY_DIR)/mkfs
	-@rm -rf $(B_DEPLOY_DIR)/fsck.xv6fs
	-@rm -rf $(B_DEPLOY_DIR_HOST)/mkfs
	-@rm -rf $(B_DEPLOY_DIR_HOST)/fsck.xv6fs
