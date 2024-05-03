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

kernel: # the kernel itself
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
	tools/mkfs/mkfs $(BUILD_DIR)/filesystem.img $(BUILD_DIR)/root/ 

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

CPUS := 4
QEMU_OPTS = -machine virt -bios $(QEMU_BIOS) -kernel $(KERNEL_FILE) -m 128M -smp $(CPUS) -nographic
QEMU_OPTS += -global virtio-mmio.force-legacy=false
QEMU_OPTS += -drive file=$(BUILD_DIR)/filesystem.img,if=none,format=raw,id=x0
QEMU_OPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

#
# Debugging in QEMU

# -S = do not start CPUs, wait for 'c' in monitor (VSCode sends this on attach)
# -s = alias for "-gdb tcp:localhost:1234"
QEMU_DEBUG_OPTS = -S -gdb tcp:localhost:$(GDB_PORT)

# run in qemu
qemu: kernel $(BUILD_DIR)/filesystem.img # run VIMIX in qemu
	@printf "\n$(YELLOW)CTRL+A X to close qemu$(NO_COLOR)\n"
	$(QEMU) $(QEMU_OPTS)

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

clean: # clean up
	$(MAKE) -C kernel clean;
	$(MAKE) -C tools/mkfs clean;
	$(MAKE) -C usr/lib clean;
	$(MAKE) -C usr/bin clean;
	-@rm -r $(BUILD_DIR)/root/*
