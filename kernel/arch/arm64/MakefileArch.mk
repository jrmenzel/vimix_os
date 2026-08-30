#
# ARM 64-bit specific settings
#

LD_ARCH_STRING := aarch64elf
OBJ_COPY_OUTPUT := elf64-littleaarch64
OBJ_COPY_ARCH := aarch64
GDB_ARCHITECTURE := aarch64
TARGET_TRIPLE := aarch64-unknown-elf

#####
# TOOLPREFIX, e.g. aarch64-elf-
# Set explicitly or try to infer the correct TOOLPREFIX
#TOOLPREFIX = aarch64-none-elf-
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if aarch64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'aarch64-unknown-elf-'; \
	elif aarch64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'aarch64-linux-gnu-'; \
	elif aarch64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'aarch64-unknown-linux-gnu-'; \
	elif aarch64-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'aarch64-elf-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a aarch64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, set TOOLPREFIX in MakefileArch.mk." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif


ARCH_CFLAGS := -mcpu=cortex-a72+nofp -mtune=cortex-a72 
ARCH_CFLAGS += -mno-outline-atomics -mgeneral-regs-only -static -mstrict-align

OBJS_ARCH := arch/arm64/asm/head.o \
	arch/arm64/asm/context_switch.o \
	arch/arm64/asm/return_to_user.o \
	arch/arm64/asm/trap_vector.o \
	arch/arm64/arch_interrupts.o \
	arch/arm64/arch_system.o \
	arch/arm64/drivers/arm_psci.o \
	arch/arm64/drivers/gic_v2.o \
	drivers/arm_pl011.o \
	drivers/arm_pl031.o \
	drivers/bcm2711_pm.o \
	drivers/bcm2835_aux.o \
	drivers/bcm2835_aux_uart.o \
	drivers/bcm2835_firmware.o \
	drivers/bcm2835_gpio.o \
	drivers/bcm2835_mbox.o
