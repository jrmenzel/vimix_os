#
# Build vimixfs tools
#
ROOT := ../../..
BUILD_DIR_SUFFIX := bin/mkfs
DEPLOY_DIR_SUFFIX := bin
include $(ROOT)/usr/MakefileCommonUser.mk

.PHONY: all directories clean 

all: directories $(B_DEPLOY_DIR)/mkfs $(B_DEPLOY_DIR)/fsck.vimixfs

SHARED_O_FILES := $(B_BUILD_DIR)/libvimixfs.o

###########################################################
# compiling apps for VIMIX

$(B_DEPLOY_DIR)/mkfs: $(B_BUILD_DIR)/mkfs.o $(COMMON_LIBS) $(SHARED_O_FILES)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LINKER)  $(LDFLAGS) -o $@ $^

$(B_DEPLOY_DIR)/fsck.vimixfs: $(B_BUILD_DIR)/fsck.vimixfs.o $(COMMON_LIBS) $(SHARED_O_FILES)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LINKER)  $(LDFLAGS) -o $@ $^

$(B_BUILD_DIR)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

###########################################################

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)

clean: # delete binary
	-@rm -rf $(B_DEPLOY_DIR)/mkfs
	-@rm -rf $(B_DEPLOY_DIR)/fsck.vimixfs