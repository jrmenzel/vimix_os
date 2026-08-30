# SPDX-License-Identifier: MIT 

TARGETS_ARM64 := arm64

# include guard as this file is included unconditionally to figure out the ARCH in the first place.
ifneq ($(filter $(TARGET),$(TARGETS_ARM64)),)

#
# common settings
#
ARCH := arm64
BITWIDTH := 64
KERNEL_FORMAT := bin
PAGE_OFFSET := 0xFFFF000000000000
TEXT_OFFSET := 0x80000
PHYS_OFFSET := 0x0
#PHYS_OFFSET := 0x40000000 # actual RAM start on qemu virt

DEFAULT_EMU := arm64

endif # TARGET
