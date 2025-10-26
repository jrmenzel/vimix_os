#
# Settings shared between the kernel and user space apps
#

BUILD_TYPE := debug
#BUILD_TYPE := release
# release builds with debug symbols increase the binary sizes
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
ifdef BUILD_PREFIX
BUILD_PREFIX_2 := $(BUILD_PREFIX)/
endif

ifeq ($(TARGET), host)
BUILD_DIR := $(BUILD_PREFIX_2)build_host
else
BUILD_DIR := $(BUILD_PREFIX_2)build
endif
BUILD_DIR_HOST := $(BUILD_PREFIX_2)build_host


KERNEL_NAME := kernel-vimix
KERNEL_FILE := $(BUILD_DIR)/boot/$(KERNEL_NAME)

# create assembly files from C, can be compared with the VSCode extension "Disassembly Explorer"
#CREATE_ASSEMBLY=yes

# First page of the user space apps. Needed by user.ld and the kernel source
# so defined here. Don't place at 0 to make NULL pointer dereferences illegal.
# 0x400000 = 4MB in aligns with super pages and is also used in Linux
USER_TEXT_START := "0x400000"

#####
# tools
ifeq ($(TARGET), host)
CC := gcc
AR := ar
LD := ld
else
CC := $(TOOLPREFIX)gcc$(GCCPOSTFIX)
AS := $(TOOLPREFIX)gas
AR := $(TOOLPREFIX)ar
LD := $(TOOLPREFIX)ld
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump
endif

#####
# C compile flags
git-hash=$(shell git log --pretty=format:'%h' -n 1)
CFLAGS_COMMON := -DGIT_HASH=$(git-hash)
CFLAGS_COMMON += -fno-omit-frame-pointer
CFLAGS_COMMON += -Wall -Werror -Wno-stringop-truncation
CFLAGS_COMMON += -I.
# Disable position independend code generation
CFLAGS_COMMON += -fno-pie -no-pie

ifeq ($(ANALYZE), 1)
CFLAGS_COMMON += -fanalyzer
endif

DEBUG_FLAGS := -ggdb -gdwarf -DDEBUG -g # debug info

ifeq ($(BUILD_TYPE), debug)
CFLAGS_COMMON += -O0 
CFLAGS_COMMON += $(DEBUG_FLAGS)
CFLAGS_COMMON += -MD
else
CFLAGS_COMMON += -O2
ifeq ($(REL_WITH_DEBUG), yes)
# release with debug symbols
CFLAGS_COMMON += $(DEBUG_FLAGS)
endif
endif

CFLAGS_TARGET_ONLY := $(ARCH_CFLAGS) -D__ARCH_$(BITWIDTH)BIT
CFLAGS_TARGET_ONLY += -ffreestanding -fno-common -nostdlib
CFLAGS_TARGET_ONLY += -nostdinc
CFLAGS_TARGET_ONLY += -fno-stack-protector

ifeq ($(TARGET), host)
# c flags for the user space apps on the host OS (with host stdlib):
CFLAGS := $(CFLAGS_COMMON) -DBUILD_ON_HOST -D__USE_REAL_STDC 

# linker command and flags
LINKER := $(CC) $(CFLAGS)
else
# c flags for the kernel and user space apps on the target OS:
CFLAGS := $(CFLAGS_COMMON) $(CFLAGS_TARGET_ONLY)

# linker command and flags
LDFLAGS := -z max-page-size=4096 $(ARCH_LFLAGS)
LINKER := $(LD)
endif

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
