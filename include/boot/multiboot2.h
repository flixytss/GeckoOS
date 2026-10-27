#pragma once
#include "sys/types.h"


/** Written into the Multiboot2 header embedded in the kernel image. */
#define MULTIBOOT2_HEADER_MAGIC     UINT32_C(0xE85250D6)

/** Architecture field value for 32-bit protected mode (i386). */
#define MULTIBOOT2_ARCH_I386        UINT32_C(0)

#define MULTIBOOT2_BOOTLOADER_MAGIC UINT32_C(0x36D76289)

#define MULTIBOOT2_HEADER_TAG_END               0
#define MULTIBOOT2_HEADER_TAG_INFORMATION_REQUEST 1
#define MULTIBOOT2_HEADER_TAG_ADDRESS           2
#define MULTIBOOT2_HEADER_TAG_ENTRY_ADDRESS     3
#define MULTIBOOT2_HEADER_TAG_CONSOLE_FLAGS     4
#define MULTIBOOT2_HEADER_TAG_FRAMEBUFFER       5
#define MULTIBOOT2_HEADER_TAG_MODULE_ALIGN      6

#define MULTIBOOT2_TAG_TYPE_END             0
#define MULTIBOOT2_TAG_TYPE_CMDLINE         1
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT2_TAG_TYPE_MODULE          3
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO   4
#define MULTIBOOT2_TAG_TYPE_BOOTDEV         5
#define MULTIBOOT2_TAG_TYPE_MMAP            6
#define MULTIBOOT2_TAG_TYPE_VBE             7
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER     8
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS    9
#define MULTIBOOT2_TAG_TYPE_APM             10
#define MULTIBOOT2_TAG_TYPE_EFI32           11
#define MULTIBOOT2_TAG_TYPE_EFI64           12
#define MULTIBOOT2_TAG_TYPE_SMBIOS          13
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD        14  /* RSDP v1 */
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW        15  /* RSDP v2 */

/* ── Memory map entry types ──────────────────────────────────────────────── */

#define MULTIBOOT2_MEMORY_AVAILABLE         1
#define MULTIBOOT2_MEMORY_RESERVED          2
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE  3
#define MULTIBOOT2_MEMORY_NVS               4
#define MULTIBOOT2_MEMORY_BADRAM            5

typedef struct multiboot2_info_header {
    u32 total_size;
    u32 reserved;
} __attribute__((packed)) multiboot2_info_header_t;

/** Every tag in the MBI begins with this common prefix. */
typedef struct multiboot2_tag {
    u32 type;
    u32 size;
} __attribute__((packed)) multiboot2_tag_t;

/** MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO */
typedef struct multiboot2_tag_basic_meminfo {
    u32 type;
    u32 size;
    u32 mem_lower;   /**< Kilobytes of lower memory (below 1 MB). */
    u32 mem_upper;   /**< Kilobytes of upper memory (above 1 MB). */
} __attribute__((packed)) multiboot2_tag_basic_meminfo_t;

/** One entry in the MULTIBOOT2_TAG_TYPE_MMAP tag. */
typedef struct multiboot2_mmap_entry {
    u64 base_addr;
    u64 length;
    u32 type;        /**< One of MULTIBOOT2_MEMORY_* */
    u32 reserved;
} __attribute__((packed)) multiboot2_mmap_entry_t;

/** MULTIBOOT2_TAG_TYPE_MMAP */
typedef struct multiboot2_tag_mmap {
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
    struct multiboot2_mmap_entry entries[];
    /* Followed by (size - 16) / entry_size entries of type multiboot2_mmap_entry_t */
} __attribute__((packed)) multiboot2_tag_mmap_t;

/** MULTIBOOT2_TAG_TYPE_CMDLINE */
typedef struct multiboot2_tag_cmdline {
    u32 type;
    u32 size;
    char     string[];    /**< Null-terminated command line string. */
} __attribute__((packed)) multiboot2_tag_cmdline_t;

/** MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME */
typedef struct multiboot2_tag_bootloader_name {
    u32 type;
    u32 size;
    char     string[];
} __attribute__((packed)) multiboot2_tag_bootloader_name_t;

/** MULTIBOOT2_TAG_TYPE_FRAMEBUFFER */
typedef struct multiboot2_tag_framebuffer {
    u32 type;
    u32 size;
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8  framebuffer_bpp;
    u8  framebuffer_type;
    u16 reserved;
} __attribute__((packed)) multiboot2_tag_framebuffer_t;

/** MULTIBOOT2_TAG_TYPE_ACPI_OLD / MULTIBOOT2_TAG_TYPE_ACPI_NEW */
typedef struct multiboot2_tag_acpi {
    u32 type;
    u32 size;
    u8  rsdp[];      /**< Raw RSDP bytes (8 bytes for v1, 36 for v2). */
} __attribute__((packed)) multiboot2_tag_acpi_t;

/**
 * Advance a tag pointer to the next tag.
 * Multiboot2 tags are 8-byte aligned relative to the MBI base.
 */
#define MULTIBOOT2_TAG_NEXT(tag) \
    ((multiboot2_tag_t *)(((uintptr_t)(tag) + (tag)->size + 7) & ~UINT32_C(7)))
