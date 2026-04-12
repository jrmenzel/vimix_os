#
# Build userspace test app "usertests"
#
ROOT := ../../..
BUILD_DIR_SUFFIX := bin/usertests
DEPLOY_DIR_SUFFIX := bin
include $(ROOT)/usr/MakefileCommonUser.mk

.PHONY: all directories clean usertests

all: directories $(B_DEPLOY_DIR)/usertests

OBJECTS := \
	$(B_BUILD_DIR)/usertests.o \
	$(B_BUILD_DIR)/common_tests.o

ifneq ($(TARGET), host)
OBJECTS += \
	$(B_BUILD_DIR)/vimix_only.o
endif

###########################################################
# compiling apps
$(B_DEPLOY_DIR)/usertests: $(OBJECTS) $(COMMON_LIBS)
	@printf "$(TASK_COLOR)Link binary: $(@)\n$(NO_COLOR)"
	@$(LINKER)  $(LDFLAGS) -o $@ $^

$(B_BUILD_DIR)/%.o: %.c 
	@printf "$(TASK_COLOR)Compile with extra flags: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -DKERNBASE=$(KERNBASE) $(USER_SPACE_INC_PARAMS) -c $< -o $@

###########################################################

directories: # make temp build directories
	@mkdir -p $(B_BUILD_DIR)

clean:
	-@rm -rf $(B_BUILD_DIR)
	-@rm -rf $(B_DEPLOY_DIR)/usertests
