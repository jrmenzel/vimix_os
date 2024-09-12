#
# The kernel (in /kernel), host tools (in /tools), user space apps (in /usr/bin)
# and user space libraries (/usr/libs) have their own Makefiles.
# The settings that are shared (e.g. compiler to use, target architecture)
# can be found in MakefileCommon.mk
#
include MakefileCommon.mk

.PHONY: all directories kernel tools userspace_lib userspace userspace_host

all: directories kernel tools userspace_lib userspace userspace_host $(BUILD_DIR)/filesystem.img

directories: # make build output directory
	mkdir -p $(BUILD_DIR);

kernel: userspace # the kernel itself, depends on userspace for the embedded ram disk only
	$(MAKE) -C kernel all;

tools: # mkfs
	$(MAKE) -C tools/mkfs all;

userspace_lib: # user space clib
	$(MAKE) -C usr/lib all;

userspace: userspace_lib # user space apps and libs
	$(MAKE) -C usr/bin all;

userspace_host: # some user space apps for the host (Linux)
	$(MAKE) -C usr/bin host; 

# filesystem in a file containing userspace to run with qemu (kernel is set manually)
$(BUILD_DIR)/filesystem.img: tools README.md userspace
	@printf "$(TASK_COLOR)Create file system: $(@)\n$(NO_COLOR)"
	cp README.md $(BUILD_DIR)/root/
	mkdir -p $(BUILD_DIR)/root/dev
	mkdir -p $(BUILD_DIR)/root/home
	tools/mkfs/mkfs $(BUILD_DIR)/filesystem.img --in $(BUILD_DIR)/root/ 

###
# qemu
ifeq ($(BITWIDTH), 64)
QEMU = qemu-system-riscv64
else
QEMU = qemu-system-riscv32
endif
GDB_PORT = 26002

ifeq ($(SBI_SUPPORT), yes)
QEMU_BIOS=default
else
QEMU_BIOS=none
endif

QEMU_OPTS = -machine virt -bios $(QEMU_BIOS) -kernel $(KERNEL_FILE) -m $(MEMORY_SIZE)M -smp $(CPUS) -nographic
QEMU_OPTS += -global virtio-mmio.force-legacy=false

ifeq ($(VIRTIO_DISK), yes)
QEMU_OPTS += -drive file=$(BUILD_DIR)/filesystem.img,if=none,format=raw,id=x0
QEMU_OPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
endif

ifeq ($(RAMDISK_BOOTLOADER), yes)
QEMU_OPTS += -initrd $(BUILD_DIR)/filesystem.img
endif

#
# Debugging in QEMU

# -S = do not start CPUs, wait for 'c' in monitor (VSCode sends this on attach)
# -s = alias for "-gdb tcp:localhost:1234"
QEMU_DEBUG_OPTS = -S -gdb tcp:localhost:$(GDB_PORT)

# run in qemu
qemu: kernel $(BUILD_DIR)/filesystem.img # run VIMIX in qemu
	@printf "\n$(YELLOW)CTRL+A X to close qemu$(NO_COLOR)\n"
	$(QEMU) $(QEMU_OPTS)

# dump device tree
qemu-dump-tree: kernel $(BUILD_DIR)/filesystem.img
	$(QEMU) $(QEMU_OPTS) -M dumpdtb=qemu-riscv.dtb
	dtc -o qemu-riscv.dts -O dts -I dtb qemu-riscv.dtb

ifeq ($(BITWIDTH), 32)
GDB_ARCHITECTURE = riscv:rv32
else
GDB_ARCHITECTURE = riscv:rv64
endif

.gdbinit: tools/gdbinit Makefile MakefileCommon.mk
	cp tools/gdbinit .gdbinit
	sed -i 's/_PORT/$(GDB_PORT)/g' .gdbinit
	sed -i "s~_KERNEL~$(KERNEL_FILE)~g" .gdbinit
	sed -i 's/_ARCHITECTURE/$(GDB_ARCHITECTURE)/g' .gdbinit


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
SPIKE=spike
#SPIKE_BUILD=../riscv-simulator/riscv-isa-sim/build
#SPIKE=$(SPIKE_BUILD)/spike
#SPIKE_SBI_FW=../opensbi/build/platform/generic/firmware/fw_payload.elf
ifeq ($(BITWIDTH), 32)
SPIKE_ISA = rv32gc
else
SPIKE_ISA = rv64gc
endif

SPIKE_OPTIONS = -m0x80000000:$(MEMORY_SIZE_BYTES) -p$(CPUS) --isa=$(SPIKE_ISA)
SPIKE_OPTIONS += --initrd=$(BUILD_DIR)/filesystem.img --kernel=$(KERNEL_FILE) $(KERNEL_FILE)

spike-requirements: kernel $(BUILD_DIR)/filesystem.img

spike: spike-requirements
	$(SPIKE) $(SPIKE_OPTIONS)

spike-log: spike-requirements
	$(SPIKE) -l --log=spike_log.txt $(SPIKE_OPTIONS) 

spike-gdb: spike-requirements
	$(SPIKE) --rbb-port=9824 $(SPIKE_OPTIONS)

spike-sbi: spike-requirements
	$(SPIKE) $(SPIKE_SBI_FW) $(SPIKE_OPTIONS)


clean: # clean up
	$(MAKE) -C kernel clean;
	$(MAKE) -C tools/mkfs clean;
	$(MAKE) -C usr/lib clean;
	$(MAKE) -C usr/bin clean;
	-@rm -r $(BUILD_DIR)/root/*
