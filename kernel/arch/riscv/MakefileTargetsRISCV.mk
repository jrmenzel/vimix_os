# SPDX-License-Identifier: MIT 

TARGETS_RISCV := rv32 rv64 rv32m rv64m

# include guard as this file is included unconditionally to figure out the ARCH in the first place.
ifneq ($(filter $(TARGET),$(TARGETS_RISCV)),)

#
# common settings
#
ARCH := riscv
KERNEL_FORMAT := elf
PHYS_OFFSET := 0x80000000

#
# RISC-V specific
#
# compile with compressed instructions:
RV_ENABLE_EXT_C := yes
# compile with sstc timer, only used if the support is detected at runtime
# so only set to "no" if SBI timers should be enforced for testing
RV_ENABLE_EXT_SSTC := yes

#
# Target specific
#
ifeq ($(TARGET), rv32)
BITWIDTH := 32
BOOT_MODE := BOOT_S_MODE
RELOC_KERNEL := yes

else ifeq ($(TARGET), rv64)
BITWIDTH := 64
BOOT_MODE := BOOT_S_MODE
RELOC_KERNEL := yes

else ifeq ($(TARGET), rv32m)
BITWIDTH := 32
BOOT_MODE := BOOT_M_MODE
RELOC_KERNEL := no

else ifeq ($(TARGET), rv64m)
BITWIDTH := 64
BOOT_MODE := BOOT_M_MODE
RELOC_KERNEL := no

endif


ifeq ($(BOOT_MODE), BOOT_M_MODE)
TEXT_OFFSET := 0
DEFAULT_EMU := m-rdisk$(BITWIDTH)
else
TEXT_OFFSET := 0x200000
DEFAULT_EMU := sbi$(BITWIDTH)
endif

# relocation address should be on a 2MB boundry (32 bit) or 
# 4MB boundry (64 bit) to maximize the size of the kernel that 
# can be mapped with the early page tables
ifeq ($(RELOC_KERNEL), yes)
ifeq ($(BITWIDTH), 32)
PAGE_OFFSET := 0x80000000
else
PAGE_OFFSET := 0xFFFFFFC000000000
endif
else
PAGE_OFFSET := $(PHYS_OFFSET)
endif

endif #TARGET
