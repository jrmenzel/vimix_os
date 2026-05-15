/* SPDX-License-Identifier: MIT */

#include <init/embedded_dtbs.h>

const char emdtb_jh7110[] = {
#embed "../../boot/dtb/jh7110-starfive-visionfive-2-v1.3b.dtb"
};

const char emdtb_orangepi[] = {
#embed "../../boot/dtb/x1_orangepi-rv2.dtb"
};

const char *get_embedded_dtb(enum Compatible_System compatible)
{
    switch (compatible)
    {
        case SYSTEM_RISCV_QEMU: return NULL;
        case SYSTEM_RISCV_SPIKE: return NULL;
        case SYSTEM_RISCV_VISIONFIVE2: return emdtb_jh7110;
        case SYSTEM_RISCV_ORANGEPI_RV2: return emdtb_orangepi;
        default: return NULL;
    }
}
