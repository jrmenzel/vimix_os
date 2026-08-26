/* SPDX-License-Identifier: MIT */

#include <init/embedded_dtbs.h>

// if the C23 #embed feature is available
#if defined(__has_embed)

const unsigned char dtb_embedded[] = {
#ifdef __CONFIG_DTB_FILE_PATH
#embed __CONFIG_DTB_FILE_PATH
#endif
};

const unsigned char emdtb_jh7110[] = {
#embed "./boot/dtb/jh7110-starfive-visionfive-2-v1.3b.dtb"
};

const unsigned char emdtb_orangepi[] = {
#embed "./boot/dtb/x1_orangepi-rv2.dtb"
};

#else

#ifdef __CONFIG_DTB_FILE_PATH
extern const unsigned char dtb_embedded[];
#else
const unsigned char dtb_embedded[1] = {0};
#endif
extern const unsigned char emdtb_jh7110[];
extern const unsigned char emdtb_orangepi[];

#endif

const unsigned char *get_pointer(size_t p) { return (const unsigned char *)p; }

const unsigned char *get_embedded_dtb(enum Compatible_System compatible)
{
    switch (compatible)
    {
        case SYSTEM_RISCV_QEMU: return NULL;
        case SYSTEM_RISCV_SPIKE: return NULL;
        case SYSTEM_RISCV_VISIONFIVE2: return get_pointer((size_t)emdtb_jh7110);
        case SYSTEM_RISCV_ORANGEPI_RV2:
            return get_pointer((size_t)emdtb_orangepi);
        default: return NULL;
    }
}
