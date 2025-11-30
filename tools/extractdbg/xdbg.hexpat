import type.magic;
import std.core;
import std.string;

#pragma array_limit 1024000

using NullString = std::string::NullString;

struct Instruction {
    u16 file_index [[color("008000")]];
    u16 line [[color("008080")]];
};

struct dbg_header {
    type::Magic<"\x58\x44\x42\x47\x76\x31\x00\x00"> magic [[comment("Magic Number.")]];
    u64 cu_name_count;
    u64 cu_name_list_size;
    u64 cu_name_offset;
    u64 lowest_address;
    u64 highest_address;
    u64 instruction_slots;
    
    NullString files[cu_name_count];
    Instruction instructions[instruction_slots];
};

dbg_header header @ 0x00;
