#
# The kernel (in /kernel), host tools (in /tools), user space apps (in /usr/bin)
# and user space libraries (/usr/libs) have their own Makefiles.
# The settings that are shared (e.g. compiler to use, target architecture)
# can be found in MakefileCommon.mk
#
include MakefileCommon.mk

.PHONY: all kernel tools userspace clean

all: directories kernel tools userspace_lib userspace

directories:
	mkdir -p $(BUILD_DIR);

kernel:
	pushd kernel; \
	$(MAKE) all; \
	popd;

tools:
	pushd tools/mkfs; \
	$(MAKE) all; \
	popd;

userspace_lib:
	pushd usr/lib; \
	$(MAKE) all; \
	popd;

userspace: userspace_lib
	pushd usr/bin; \
	$(MAKE) all; \
	popd;


# filesystem in a file containing userspace to run with qemu (kernel is set manually)
$(BUILD_DIR)/filesystem.img: tools README userspace
	tools/mkfs/mkfs $(BUILD_DIR)/filesystem.img README $(BUILD_DIR)/root/* 

###
# qemu
QEMU = qemu-system-riscv64
GDB_PORT = 26002
QEMU_BIOS=none

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
qemu: kernel $(BUILD_DIR)/filesystem.img
	@echo
	@echo "*** CTRL+A X to close qemu"
	@echo
	$(QEMU) $(QEMU_OPTS)

GDB_ARCHITECTURE = riscv:rv64

.gdbinit: tools/gdbinit Makefile MakefileCommon.mk
	cp tools/gdbinit .gdbinit
	sed -i 's/_PORT/$(GDB_PORT)/g' .gdbinit
	sed -i "s~_KERNEL~$(KERNEL_FILE)~g" .gdbinit
	sed -i 's/_ARCHITECTURE/$(GDB_ARCHITECTURE)/g' .gdbinit


# run in qemu waiting for a debugger
qemu-gdb: kernel .gdbinit $(BUILD_DIR)/filesystem.img
	@echo
	@echo "*** CTRL+A X to close qemu"
	@echo
	@echo "    Now run 'gdb' in another window."
	@echo " OR Attach with VSCode for debugging."
	$(QEMU) $(QEMU_OPTS) $(QEMU_DEBUG_OPTS)

clean:
	-@rm -r $(BUILD_DIR)/*
