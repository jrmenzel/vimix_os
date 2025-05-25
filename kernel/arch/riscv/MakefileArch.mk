#
# RISC V specific settings
#

# qemu machine virt:
#PLATFORM := qemu32
PLATFORM := qemu64

# spike simulator:
#PLATFORM := spike32
#PLATFORM := spike64

# hardware:
#PLATFORM := visionfive2

# RAM in MB
# Note:
# - Used as a fallback if no device tree file is provided by the boot loader
#   - Also serves as a maximum if run on HW if set -> faster usertests / memory init
# - Used by the usertests app as long as there is no API to query the available memory
# - Of course used to set up the emulators with the same amount of memory
MEMORY_SIZE := 64

# compile with compressed instructions:
RV_ENABLE_EXT_C := yes

# compile with sstc timer, only used if the support is detected at runtime
# so only set to "no" if SBI timers should be enforced for testing
RV_ENABLE_EXT_SSTC := yes

# for emulation only
CPUS := 4

ifeq ($(PLATFORM), qemu64)
BITWIDTH := 64
BOOT_MODE := BOOT_S_MODE
VIRTIO_DISK := yes
#RAMDISK_EMBEDDED := yes
#RAMDISK_BOOTLOADER := yes
QEMU_MACHINE := virt
else ifeq ($(PLATFORM), qemu32)
# test config
BITWIDTH := 32
BOOT_MODE := BOOT_M_MODE
RAMDISK_BOOTLOADER := yes
QEMU_MACHINE := virt
else ifeq ($(PLATFORM), spike32)
BITWIDTH := 32
BOOT_MODE := BOOT_M_MODE
#RAMDISK_EMBEDDED := yes
RAMDISK_BOOTLOADER := yes
else ifeq ($(PLATFORM), spike64)
BITWIDTH := 64
BOOT_MODE := BOOT_M_MODE
RAMDISK_BOOTLOADER := yes
else ifeq ($(PLATFORM), visionfive2)
DTB_FILE := boot/dtb/jh7110-starfive-visionfive-2-v1.3b.dtb
#DTB_EMBEDDED := yes
BITWIDTH := 64
BOOT_MODE := BOOT_S_MODE
RAMDISK_BOOTLOADER := yes
else
$(error PLATFORM not set)
endif

ifeq ($(PLATFORM), visionfive2)
KERNBASE := 0x40200000
else
# qemu & spike:
ifeq ($(BOOT_MODE), BOOT_S_MODE)
KERNBASE := 0x80200000
else
KERNBASE := 0x80000000
endif
endif

#####
# TOOLPREFIX, e.g. riscv64-unknown-elf-
# Set explicitly or try to infer the correct TOOLPREFIX
# first look for matching bit width in toolset, then try the 64 bit versions
#TOOLPREFIX = 
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv$(BITWIDTH)-unknown-elf-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv$(BITWIDTH)-unknown-elf-'; \
	elif riscv$(BITWIDTH)-linux-gnu-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv$(BITWIDTH)-linux-gnu-'; \
	elif riscv$(BITWIDTH)-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv$(BITWIDTH)-unknown-linux-gnu-'; \
	elif riscv$(BITWIDTH)-elf-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv$(BITWIDTH)-elf-'; \
	elif riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-elf-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv64-elf-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv$(BITWIDTH) version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, set TOOLPREFIX in MakefileArch.mk." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# for .gdbinit:
GDB_ARCHITECTURE := riscv:rv$(BITWIDTH)

# target instruction set and bit width
MARCH := rv$(BITWIDTH)ima
# optional: compressed instructions
ifeq ($(RV_ENABLE_EXT_C), yes)
MARCH := $(MARCH)c
endif

# prior to gcc 12 the extensions zicsr and zifencei were implicit in the base ISA
TARGET_GCC_VERSION_AT_LEAST_12 := $(shell expr `$(TOOLPREFIX)gcc$(GCCPOSTFIX) -dumpversion | cut -f1 -d.` \>= 12)
TARGET_GCC_VERSION_AT_LEAST_13 := $(shell expr `$(TOOLPREFIX)gcc$(GCCPOSTFIX) -dumpversion | cut -f1 -d.` \>= 13)
TARGET_GCC_VERSION_AT_LEAST_14 := $(shell expr `$(TOOLPREFIX)gcc$(GCCPOSTFIX) -dumpversion | cut -f1 -d.` \>= 14)
ifeq "$(TARGET_GCC_VERSION_AT_LEAST_12)" "1"
# mandatory: CSRs and fence instructions
MARCH := $(MARCH)_zicsr_zifencei
endif

ifeq "$(TARGET_GCC_VERSION_AT_LEAST_14)" "1"
# optional: s-mode timer extension, needs gcc 14
ifeq ($(RV_ENABLE_EXT_SSTC), yes)
MARCH := $(MARCH)_sstc
endif
endif

ifeq ($(RV_ENABLE_EXT_SSTC), yes)
EXT_DEFINES += -D__RISCV_EXT_SSTC
endif

# calling convention
ifeq ($(BITWIDTH), 32)
MABI := ilp32
else
MABI := lp64
endif

ARCH_LFLAGS := -melf$(BITWIDTH)lriscv
ARCH_CFLAGS := -march=$(MARCH) -mabi=$(MABI) $(EXT_DEFINES)
ARCH_CFLAGS += -mcmodel=medany -mno-relax

ARCH_KERNEL_CFLAGS += -DCONFIG_RISCV_$(BOOT_MODE)

ifeq ($(PLATFORM), visionfive2)
ARCH_KERNEL_CFLAGS += -D__PLATFORM_VISIONFIVE2 -D__LIMIT_MEMORY
endif
ifeq ($(PLATFORM), spike32)
ARCH_KERNEL_CFLAGS += -D__PLATFORM_SPIKE
endif
ifeq ($(PLATFORM), spike64)
ARCH_KERNEL_CFLAGS += -D__PLATFORM_SPIKE
endif

#
# Arch specific files
#
OBJS_ARCH := arch/riscv/asm/entry.o \
	arch/riscv/asm/s_mode_trap_vector.o \
	arch/riscv/asm/u_mode_trap_vector.o \
	arch/riscv/asm/context_switch.o \
	arch/riscv/plic.o \
	arch/riscv/timer.o \
	arch/riscv/scause.o \
	arch/riscv/sbi.o \
	drivers/jh7110_temp.o \
	drivers/jh7110_syscrg.o

ifeq ($(BOOT_MODE), BOOT_M_MODE)
OBJS_ARCH += arch/riscv/asm/m_mode_trap_vector.o
OBJS_ARCH += arch/riscv/asm/m_mode.o
OBJS_ARCH += arch/riscv/m_mode.o
endif


#
# Qemu
#
QEMU := qemu-system-riscv$(BITWIDTH)

ifeq ($(BOOT_MODE), BOOT_M_MODE)
QEMU_BIOS := none
else
QEMU_BIOS := default
endif

QEMU_OPTS_ARCH := -machine $(QEMU_MACHINE) -bios $(QEMU_BIOS)
QEMU_OPTS_ARCH += -global virtio-mmio.force-legacy=false

