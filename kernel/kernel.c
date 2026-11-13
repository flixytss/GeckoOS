#include "boot/multiboot2.h"
#include "drivers/acpi/acpi-driver.h"
#include "drivers/apic/lapic.h"
#include "drivers/ps2.h"
#include "arch/x86_64/isr.h"
#include <drivers/pit.h>
#include "mem/mmu.h"
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
#include "sys/types.h"
#include <terminal/terminal.h>
#include <fs/fs.h>
#include <drivers/hid/keyboard.h>
#include <fs/vfs.h>
#include <fs/types/devfs.h>
#include <sys/errno.h>
#ifdef DEBUG
#   ifdef ACPI_ENABLED
#       include "drivers/acpi/acpi.h"
#   endif
#endif

#define GECKO_VERSION "26.03"

void process_input(unsigned char *buffer)
    { run_command(buffer, TERM_COLOR); }

void kmain();

#ifdef DEBUG
    extern struct multiboot2_tag_bootloader_name* bootloader_info;
#endif

extern struct multiboot2_mmap_entry max_mem_used;
bool has_apic;

typedef void (*driver_init)();
driver_init drivers[] = {
    terminal_init,
    enumerate_pci,
    pci_detect_controllers,
    net_init,
    arp_init,
    keyboard_install
};

__attribute__((section(".text.entry"))) void _entry(u64 mbi) {
    initialize_memory_manager_from_mbi(mbi);
    kalloc_init(max_mem_used.base_addr + 0x100000, max_mem_used.length);

    if (!vmm_init()) {
        printc("Vmm_init failed -- halting\n", VGA_COLOR_RED);
        for (;;) HALT();
    }

    has_apic = cpu_has_apic();
    global_acpi = acpi_init();

    init_idt();
    irq_install();

    if (has_apic) {
        CLI();
        if (lapic_init())
            printf("Setting up LAPIC failed.");
        
        ioapic_init();
        STI();

        start_pit_timer(PITHZ);
        lapic_timer_start();
        lapic_start_cores();

        #ifdef DEBUG
            #ifdef ACPI_ENABLED
                printc("Enabling ACPI\n", VGA_COLOR_LIGHT_GREY);

                acpi_enable(global_acpi); // Takes 3 seconds
                acpi_aml_init(global_acpi);
            #endif
        #endif
    }

    register_interrupt_handler(INT_PAGEFAULT, page_fault);
    register_interrupt_handler(INT_INVINS, ud_exception_handler);
    
    for (int i = 0; i < (sizeof(drivers) / sizeof(drivers[0])); i++) drivers[i]();
    if (has_ps2mouse_support) mouse_init();

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
    int i;
    for (i = 0; i < 32; i++) { // Note: Hardcode size
        if (!get_kdrive(i)) continue;
        fsmount(i);
    }

    if (!fss[actual_fs]) {
        printf("There is no filesystem available, Exploding...\n");
    }

    // Adds the first drive partition's filesystem to the virtual filesystems array
    vfs_add_fs(fss[actual_fs]);

    // Adds the devfs to the virtual filesystems array
    vfs_devfs_init();
    vfs_add_vfs(create_devfs());

    // Set the first virtual filesystem as the root one
    vfs_set_root_(get_vfs_(0));

    // Create /dev in the main filesystem and mount devfs there
    vfs_mkdir_path("/dev");
    vfs_mount("/dev", get_vfs_(2));

    // Adds every partition to its own directory
    struct kdrive_t* drive = get_kdrive(0);
    for (int i = 0; i < drive->partitions.count; i++) {
        char path[512];
        npf_snprintf(path, 512, "P%s", drive->partitions.partitions->sysname);

        vfs_mkdir_path(path);
        vfs_mount(path, get_vfs_(vfs_add_fs(fs_partition_open(drive, &drive->partitions.partitions[i]))));
    }

    while (1) {
        printc("gecko> ", PROMPT_COLOR);
        unsigned char buff[512];
        input(buff, 512, TERM_COLOR);
        process_input(buff);
    }
}
