#
# Build mkfs
#

.PHONY: all clean

all: mkfs

# for help and color macros
include ../../MakefileCommon.mk

KERNEL_INCLUDE_PATH=../../kernel/include
CFLAGS  = -Werror -Wall
CFLAGS += -D__USE_REAL_STDC__ # __USE_REAL_STDC__ prevents redefinitions when using includes from VIMIX

mkfs: mkfs.c $(KERNEL_INCLUDE_PATH)/kernel/fs.h $(KERNEL_INCLUDE_PATH)/kernel/param.h # make file system and copy in files
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@gcc $(CFLAGS) -I$(KERNEL_INCLUDE_PATH) -I$(KERNEL_INCLUDE_PATH)/../ -o mkfs mkfs.c

clean: # delete binary
	-@rm mkfs
