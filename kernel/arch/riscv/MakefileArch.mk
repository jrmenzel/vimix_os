#
# RISC V specific settings
#

LD_ARCH_STRING := elf$(BITWIDTH)lriscv
OBJ_COPY_OUTPUT := elf$(BITWIDTH)-littleriscv
OBJ_COPY_ARCH := riscv
GDB_ARCHITECTURE := riscv:rv$(BITWIDTH)

#####
# TOOLPREFIX, e.g. riscv64-unknown-elf-
# Set explicitly or try to infer the correct TOOLPREFIX
#TOOLPREFIX = 
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


# target instruction set and bit width
MARCH := rv$(BITWIDTH)ima
# optional: compressed instructions
ifeq ($(RV_ENABLE_EXT_C), yes)
MARCH := $(MARCH)c
endif

TARGET_GCC_VERSION_AT_LEAST_14 := $(shell expr `$(TOOLPREFIX)gcc$(GCCPOSTFIX) -dumpversion | cut -f1 -d.` \>= 14)

# mandatory: CSRs and fence instructions
MARCH := $(MARCH)_zicsr_zifencei

# optional: s-mode timer extension, needs gcc 14
ifeq "$(TARGET_GCC_VERSION_AT_LEAST_14)" "1"
ifeq ($(RV_ENABLE_EXT_SSTC), yes)
MARCH := $(MARCH)_sstc
endif

EXT_DEFINES += -D__RISCV_EXT_SSTC
endif

# calling convention and linker architecture:
ifeq ($(BITWIDTH), 32)
MABI := ilp32
else
MABI := lp64
endif

ARCH_LFLAGS := -melf$(BITWIDTH)lriscv
ARCH_CFLAGS := -march=$(MARCH) -mabi=$(MABI) $(EXT_DEFINES)
ARCH_CFLAGS += -mcmodel=medany -mno-relax

ARCH_KERNEL_CFLAGS += -DCONFIG_RISCV_$(BOOT_MODE)

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
