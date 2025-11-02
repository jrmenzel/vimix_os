#
# Build all userspace libs
#
ROOT := ../../..
include ../../MakefileCommonUser.mk

# for all libs:
LIB_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr/lib

# for vimixutils:
LIB_VIMIXUTILS_BUILD_DIR := $(LIB_BUILD_DIR)/libvimixutils

LIB_VIMIXUTILS_O := \
	path.o \
	security.o \
	sysfs.o \
	time.o

LIB_VIMIXUTILS := $(foreach a, $(LIB_VIMIXUTILS_O), $(LIB_VIMIXUTILS_BUILD_DIR)/$a)

.PHONY: all directories clean

all: directories $(LIB_BUILD_DIR)/vimixutils.a 

CFLAGS += -fPIC

# build libvimixutils
$(LIB_BUILD_DIR)/vimixutils.a : $(LIB_VIMIXUTILS) 
	@printf "$(TASK_COLOR)Create library: $(@)\n$(NO_COLOR)"
	@$(AR) -r -o $(LIB_BUILD_DIR)/vimixutils.a $^

# generic rule to compile .c files to .o files on target
$(LIB_VIMIXUTILS_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) $(USER_SPACE_INC_PARAMS) -c $< -o $@

directories: # temp build directories
	@mkdir -p $(LIB_VIMIXUTILS_BUILD_DIR)

clean:
	-@rm -rf $(LIB_BUILD_DIR)
