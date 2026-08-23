#
# The boot loader u-boot will look for a compiled script
# named /boot/boot.scr and execute it.
#
# Compile this script with 
#  mkimage -C none -A riscv -T script -d boot.cmd boot.scr
#

echo ""
echo "/boot/boot.scr loaded from ${devtype} ${devnum}"
echo ""

setenv kernel_load_addr  ${loadaddr}
setenv fdt_addr ${fdtcontroladdr}

if test "${board}" = "x1"; 
then 
    echo "stopping Banana Pi RV2 watchdog"
    wdt dev watchdog@D4080000
    wdt stop
fi

setexpr ramdisk_load_addr ${kernel_load_addr} + 0x100000

fdt addr ${fdt_addr}

echo "load ram disk"
load ${devtype} ${devnum} ${ramdisk_load_addr} "boot/filesystem.img"
setenv ram_disk_size ${filesize}
fdt resize 0x1000

# make booti run the kernel
setenv autostart yes

echo "load kernel to memory"
load ${devtype} ${devnum} ${kernel_load_addr} boot/kernel-vimix.img
echo "boot kernel with DTB from ${fdt_addr}"
booti ${kernel_load_addr} ${ramdisk_load_addr}:${ram_disk_size} ${fdt_addr}

echo ""
echo "something went wrong"
echo ""

