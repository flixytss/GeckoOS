#include "ports.h"
#include <boot/multiboot2.h>
#include <drivers/framebuffer.h>
#include <sys/types.h>
#include <terminal/printf.h>

#define VGA_COLOR_BLACK     0
#define VGA_COLOR_LIGHT_RED 12

#define MULTIBOOT1_BOOTLOADER_MAGIC 0x2BADB002

u64 g_mbi_addr         = 0;
u64 g_multiboot2_magic = 0;

u64 g_fb_addr;
u32 g_fb_width;
u32 g_fb_height;
u32 g_fb_pitch;
u8  g_fb_bpp;

struct multiboot2_tag_acpi* acpi_info_grub = NULL;
u64 max_addr_used     = 0; // To map physical memory
struct multiboot2_mmap_entry max_mem_used; // For the heap

#ifdef DEBUG
    struct multiboot2_tag_bootloader_name* bootloader_info;
#endif

extern void _entry(u64);

void multiboot2_main(u64 magic, u64 mbi_addr)
{
    g_multiboot2_magic = magic;
    g_mbi_addr         = mbi_addr;

    int valid = (magic == MULTIBOOT2_BOOTLOADER_MAGIC);

    if (!valid)
        for (;;) HALT();

    multiboot2_info_header_t *mbi = (multiboot2_info_header_t *)(uintptr_t)mbi_addr;
    multiboot2_tag_t *tag = (multiboot2_tag_t *)((uintptr_t)mbi + sizeof(multiboot2_info_header_t));

    struct multiboot2_tag_framebuffer *fb = NULL;
    struct multiboot2_tag_mmap *mmap_tag = NULL;
    struct multiboot2_mmap_entry *entry = NULL;

    for (;;) {
        switch (tag->type) {
            case MULTIBOOT2_TAG_TYPE_FRAMEBUFFER:
                fb = (struct multiboot2_tag_framebuffer *)tag;
                g_fb_addr   = fb->framebuffer_addr;
                g_fb_width  = fb->framebuffer_width;
                g_fb_height = fb->framebuffer_height;
                g_fb_pitch  = fb->framebuffer_pitch;
                g_fb_bpp    = fb->framebuffer_bpp;

                fb_init(g_fb_addr, g_fb_width, g_fb_height, g_fb_pitch, g_fb_bpp);
                break;
            case MULTIBOOT2_TAG_TYPE_ACPI_NEW:
            case MULTIBOOT2_TAG_TYPE_ACPI_OLD:
                acpi_info_grub = ((struct multiboot2_tag_acpi*)tag);
                break;
            #ifdef DEBUG
                case MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME:
                    bootloader_info = ((struct multiboot2_tag_bootloader_name*)tag);
                    break;
            #endif
            case MULTIBOOT2_TAG_TYPE_MMAP:
                mmap_tag = (struct multiboot2_tag_mmap *)tag;
                entry  = mmap_tag->entries;

                while ((uintptr_t)entry < (uintptr_t)tag + tag->size) {
                    if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE) {                            
                        if (entry->length > max_mem_used.length) {
                            max_mem_used = *entry;
                            max_addr_used = entry->base_addr + entry->length;
                        }
                    }
                    entry = (multiboot2_mmap_entry_t *)((uintptr_t)entry +
                                                        mmap_tag->entry_size);
                } break;
            case MULTIBOOT2_TAG_TYPE_END: goto exit;
        }
        tag = MULTIBOOT2_TAG_NEXT(tag);
    }
exit:
    _entry(mbi_addr);
}