#
# The kernel (in /kernel), user space apps (in /usr/bin) incl tools
# and user space libraries (/usr/libs) have their own Makefiles.
# The settings that are shared (e.g. compiler to use, target architecture)
# can be found in MakefileCommon.mk
#
include MakefileCommon.mk

.PHONY: all directories kernel userspace_lib userspace host

all: directories userspace_lib userspace host $(BUILD_DIR)/filesystem.img kernel

directories: # make build output directory
	@mkdir -p $(BUILD_DIR);

kernel: userspace $(BUILD_DIR)/filesystem.img # the kernel itself, depends on userspace for the embedded ram disk only
	@$(MAKE) -C kernel all;

userspace_lib: # user space clib
	@$(MAKE) -C usr/lib all;

userspace: userspace_lib # user space apps and libs
	@$(MAKE) -C usr/bin all;
	@$(MAKE) -C usr/local/bin/dhrystone all;

host: # some user space apps for the host (Linux)
	@$(MAKE) -C usr/bin host;
	@$(MAKE) -C usr/local/bin/dhrystone host; 

# filesystem in a file containing userspace to run with qemu (kernel is set manually)
$(BUILD_DIR)/filesystem.img: README.md userspace host
	@printf "$(TASK_COLOR)Create file system: $(@)\n$(NO_COLOR)"
	@cp README.md $(BUILD_DIR)/root/
	@mkdir -p $(BUILD_DIR)/root/dev
	@mkdir -p $(BUILD_DIR)/root/home
	@mkdir -p $(BUILD_DIR)/root/tests
	@cp -r root/tests/* $(BUILD_DIR)/root/tests
	$(BUILD_DIR_HOST)/root/usr/bin/mkfs $(BUILD_DIR)/filesystem.img --in $(BUILD_DIR)/root/ 

###
# qemu
GDB_PORT := 26002

QEMU_OPTS := $(QEMU_OPTS_ARCH) -kernel $(KERNEL_FILE) -m $(MEMORY_SIZE)M -smp $(CPUS) -nographic

ifeq ($(VIRTIO_DISK), yes)
QEMU_OPTS += -drive file=$(BUILD_DIR)/filesystem.img,if=none,format=raw,id=x0
QEMU_OPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
# add a second file system if it is present
ifneq ("$(wildcard home.img)","")
    QEMU_OPTS += -drive file=home.img,if=none,format=raw,id=x1
	QEMU_OPTS += -device virtio-blk-device,drive=x1,bus=virtio-mmio-bus.1
endif
endif

ifeq ($(RAMDISK_BOOTLOADER), yes)
QEMU_OPTS += -initrd $(BUILD_DIR)/filesystem.img
endif

#
# Debugging in QEMU

# -S = do not start CPUs, wait for 'c' in monitor (VSCode sends this on attach)
# -s = alias for "-gdb tcp:localhost:1234"
QEMU_DEBUG_OPTS := -S -gdb tcp:localhost:$(GDB_PORT)

# run in qemu
qemu: kernel $(BUILD_DIR)/filesystem.img # run VIMIX in qemu
	@printf "\n$(YELLOW)CTRL+A X to close qemu$(NO_COLOR)\n"
	$(QEMU) $(QEMU_OPTS)

# dump device tree
qemu-dump-tree: kernel $(BUILD_DIR)/filesystem.img
	$(QEMU) $(QEMU_OPTS) -M dumpdtb=tree.dtb
	dtc -o tree.dts -O dts -I dtb tree.dtb

.gdbinit: tools/gdbinit Makefile MakefileCommon.mk
	@cp tools/gdbinit .gdbinit
	@sed -i 's/_PORT/$(GDB_PORT)/g' .gdbinit
	@sed -i "s~_KERNEL~$(KERNEL_FILE)~g" .gdbinit
	@sed -i 's/_ARCHITECTURE/$(GDB_ARCHITECTURE)/g' .gdbinit


# run in qemu waiting for a debugger
qemu-gdb: kernel .gdbinit $(BUILD_DIR)/filesystem.img # run VIMIX in qemu waiting for a debugger
	@printf "\n$(YELLOW)CTRL+A X to close qemu\n"
	@printf " Now run 'gdb' in another window.\n"
	@printf " OR attach with VSCode for debugging.$(NO_COLOR)\n"
	$(QEMU) $(QEMU_OPTS) $(QEMU_DEBUG_OPTS)

#
# Spike simulator
# Note: see docs on how to build VIMIX for Spike

# spike binary, edit to use e.g. a self compiled version
#SPIKE_BUILD := ../riscv-simulator/riscv-isa-sim/build/
SPIKE := $(SPIKE_BUILD)spike
#SPIKE_SBI_FW=../opensbi/build/platform/generic/firmware/fw_payload.elf
ifeq ($(BITWIDTH), 32)
SPIKE_ISA := rv32gc
else
SPIKE_ISA := rv64gc
endif

MEMORY_SIZE_BYTES := $(shell echo $$(( $(MEMORY_SIZE) * 1024 * 1024 )))
SPIKE_OPTIONS := -m0x80000000:$(MEMORY_SIZE_BYTES) -p$(CPUS) --isa=$(SPIKE_ISA) --real-time-clint
ifeq ($(RAMDISK_BOOTLOADER), yes)
SPIKE_OPTIONS += --initrd=$(BUILD_DIR)/filesystem.img
endif
SPIKE_OPTIONS += --kernel=$(KERNEL_FILE) $(KERNEL_FILE)

spike-requirements: kernel $(BUILD_DIR)/filesystem.img

spike: spike-requirements
	$(SPIKE) $(SPIKE_OPTIONS)

spike-log: spike-requirements
	$(SPIKE) -l --log=spike_log.txt $(SPIKE_OPTIONS) 

spike-gdb: spike-requirements
	$(SPIKE) --rbb-port=9824 $(SPIKE_OPTIONS)

spike-sbi: spike-requirements
	$(SPIKE) $(SPIKE_SBI_FW) $(SPIKE_OPTIONS)

# dump device tree
spike-dump-tree: spike-requirements
	$(SPIKE) --dump-dts $(SPIKE_OPTIONS) 

clean: # clean up
	@$(MAKE) -C kernel clean;
	@$(MAKE) -C usr/lib clean;
	@$(MAKE) -C usr/bin clean;
	@$(MAKE) -C usr/local/bin/dhrystone clean;
	-@rm -rf $(BUILD_DIR)/root/*
	-@rm -rf $(BUILD_DIR_HOST)/root/*
