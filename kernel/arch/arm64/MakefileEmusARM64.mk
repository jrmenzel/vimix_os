# SPDX-License-Identifier: MIT 

EMUS_ARM64 := arm64 raspi4 kvm

# include guard as this file is included unconditionally to figure out the ARCH in the first place.
ifneq ($(filter $(EMU),$(EMUS_ARM64)),)

ifeq ($(EMU), arm64)
QEMU_MACHINE       := virt,gic-version=2
CPUS               := 2
MEMORY_SIZE        := 64
QEMU_OPTS_ARCH     := -cpu cortex-a72 -machine $(QEMU_MACHINE) -serial mon:stdio 
RAMDISK_BOOTLOADER := yes
GDB_PHYS_OFFSET    := 0x40000000

else ifeq ($(EMU), kvm)
QEMU_MACHINE       := virt,gic-version=2
CPUS               := 2
MEMORY_SIZE        := 64
QEMU_OPTS_ARCH     := -enable-kvm -cpu host -machine $(QEMU_MACHINE) -no-reboot -serial mon:stdio 
RAMDISK_BOOTLOADER := yes
GDB_PHYS_OFFSET    := 0x40000000

else ifeq ($(EMU), raspi4)
QEMU_MACHINE       := raspi4b
CPUS               := 4
MEMORY_SIZE        := 2048
QEMU_OPTS_ARCH     := -cpu cortex-a72 -machine $(QEMU_MACHINE) -no-reboot -serial null -serial mon:stdio 

# Raspberry Pi 4 emulation on qemu requires us to provide the DTB file.
# That must have the correct location of the ramdisk.
# Load filesystem 16 MB into RAM, DTB must tell the same location!
RAMDISK_LOAD_ADDR := 0x01000000
DTB_FILE          := ./boot/dtb/bcm2711-rpi-4-b-2gb.dtb
QEMU_OPTS_ARCH += -dtb $(DTB_FILE)

endif

#
# Qemu
#
QEMU := qemu-system-aarch64

endif
