#include "boot/multiboot2.h"
#include "drivers/acpi.h"
#include "drivers/apic/lapic.h"
#include "drivers/ata.h"
#include "drivers/ps2.h"
#include "arch/x86_64/isr.h"
#include <drivers/pit.h>
#include "mem.h"
#include "ports.h"
#include "terminal/printf.h"
#include <colors.h>
#include <commands.h>
#include <drivers/apic/ioapic.h>
#include <drivers/drives.h>
#include <drivers/pci.h>
#include <drivers/serial.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/irq.h>
#include <drivers/vga.h>
#include <layouts/kb_layouts.h>
#include <mem/paging.h>
#include <mem/physical_mem.h>
#include <net/arp.h>
#include <net/net.h>
#include <stdbool.h>
#include <stdint.h>
#include <terminal/terminal.h>
#include <fs/fs.h>
#include <drivers/hid/keyboard.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <sys/errno.h>
// I think all of these includes are useless, they are there because someone (me) forgot to delete them after finishing them

#define GECKO_VERSION "2.3"

void process_input(unsigned char *buffer) {
    run_command(buffer, TERM_COLOR);
}

void kmain();

#ifdef DEBUG
    extern struct multiboot2_tag_bootloader_name* bootloader_info;
#endif

extern struct multiboot2_mmap_entry max_mem_used;
bool has_apic;

typedef void (*driver_init)();
driver_init drivers[] = {
    terminal_init,
    ata_init,
    enumerate_pci,
    pci_detect_controllers,
    net_init,
    arp_init,
    keyboard_install
};

__attribute__((section(".text.entry"))) void _entry(uint64_t mbi) {
    initialize_memory_manager_from_mbi(mbi);
    kalloc_init(max_mem_used.base_addr + 0x100000, max_mem_used.length);

    if (!vmm_init()) {
        printc("Vmm_init failed -- halting\n", VGA_COLOR_RED);
        for (;;)
            asm volatile("hlt");
    }

    // Setting up interrupts

    has_apic = cpu_has_apic();

    int ret = acpi_init();
    if (ret != 0) {
        set_printf_color(VGA_COLOR_RED);
            printf("Initializing apic failed: %d \n", ret);
        set_printf_color(VGA_COLOR_WHITE);
    }

    init_idt();
    irq_install();

    if (has_apic) {
        asm volatile("cli"); // cutting interrupts while we set em up
        ret = lapic_init();
        if (ret) {
            printf("Setting up LAPIC failed err %d", ret);
        }
        ioapic_init();
        asm volatile("sti"); // repoening interrupts

        start_pit_timer(PITHZ);
        lapic_timer_start();
        lapic_start_cores();
    }

    printc("Enabling some drivers\n", VGA_COLOR_LIGHT_GREY);
    for (int i = 0; i < (sizeof(drivers) / sizeof(drivers[0])); i++) drivers[i]();
    if (has_ps2mouse_support) mouse_init();

    register_interrupt_handler(INT_PAGEFAULT, page_fault);
    register_interrupt_handler(INT_INVINS, ud_exception_handler);
    outb(0x22, 0x70); // I think this is support for Cyrix processors
    outb(0x23, 0x01);

    set_layout(PS2_LAYOUTS[0]);

    #ifndef DEBUG
        terminal_clear(TERM_COLOR);
    #else
        printf("\n");
    #endif

    printf("GeckoOS Version %s\n", GECKO_VERSION);
    #ifdef DEBUG
        printf("Booted via %s/Multiboot2.\n", bootloader_info->string);
    #else
        printc("Booted via GRUB/Multiboot2.\n", TERM_COLOR);
    #endif

    kmain();
}

void kmain() {
    for (int i = 0; i < (sizeof(drives) / sizeof(drives[0])); i++) {
        if (!drives[i].sector_size) continue;
        fsmount(i);
    }

    // Adds the first drive's filesystem to the virtual filesystems array
    vfs_add(fss[0]);

    // Adds the devfs to the virtual filesystems array
    vfs_devfs_init();
    vfs_add_vfs(create_devfs());

    // Set the first virtual filesystem as the root one
    vfs_set_root_(get_vfs_(0));

    vfs_mkdir_path("/dev");
    vfs_mount("/dev", get_vfs_(1));

    // vfs_mkdir_path("/dev/dir1");
    // vfs_mkdir_path("/dev/dir2");
    // vfs_mkdir_path("/dev/dir2/dir3");

    while (1) {
        printc("gecko> ", PROMPT_COLOR);
        unsigned char buff[512];
        input(buff, 512, TERM_COLOR);
        process_input(buff);
    }
}