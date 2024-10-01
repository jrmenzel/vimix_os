#
# Build tools
#

.PHONY: all clean

all: mkfs fsck.xv6fs

# for help and color macros
include ../../MakefileCommon.mk

KERNEL_INCLUDE_PATH := ../../kernel/include
CFLAGS_HOST += -Wno-stringop-truncation -D__USE_REAL_STDC__ # __USE_REAL_STDC__ prevents redefinitions when using includes from VIMIX

mkfs: mkfs.c $(KERNEL_INCLUDE_PATH)/kernel/fs.h $(KERNEL_INCLUDE_PATH)/kernel/param.h # make file system and copy in files
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -I$(KERNEL_INCLUDE_PATH) -I$(KERNEL_INCLUDE_PATH)/../ -o mkfs mkfs.c

fsck.xv6fs: fsck.xv6fs.c $(KERNEL_INCLUDE_PATH)/kernel/fs.h $(KERNEL_INCLUDE_PATH)/kernel/param.h
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC_HOST) $(CFLAGS_HOST) -I$(KERNEL_INCLUDE_PATH) -I$(KERNEL_INCLUDE_PATH)/../ -o fsck.xv6fs fsck.xv6fs.c

clean: # delete binary
	-@rm mkfs fsck.xv6fs
