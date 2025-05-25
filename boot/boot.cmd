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

setenv device_tree "0x41000000"
echo "load device tree to ${device_tree}"
load ${devtype} ${devnum} ${device_tree} "boot/dtb/jh7110-starfive-visionfive-2-v1.3b.dtb"

echo "set fdt address to ${device_tree}"
fdt addr ${device_tree}

setenv ram_disk "0x42000000"
echo "load ram disk"
load ${devtype} ${devnum} ${ram_disk} "boot/filesystem.img"
fdt resize
# begin + end
echo "add ram disk to device tree"
fdt chosen 0x42000000 0x423FFFFF

# make bootelf run the kernel
setenv autostart yes

echo "load kernel to memory"
load ${devtype} ${devnum} ${loadaddr} boot/kernel-vimix
echo "boot kernel"
bootelf ${loadaddr}

echo ""
echo "something went wrong"
echo ""

