#
# Build mkfs
#

.PHONY: all clean

all: mkfs

KERNEL_INCLUDE_PATH=../../kernel/include
CFLAGS  = -Werror -Wall
CFLAGS += -D__USE_REAL_STDC__ # prevents redefinitions

mkfs: mkfs.c $(KERNEL_INCLUDE_PATH)/kernel/fs.h $(KERNEL_INCLUDE_PATH)/kernel/param.h
	gcc $(CFLAGS) -I$(KERNEL_INCLUDE_PATH) -I$(KERNEL_INCLUDE_PATH)/../ -o mkfs mkfs.c

clean:
	-@rm mkfs