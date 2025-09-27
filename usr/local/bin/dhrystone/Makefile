ROOT := ../../../..
BUILD_DIR_SUFFIX := local/bin
DEPLOY_DIR_SUFFIX := local/bin
include $(ROOT)/usr/MakefileCommonUser.mk

USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I$(ROOT)/$d)
USER_SPACE_INC_PARAMS_HOST := $(foreach d, $(USER_SPACE_INC_HOST), -I$d)

.PHONY: all directories clean

all: directories $(B_DEPLOY_DIR)/dhrystone

host: directories $(B_DEPLOY_DIR_HOST)/dhrystone

###########################################################
# compiling for VIMIX
$(B_DEPLOY_DIR)/dhrystone: $(B_BUILD_DIR)/dhry_1.o $(B_BUILD_DIR)/dhry_2.o $(LIB_PATH)/stdlib.a
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T ../../../bin/user-$(ARCH).ld $(LIB_PATH)/stdlib.a $^ -o $@

$(B_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

$(LIB_PATH)/stdlib.a: 
	@$(MAKE) -C ../lib all;

###########################################################
# compiling for the host
$(B_DEPLOY_DIR_HOST)/dhrystone: $(B_BUILD_DIR_HOST)/dhry_1.o $(B_BUILD_DIR_HOST)/dhry_2.o
	@printf "$(TASK_COLOR)Link for host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $^ -o $@

$(B_BUILD_DIR_HOST)/%.o: %.c
	@printf "$(TASK_COLOR)Compile for host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $(USER_SPACE_INC_PARAMS_HOST) -c $< -o $@


directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)
	@mkdir -p $(B_BUILD_DIR_HOST)
	@mkdir -p $(B_DEPLOY_DIR)
	@mkdir -p $(B_DEPLOY_DIR_HOST)

clean: # delete build dir
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_BUILD_DIR_HOST)
	-@rm -rf $(B_DEPLOY_DIR)
	-@rm -rf $(B_DEPLOY_DIR_HOST)
