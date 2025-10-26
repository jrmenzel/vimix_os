ROOT := ../../../..
BUILD_DIR_SUFFIX := local/bin
DEPLOY_DIR_SUFFIX := local/bin
include $(ROOT)/usr/MakefileCommonUser.mk

.PHONY: all directories clean

all: directories $(B_DEPLOY_DIR)/dhrystone

###########################################################
# compiling
$(B_DEPLOY_DIR)/dhrystone: $(B_BUILD_DIR)/dhry_1.o $(B_BUILD_DIR)/dhry_2.o $(COMMON_LIBS)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LINKER)  $(LDFLAGS) $^ -o $@

$(B_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)
	@mkdir -p $(B_DEPLOY_DIR)

clean: # delete build dir
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_DEPLOY_DIR)
