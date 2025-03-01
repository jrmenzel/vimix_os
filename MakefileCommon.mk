#
# Settings shared between the kernel and user space apps
#

BUILD_TYPE := debug
#BUILD_TYPE := release
# release builds with debug symbols increase the binary sizes and can 
# push usertests over the limit of fitting on a xv6 filesystem
#REL_WITH_DEBUG := yes

#####
# target architecture subpath in kernel/
ARCH := riscv

# dir of this makefile: last in the MAKEFILE_LIST is the current file
# (so call before including anything), realpath and dir get a dir incl
# trailing '/'
ROOT_DIR_MK_COMMON := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
include $(ROOT_DIR_MK_COMMON)kernel/arch/$(ARCH)/MakefileArch.mk


# "build" is used as the foldername outside of the Makefiles as well.
# So if this gets changed, the Makefiles should be fine, but externel
# configs and scripts might need to be updated.
# (e.g. launch.json).
BUILD_DIR := build

KERNEL_NAME := kernel-vimix
KERNEL_FILE := $(BUILD_DIR)/$(KERNEL_NAME)

# create assembly files from C, can be compared with the VSCode extension "Disassembly Explorer"
#CREATE_ASSEMBLY=yes

# First page of the user space apps. Needed by user.ld and the kernel source
# so defined here. Don't place at 0 to make NULL pointer dereferences illegal.
USER_TEXT_START := "0x1000"

#####
# tools
CC := $(TOOLPREFIX)gcc$(GCCPOSTFIX)
AS := $(TOOLPREFIX)gas
AR := $(TOOLPREFIX)ar
LD := $(TOOLPREFIX)ld
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

# for compiling userspace apps on the host platform
CC_HOST := gcc
HOST_GCC_VERSION_AT_LEAST_12 := $(shell expr `$(CC_HOST) -dumpversion | cut -f1 -d.` \>= 12)
HOST_GCC_VERSION_AT_LEAST_13 := $(shell expr `$(CC_HOST) -dumpversion | cut -f1 -d.` \>= 13)
HOST_GCC_VERSION_AT_LEAST_14 := $(shell expr `$(CC_HOST) -dumpversion | cut -f1 -d.` \>= 14)

git-hash=$(shell git log --pretty=format:'%h' -n 1)
CFLAGS_COMMON := -DGIT_HASH=$(git-hash)

#####
# C compile flags
CFLAGS_COMMON += -fno-omit-frame-pointer
CFLAGS_COMMON += -Wall -Werror -Wno-stringop-truncation
CFLAGS_COMMON += -I.
# Disable position independend code generation
CFLAGS_COMMON += -fno-pie -no-pie

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

CFLAGS_TARGET_ONLY += $(ARCH_CFLAGS)
CFLAGS_TARGET_ONLY += -ffreestanding -fno-common -nostdlib
CFLAGS_TARGET_ONLY += -nostdinc
CFLAGS_TARGET_ONLY += -fno-stack-protector

# c flags for the kernel and user space apps on the target OS:
CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_TARGET_ONLY)
# c flags for the user space apps on the host OS (with host stdlib):
CFLAGS_HOST := $(CFLAGS_COMMON) -DBUILD_ON_HOST -D__USE_REAL_STDC 

#####
# linker flags
LDFLAGS := -z max-page-size=4096 $(ARCH_LFLAGS)

BUILD_DIR_HOST := build_host

TASK_COLOR  := \033[0;34m
YELLOW      := \033[93m
NO_COLOR    := \033[m
ERROR_COLOR := \033[0;31m

# add "make help" to all Makefiles, based on https://blog.ovhcloud.com/pimp-my-makefile/
help: # Print help on Makefile
	@grep '^[^.#]\+:\s\+.*#' Makefile | \
	sed "s/\(.\+\):\s*\(.*\) #\s*\(.*\)/`printf "$(YELLOW)"`\1`printf "$(NO_COLOR)"`	\3/" | \
	expand -t20

# just calling "make" will execute the default target, which would be "help" as it is
# the first in the file. Avaid a second include for help at the end of the makefiles
# by setting an explicit default.
.DEFAULT_GOAL := all
