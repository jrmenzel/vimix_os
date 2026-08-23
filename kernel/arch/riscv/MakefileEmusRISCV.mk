# SPDX-License-Identifier: MIT 

EMUS_RISCV := sbi32 sbi64 sbi-rdisk32 sbi-rdisk64 m32 m64 m-rdisk32 m-rdisk64

# include guard as this file is included unconditionally to figure out the ARCH in the first place.
ifneq ($(filter $(EMU),$(EMUS_RISCV)),)

CPUS := 4
MEMORY_SIZE := 64
QEMU_MACHINE := virt

# if EMU config contains "sbi"
ifneq ($(findstring sbi,$(EMU)),)
# run with OpenSBI, boot in s mode
QEMU_BIOS := default
else
# boot in m mode
QEMU_BIOS := none
endif

# if EMU config contains "rdisk"
ifneq ($(findstring rdisk,$(EMU)),)
RAMDISK_BOOTLOADER := yes
else
VIRTIO_DISK := yes
endif

# if EMU config end in "32"
ifneq ($(filter %32,$(EMU)),)
EMU_BITWIDTH := 32
else
EMU_BITWIDTH := 64
endif

#
# Qemu
#
QEMU := qemu-system-riscv$(EMU_BITWIDTH)

QEMU_OPTS_ARCH := -machine $(QEMU_MACHINE) -bios $(QEMU_BIOS)
QEMU_OPTS_ARCH += -global virtio-mmio.force-legacy=false

#
# Spike simulator
# 

# spike binary, edit to use e.g. a self compiled version
ifneq ("$(wildcard spike/riscv-isa-sim/build/spike)","")
SPIKE_BUILD := spike/riscv-isa-sim/build/
endif
SPIKE := $(SPIKE_BUILD)spike

ifeq ($(EMU_BITWIDTH), 32)
SPIKE_ISA := rv32gc
else
SPIKE_ISA := rv64gc
endif

MEMORY_SIZE_BYTES := $(shell echo $$(( $(MEMORY_SIZE) * 1024 * 1024 )))
SPIKE_OPTIONS := -m0x80000000:$(MEMORY_SIZE_BYTES) -p$(CPUS) --isa=$(SPIKE_ISA) --real-time-clint
SPIKE_OPTIONS += $(KERNEL_FILE)

endif # EMU
