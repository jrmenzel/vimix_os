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
	$(ARCH_DIR)/start.o \
	$(ARCH_DIR)/trap.o \
	drivers/device.o \
	drivers/console.o \
	drivers/uart16550.o \
	drivers/dev_null.o \
	drivers/dev_random.o \
	drivers/dev_zero.o \
	drivers/devices_list.o \
	drivers/htif.o \
	drivers/ramdisk.o \
	drivers/rtc.o \
	drivers/syscon.o \
	drivers/virtio_disk.o \
	fs/fs.o \
	fs/mount.o \
	fs/vfs.o \
	fs/devfs/devfs.o \
	fs/xv6fs/log.o \
	fs/xv6fs/xv6fs.o \
	init/dtb.o \
	init/main.o \
	init/start.o \
	ipc/pipe.o \
	kernel/cpu.o \
	kernel/kalloc.o \
	kernel/vm.o \
	kernel/proc.o \
	kernel/bio.o \
	kernel/file.o \
	kernel/exec.o \
	kernel/reset.o \
	kernel/scheduler.o \
	kernel/slab.o \
	kernel/trap.o \
	kernel/kticks.o \
	syscalls/sys_device.o \
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

ifeq ($(DTB_EMBEDDED), yes)
OBJS += dtb_embedded.o
DTB_C := $(K_BUILD_DIR)/dtb_embedded.c
endif

# add libfdt for Device Tree parsing
OBJS += $(addprefix lib/libfdt/, $(LIBFDT_OBJS))

SUB_DIRS := \
  kernel \
  fs \
  fs/devfs \
  fs/xv6fs \
  init \
  ipc \
  drivers \
  lib \
  lib/libfdt \
  syscalls \
  $(ARCH_DIR) \
  $(ARCH_DIR)/arch \
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
ifeq ($(RAMDISK_EMBEDDED), yes)
KERNEL_SPACE_INC += $(K_BUILD_DIR)
endif
ifeq ($(DTB_EMBEDDED), yes)
KERNEL_SPACE_INC += $(K_BUILD_DIR)
endif
KERNEL_SPACE_INC_PARAMS = $(foreach d, $(KERNEL_SPACE_INC), -I$d)

KBUILD_CFLAGS := $(CFLAGS)
KBUILD_CFLAGS += $(EXTERNAL_KERNEL_FLAGS) # can be set by external scripts
KBUILD_CFLAGS += $(ARCH_KERNEL_CFLAGS)
KBUILD_CFLAGS += -DMEMORY_SIZE=$(MEMORY_SIZE)
KBUILD_CFLAGS += -DUSER_TEXT_START=$(USER_TEXT_START)
KBUILD_CFLAGS += -D__ARCH_$(ARCH)
#KBUILD_CFLAGS += -fpic -fpie

ifeq ($(RAMDISK_EMBEDDED), yes)
KBUILD_CFLAGS += -D__CONFIG_RAMDISK_EMBEDDED
endif
ifeq ($(DTB_EMBEDDED), yes)
KBUILD_CFLAGS += -D__CONFIG_DTB_EMBEDDED
endif

# assembler flags
KBUILD_AFLAGS := -D__ASSEMBLY__ $(KBUILD_CFLAGS)

.PHONY: all directories clean
all: directories $(KERNEL_PATH) # build all

directories: # create temp build directories
	@mkdir -p $(SUB_DIRS_TO_MAKE)

# compile assembly
$(K_BUILD_DIR)/%.o: %.S
	@$(CC) $(KBUILD_AFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

# compile C, also create assembly listing
$(K_BUILD_DIR)/%.o: %.c
	@printf "$(TASK_COLOR)Compile: $(@)\n$(NO_COLOR)"
	@$(CC) $(KBUILD_CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@
ifeq ($(CREATE_ASSEMBLY), yes)
	@printf "$(TASK_COLOR)Assembly $(basename $@).s\n$(NO_COLOR)"
	@$(CC) $(KBUILD_CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -S $< -g -o $(basename $@).s
endif


# embedd the filesystem image into the kernel as an array:
ifeq ($(RAMDISK_EMBEDDED), yes)
$(K_BUILD_DIR)/ramdisk_fs.h:
	@printf "$(TASK_COLOR)Creating ramdisk_fs.h\n$(NO_COLOR)"
	@echo "#pragma once" > $(K_BUILD_DIR)/ramdisk_fs.h
	@echo "#include <kernel/kernel.h>" >> $(K_BUILD_DIR)/ramdisk_fs.h
	@echo "extern unsigned char ramdisk_fs[];" >> $(K_BUILD_DIR)/ramdisk_fs.h
	@echo -n "#define ramdisk_fs_size " >> $(K_BUILD_DIR)/ramdisk_fs.h
	@stat -c %s ../$(BUILD_DIR)/filesystem.img >> $(K_BUILD_DIR)/ramdisk_fs.h

$(K_BUILD_DIR)/ramdisk_fs.c:
	@printf "$(TASK_COLOR)Creating ramdisk_fs.c\n$(NO_COLOR)"
	@echo "#include \"ramdisk_fs.h\"" > $(K_BUILD_DIR)/ramdisk_fs.c
	@echo "unsigned char ramdisk_fs[] = {" >> $(K_BUILD_DIR)/ramdisk_fs.c
	@xxd -i < ../$(BUILD_DIR)/filesystem.img >> $(K_BUILD_DIR)/ramdisk_fs.c
	@echo "};" >> $(K_BUILD_DIR)/ramdisk_fs.c

$(K_BUILD_DIR)/ramdisk_fs.o: $(K_BUILD_DIR)/ramdisk_fs.c
	@printf "$(TASK_COLOR)Compile ramdisk_fs\n$(NO_COLOR)"
	@$(CC) $(KBUILD_CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $(K_BUILD_DIR)/ramdisk_fs.c -o $(K_BUILD_DIR)/ramdisk_fs.o

else
$(K_BUILD_DIR)/ramdisk_fs.h:
	@touch $(K_BUILD_DIR)/ramdisk_fs.h
endif

# embedd the device tree binary into the kernel as an array:
ifeq ($(DTB_EMBEDDED), yes)
$(K_BUILD_DIR)/dtb_embedded.h:
	@printf "$(TASK_COLOR)Creating dtb_embedded.h\n$(NO_COLOR)"
	@echo "#pragma once" > $(K_BUILD_DIR)/dtb_embedded.h
	@echo "#include <kernel/kernel.h>" >> $(K_BUILD_DIR)/dtb_embedded.h
	@echo "extern unsigned char dtb_embedded[];" >> $(K_BUILD_DIR)/dtb_embedded.h
	@echo -n "#define dtb_embedded_size " >> $(K_BUILD_DIR)/dtb_embedded.h
	@stat -c %s ../$(DTB_FILE) >> $(K_BUILD_DIR)/dtb_embedded.h

$(K_BUILD_DIR)/dtb_embedded.c:
	@printf "$(TASK_COLOR)Creating dtb_embedded.c\n$(NO_COLOR)"
	@echo "#include \"dtb_embedded.h\"" > $(K_BUILD_DIR)/dtb_embedded.c
	@echo "unsigned char dtb_embedded[] = {" >> $(K_BUILD_DIR)/dtb_embedded.c
	@xxd -i < ../$(DTB_FILE) >> $(K_BUILD_DIR)/dtb_embedded.c
	@echo "};" >> $(K_BUILD_DIR)/dtb_embedded.c

$(K_BUILD_DIR)/dtb_embedded.o: $(K_BUILD_DIR)/dtb_embedded.c
	@printf "$(TASK_COLOR)Compile dtb_embedded\n$(NO_COLOR)"
	@$(CC) $(KBUILD_CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $(K_BUILD_DIR)/dtb_embedded.c -o $(K_BUILD_DIR)/dtb_embedded.o

else
$(K_BUILD_DIR)/dtb_embedded.h:
	@touch $(K_BUILD_DIR)/dtb_embedded.h
endif

$(KERNEL_PATH): $(K_BUILD_DIR)/ramdisk_fs.h $(K_BUILD_DIR)/dtb_embedded.h $(OBJECT_FILES) $(ARCH_DIR)/kernel.ld # build kernel
	@printf "$(TASK_COLOR)Link kernel: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) --defsym=KERNBASE=$(KERNBASE) $(ARCH_LD_FLAGS) -T $(ARCH_DIR)/kernel.ld -o $(KERNEL_PATH) $(OBJECT_FILES)
ifeq ($(CREATE_ASSEMBLY), yes)
	@printf "$(TASK_COLOR)Disassemble kernel: $(K_BUILD_DIR)/$(KERNEL_NAME).asm\n$(NO_COLOR)"
	@$(OBJDUMP) -S $(KERNEL_PATH) > $(K_BUILD_DIR)/$(KERNEL_NAME).asm
endif
	@$(OBJCOPY) -O binary $(KERNEL_PATH) $(KERNEL_PATH).bin

clean:
	-@rm -f $(KERNEL_PATH)
	-@rm -f $(KERNEL_PATH).bin
	-@rm -rf $(K_BUILD_DIR)
	-@rm -f $(K_BUILD_DIR)/ramdisk_fs.h
	-@rm -f $(RAMDISC_C)
	-@rm -f $(DTB_C)
	-@rm -f ../$(BUILD_DIR)/*.img

