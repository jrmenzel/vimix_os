// be warned, most of this little tool was done by AI

#include <elfutils/libdwfl.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lib/xdbg/xdbg.h>

const uint8_t DEBUG_INFO_MAGIC[8] = XDBG_MAGIC;

struct cu_list
{
    char **names;
    size_t count;
    size_t capacity;
};

void debug_info_init(struct debug_info *info)
{
    if (!info) return;
    memset(info, 0, sizeof(*info));
}

static void debug_info_free(struct debug_info *info)
{
    if (!info) return;
    free(info->cu_name_list);
    free(info->instructions);
    memset(info, 0, sizeof(*info));
}

static size_t instruction_slot_count(uint64_t lowest, uint64_t highest)
{
    if (highest < lowest) return 0;
    uint64_t span = highest - lowest;
    if (span & 1ULL) ++span;
    return (size_t)(span / 2ULL + 1ULL);
}

static size_t name_blob_size_from_list(const struct cu_list *cus)
{
    size_t total = 0;
    for (size_t i = 0; i < cus->count; ++i) total += strlen(cus->names[i]) + 1;
    return (total == 0) ? 1 : total;
}

static int build_cu_name_blob(const struct cu_list *cus,
                              struct debug_info *dbg_info)
{
    dbg_info->cu_name_count = cus->count;
    dbg_info->cu_name_offset = 0;

    if (cus->count == 0)
    {
        dbg_info->cu_name_list_size = 1;
        dbg_info->cu_name_list = calloc(1, 1);
        return dbg_info->cu_name_list ? 0 : -1;
    }

    dbg_info->cu_name_list_size = name_blob_size_from_list(cus);
    dbg_info->cu_name_list = malloc(dbg_info->cu_name_list_size);
    if (!dbg_info->cu_name_list) return -1;

    char *dst = dbg_info->cu_name_list + dbg_info->cu_name_offset;
    for (size_t i = 0; i < cus->count; ++i)
    {
        size_t len = strlen(cus->names[i]) + 1;
        memcpy(dst, cus->names[i], len);
        dst += len;
    }

    return 0;
}

static const char *debug_info_get_name(const struct debug_info *info,
                                       size_t index)
{
    if (!info || !info->cu_name_list || index >= info->cu_name_count)
        return NULL;

    const char *ptr = info->cu_name_list + info->cu_name_offset;
    const char *end = info->cu_name_list + info->cu_name_list_size;

    for (size_t i = 0; i < info->cu_name_count && ptr < end; ++i)
    {
        if (i == index) return ptr;
        size_t remaining = (size_t)(end - ptr);
        size_t len = strnlen(ptr, remaining);
        ptr += len + 1;
    }
    return NULL;
}

static const char *resolve_cu_name(size_t index, const struct cu_list *cus,
                                   const struct debug_info *dbg_info)
{
    if (cus && index < cus->count) return cus->names[index];
    const char *name = debug_info_get_name(dbg_info, index);
    return name ? name : "<invalid file>";
}

/* Forward declarations for I/O (definitions below) */
static int save_debug_info(const char *output_file,
                           const struct debug_info *dbg_info);
static int read_debug_info(const char *input_file, struct debug_info *dbg_info);

/* ---------------- CU helpers ---------------- */

static int cu_list_append(struct cu_list *list, const char *name)
{
    if (!name || !*name) name = "<unnamed>";
    if (list->count == list->capacity)
    {
        size_t new_cap = list->capacity ? list->capacity * 2 : 8;
        char **tmp = realloc(list->names, new_cap * sizeof(*tmp));
        if (!tmp) return -1;
        list->names = tmp;
        list->capacity = new_cap;
    }
    char *copy = strdup(name);
    if (!copy) return -1;
    list->names[list->count++] = copy;
    return 0;
}

static void cu_list_free(struct cu_list *list)
{
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) free(list->names[i]);
    free(list->names);
    list->names = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ---------------- DWFL setup ---------------- */

static int find_debuginfo(Dwfl_Module *mod, void **userdata, const char *name,
                          Dwarf_Addr base, const char *file,
                          const char *debuglink, GElf_Word crc,
                          char **debuginfo)
{
    (void)mod;
    (void)userdata;
    (void)name;
    (void)base;
    (void)file;
    (void)debuglink;
    (void)crc;
    *debuginfo = NULL;
    return 0;
}

static int setup_dwfl(const char *elf_file, Dwfl **dwfl_out,
                      Dwfl_Module **mod_out, Dwarf **dbg_out,
                      Dwarf_Addr *bias_out)
{
    Dwfl_Callbacks callbacks = {.find_elf = dwfl_build_id_find_elf,
                                .find_debuginfo = find_debuginfo};

    Dwfl *dwfl = dwfl_begin(&callbacks);
    if (!dwfl)
    {
        fprintf(stderr, "dwfl_begin failed: %s\n", dwfl_errmsg(-1));
        return -1;
    }

    Dwfl_Module *mod = dwfl_report_offline(dwfl, elf_file, elf_file, -1);
    if (!mod)
    {
        fprintf(stderr, "dwfl_report_offline failed: %s\n", dwfl_errmsg(-1));
        dwfl_end(dwfl);
        return -1;
    }

    dwfl_report_end(dwfl, NULL, NULL);

    Dwarf_Addr bias;
    Dwarf *dbg = dwfl_module_getdwarf(mod, &bias);
    if (!dbg)
    {
        fprintf(stderr, "dwfl_module_getdwarf failed: %s\n", dwfl_errmsg(-1));
        dwfl_end(dwfl);
        return -1;
    }

    *dwfl_out = dwfl;
    *mod_out = mod;
    *dbg_out = dbg;
    *bias_out = bias;
    return 0;
}

/* --------------- Pass 1: collect CUs + range --------------- */

static int collect_compilation_units(Dwarf *dbg, Dwarf_Addr bias,
                                     struct cu_list *cus, uint64_t *lowest_out,
                                     uint64_t *highest_out)
{
    Dwarf_Off cu_offset = 0;
    size_t cu_index = 0;
    uint64_t lowest = UINT64_MAX;
    uint64_t highest = 0;
    int have_addresses = 0;

    // printf("[extractdbg] Collecting compilation units and address
    // ranges...\n");

    while (1)
    {
        Dwarf_Die cu_die;
        size_t cu_header_size = 0;
        Dwarf_Off next_cu_offset = 0;

        if (dwarf_nextcu(dbg, cu_offset, &next_cu_offset, &cu_header_size, NULL,
                         NULL, NULL) != 0)
            break;

        Dwarf_Off die_offset = cu_offset + cu_header_size;
        if (dwarf_offdie(dbg, die_offset, &cu_die) == NULL)
        {
            fprintf(stderr,
                    "[extractdbg] Failed to get CU DIE at offset 0x%llx\n",
                    (unsigned long long)die_offset);
            cu_offset = next_cu_offset;
            continue;
        }

        const char *cu_name = dwarf_diename(&cu_die);
        if (!cu_name || !*cu_name) cu_name = "<unnamed>";
        if (cu_list_append(cus, cu_name) != 0)
        {
            fprintf(stderr, "[extractdbg] Failed to store CU name '%s'\n",
                    cu_name);
            return -1;
        }
        // printf("  CU %zu: %s\n", cu_index, cu_name);

        Dwarf_Lines *lines;
        size_t nlines;
        if (dwarf_getsrclines(&cu_die, &lines, &nlines) != 0)
        {
            cu_offset = next_cu_offset;
            ++cu_index;
            continue;
        }

        for (size_t i = 0; i < nlines; ++i)
        {
            Dwarf_Line *line = dwarf_onesrcline(lines, i);
            Dwarf_Addr addr;
            if (dwarf_lineaddr(line, &addr) != 0) continue;
            addr += bias;

            if (!have_addresses || addr < lowest) lowest = addr;
            if (!have_addresses || addr > highest) highest = addr;
            have_addresses = 1;
        }

        cu_offset = next_cu_offset;
        ++cu_index;
    }

    if (!have_addresses)
    {
        fprintf(stderr, "[extractdbg] No instruction addresses discovered.\n");
        return -1;
    }

    *lowest_out = lowest;
    *highest_out = highest;
    return 0;
}

/* --------------- Instruction table helpers --------------- */

static int allocate_instruction_table(struct debug_info *dbg_info)
{
    dbg_info->instruction_slots = instruction_slot_count(
        dbg_info->lowest_address, dbg_info->highest_address);

    if (dbg_info->instruction_slots == 0)
    {
        fprintf(stderr,
                "[extractdbg] Invalid address range (%" PRIu64 " .. %" PRIu64
                ")\n",
                dbg_info->lowest_address, dbg_info->highest_address);
        return -1;
    }

    dbg_info->instructions =
        calloc(dbg_info->instruction_slots, sizeof(struct instruction));
    if (!dbg_info->instructions)
    {
        perror("[extractdbg] calloc");
        return -1;
    }

    /* Ensure all slots are explicitly zeroed: invalid addresses must have
       file==0 and line==0. calloc already zeros, but be explicit here so
       future changes don't break the invariant. */
    memset(dbg_info->instructions, 0,
           dbg_info->instruction_slots * sizeof(struct instruction));

    return 0;
}

uint16_t get_file_index(const struct cu_list *cus, const char *file_name)
{
    for (size_t i = 0; i < cus->count; ++i)
    {
        if (strcmp(cus->names[i], file_name) == 0)
        {
            return (uint16_t)i;
        }
    }
    return (uint16_t)(-1);
}

/* change signature to accept Dwfl* so we can verify addresses belong to a
   module (heuristic for "valid instruction" addresses) */
static void populate_instruction_table(Dwfl *dwfl, Dwarf *dbg, Dwarf_Addr bias,
                                       const struct cu_list *cus,
                                       struct debug_info *dbg_info)
{
    if (!dbg_info || !dbg_info->instructions ||
        dbg_info->instruction_slots == 0 || !dbg)
        return;

    /* Keep everything zeroed so “invalid” addresses stay file==line==0. */
    memset(dbg_info->instructions, 0,
           dbg_info->instruction_slots * sizeof(struct instruction));

    for (size_t addr = dbg_info->lowest_address;
         addr <= dbg_info->highest_address; addr += 2ULL)
    {
        Dwfl_Module *module = dwfl_addrmodule(dwfl, addr);
        if (!module) continue;
        Dwarf_Die die;
        if (!dwarf_addrdie(dbg, addr - bias, &die)) continue;

        const char *file_name = dwarf_diename(&die);

        Dwarf_Line *line = dwarf_getsrc_die(&die, addr - bias);
        if (!line) continue;

        size_t inst_idx = (size_t)((addr - dbg_info->lowest_address) / 2ULL);
        dbg_info->instructions[inst_idx].file = get_file_index(cus, file_name);
        int line_no;
        dwarf_lineno(line, &line_no);
        dbg_info->instructions[inst_idx].line = line_no;
    }
}

/* --------------- Pass 3: emit --------------- */

static void emit_instructions(const struct debug_info *dbg_info,
                              const struct cu_list *cus)
{
    if (!dbg_info || !dbg_info->instructions) return;

    for (size_t i = 0; i < dbg_info->instruction_slots; ++i)
    {
        if (dbg_info->instructions[i].line == 0) continue;

        uint64_t addr = dbg_info->lowest_address + (uint64_t)i * 2ULL;
        const char *file_name =
            resolve_cu_name(dbg_info->instructions[i].file, cus, dbg_info);
        if (!file_name) file_name = "<invalid file>";

        printf("0x%llx: %s:%u\n", (unsigned long long)addr, file_name,
               dbg_info->instructions[i].line);
    }
}

/* --------------- Debug-info I/O --------------- */

static int write_bytes(FILE *fp, const void *data, size_t size)
{
    return (size && fwrite(data, 1, size, fp) != size) ? -1 : 0;
}

static int read_bytes(FILE *fp, void *data, size_t size)
{
    return (size && fread(data, 1, size, fp) != size) ? -1 : 0;
}

static int save_debug_info(const char *output_file,
                           const struct debug_info *dbg_info)
{
    if (!output_file || !dbg_info) return -1;

    FILE *fp = fopen(output_file, "wb");
    if (!fp)
    {
        fprintf(stderr, "[extractdbg] Failed to open %s for writing: %s\n",
                output_file, strerror(errno));
        return -1;
    }

    struct dbg_file_header hdr = {
        .magic = {0},
        .cu_name_count = dbg_info->cu_name_count,
        .cu_name_list_size = dbg_info->cu_name_list_size,
        .cu_name_offset = dbg_info->cu_name_offset,
        .lowest_address = dbg_info->lowest_address,
        .highest_address = dbg_info->highest_address,
        .instruction_slots = dbg_info->instruction_slots};

    memcpy(hdr.magic, DEBUG_INFO_MAGIC, sizeof(DEBUG_INFO_MAGIC));

    int rc = 0;
    if (write_bytes(fp, &hdr, sizeof(hdr)) != 0 ||
        write_bytes(fp, dbg_info->cu_name_list, dbg_info->cu_name_list_size) !=
            0 ||
        write_bytes(fp, dbg_info->instructions,
                    dbg_info->instruction_slots * sizeof(struct instruction)) !=
            0)
    {
        fprintf(stderr, "[extractdbg] Failed to write debug info: %s\n",
                strerror(errno));
        rc = -1;
    }

    fclose(fp);
    return rc;
}

static int read_debug_info(const char *input_file, struct debug_info *dbg_info)
{
    if (!input_file || !dbg_info) return -1;
    debug_info_init(dbg_info);

    FILE *fp = fopen(input_file, "rb");
    if (!fp)
    {
        fprintf(stderr, "[extractdbg] Failed to open %s: %s\n", input_file,
                strerror(errno));
        return -1;
    }

    struct dbg_file_header hdr;
    if (read_bytes(fp, &hdr, sizeof(hdr)) != 0 ||
        memcmp(hdr.magic, DEBUG_INFO_MAGIC, sizeof(DEBUG_INFO_MAGIC)) != 0)
    {
        fprintf(stderr, "[extractdbg] Invalid debug-info file: %s\n",
                input_file);
        fclose(fp);
        return -1;
    }

    if (hdr.lowest_address > hdr.highest_address)
    {
        fprintf(stderr, "[extractdbg] Corrupt debug-info range\n");
        fclose(fp);
        return -1;
    }

    size_t expected_slots =
        instruction_slot_count(hdr.lowest_address, hdr.highest_address);
    if (hdr.instruction_slots != expected_slots)
    {
        fprintf(stderr, "[extractdbg] Corrupt slot count\n");
        fclose(fp);
        return -1;
    }

    if (hdr.cu_name_list_size > SIZE_MAX || hdr.instruction_slots > SIZE_MAX)
    {
        fprintf(stderr, "[extractdbg] Debug-info too large to load\n");
        fclose(fp);
        return -1;
    }

    dbg_info->cu_name_count = (size_t)hdr.cu_name_count;
    dbg_info->cu_name_list_size = (size_t)hdr.cu_name_list_size;
    dbg_info->cu_name_offset = (size_t)hdr.cu_name_offset;
    dbg_info->lowest_address = hdr.lowest_address;
    dbg_info->highest_address = hdr.highest_address;
    dbg_info->instruction_slots = (size_t)hdr.instruction_slots;

    if (dbg_info->cu_name_list_size)
    {
        dbg_info->cu_name_list = malloc(dbg_info->cu_name_list_size);
        if (!dbg_info->cu_name_list)
        {
            fclose(fp);
            debug_info_free(dbg_info);
            return -1;
        }
        if (read_bytes(fp, dbg_info->cu_name_list,
                       dbg_info->cu_name_list_size) != 0)
        {
            fprintf(stderr, "[extractdbg] Failed to read CU names: %s\n",
                    strerror(errno));
            fclose(fp);
            debug_info_free(dbg_info);
            return -1;
        }
    }

    if (dbg_info->instruction_slots)
    {
        dbg_info->instructions =
            malloc(dbg_info->instruction_slots * sizeof(struct instruction));
        if (!dbg_info->instructions)
        {
            fclose(fp);
            debug_info_free(dbg_info);
            return -1;
        }
        if (read_bytes(
                fp, dbg_info->instructions,
                dbg_info->instruction_slots * sizeof(struct instruction)) != 0)
        {
            fprintf(stderr,
                    "[extractdbg] Failed to read instruction table: %s\n",
                    strerror(errno));
            fclose(fp);
            debug_info_free(dbg_info);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

/* --------------- main --------------- */

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s <ELF file> [output.dbg]\n"
            "  %s --read <debug-info-file>\n",
            prog, prog);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--read") == 0)
    {
        if (argc != 3)
        {
            print_usage(argv[0]);
            return 1;
        }

        struct debug_info dbg_info;
        if (read_debug_info(argv[2], &dbg_info) != 0) return 1;
        emit_instructions(&dbg_info, NULL);
        debug_info_free(&dbg_info);
        return 0;
    }

    if (argc < 2 || argc > 3)
    {
        print_usage(argv[0]);
        return 1;
    }

    const char *elf_file = argv[1];
    const char *output_file = (argc == 3) ? argv[2] : NULL;

    Dwfl *dwfl = NULL;
    Dwfl_Module *mod = NULL;
    Dwarf *dbg = NULL;
    Dwarf_Addr bias = 0;
    if (setup_dwfl(elf_file, &dwfl, &mod, &dbg, &bias) != 0) return 1;

    struct cu_list cus = {0};
    uint64_t lowest_addr = 0;
    uint64_t highest_addr = 0;
    if (collect_compilation_units(dbg, bias, &cus, &lowest_addr,
                                  &highest_addr) != 0)
    {
        cu_list_free(&cus);
        dwfl_end(dwfl);
        return 1;
    }

    struct debug_info dbg_info;
    debug_info_init(&dbg_info);
    dbg_info.lowest_address = lowest_addr;
    dbg_info.highest_address = highest_addr;

    if (build_cu_name_blob(&cus, &dbg_info) != 0 ||
        allocate_instruction_table(&dbg_info) != 0)
    {
        cu_list_free(&cus);
        debug_info_free(&dbg_info);
        dwfl_end(dwfl);
        return 1;
    }

    populate_instruction_table(dwfl, dbg, bias, &cus, &dbg_info);
    // emit_instructions(&dbg_info, &cus);

    if (output_file && save_debug_info(output_file, &dbg_info) != 0)
    {
        fprintf(stderr, "[extractdbg] Failed to save debug info\n");
    }

    cu_list_free(&cus);
    debug_info_free(&dbg_info);
    dwfl_end(dwfl);
    return 0;
}