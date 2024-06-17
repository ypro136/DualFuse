
struct multiboot_aout_symbol_table
{
    uint32_t tab_size;
    uint32_t string_size;
    uint32_t address;
    uint32_t reserved;
};

struct multiboot_elf_section_header_table
{
    uint32_t number;
    uint32_t size;
    uint32_t address;
    uint32_t shndx;
};

struct multiboot_information{
    uint32_t flags;
    uint32_t memory_lower;
    uint32_t memory_upper;
    uint32_t boot_device;

    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_address;

    union{
        struct multiboot_aout_symbol_table aout_sym;
        struct multiboot_elf_section_header_table elf_sec;
    } u;

    uint32_t memory_map_length;
    uint32_t memory_map_address;

    uint32_t drives_length;
    uint32_t drives_address;

    uint32_t config_table;
    uint32_t boot_loader_name;

    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
};

struct multiboot_memory_map_entry
{
  uint32_t size;
  uint32_t address_low;
  uint32_t address_high;
  uint32_t length_low;
  uint32_t length_high;
#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5
  uint32_t type;
} __attribute__((packed));



