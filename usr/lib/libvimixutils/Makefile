#
# Build all userspace libs
#
ROOT := ../../..
include ../../MakefileCommonUser.mk
USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I../../../$d)
USER_SPACE_INC_PARAMS_HOST := $(foreach d, $(USER_SPACE_INC_HOST), -I$d)

# for all libs:
LIB_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr/lib
LIB_BUILD_DIR_HOST := $(ROOT)/$(BUILD_DIR_HOST)/usr/lib

# for vimixutils:
LIB_VIMIXUTILS_BUILD_DIR := $(LIB_BUILD_DIR)/libvimixutils
LIB_VIMIXUTILS_BUILD_DIR_HOST := $(LIB_BUILD_DIR_HOST)/libvimixutils

LIB_VIMIXUTILS_O := \
	path.o \
	sysfs.o \
	time.o

LIB_VIMIXUTILS := $(foreach a, $(LIB_VIMIXUTILS_O), $(LIB_VIMIXUTILS_BUILD_DIR)/$a)
LIB_VIMIXUTILS_HOST := $(foreach a, $(LIB_VIMIXUTILS_O), $(LIB_VIMIXUTILS_BUILD_DIR_HOST)/$a)

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/vimixutils.a $(LIB_BUILD_DIR_HOST)/vimixutils.a

CFLAGS += -fPIC

# build libvimixutils
$(LIB_BUILD_DIR)/vimixutils.a : $(LIB_VIMIXUTILS) 
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/vimixutils.a $^

# generic rule to compile .c files to .o files on target
$(LIB_VIMIXUTILS_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

# build libvimixutils on host
$(LIB_BUILD_DIR_HOST)/vimixutils.a : $(LIB_VIMIXUTILS_HOST)
	@printf "$(TASK_COLOR)Create library (host): $(@)\n$(NO_COLOR)"
	@$(AR_HOST) -r -o $(LIB_BUILD_DIR_HOST)/vimixutils.a $^

# generic rule to compile .c files to .o files on host
$(LIB_VIMIXUTILS_BUILD_DIR_HOST)/%.o: %.c
	@printf "$(TASK_COLOR)Compile on host: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) $(USER_SPACE_INC_PARAMS_HOST) -c $< -o $@


directories: # temp build directories
	@mkdir -p $(LIB_VIMIXUTILS_BUILD_DIR)
	@mkdir -p $(LIB_VIMIXUTILS_BUILD_DIR_HOST)

clean:
	-@rm -rf $(LIB_BUILD_DIR)
	-@rm -rf $(LIB_BUILD_DIR_HOST)
