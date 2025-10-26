# Contributors

VIMIX OS is based on xv6, see [README-xv6](README-xv6.md) for original contributors.

## To the OS

The initial 32-bit [RISCV](riscv/RISCV.md) support was inspired by a 32-bit xv6 port by Michael Schröder (https://github.com/x653/xv6-riscv-fpga/tree/main/xv6-riscv).

[Device tree](misc/device_tree.md) parsing is done via libfdt from https://github.com/dgibson/dtc (see `kernel/lib/libfdt/readme.txt` for details).

## To the user space

Toml config file parsing is done via tomlc17, see [README](../usr/lib/tomlc17/README.md).

Ported third party applications:
- [dhrystone](userspace/local/bin/dhrystone.md)
- [wumpus](userspace/bin/wumpus.md)

