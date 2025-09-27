#
# Build userspace test app "usertests"
#
ROOT := ../../..
BUILD_DIR_SUFFIX := bin/usertests
DEPLOY_DIR_SUFFIX := bin
include $(ROOT)/usr/MakefileCommonUser.mk

USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I$(ROOT)/$d)
USER_SPACE_INC_PARAMS_HOST := $(foreach d, $(USER_SPACE_INC_HOST), -I$d)

.PHONY: all directories clean usertests

# usertest includes internal kernel API and needs more include search paths
KERNEL_SPACE_INC := ./arch/$(ARCH) # selected architecture, needed for USER_VA_END
KERNEL_SPACE_INC += ./include # public kernel API
KERNEL_SPACE_INC_PARAMS := $(foreach d, $(KERNEL_SPACE_INC), -I../../../kernel/$d)

vimix: directories $(B_DEPLOY_DIR)/usertests

host: directories $(B_DEPLOY_DIR_HOST)/usertests

###########################################################
# compiling apps for VIMIX
$(B_DEPLOY_DIR)/usertests: $(B_BUILD_DIR)/usertests.o $(B_BUILD_DIR)/vimix_only.o $(B_BUILD_DIR)/common_tests.o $(LIB_PATH)/vimixutils.a $(LIB_PATH)/stdlib.a
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) -T ../user-$(ARCH).ld -o $@ $^

$(B_BUILD_DIR)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile with extra flags: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -DKERNBASE=$(KERNBASE) -DMEMORY_SIZE=$(MEMORY_SIZE) $(USER_SPACE_INC_PARAMS) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

$(LIB_PATH)/stdlib.a: 
	@$(MAKE) -C ../../lib stdlib;

$(LIB_PATH)/vimixutils.a: 
	@$(MAKE) -C ../../lib vimixutils;

###########################################################
# compiling apps for the host

$(B_DEPLOY_DIR_HOST)/usertests: $(B_BUILD_DIR_HOST)/usertests.o $(B_BUILD_DIR_HOST)/common_tests.o $(LIB_PATH_HOST)/vimixutils.a
	@$(CC_HOST) $(CFLAGS_HOST) -o $@ $^

$(B_BUILD_DIR_HOST)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile with extra flags (host): $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -DMEMORY_SIZE=$(MEMORY_SIZE) $(USER_SPACE_INC_PARAMS_HOST) $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

$(LIB_PATH_HOST)/vimixutils.a: 
	@$(MAKE) -C ../../lib vimixutils;

###########################################################

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)
	@mkdir -p $(B_BUILD_DIR_HOST)

clean:
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_BUILD_DIR_HOST)
	-@rm -rf $(B_DEPLOY_DIR)/usertests
	-@rm -rf $(B_DEPLOY_DIR_HOST)/usertests
