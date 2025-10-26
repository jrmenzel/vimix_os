#
# Makefile settings common to all user space apps
#

ROOT_DIR_MK_USER_COMMON := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
include $(ROOT_DIR_MK_USER_COMMON)../MakefileCommon.mk

#
# user space libs to link against
#
LIB_PATH := $(ROOT)/$(BUILD_DIR)/usr/lib
COMMON_LIBS := $(LIB_PATH)/vimixutils.a $(LIB_PATH)/tomlc17.a

#
# User space include paths
#
USER_SPACE_INC := $(ROOT)/kernel/include
USER_SPACE_INC += $(ROOT)/kernel/arch/$(ARCH)
USER_SPACE_INC += $(ROOT)/usr/lib/libvimixutils
USER_SPACE_INC += $(ROOT)/usr/lib/tomlc17/src

ifeq ($(TARGET), host)
USER_SPACE_INC += /usr/include

else
LDFLAGS += --defsym=USER_TEXT_START=$(USER_TEXT_START)
LDFLAGS += -T $(ROOT)/usr/bin/user-$(ARCH).ld
COMMON_LIBS += $(LIB_PATH)/stdlib.a

USER_SPACE_INC += $(ROOT)/usr/include
endif

B_BUILD_DIR := $(ROOT)/$(BUILD_DIR)/usr/$(BUILD_DIR_SUFFIX)
B_DEPLOY_DIR := $(ROOT)/$(BUILD_DIR)/root/usr/$(DEPLOY_DIR_SUFFIX)

USER_SPACE_INC_PARAMS := $(foreach d, $(USER_SPACE_INC), -I$d)
