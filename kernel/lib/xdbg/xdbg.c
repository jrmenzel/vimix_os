/* SPDX-License-Identifier: MIT */

#include <kernel/errno.h>
#include <kernel/fs.h>
#include <kernel/string.h>
#include <lib/xdbg/xdbg.h>
#include <mm/kalloc.h>

struct debug_info *debug_info_alloc()
{
    return kmalloc(sizeof(struct debug_info), ALLOC_FLAG_ZERO_MEMORY);
}

void debug_info_free(struct debug_info *info)
{
    if (!info) return;
    if (info->cu_name_list) kfree(info->cu_name_list);
    if (info->instructions)
        free_pages(info->instructions, info->instruction_alloc_order);
    kfree(info);
}

const uint8_t DEBUG_INFO_MAGIC[8] = XDBG_MAGIC;

syserr_t debug_info_read(struct debug_info *info, struct inode *ip)
{
    struct dbg_file_header hdr;
    size_t header_read = inode_read(ip, false, (size_t)&hdr, 0, sizeof(hdr));
    if (header_read != sizeof(hdr))
    {
        printk("Failed to read debug info header\n");
        return -EIO;
    }

    if (memcmp(hdr.magic, DEBUG_INFO_MAGIC, sizeof(DEBUG_INFO_MAGIC)) != 0)
    {
        printk("Failed to read debug info header, bad magic\n");
        return -EIO;
    }

    if (hdr.lowest_address > hdr.highest_address)
    {
        printk("Failed to read debug info header, address error\n");
        return -EIO;
    }

    info->cu_name_count = (size_t)hdr.cu_name_count;
    info->cu_name_list_size = (size_t)hdr.cu_name_list_size;
    info->cu_name_offset = (size_t)hdr.cu_name_offset;
    info->lowest_address = hdr.lowest_address;
    info->highest_address = hdr.highest_address;
    info->instruction_slots = (size_t)hdr.instruction_slots;

    if (info->cu_name_list_size)
    {
        if (info->cu_name_list_size > PAGE_SIZE)
        {
            printk("CU name list too large\n");
            return -ENOMEM;
        }

        info->cu_name_list = kmalloc(info->cu_name_list_size, ALLOC_FLAG_NONE);
        if (!info->cu_name_list)
        {
            printk("Failed to allocate CU name list\n");
            return -ENOMEM;
        }

        size_t read_bytes = inode_read(ip, false, (size_t)info->cu_name_list,
                                       sizeof(hdr), info->cu_name_list_size);
        if (read_bytes != info->cu_name_list_size)
        {
            printk("Failed to read CU names\n");
            return -EIO;
        }
    }

    if (info->instruction_slots)
    {
        size_t bytes_to_read =
            info->instruction_slots * sizeof(struct instruction);
        size_t instructions_offset = sizeof(hdr) + info->cu_name_list_size;

        size_t alloc_order = 0;
        size_t req_pages = PAGE_ROUND_UP(bytes_to_read) / PAGE_SIZE;
        while ((1UL << alloc_order) < req_pages) ++alloc_order;

        info->instructions = alloc_pages(ALLOC_FLAG_ZERO_MEMORY, alloc_order);
        if (!info->instructions)
        {
            printk("Failed to allocate instruction table\n");
            return -ENOMEM;
        }
        info->instruction_alloc_order = alloc_order;

        size_t read_bytes = inode_read(ip, false, (size_t)info->instructions,
                                       instructions_offset, bytes_to_read);
        if (read_bytes != bytes_to_read)
        {
            printk("Failed to read instruction table\n");
            return -EIO;
        }
    }

    return 0;
}

const struct instruction *debug_info_lookup_instruction(
    const struct debug_info *dbg_info, size_t address)
{
    if (!dbg_info || !dbg_info->instructions ||
        address < dbg_info->lowest_address ||
        address > dbg_info->highest_address)
    {
        return NULL;
    }

    size_t offset = address - dbg_info->lowest_address;
    size_t index = offset / 2ULL;
    if (index >= dbg_info->instruction_slots)
    {
        return NULL;
    }

    const struct instruction *instr = &dbg_info->instructions[index];
    if (instr->file == 0 && instr->line == 0)
    {
        return NULL;
    }
    return instr;
}

const struct instruction *debug_info_lookup_caller(
    const struct debug_info *dbg_info, size_t ra)
{
    // try to find calling instruction,
    // can be 2 bytes or 4 bytes before return address on RISC V
    size_t caller_addr = ra - 2;
    const struct instruction *instr =
        debug_info_lookup_instruction(dbg_info, caller_addr);
    if (!instr)
    {
        caller_addr -= 2;
        instr = debug_info_lookup_instruction(dbg_info, caller_addr);
    }
    return instr;
}

const char *debug_info_resolve_cu_name(const struct debug_info *dbg_info,
                                       uint16_t cu_index)
{
    if (!dbg_info || !dbg_info->cu_name_list ||
        cu_index >= dbg_info->cu_name_count)
    {
        return NULL;
    }

    size_t index = 0;
    size_t offset = 0;
    while (index < cu_index)
    {
        // skip current name
        while (offset < dbg_info->cu_name_list_size &&
               dbg_info->cu_name_list[offset] != '\0')
        {
            ++offset;
        }
        // skip null terminator
        if (offset < dbg_info->cu_name_list_size)
        {
            ++offset;
        }
        ++index;
    }

    if (offset >= dbg_info->cu_name_list_size)
    {
        return NULL;
    }

    return &dbg_info->cu_name_list[offset];
}

void debug_info_print_instruction(const struct debug_info *dbg_info,
                                  const struct instruction *instr)
{
    if (!instr)
    {
        printk("<unknown>\n");
        return;
    }
    const char *file_name = debug_info_resolve_cu_name(dbg_info, instr->file);
    printk("%s:%u", file_name ? file_name : "???", instr->line);
}
