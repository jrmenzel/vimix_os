/* SPDX-License-Identifier: MIT */
#pragma once

#include <kernel/kernel.h>

struct instruction
{
    uint16_t file;
    uint16_t line;
};

/// Minimal debug info extracted from an ELF file
struct debug_info
{
    size_t cu_name_count;
    size_t cu_name_offset;
    size_t cu_name_list_size;
    uint64_t lowest_address;
    uint64_t highest_address;
    size_t instruction_slots;
    char *cu_name_list;
    struct instruction *instructions;
    size_t instruction_alloc_order;
};

#define XDBG_MAGIC {'X', 'D', 'B', 'G', 'v', '1', '\0', '\0'}

struct dbg_file_header
{
    uint8_t magic[8];
    uint64_t cu_name_count;
    uint64_t cu_name_list_size;
    uint64_t cu_name_offset;
    uint64_t lowest_address;
    uint64_t highest_address;
    uint64_t instruction_slots;
};

#if defined(BUILD_ON_HOST)
// note: host apps only get the declarations, not the functions from the .c file
#else

struct debug_info *debug_info_alloc();

void debug_info_free(struct debug_info *info);

struct inode;
syserr_t debug_info_read(struct debug_info *info, struct inode *ip);

const struct instruction *debug_info_lookup_instruction(
    const struct debug_info *dbg_info, size_t address);

const struct instruction *debug_info_lookup_caller(
    const struct debug_info *dbg_info, size_t ra);

const char *debug_info_resolve_cu_name(const struct debug_info *dbg_info,
                                       uint16_t cu_index);

void debug_info_print_instruction(const struct debug_info *dbg_info,
                                  const struct instruction *instr);

#endif
