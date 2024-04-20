#
# Settings shared between the kernel and user space apps
#

#####
# target architecture subpath in kernel/
ARCH=arch/riscv
BUILD_TYPE=debug
#BUILD_TYPE=release

KERNEL_NAME=kernel-xv6
KERNEL_FILE=$(BUILD_DIR)/$(KERNEL_NAME)

#####
# e.g. riscv64-unknown-elf-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	elif riscv64-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-elf-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, set TOOLPREFIX in MakefileCommon.mk." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# "build" is used as the foldername outside of the Makefiles as well.
# So if this gets changed, the Makefiles should be fine, but externel
# configs and scripts might need to be updated.
# (e.g. launch.json).
BUILD_DIR=build

#####
# tools
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
AR = $(TOOLPREFIX)ar
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

#####
# C compile flags

CFLAGS += -march=rv64gc
CFLAGS += -fno-omit-frame-pointer
CFLAGS += -Wall -Werror

ifeq ($(BUILD_TYPE), debug)
CFLAGS += -O0 
CFLAGS += -ggdb -gdwarf-2 -DDEBUG # debug info
CFLAGS += -g # native format for debug info
CFLAGS += -MD
else
CFLAGS += -O2
endif

CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -nostdinc
CFLAGS += -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
CFLAGS += $(EXTRA_DEBUG_FLAGS)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

#####
# linker flags
LDFLAGS = -z max-page-size=4096

#
# User space include paths
#
USER_SPACE_INC  = ../../usr/include     # /usr/include : general purpose C headers for userspace
USER_SPACE_INC += ../../kernel/include  # OS public API, required for stdlib
USER_SPACE_INC_PARAMS=$(foreach d, $(USER_SPACE_INC), -I$d)
