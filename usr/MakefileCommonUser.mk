#
# Makefile settings common to all user space apps
#

ROOT_DIR_MK_USER_COMMON := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
include $(ROOT_DIR_MK_USER_COMMON)../MakefileCommon.mk

#
# User space include paths
#
USER_SPACE_INC := $(ROOT)/kernel/include
USER_SPACE_INC += $(ROOT)/kernel/arch/$(ARCH)
USER_SPACE_INC += $(ROOT)/usr/lib/libvimixutils
USER_SPACE_INC += $(ROOT)/usr/lib/tomlc17/src

#
# user space libs to link against
#
LIB_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr$(TARGET_SUFFIX)/lib
COMMON_LIBS := $(LIB_BUILD_DIR)/vimixutils.a $(LIB_BUILD_DIR)/tomlc17.a

B_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr$(TARGET_SUFFIX)/$(BUILD_DIR_SUFFIX)
B_DEPLOY_DIR := $(ROOT)/$(BUILD_DIR)/root$(TARGET_SUFFIX)/usr/$(DEPLOY_DIR_SUFFIX)
XDBG_DEPLOY_DIR := $(ROOT)/$(BUILD_DIR)/root$(TARGET_SUFFIX)/xdbg/usr/$(DEPLOY_DIR_SUFFIX)

ifeq ($(TARGET_OR_HOST), host)
USER_SPACE_INC += /usr/include
else
USER_SPACE_INC += $(ROOT)/usr/include
LDFLAGS += --defsym=USER_TEXT_START=$(USER_TEXT_START)
LDFLAGS += -T $(ROOT)/usr/bin/user.ld -m $(LD_ARCH_STRING)
COMMON_LIBS += $(LIB_BUILD_DIR)/stdlib.a
endif

USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I$d)
