/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

/// ELF file header
struct elfhdr
{
    uint32_t magic;  // must equal ELF_MAGIC
    uint8_t elf[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    size_t entry;
    size_t phoff;
    size_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

/// ELF program section header
struct proghdr
{
    uint32_t type;
#if defined(_arch_is_64bit)
    uint32_t flags;
#endif
    size_t off;
    size_t vaddr;
    size_t paddr;
    size_t filesz;
    size_t memsz;
#if defined(_arch_is_32bit)
    uint32_t flags;
#endif
    size_t align;
};

/// Values for Proghdr type
#define ELF_PROG_LOAD 1

/// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC 1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ 4
