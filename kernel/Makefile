#
# Just builds the kernel.
#
include ../MakefileCommon.mk
include lib/libfdt/Makefile.libfdt
K_BUILD_DIR := ../$(BUILD_DIR)/kernel
KERNEL_PATH := ../$(KERNEL_FILE)
ARCH_DIR    := arch/$(ARCH)
INIT_BUILD_DIR := $(K_BUILD_DIR)/$(ARCH_DIR)/asm

#
# Entry code must be located at the beginning of the binary.
# This is set in the kernel linker script.
#
OBJS := $(OBJS_ARCH)
OBJS +=	$(ARCH_DIR)/context.o \
	$(ARCH_DIR)/mm/vm.o \
    drivers/device.o \
	drivers/console.o \
	drivers/uart16550.o \
	drivers/dev_null.o \
	drivers/dev_zero.o \
	drivers/devices_list.o \
	drivers/ramdisk.o \
	drivers/rtc.o \
	drivers/syscon.o \
	drivers/virtio_disk.o \
	fs/fs.o \
	fs/xv6fs/log.o \
	fs/xv6fs/xv6fs.o \
	init/dtb.o \
	init/main.o \
	ipc/pipe.o \
	kernel/cpu.o \
	kernel/kalloc.o \
	kernel/vm.o \
	kernel/proc.o \
	kernel/bio.o \
	kernel/file.o \
	kernel/exec.o \
	kernel/scheduler.o \
	kernel/kticks.o \
	syscalls/sys_file.o \
	syscalls/sys_ipc.o \
	syscalls/sys_system.o \
	syscalls/sys_process.o \
	syscalls/syscall.o \
	lib/string.o \
	lib/sleeplock.o \
	lib/spinlock.o \
	lib/print_impl.o \
	lib/printk.o

ifeq ($(BITWIDTH), 32)
OBJS += lib/div64.o
endif

ifeq ($(RAMDISK_EMBEDDED), yes)
OBJS += ramdisk_fs.o
RAMDISC_C := $(K_BUILD_DIR)/ramdisk_fs.c
endif

# add libfdt for Device Tree parsing
OBJS += $(addprefix lib/libfdt/, $(LIBFDT_OBJS))

SUB_DIRS := \
  kernel \
  fs \
  fs/xv6fs \
  init \
  ipc \
  drivers \
  lib \
  lib/libfdt \
  syscalls \
  $(ARCH_DIR) \
  $(ARCH_DIR)/mm \
  $(ARCH_DIR)/asm

OBJECT_FILES := $(addprefix $(K_BUILD_DIR)/, $(OBJS))
SUB_DIRS_TO_MAKE := $(addprefix $(K_BUILD_DIR)/, $(SUB_DIRS))

#
# Include paths for the kernel
#
KERNEL_SPACE_INC := $(ARCH_DIR) # selected architecture
KERNEL_SPACE_INC += ./include   # public kernel API
KERNEL_SPACE_INC += ./lib/libfdt   # libfdt
KERNEL_SPACE_INC += $(K_BUILD_DIR)/$(ARCH_DIR) # for initcode.h
KERNEL_SPACE_INC_PARAMS = $(foreach d, $(KERNEL_SPACE_INC), -I$d)

CFLAGS += $(ARCH_KERNEL_CFLAGS)
CFLAGS += -DMEMORY_SIZE=$(MEMORY_SIZE)
CFLAGS += -DUSER_TEXT_START=$(USER_TEXT_START)
CFLAGS += -D__ARCH_$(ARCH)

ifeq ($(RAMDISK_EMBEDDED), yes)
CFLAGS += -DRAMDISK_EMBEDDED
endif

.PHONY: all directories clean
all: directories $(INIT_BUILD_DIR)/initcode.h $(KERNEL_PATH) # build all

directories: # create temp build directories
	@mkdir -p $(SUB_DIRS_TO_MAKE)

# compile assembly
$(K_BUILD_DIR)/%.o: %.S
	@$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

# compile C, also create assembly listing
$(K_BUILD_DIR)/%.o: %.c $(INIT_BUILD_DIR)/initcode.h
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@
ifeq ($(CREATE_ASSEMBLY), yes)
	@printf "$(TASK_COLOR)Assembly $(basename $@).s\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -S $< -g -o $(basename $@).s
endif

# special case: initcode
# ld flag --no-warn-rwx-segments requres newer linker versions (2.39 or later)
$(INIT_BUILD_DIR)/initcode.h: $(ARCH_DIR)/asm/initcode.S 
	@printf "$(TASK_COLOR)Compile initcode and dump it into initcode.h\n$(NO_COLOR)"
	@$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $(ARCH_DIR)/asm/initcode.S -o $(INIT_BUILD_DIR)/initcode.o
	@$(LD) $(LDFLAGS) -N -e start -Ttext $(USER_TEXT_START) -o $(INIT_BUILD_DIR)/initcode.out $(INIT_BUILD_DIR)/initcode.o
	@$(OBJCOPY) -S -O binary $(INIT_BUILD_DIR)/initcode.out $(INIT_BUILD_DIR)/initcode
	@echo "unsigned char g_initcode[] = {" > $(INIT_BUILD_DIR)/initcode.h
	@xxd -i < $(INIT_BUILD_DIR)/initcode >> $(INIT_BUILD_DIR)/initcode.h
	@echo "};" >> $(INIT_BUILD_DIR)/initcode.h


# embedd the filesystem image into the kernel as an array:
ifeq ($(RAMDISK_EMBEDDED), yes)
$(K_BUILD_DIR)/ramdisk_fs.h:
	echo "#pragma once" > $(K_BUILD_DIR)/ramdisk_fs.h
	echo "#include <kernel/kernel.h>" >> $(K_BUILD_DIR)/ramdisk_fs.h
	echo "extern unsigned char ramdisk_fs[];" >> $(K_BUILD_DIR)/ramdisk_fs.h
	echo -n "#define ramdisk_fs_size " >> $(K_BUILD_DIR)/ramdisk_fs.h
	stat -c %s ../$(BUILD_DIR)/filesystem.img >> $(K_BUILD_DIR)/ramdisk_fs.h
	#echo "extern const size_t ramdisk_fs_size;" >> $(K_BUILD_DIR)/ramdisk_fs.h

$(K_BUILD_DIR)/ramdisk_fs.c:
	echo "#include \"ramdisk_fs.h\"" > $(K_BUILD_DIR)/ramdisk_fs.c
	echo "unsigned char ramdisk_fs[] = {" >> $(K_BUILD_DIR)/ramdisk_fs.c
	xxd -i < ../$(BUILD_DIR)/filesystem.img >> $(K_BUILD_DIR)/ramdisk_fs.c
	echo "};" >> $(K_BUILD_DIR)/ramdisk_fs.c
#	echo "size_t const ramdisk_fs_size = sizeof(ramdisk_fs);" >> $(K_BUILD_DIR)/ramdisk_fs.c

$(K_BUILD_DIR)/ramdisk_fs.o: $(K_BUILD_DIR)/ramdisk_fs.c
	$(CC) $(CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $(K_BUILD_DIR)/ramdisk_fs.c -o $(K_BUILD_DIR)/ramdisk_fs.o

else
$(K_BUILD_DIR)/ramdisk_fs.h:
	touch $(K_BUILD_DIR)/ramdisk_fs.h
endif


$(KERNEL_PATH): $(INIT_BUILD_DIR)/initcode.h $(K_BUILD_DIR)/ramdisk_fs.h $(OBJECT_FILES) $(ARCH_DIR)/kernel.ld # build kernel
	@printf "$(TASK_COLOR)Link kernel: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) $(ARCH_LD_FLAGS) -T $(ARCH_DIR)/kernel.ld -o $(KERNEL_PATH) $(OBJECT_FILES)
ifeq ($(CREATE_ASSEMBLY), yes)
	@printf "$(TASK_COLOR)Disassemble kernel: $(K_BUILD_DIR)/$(KERNEL_NAME).asm\n$(NO_COLOR)"
	@$(OBJDUMP) -S $(KERNEL_PATH) > $(K_BUILD_DIR)/$(KERNEL_NAME).asm
endif

clean:
	-@rm $(KERNEL_PATH)
	-@rm -rf $(K_BUILD_DIR)
	-@rm $(ARCH_DIR)/asm/initcode.h
	-@rm $(K_BUILD_DIR)/ramdisk_fs.h
	-@rm $(RAMDISC_C)

