#
# Just builds the kernel.
#
include ../MakefileCommon.mk
K_BUILD_DIR=../$(BUILD_DIR)/kernel

#
# Entry code must be located at the beginning of the binary.
# This is set in the kernel linker script.
#
OBJS = $(ARCH)/asm/entry.o \
	$(ARCH)/asm/s_mode_trap_vector.o \
	$(ARCH)/asm/m_mode_trap_vector.o \
	$(ARCH)/asm/u_mode_trap_vector.o \
	$(ARCH)/asm/context_switch.o \
	$(ARCH)/mm/vm.o \
	$(ARCH)/start.o \
	$(ARCH)/clint.o \
	$(ARCH)/plic.o \
	$(ARCH)/context.o \
	$(ARCH)/trap.o \
	$(ARCH)/sbi.o \
	drivers/uart16550.o \
	drivers/virtio_disk.o \
	fs/fs.o \
	fs/xv6fs/log.o \
	init/main.o \
	ipc/pipe.o \
	kernel/console.o \
	kernel/cpu.o \
	kernel/kalloc.o \
	kernel/vm.o \
	kernel/proc.o \
	kernel/bio.o \
	kernel/file.o \
	kernel/exec.o \
	kernel/scheduler.o \
	syscalls/sys_file.o \
	syscalls/sys_ipc.o \
	syscalls/sys_system.o \
	syscalls/sys_process.o \
	syscalls/syscall.o \
	lib/string.o \
	lib/sleeplock.o \
	lib/spinlock.o \
	lib/printk.o

SUB_DIRS = \
  kernel \
  fs \
  fs/xv6fs \
  init \
  ipc \
  drivers \
  lib \
  syscalls \
  $(ARCH) \
  $(ARCH)/mm \
  $(ARCH)/asm

OBJECT_FILES=$(addprefix $(K_BUILD_DIR)/, $(OBJS))
SUB_DIRS_TO_MAKE=$(addprefix $(K_BUILD_DIR)/, $(SUB_DIRS))

#
# Include paths for the kernel
#
KERNEL_SPACE_INC = ./arch/riscv # selected architecture
KERNEL_SPACE_INC += ./include   # public kernel API
KERNEL_SPACE_INC_PARAMS = $(foreach d, $(KERNEL_SPACE_INC), -I$d)

ifeq ($(SBI_SUPPORT), yes)
CFLAGS += -D__ENABLE_SBI__
endif


.PHONY: all directories clean
all: directories $(ARCH)/asm/initcode.h $(KERNEL_FILE) 

directories:
	mkdir -p $(SUB_DIRS_TO_MAKE)

# compile assembly
$(K_BUILD_DIR)/%.o: %.S
	$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@
	$(OBJDUMP) -S $@ > $(basename $@).s

# compile C
$(K_BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@
	$(OBJDUMP) -S $@ > $(basename $@).s

# special case: initcode
INIT_BUILD_DIR=$(K_BUILD_DIR)/$(ARCH)/asm
$(ARCH)/asm/initcode.h: $(ARCH)/asm/initcode.S directories
	$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $(ARCH)/asm/initcode.S -o $(INIT_BUILD_DIR)/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(INIT_BUILD_DIR)/initcode.out $(INIT_BUILD_DIR)/initcode.o
	$(OBJCOPY) -S -O binary $(INIT_BUILD_DIR)/initcode.out $(INIT_BUILD_DIR)/initcode
	xxd -i $(INIT_BUILD_DIR)/initcode > $(ARCH)/asm/initcode.h



ifeq ($(SBI_SUPPORT), yes)
LD_DEFINES=--defsym=openbsi=1
endif

# the kernel itself
KERNEL_PATH=../$(KERNEL_FILE)
$(KERNEL_FILE): $(ARCH)/asm/initcode.h $(OBJECT_FILES) $(ARCH)/kernel.ld
	$(LD) $(LDFLAGS) $(LD_DEFINES) -T $(ARCH)/kernel.ld -o $(KERNEL_PATH) $(OBJECT_FILES) 
	$(OBJDUMP) -S $(KERNEL_PATH) > $(K_BUILD_DIR)/$(KERNEL_NAME).asm

clean:
	-@rm $(KERNEL_PATH)
	-@rm -rf $(K_BUILD_DIR)
	-@rm $(ARCH)/asm/initcode.h

