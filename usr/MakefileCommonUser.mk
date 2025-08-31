#
# Makefile settings common to all user space apps
#

ROOT_DIR_MK_USER_COMMON := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
include $(ROOT_DIR_MK_USER_COMMON)../MakefileCommon.mk

LDFLAGS += --defsym=USER_TEXT_START=$(USER_TEXT_START)

B_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr/$(BUILD_DIR_SUFFIX)
B_BUILD_DIR_HOST := $(ROOT)/$(BUILD_DIR_HOST)/usr/$(BUILD_DIR_SUFFIX)

B_DEPLOY_DIR := $(ROOT)/$(BUILD_DIR)/root/usr/$(DEPLOY_DIR_SUFFIX)
B_DEPLOY_DIR_HOST := $(ROOT)/$(BUILD_DIR_HOST)/root/usr/$(DEPLOY_DIR_SUFFIX)

LIB_PATH := $(ROOT)/$(BUILD_DIR)/usr/lib
LIB_PATH_HOST := $(ROOT)/$(BUILD_DIR_HOST)/usr/lib

#
# User space include paths
#
USER_SPACE_INC := usr/include           # /usr/include : general purpose C headers for userspace
USER_SPACE_INC += kernel/include        # OS public API, required for stdlib
USER_SPACE_INC += usr/lib/libvimixutils # <vimixutils/foo> headers

USER_SPACE_INC_HOST := /usr/include
USER_SPACE_INC_HOST += $(ROOT)/usr/lib/libvimixutils # <vimixutils/foo> headers
