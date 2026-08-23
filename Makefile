#
# The kernel (in /kernel), user space apps (in /usr/bin) incl tools
# and user space libraries (/usr/libs) have their own Makefiles.
# The settings that are shared (e.g. compiler to use, target architecture)
# can be found in MakefileCommon.mk
#
include MakefileCommon.mk

# use default EMU settings if a target was set but no explicit EMU setting
ifndef EMU
ifdef TARGET
EMU := $(DEFAULT_EMU)
$(info EMU set to $(EMU) by default for TARGET=$(TARGET))
endif
endif

include kernel/arch/riscv/MakefileEmusRISCV.mk

.PHONY: all directories kernel userspace host

FILESYSTEM_IMG_NAME := filesystem_$(TARGET)$(BUILD_TYPE_SHORT).img
FILESYSTEM_IMG := $(BUILD_DIR)/$(FILESYSTEM_IMG_NAME)
FILESYSTEM_IMG_DEPLOY := $(BUILD_DIR)/boot/filesystem.img
DEPLOYED_TARGET_FILE := $(BUILD_DIR)/boot/current_target_$(TARGET)$(BUILD_TYPE_SHORT).txt

all: directories $(EXTRACTDGB_TOOL) $(DEPLOYED_TARGET_FILE)

# make build output directory
directories:
	@mkdir -p $(BUILD_DIR);
	@mkdir -p $(BUILD_DIR)/boot;

# Build host-side tool once in the top-level graph so parallel sub-makes
# (kernel/userspace/host) do not race creating the same binary.
$(EXTRACTDGB_TOOL): | directories
	@$(MAKE) -C tools/extractdbg all;

# the kernel itself depends on userspace for the embedded ram disk only
ifeq ($(RAMDISK_EMBEDDED), yes)
KERNEL_REQS := directories $(EXTRACTDGB_TOOL) $(FILESYSTEM_IMG)
else
KERNEL_REQS := directories $(EXTRACTDGB_TOOL)
endif
kernel: $(KERNEL_REQS) # the kernel itself
	@$(MAKE) -C kernel all;

userspace: $(EXTRACTDGB_TOOL) # user space apps and libs
	@$(MAKE) -C usr/lib all;
	@$(MAKE) -C usr/bin all;
	@$(MAKE) -C usr/local/bin/dhrystone all;

host: $(EXTRACTDGB_TOOL) # some user space apps for the host (Linux)
	@$(MAKE) TARGET_OR_HOST=host -C usr/lib all;
	@$(MAKE) TARGET_OR_HOST=host -C usr/bin all;
	@$(MAKE) TARGET_OR_HOST=host -C usr/local/bin/dhrystone all; 

$(BUILD_DIR)/boot/boot.scr:
	@mkdir -p $(BUILD_DIR)/boot/dtb
	@cp -r boot/dtb/* $(BUILD_DIR)/boot/dtb
	@mkimage -C none -A riscv -T script -d boot/boot.cmd $(BUILD_DIR)/boot/boot.scr

# boot directory content for deployment
$(DEPLOYED_TARGET_FILE): $(FILESYSTEM_IMG) $(BUILD_DIR)/boot/boot.scr
	@echo "update deployed target to $(TARGET)$(BUILD_TYPE_SHORT)"
	@rm -f $(BUILD_DIR)/boot/current_target_*
	@rm -f $(BUILD_DIR)/boot/kernel*
	@cp $(FILESYSTEM_IMG) $(FILESYSTEM_IMG_DEPLOY)
	@$(MAKE) -C kernel deploy;
	@touch $(DEPLOYED_TARGET_FILE)


# filesystem in a file containing userspace as initrd (kernel is set manually)
$(FILESYSTEM_IMG): host userspace | directories
	@rm -f $(BUILD_DIR)/root
	@ln -s root$(TARGET_SUFFIX) $(BUILD_DIR)/root
	@printf "$(TASK_COLOR)Create file system: $(@)\n$(NO_COLOR)"
	@./tools/make_filesystem.sh $(BUILD_DIR) $(BUILD_DIR_HOST) $(FILESYSTEM_IMG_NAME)

###
# qemu
GDB_PORT := 26000

QEMU_OPTS := $(QEMU_OPTS_ARCH) -m $(MEMORY_SIZE)M -smp $(CPUS) -nographic

ifneq ("$(wildcard $(KERNEL_FILE).img)","")
QEMU_OPTS += -kernel $(KERNEL_FILE).img
else
QEMU_OPTS += -kernel $(KERNEL_FILE)
endif

ifeq ($(VIRTIO_DISK), yes)
QEMU_OPTS += -drive file=$(FILESYSTEM_IMG_DEPLOY),if=none,format=raw,id=x0
QEMU_OPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
#QEMU_OPTS += -d int,mmu,in_asm -D qemu_mmu.log
# add a second file system if it is present
ifneq ("$(wildcard home.img)","")
	QEMU_OPTS += -drive file=home.img,if=none,format=raw,id=x1
	QEMU_OPTS += -device virtio-blk-device,drive=x1,bus=virtio-mmio-bus.1
endif
endif

ifeq ($(RAMDISK_BOOTLOADER), yes)
QEMU_OPTS += -initrd $(FILESYSTEM_IMG_DEPLOY)
endif
ifdef RAMDISK_LOAD_ADDR
QEMU_OPTS += -device loader,file=$(FILESYSTEM_IMG_DEPLOY),addr=$(RAMDISK_LOAD_ADDR)
endif

#
# Debugging in QEMU

# -S = do not start CPUs, wait for 'c' in monitor (VSCode sends this on attach)
# -s = alias for "-gdb tcp:localhost:1234"
QEMU_DEBUG_OPTS := -S -gdb tcp:localhost:$(GDB_PORT)

emu-check:
ifeq ($(filter $(EMU),$(EMUS_RISCV)),)
	$(error "EMU not set correctly, select one of: $(EMUS_RISCV)")
endif

emu-requirements: emu-check $(DEPLOYED_TARGET_FILE)

qemu: emu-requirements # run in qemu, rebuilds if needed
	@printf "\n$(YELLOW)CTRL+A X to close qemu$(NO_COLOR)\n"
	$(QEMU) $(QEMU_OPTS)

qemu-log: emu-requirements # run in qemu with logging, rebuilds if needed
	@printf "\n$(YELLOW)CTRL+A X to close qemu$(NO_COLOR)\n"
	$(QEMU) $(QEMU_OPTS) -d cpu_reset -d int -d in_asm -D log_${TARGET}.txt

qemu-run: emu-check # run in qemu without rebuilding, useful for automated tests
	@printf "\n$(YELLOW)DID NOT REBUILD ANYTHING$(NO_COLOR)\n"
	@printf "\n$(YELLOW)CTRL+A X to close qemu$(NO_COLOR)\n"
	$(QEMU) --version
	$(QEMU) $(QEMU_OPTS)

# dump device tree
qemu-dump-tree: emu-requirements
	$(QEMU) $(QEMU_OPTS) -machine dumpdtb=tree_$(EMU).dtb
	dtc -o tree_$(EMU).dts -O dts -I dtb tree_$(EMU).dtb

.gdbinit: tools/gdbinit Makefile MakefileCommon.mk
	@cp tools/gdbinit .gdbinit
	@sed -i 's/_PORT/$(GDB_PORT)/g' .gdbinit
	@sed -i 's/_KERNEL_VIRT_BASE/($(PAGE_OFFSET)+$(TEXT_OFFSET))/g' .gdbinit
	@sed -i 's/_KERNEL_PHYS_BASE/($(PHYS_OFFSET)+$(TEXT_OFFSET))/g' .gdbinit
	@sed -i "s~_KERNEL~$(KERNEL_FILE)~g" .gdbinit
	@sed -i 's/_ARCHITECTURE/$(GDB_ARCHITECTURE)/g' .gdbinit

.gdbinit_vscode: tools/gdbinit_vscode Makefile MakefileCommon.mk
	@cp tools/gdbinit_vscode .gdbinit_vscode
	@sed -i 's/_PORT/$(GDB_PORT)/g' .gdbinit_vscode
	@sed -i 's/_KERNEL_VIRT_BASE/($(PAGE_OFFSET)+$(TEXT_OFFSET))/g' .gdbinit_vscode
	@sed -i 's/_KERNEL_PHYS_BASE/($(PHYS_OFFSET)+$(TEXT_OFFSET))/g' .gdbinit_vscode
	@sed -i "s~_KERNEL~$(KERNEL_FILE)~g" .gdbinit_vscode
	@sed -i 's/_ARCHITECTURE/$(GDB_ARCHITECTURE)/g' .gdbinit_vscode

qemu-gdb: emu-requirements .gdbinit_vscode # run in qemu waiting for a debugger
	@printf "\n$(YELLOW)CTRL+A X to close qemu\n"
	@printf " Now run 'gdb' in another window.\n"
	@printf " OR attach with VSCode for debugging.$(NO_COLOR)\n"
	$(QEMU) $(QEMU_OPTS) $(QEMU_DEBUG_OPTS)

#
# Spike simulator
#

ifeq ($(RAMDISK_BOOTLOADER), yes)
SPIKE_OPTIONS := --initrd=$(FILESYSTEM_IMG_DEPLOY) $(SPIKE_OPTIONS)
endif

spike: emu-requirements # run in spike, rebuilds if needed
	@printf "\n$(YELLOW)CTRL+C q Enter to close Spike$(NO_COLOR)\n"
	$(SPIKE) $(SPIKE_OPTIONS)

spike-log: emu-requirements # run in spike with logging, rebuilds if needed
	$(SPIKE) -l --log=log_${TARGET}.txt $(SPIKE_OPTIONS) 

spike-run: # run in spike, without rebuilding, useful for automated tests
	@printf "\n$(YELLOW)DID NOT REBUILD ANYTHING$(NO_COLOR)\n"
	@printf "\n$(YELLOW)CTRL+C q Enter to close Spike$(NO_COLOR)\n"
	$(SPIKE) $(SPIKE_OPTIONS)

spike-gdb: emu-requirements .gdbinit_vscode # run in spike waiting for a debugger, rebuilds if needed
	$(SPIKE) --rbb-port=9824 --halted $(SPIKE_OPTIONS)

# dump device tree
spike-dump-tree: emu-requirements
	$(SPIKE) --dump-dts $(SPIKE_OPTIONS) > tree_$(EMU).dts

clean: # clean up
	@$(MAKE) -C tools/extractdbg clean;
	-@rm -rf build/*
	-@rm -f build/.*.stamp
	-@rm -rf build_host/*
	-@rm -f .gdbinit
	-@rm -f .gdbinit_vscode
