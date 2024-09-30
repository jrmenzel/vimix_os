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
# - Used as a fallback if no device tree file is provided by the boot loader
# - Used by the usertests app as long as there is no API to query the available memory
# - Of course used to set up the emulators with the same amount of memory
MEMORY_SIZE=64

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
RV_ENABLE_EXT_SSTC=yes
# use this or a ramdisk
VIRTIO_DISK=yes
#RAMDISK_EMBEDDED=yes
#RAMDISK_BOOTLOADER=yes
else ifeq ($(PLATFORM), spike)
# 64 fails if Spike defaults to a Sv48 MMU
BITWIDTH=32
RV_ENABLE_EXT_C=yes
#RAMDISK_EMBEDDED=yes
RAMDISK_BOOTLOADER=yes
else
$(error PLATFORM not set)
endif

# pick timer source
ifeq ($(SBI_SUPPORT), yes)
TIMER_SOURCE := TIMER_SOURCE_SBI
BOOT_MODE = BOOT_S_MODE
else
TIMER_SOURCE := TIMER_SOURCE_CLINT
BOOT_MODE = BOOT_M_MODE
endif
# preferred timer source
ifeq ($(RV_ENABLE_EXT_SSTC), yes)
TIMER_SOURCE := TIMER_SOURCE_SSTC
endif


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

# target instruction set and bit width
MARCH = rv$(BITWIDTH)ima
# optional: compressed instructions
ifeq ($(RV_ENABLE_EXT_C), yes)
MARCH := $(MARCH)c
endif

# prior to gcc 12 the extensions zicsr and zifencei were implicit in the base ISA
GCC_VERSION_AT_LEAST_12 := $(shell expr `$(CC) -dumpversion | cut -f1 -d.` \>= 12)
ifeq "$(GCC_VERSION_AT_LEAST_12)" "1"
# mandatory: CSRs and fence instructions
MARCH := $(MARCH)_zicsr_zifencei
# optional: s-mode timer extension
ifeq ($(RV_ENABLE_EXT_SSTC), yes)
MARCH := $(MARCH)_sstc
endif
endif

ifeq ($(RV_ENABLE_EXT_SSTC), yes)
EXT_DEFINES = -D__RISCV_EXT_SSTC
endif

# calling convention
ifeq ($(BITWIDTH), 32)
MABI = ilp32
else
MABI = lp64
endif

CFLAGS_TARGET_ONLY = -march=$(MARCH) -mabi=$(MABI) -D_ARCH_$(BITWIDTH)BIT $(EXT_DEFINES)

CFLAGS_TARGET_ONLY += -mcmodel=medany
CFLAGS_TARGET_ONLY += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS_TARGET_ONLY += -nostdinc
CFLAGS_TARGET_ONLY += -fno-stack-protector

# c flags for the kernel and user space apps on the target OS:
CFLAGS = $(CFLAGS_TARGET_ONLY) $(CFLAGS_COMMON) $(EXTRA_DEBUG_FLAGS)
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
