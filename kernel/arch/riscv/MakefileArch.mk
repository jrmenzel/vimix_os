#
# RISC V specific settings
#

LD_ARCH_STRING := elf$(BITWIDTH)lriscv
OBJ_COPY_OUTPUT := elf$(BITWIDTH)-littleriscv
OBJ_COPY_ARCH := riscv
GDB_ARCHITECTURE := riscv:rv$(BITWIDTH)
TARGET_TRIPLE := riscv$(BITWIDTH)-unknown-elf

#####
# TOOLPREFIX, e.g. riscv64-unknown-elf-
# Set explicitly or try to infer the correct TOOLPREFIX
#TOOLPREFIX = 
ifeq ($(COMPILER), gcc)
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	elif riscv64-elf-objdump -i 2>&1 | grep 'elf$(BITWIDTH)-big' >/dev/null 2>&1; \
	then echo 'riscv64-elf-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a $(BITWIDTH) bit version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, set TOOLPREFIX in MakefileArch.mk." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif
endif


# calling convention and linker architecture:
ifeq ($(BITWIDTH), 32)
MABI := ilp32
else
MABI := lp64
endif

# target instruction set and bit width
MARCH := rv$(BITWIDTH)ima
# optional: compressed instructions
ifeq ($(RV_ENABLE_EXT_C), yes)
MARCH := $(MARCH)c
endif

# mandatory: CSRs and fence instructions
MARCH := $(MARCH)_zicsr_zifencei

ifeq ($(COMPILER), clang)
RISCV_TARGET_CC_FOR_FEATURE_TEST ?= clang --target=$(TARGET_TRIPLE)
else
RISCV_TARGET_CC_FOR_FEATURE_TEST ?= $(TOOLPREFIX)gcc
endif
RISCV_CC_HAS_EXT_SSTC := $(shell tmp=/tmp/vimixos_sstc_test_$$$$.o; \
	if printf '' | $(RISCV_TARGET_CC_FOR_FEATURE_TEST) -x c -c -o $$tmp \
		-march=$(MARCH)_sstc -mabi=$(MABI) - >/dev/null 2>&1; \
	then echo 1; else echo 0; fi; rm -f $$tmp)

# optional: s-mode timer extension
ifeq ($(RV_ENABLE_EXT_SSTC), yes)
ifeq ($(RISCV_CC_HAS_EXT_SSTC), 1)
MARCH := $(MARCH)_sstc
EXT_DEFINES += -D__RISCV_EXT_SSTC
endif
endif

ARCH_LFLAGS := -melf$(BITWIDTH)lriscv
ARCH_CFLAGS := -march=$(MARCH) -mabi=$(MABI) $(EXT_DEFINES)
ARCH_CFLAGS += -mcmodel=medany -mno-relax

ARCH_KERNEL_CFLAGS += -DCONFIG_RISCV_$(BOOT_MODE)

# clang can generate jump tables in libfdt functions called before relocation.
# Those tables contain post-relocation addresses, so disable them for the kernel.
ifeq ($(COMPILER), clang)
ARCH_KERNEL_CFLAGS += -fno-jump-tables
endif

#
# Arch specific files
#
OBJS_ARCH := arch/riscv/asm/head.o \
	arch/riscv/asm/s_mode_trap_vector.o \
	arch/riscv/asm/u_mode_trap_vector.o \
	arch/riscv/asm/context_switch.o \
	arch/riscv/asm/shared_asm.o \
	arch/riscv/drivers/plic.o \
	arch/riscv/scause.o \
	arch/riscv/sbi.o \
	drivers/jh7110_temp.o \
	drivers/jh7110_syscrg.o

ifeq ($(BOOT_MODE), BOOT_M_MODE)
OBJS_ARCH += arch/riscv/asm/m_mode_trap_vector.o
OBJS_ARCH += arch/riscv/asm/m_mode.o
OBJS_ARCH += arch/riscv/m_mode.o
endif
