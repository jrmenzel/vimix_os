# SPDX-License-Identifier: MIT 

EMUS_ARM64 := arm64 arm64-rdisk kvm kvm-rdisk raspi4

# include guard as this file is included unconditionally to figure out the ARCH in the first place.
ifneq ($(filter $(EMU),$(EMUS_ARM64)),)

ifneq ($(findstring arm64,$(EMU)),)
QEMU_MACHINE       := virt,gic-version=2
CPUS               := 4
MEMORY_SIZE        := 64
QEMU_OPTS_ARCH     := -cpu cortex-a72 -machine $(QEMU_MACHINE) 
GDB_PHYS_OFFSET    := 0x40000000

else ifneq ($(findstring kvm,$(EMU)),)
QEMU_MACHINE       := virt,gic-version=2
CPUS               := 2
MEMORY_SIZE        := 64
QEMU_OPTS_ARCH     := -enable-kvm -cpu host -machine $(QEMU_MACHINE) -no-reboot 
GDB_PHYS_OFFSET    := 0x40000000

else ifneq ($(findstring raspi4,$(EMU)),)
QEMU_MACHINE       := raspi4b
CPUS               := 4
MEMORY_SIZE        := 2048
QEMU_OPTS_ARCH     := -cpu cortex-a72 -machine $(QEMU_MACHINE) -no-reboot -serial null 
RAMDISK_BOOTLOADER := yes

# Raspberry Pi 4 emulation on qemu requires us to provide the DTB file.
# That must have the correct location of the ramdisk.
# Load filesystem 16 MB into RAM, DTB must tell the same location!
RAMDISK_LOAD_ADDR := 0x01000000
QEMU_OPTS_ARCH    += -dtb ./boot/dtb/bcm2711-rpi-4-b-2gb.dtb

endif

# if EMU config contains "rdisk" (skip if raspi4)
ifeq ($(findstring raspi4,$(EMU)),)
ifneq ($(findstring rdisk,$(EMU)),)
RAMDISK_BOOTLOADER := yes
else
VIRTIO_DISK := yes
endif
endif # not for raspi4

#
# Qemu
#
QEMU := qemu-system-aarch64

endif
