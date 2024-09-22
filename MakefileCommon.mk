#
# Settings shared between the kernel and user space apps
#

PLATFORM=qemu
#PLATFORM=spike

BUILD_TYPE=debug
#BUILD_TYPE=release
#REL_WITH_DEBUG=yes

# RAM in MB
# Note:
# - OpenSBI uses the lowest 2MB for itself
# - if RAMDISK_BOOTLOADER is set, the ram disk can use up a few MB
# (the boot loader might reserve more than is needed)
MEMORY_SIZE=16

# for qemu and Spike
CPUS=4

#####
# target architecture subpath in kernel/
ARCH=arch/riscv

ifeq ($(PLATFORM), qemu)
# 32 and 64 supported
BITWIDTH=64
# with and without SBI supported
SBI_SUPPORT=yes
RV_ENABLE_EXT_C=yes
# use this or a ramdisk
VIRTIO_DISK=yes
#RAMDISK_EMBEDDED=yes
#RAMDISK_BOOTLOADER=yes
PLATFORM_DEFINE = -D_PLATFORM_QEMU
else ifeq ($(PLATFORM), spike)
# 64 fails if Spike defaults to a Sv48 MMU
BITWIDTH=32
RV_ENABLE_EXT_C=yes
RAMDISK_EMBEDDED=yes
PLATFORM_DEFINE = -D_PLATFORM_SPIKE
else
$(error PLATFORM not set)
endif


# compile with C extension for compressed instructions
#RV_ENABLE_EXT_C=yes
# assume root partition on virtio disk
#VIRTIO_DISK=yes
# statically added to the kernel binary, low flexibility but no need fo boot loader support
#RAMDISK_EMBEDDED=yes
# let qemu or a boot loader load the file from a filesystem and load it for us in memory
#RAMDISK_BOOTLOADER=yes

# "build" is used as the foldername outside of the Makefiles as well.
# So if this gets changed, the Makefiles should be fine, but externel
# configs and scripts might need to be updated.
# (e.g. launch.json).
BUILD_DIR=build

KERNEL_NAME=kernel-vimix
KERNEL_FILE=$(BUILD_DIR)/$(KERNEL_NAME)

# create assembly files from C, can be compared with the VSCode extension "Disassembly Explorer"
#CREATE_ASSEMBLY=yes

# Start automated tests and shutdown OS afterwards. Will also shutdown on panic.
#EXTRA_DEBUG_FLAGS=-DDEBUG_AUTOSTART_USERTESTS

# First page of the user space apps. Needed by user.ld and the kernel source
# so defined here. Don't place at 0 to make NULL pointer dereferences illegal.
USER_TEXT_START="0x1000"

#####
# e.g. riscv64-unknown-elf-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
ifeq ($(BITWIDTH), 64)
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
else
TOOLPREFIX := $(shell if riscv32-unknown-elf-objdump -i 2>&1 | grep 'elf32-big' >/dev/null 2>&1; \
	then echo 'riscv32-unknown-elf-'; \
	elif riscv32-linux-gnu-objdump -i 2>&1 | grep 'elf32-big' >/dev/null 2>&1; \
	then echo 'riscv32-linux-gnu-'; \
	elif riscv32-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf32-big' >/dev/null 2>&1; \
	then echo 'riscv32-unknown-linux-gnu-'; \
	elif riscv32-elf-objdump -i 2>&1 | grep 'elf32-big' >/dev/null 2>&1; \
	then echo 'riscv32-elf-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv32 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, set TOOLPREFIX in MakefileCommon.mk." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif # bitwidth
endif

#####
# tools
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
AR = $(TOOLPREFIX)ar
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# for compiling userspace apps on the host platform
CC_HOST = gcc

git-hash=$(shell git log --pretty=format:'%h' -n 1)
CFLAGS_COMMON = -DGIT_HASH=$(git-hash)

#####
# C compile flags
CFLAGS_COMMON += -fno-omit-frame-pointer
CFLAGS_COMMON += -Wall -Werror
CFLAGS_COMMON += -I.

ifeq ($(BUILD_TYPE), debug)
CFLAGS_COMMON += -O0 
CFLAGS_COMMON += -ggdb -gdwarf-2 -DDEBUG # debug info
CFLAGS_COMMON += -g # native format for debug info
CFLAGS_COMMON += -MD
else
CFLAGS_COMMON += -O2
ifeq ($(REL_WITH_DEBUG), yes)
# release with debug symbols
CFLAGS_COMMON += -ggdb -gdwarf-2 -DDEBUG # debug info
CFLAGS_COMMON += -g # native format for debug info
endif
endif

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS_COMMON += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS_COMMON += -fno-pie -nopie
endif

ifeq ($(BITWIDTH), 64)
ifeq ($(RV_ENABLE_EXT_C), yes)
CFLAGS_TARGET_ONLY = -march=rv64gc -D_ARCH_64BIT
else
CFLAGS_TARGET_ONLY = -march=rv64g -D_ARCH_64BIT
endif
else
ifeq ($(RV_ENABLE_EXT_C), yes)
CFLAGS_TARGET_ONLY = -march=rv32gc -D_ARCH_32BIT
else
CFLAGS_TARGET_ONLY = -march=rv32g -D_ARCH_32BIT
endif
endif
CFLAGS_TARGET_ONLY += -mcmodel=medany
CFLAGS_TARGET_ONLY += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS_TARGET_ONLY += -nostdinc
CFLAGS_TARGET_ONLY += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# c flags for the kernel and user space apps on the target OS:
CFLAGS = $(CFLAGS_TARGET_ONLY) $(CFLAGS_COMMON) $(EXTRA_DEBUG_FLAGS) $(PLATFORM_DEFINE)
# c flags for the user space apps on the host OS (with host stdlib):
CFLAGS_HOST = $(CFLAGS_COMMON) $(EXTRA_DEBUG_FLAGS) -DBUILD_ON_HOST

#####
# linker flags
LDFLAGS = -z max-page-size=4096

#
# User space include paths
#
USER_SPACE_INC  = ../../usr/include     # /usr/include : general purpose C headers for userspace
USER_SPACE_INC += ../../kernel/include  # OS public API, required for stdlib
USER_SPACE_INC_PARAMS=$(foreach d, $(USER_SPACE_INC), -I$d)

USER_SPACE_INC_HOST  = /usr/include
USER_SPACE_INC_HOST_PARAMS=$(foreach d, $(USER_SPACE_INC_HOST), -I$d)

BUILD_DIR_HOST=build_host

TASK_COLOR  = \033[0;34m
YELLOW      = \033[93m
NO_COLOR    = \033[m
ERROR_COLOR = \033[0;31m

# add "make help" to all Makefiles, based on https://blog.ovhcloud.com/pimp-my-makefile/
help: # Print help on Makefile
	@grep '^[^.#]\+:\s\+.*#' Makefile | \
	sed "s/\(.\+\):\s*\(.*\) #\s*\(.*\)/`printf "$(YELLOW)"`\1`printf "$(NO_COLOR)"`	\3/" | \
	expand -t20

# just calling "make" will execute the default target, which would be "help" as it is
# the first in the file. Avaid a second include for help at the end of the makefiles
# by setting an explicit default.
.DEFAULT_GOAL := all
