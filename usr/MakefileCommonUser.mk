#
# Makefile settings common to all user space apps
#

LIB_PATH=$(BUILD_DIR_ROOT)/usr/lib

LDFLAGS+=--defsym=USER_TEXT_START=$(USER_TEXT_START)

BUILD_DIR_ROOT=$(ROOT)/$(BUILD_DIR)
BUILD_DIR_ROOT_HOST=$(ROOT)/$(BUILD_DIR_HOST)

B_BUILD_DIR=$(BUILD_DIR_ROOT)/usr/bin
B_BUILD_DIR_HOST=$(BUILD_DIR_ROOT_HOST)/usr/bin

B_DEPLOY_DIR=$(BUILD_DIR_ROOT)/root/usr/bin
B_DEPLOY_DIR_HOST=$(BUILD_DIR_ROOT_HOST)/root/usr/bin
