#
# Just builds the kernel.
#
include ../MakefileCommon.mk
include lib/libfdt/Makefile.libfdt
K_BUILD_DIR := ../$(BUILD_DIR)/kernel_$(TARGET)$(BUILD_TYPE_SHORT)
KERNEL_PATH := $(K_BUILD_DIR)/$(KERNEL_NAME)
ARCH_DIR    := arch/$(ARCH)
INIT_BUILD_DIR := $(K_BUILD_DIR)/$(ARCH_DIR)/asm

#
# Entry code must be located at the beginning of the binary.
# This is set in the kernel linker script.
#
OBJS := $(OBJS_ARCH)
OBJS +=	$(ARCH_DIR)/context.o \
	$(ARCH_DIR)/mm/arch_vm.o \
	$(ARCH_DIR)/arch_start.o \
	$(ARCH_DIR)/arch_timer.o \
	$(ARCH_DIR)/arch_trap.o \
	drivers/device.o \
	drivers/console.o \
	drivers/uart16550.o \
	drivers/dev_null.o \
	drivers/dev_random.o \
	drivers/dev_zero.o \
	drivers/devices_list.o \
	drivers/driver_list.o \
	drivers/htif.o \
	drivers/ramdisk.o \
	drivers/goldfish_rtc.o \
	drivers/syscon.o \
	drivers/virtio_disk.o \
	fs/dentry.o \
	fs/dentry_cache.o \
	fs/dentry_cache_sysfs.o \
	fs/fs.o \
	fs/fs_lookup.o \
	fs/mount.o \
	fs/sysfs/sys_kernel.o \
	fs/sysfs/sysfs_helper.o \
	fs/sysfs/sysfs.o \
	fs/sysfs/sysfs_node.o \
	fs/vfs.o \
	fs/devfs/devfs.o \
	fs/vimixfs/bmap.o \
	fs/vimixfs/log.o \
	fs/vimixfs/vimixfs.o \
	fs/vimixfs/vimixfs_sysfs.o \
	init/dtb.o \
	init/early_pgtable.o \
	init/embedded_dtbs.o \
	init/main.o \
	init/start.o \
	init/system.o \
	ipc/pipe.o \
	kernel/cpu.o \
	kernel/interrupt_controller.o \
	kernel/ipi.o \
	kernel/kobject.o \
	kernel/proc.o \
	kernel/bio.o \
	kernel/bio_sysfs.o \
	kernel/buf.o \
	kernel/file.o \
	kernel/exec.o \
	kernel/permission.o \
	kernel/process.o \
	kernel/reset.o \
	kernel/rtc.o \
	kernel/scheduler.o \
	kernel/timer.o \
	kernel/trap.o \
	kernel/kticks.o \
	mm/cache.o \
	mm/kalloc.o \
	mm/kmem_sysfs.o \
	mm/memory_map.o \
	mm/slab.o \
	mm/page_table.o \
	mm/vm.o \
	syscalls/sys_cred.o \
	syscalls/sys_device.o \
	syscalls/sys_file.o \
	syscalls/sys_file_meta.o \
	syscalls/sys_filesystem.o \
	syscalls/sys_ipc.o \
	syscalls/sys_mk.o \
	syscalls/sys_link.o \
	syscalls/sys_system.o \
	syscalls/sys_process.o \
	syscalls/syscall.o \
	lib/bitmap.o \
	lib/cbuffer.o \
	lib/rwspinlock.o \
	lib/string.o \
	lib/sleeplock.o \
	lib/spinlock.o \
	lib/panic.o \
	lib/print_impl.o \
	lib/printk.o \
	lib/qsort_impl.o \
	lib/xdbg/xdbg.o

ifeq ($(BITWIDTH), 32)
OBJS += lib/div64.o
endif

ifeq ($(RAMDISK_EMBEDDED), yes)
OBJS += ramdisk_fs.o
RAMDISC_C := $(K_BUILD_DIR)/ramdisk_fs.c
endif
ifneq "$(TARGET_CC_HAS_C23_EMBED)" "1"
# can't use the C23 embed feature, use fallback:
OBJS += jh7110.o orangepi_rv2.o
ifdef DTB_EMBEDDED
OBJS += embedded_boot_dtb.o
endif

endif

# add libfdt for Device Tree parsing
OBJS += $(addprefix lib/libfdt/, $(LIBFDT_OBJS))

SUB_DIRS := \
  drivers \
  fs \
  fs/devfs \
  fs/sysfs \
  fs/vimixfs \
  init \
  ipc \
  kernel \
  lib \
  lib/libfdt \
  lib/xdbg \
  mm \
  syscalls \
  $(ARCH_DIR) \
  $(ARCH_DIR)/arch \
  $(ARCH_DIR)/drivers \
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
KERNEL_SPACE_INC_PARAMS = $(foreach d, $(KERNEL_SPACE_INC), -I$d)

KBUILD_CFLAGS := $(CFLAGS)
KBUILD_CFLAGS += $(EXTERNAL_KERNEL_FLAGS) # can be set by external scripts
KBUILD_CFLAGS += $(ARCH_KERNEL_CFLAGS)
KBUILD_CFLAGS += -DUSER_TEXT_START=$(USER_TEXT_START)
KBUILD_CFLAGS += -D__ARCH_$(ARCH)
KBUILD_CFLAGS += -DTEXT_OFFSET=$(TEXT_OFFSET) -DPAGE_OFFSET=$(PAGE_OFFSET)
KBUILD_CFLAGS += -fno-omit-frame-pointer

ifeq ($(RELOC_KERNEL), yes)
KBUILD_CFLAGS += -D__CONFIG_RELOC_KERNEL
endif

ifeq ($(RAMDISK_EMBEDDED), yes)
KBUILD_CFLAGS += -D__CONFIG_RAMDISK_EMBEDDED
endif
ifeq ($(DTB_EMBEDDED), yes)
KBUILD_CFLAGS += -D__CONFIG_DTB_FILE_PATH=\"$(DTB_FILE)\"
endif

# assembler flags
KBUILD_AFLAGS := -D__ASSEMBLY__ $(KBUILD_CFLAGS)

ifeq "$(TARGET_CC_HAS_C23_EMBED)" "1"
KBUILD_CFLAGS += -std=gnu23
KBUILD_CFLAGS += --embed-dir=".." # embed relative to project root
endif

.PHONY: all directories clean
all: directories $(KERNEL_PATH) # build all

directories: # create temp build directories
	@mkdir -p ../$(BUILD_DIR)/boot
	@mkdir -p $(SUB_DIRS_TO_MAKE)
	@mkdir -p $(K_BUILD_DIR)

# build the extractdbg tool for extracting debug info from ELF files
$(EXTRACTDGB_TOOL):
	@$(MAKE) -C ../tools/extractdbg all;

# compile assembly
$(K_BUILD_DIR)/%.o: %.S | directories
	@$(CC) $(KBUILD_AFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@

# compile C, also create assembly listing
$(K_BUILD_DIR)/%.o: %.c | directories
	@printf "$(TASK_COLOR)Compile with $(CC): $(@)\n$(NO_COLOR)"
	@$(CC) $(KBUILD_CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $< -o $@
ifeq ($(CREATE_ASSEMBLY), yes)
	@printf "$(TASK_COLOR)Assembly $(basename $@).s\n$(NO_COLOR)"
	@$(CC) $(KBUILD_CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -S $< -g -o $(basename $@).s
endif


# embedd the filesystem image into the kernel as an array:
ifeq ($(RAMDISK_EMBEDDED), yes)
$(K_BUILD_DIR)/ramdisk_fs.h:
	@printf "$(TASK_COLOR)Creating ramdisk_fs.h\n$(NO_COLOR)"
	@mkdir -p $(K_BUILD_DIR)
	@echo "#pragma once" > $(K_BUILD_DIR)/ramdisk_fs.h
	@echo "#include <kernel/kernel.h>" >> $(K_BUILD_DIR)/ramdisk_fs.h
	@echo "extern unsigned char ramdisk_fs[];" >> $(K_BUILD_DIR)/ramdisk_fs.h
	@echo -n "#define ramdisk_fs_size " >> $(K_BUILD_DIR)/ramdisk_fs.h
	@stat -c %s $(FILESYSTEM_IMG) >> $(K_BUILD_DIR)/ramdisk_fs.h

$(K_BUILD_DIR)/ramdisk_fs.c:
	@printf "$(TASK_COLOR)Creating ramdisk_fs.c\n$(NO_COLOR)"
	@mkdir -p $(K_BUILD_DIR)
	@echo "#include \"ramdisk_fs.h\"" > $(K_BUILD_DIR)/ramdisk_fs.c
	@echo "unsigned char ramdisk_fs[] = {" >> $(K_BUILD_DIR)/ramdisk_fs.c
	@xxd -i < $(FILESYSTEM_IMG) >> $(K_BUILD_DIR)/ramdisk_fs.c
	@echo "};" >> $(K_BUILD_DIR)/ramdisk_fs.c

$(K_BUILD_DIR)/ramdisk_fs.o: $(K_BUILD_DIR)/ramdisk_fs.c
	@printf "$(TASK_COLOR)Compile ramdisk_fs\n$(NO_COLOR)"
	@$(CC) $(KBUILD_CFLAGS) -I. $(KERNEL_SPACE_INC_PARAMS) -c $(K_BUILD_DIR)/ramdisk_fs.c -o $(K_BUILD_DIR)/ramdisk_fs.o

else
$(K_BUILD_DIR)/ramdisk_fs.h:
	@mkdir -p $(K_BUILD_DIR)
	@touch $(K_BUILD_DIR)/ramdisk_fs.h
endif

KERNEL_LD_FLAGS := --defsym=TEXT_OFFSET=$(TEXT_OFFSET) --defsym=PHYS_OFFSET=$(PHYS_OFFSET) --defsym=PAGE_OFFSET=$(PAGE_OFFSET)
KERNEL_LD_FLAGS += -m $(LD_ARCH_STRING)

$(KERNEL_PATH): $(EXTRACTDGB_TOOL) $(K_BUILD_DIR)/ramdisk_fs.h $(OBJECT_FILES) ./kernel.ld # build kernel
	@printf "$(TASK_COLOR)Link kernel: $(@)\n$(NO_COLOR)"
	@$(LD) $(LDFLAGS) $(KERNEL_LD_FLAGS) $(ARCH_LD_FLAGS) -T ./kernel.ld -o $(KERNEL_PATH) $(OBJECT_FILES)
ifeq ($(CREATE_ASSEMBLY), yes)
	@printf "$(TASK_COLOR)Disassemble kernel: $(K_BUILD_DIR)/$(KERNEL_NAME).asm\n$(NO_COLOR)"
	@$(OBJDUMP) -S $(KERNEL_PATH) > $(K_BUILD_DIR)/$(KERNEL_NAME).asm
endif
	@$(OBJCOPY) -O binary $(KERNEL_PATH) $(KERNEL_PATH).img
ifeq ($(CREATE_DEBUG_INFO), yes)
	@printf "$(TASK_COLOR)Extract debug info: $(KERNEL_PATH).xdbg\n$(NO_COLOR)"
	@$(EXTRACTDGB_TOOL) $(KERNEL_PATH) $(KERNEL_PATH).xdbg
endif

deploy: $(KERNEL_PATH)
ifneq ($(KERNEL_FORMAT), elf)
	@cp $(KERNEL_PATH).img ../$(BUILD_DIR)/boot/$(KERNEL_NAME).img
endif
	@cp $(KERNEL_PATH) ../$(BUILD_DIR)/boot/$(KERNEL_NAME)
	@cp $(KERNEL_PATH).xdbg ../$(BUILD_DIR)/boot/$(KERNEL_NAME).xdbg

clean:
	-@rm -f $(KERNEL_PATH)
	-@rm -f $(KERNEL_PATH).img
	-@rm -rf $(K_BUILD_DIR)
	-@rm -f $(K_BUILD_DIR)/ramdisk_fs.h
	-@rm -f $(RAMDISC_C)
	-@rm -f $(DTB_C)
	-@rm -f ../$(BUILD_DIR)/*.img


ifneq "$(TARGET_CC_HAS_C23_EMBED)" "1"
# can't use the C23 embed feature, use fallback:

OBJ_COPY_OPTIONS := --input binary --output $(OBJ_COPY_OUTPUT) --binary-architecture $(OBJ_COPY_ARCH)

$(K_BUILD_DIR)/jh7110.o: ../boot/dtb/jh7110-starfive-visionfive-2-v1.3b.dtb
	@printf "$(TASK_COLOR)Creating binary of DTB jh7110\n$(NO_COLOR)"
	@$(OBJCOPY) $(OBJ_COPY_OPTIONS) \
	--redefine-sym _binary____boot_dtb_jh7110_starfive_visionfive_2_v1_3b_dtb_start=emdtb_jh7110 \
	$< $@

$(K_BUILD_DIR)/orangepi_rv2.o: ../boot/dtb/x1_orangepi-rv2.dtb
	@printf "$(TASK_COLOR)Creating binary of DTB orangepi_rv2\n$(NO_COLOR)"
	@$(OBJCOPY) $(OBJ_COPY_OPTIONS) \
	--redefine-sym _binary____boot_dtb_x1_orangepi_rv2_dtb_start=emdtb_orangepi \
	$< $@

endif
