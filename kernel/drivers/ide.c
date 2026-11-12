#include "drivers/drives.h"
#include "drivers/pit.h"
#include "mem/mmu.h"
#include "ports.h"
#include "drivers/ide.h"
#include "sys/types.h"
#include "terminal/printf.h"
#include "sys/std.h"

static u8 ide_buf[2048];
volatile unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8};

struct IDEChannelRegisters {
    u16 base;  // I/O Base.
    u16 ctrl;  // Control Base
    u16 bmide; // Bus Master IDE
    u8  nIEN;  // nIEN (No Interrupt);
} channels[2];

struct ide_device {
    u8  Reserved;    // If 1, there is a drive here
    u8  Channel;     // Primary channel or secundary channel
    u8  Drive;       // Master drive or slave drive
    u16 Type;        // ATA or ATAPI
    u16 Signature;
    u16 Capabilities;
    u32 CommandSets;
    u32 Size;        // Size in sector
    u8  Model[41];
} ide_devices[4];

void ide_write(u8 channel, u8 reg, u8 data) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        outb(channels[channel].base  + reg - 0x00, data);
    else if (reg < 0x0C)
        outb(channels[channel].base  + reg - 0x06, data);
    else if (reg < 0x0E)
        outb(channels[channel].ctrl  + reg - 0x0A, data);
    else if (reg < 0x16)
        outb(channels[channel].bmide + reg - 0x0E, data);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

u8 ide_read(u8 channel, u8 reg) {
    u8 result;
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        result = inb(channels[channel].base + reg - 0x00);
    else if (reg < 0x0C)
        result = inb(channels[channel].base  + reg - 0x06);
    else if (reg < 0x0E)
        result = inb(channels[channel].ctrl  + reg - 0x0A);
    else if (reg < 0x16)
        result = inb(channels[channel].bmide + reg - 0x0E);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
    return result;
}

void ide_read_buffer(u8 channel, u8 reg, u32 buffer, u32 quads) {
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    if (reg < 0x08)
        insl(channels[channel].base  + reg - 0x00, (void*)(size_t)buffer, quads);
    else if (reg < 0x0C)
        insl(channels[channel].base  + reg - 0x06, (void*)(size_t)buffer, quads);
    else if (reg < 0x0E)
        insl(channels[channel].ctrl  + reg - 0x0A, (void*)(size_t)buffer, quads);
    else if (reg < 0x16)
        insl(channels[channel].bmide + reg - 0x0E, (void*)(size_t)buffer, quads);
    if (reg > 0x07 && reg < 0x0C)
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

u8 ide_polling(u8 channel, u8 advanced_check) {
    /*
        Checks for errors
        Meaning of returned errors;

        0: No error
        1: Device fault
        2: General error
        3: DRQ is not set

        A timeout will be treated as a device fault error
    */

    int i;
    int timeout;
    u8 state;

    /*
        Waiting for BSY to be set, around 400ns
        Reading the alternate status port wastes 100ns
    */
    for(i = 0; i < 4; i++) ide_read(channel, ATA_REG_ALTSTATUS);

    timeout = 100;
    while ((ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) && (--timeout))
        pit_timer_wait_ms(1);
    if (!timeout)
        return 1;

    if (advanced_check) {
        state = ide_read(channel, ATA_REG_STATUS);

        if (state & ATA_SR_ERR)
            return 2;
        if (state & ATA_SR_DF)
            return 1;
        if ((state & ATA_SR_DRQ) == 0)
            return 3;
    }

    return 0;

}

u8 ide_match_error(u32 drive, u8 err) {
    /*
        See ide_polling for more information
        about errors
    */
    u8 st;

    switch (err) {
        case 1: return 19;                   // "Device Fault"
        case 2:
            st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);

            if (st & ATA_ER_AMNF)  err = 7;  // "No Address Mark Found"
            if (st & ATA_ER_TK0NF) err = 3;  // "No Media or Media Error"
            if (st & ATA_ER_ABRT)  err = 20; // "Command Aborted"
            if (st & ATA_ER_MCR)   err = 3;  // "No Media or Media Error"
            if (st & ATA_ER_IDNF)  err = 21; // "ID mark not Found"
            if (st & ATA_ER_MC)    err = 3;  // "No Media or Media Error"
            if (st & ATA_ER_UNC)   err = 22; // "Uncorrectable Data Error"
            if (st & ATA_ER_BBK)   err = 13; // "Bad Sectors"
            break;
        case 3:
            err = 23;                        // "Reads Nothing"
            break;
        case 4:
            err = 8;                         // "Write Protected"
            break;
    }

    return err;
}

static long ide_drives_read(struct kdrive_t* drive, u64 lba, size_t count, u8* buffer)
    { return ide_ata_access(ATA_READ, drive->userdata1, lba, count, 0, (u32)(size_t)buffer); }
static long ide_drives_write(struct kdrive_t* drive, u64 lba, size_t count, const u8* buffer)
    { return ide_ata_access(ATA_WRITE, drive->userdata1, lba, count, 0, (u32)(size_t)buffer); }

void ide_init(u32 BAR0, u32 BAR1, u32 BAR2, u32 BAR3, u32 BAR4) {
    int i, j, k, count = 0;

    // Detect I/O Ports which interface IDE Controller
    channels[ATA_PRIMARY  ].base  = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
    channels[ATA_PRIMARY  ].ctrl  = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
    channels[ATA_SECONDARY].base  = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
    channels[ATA_SECONDARY].ctrl  = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
    channels[ATA_PRIMARY  ].bmide = (BAR4 & 0xFFFFFFFC) + 0; // Bus Master IDE
    channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE

    // Disable IRQs
    ide_write(ATA_PRIMARY  , ATA_REG_CONTROL, 2);
    ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

    // Looking for devices
    for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++) {
            u8 err = 0, type = IDE_ATA, status;
            ide_devices[count].Reserved = 0; // Assuming that no drive here

            ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive
            pit_timer_wait_ms(1);

            ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY); // Identify command
            pit_timer_wait_ms(1);

            if (ide_read(i, ATA_REG_STATUS) == 0) continue; // Status will be 0 if there is no device

            while (1) {
                status = ide_read(i, ATA_REG_STATUS);
                if (status & ATA_SR_ERR) {
                    err = 1; // err = 1 means the device is not ATA
                    break;
                }
                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break; // The device is fine
            }

            if (err == 1) { // Check if the device is ATAPI
                u8 cl = ide_read(i, ATA_REG_LBA1);
                u8 ch = ide_read(i, ATA_REG_LBA2);

                if (cl == 0x14 && ch == 0xEB) type = IDE_ATAPI;
                else if (cl == 0x69 && ch == 0x96) type = IDE_ATAPI;
                else continue; // Unknown Type

                ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                pit_timer_wait_ms(1);
            }

            ide_read_buffer(i, ATA_REG_DATA, (u32)(size_t)ide_buf, 128);

            ide_devices[count].Reserved     = 1;
            ide_devices[count].Type         = type;
            ide_devices[count].Channel      = i;
            ide_devices[count].Drive        = j;
            ide_devices[count].Signature    = *((u16*)(ide_buf + ATA_IDENT_DEVICETYPE));
            ide_devices[count].Capabilities = *((u16*)(ide_buf + ATA_IDENT_CAPABILITIES));
            ide_devices[count].CommandSets  = *((u32*)(ide_buf + ATA_IDENT_COMMANDSETS));

            if (ide_devices[count].CommandSets & (1 << 26))
                ide_devices[count].Size   = *((u32 *)(ide_buf + ATA_IDENT_MAX_LBA_EXT)); // Device uses 48-Bit Addressing
            else
                ide_devices[count].Size   = *((u32 *)(ide_buf + ATA_IDENT_MAX_LBA)); // Device uses CHS or 28-bit Addressing

            memset(ide_devices[count].Model, 0, sizeof(ide_devices[count].Model));
            for(k = 0; k < 40; k += 2) {
                ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
                ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
            }

            count++;
        }

    for (i = 0; i < 4; i++)
        if (ide_devices[i].Reserved) {
            struct kdrive_t* drive = kmalloc(sizeof(struct kdrive_t));
            memset(drive, 0, sizeof(*drive));

            if (ide_devices[i].Size) {
                npf_snprintf_(drive->name, 64, "IDE Drive %d - %s", i, ide_devices[i].Model);
                drive->userdata1 = i;
                drive->read = ide_drives_read;
                drive->write = ide_drives_write;
                drive->sector_size = 512; // Hardcoded sector size
                register_kdrive(drive);
            }

            #ifdef DEBUG
                printf("    Found %s Drive[%d] %d%s - %s\n",
                    (const char *[]){"ATA", "ATAPI"}[ide_devices[i].Type], /* Type */
                    i, // Very unoptimized
                    (ide_devices[i].Size / 1024 / 1024 / 2) ? (ide_devices[i].Size / 1024 / 1024 / 2) : (ide_devices[i].Size / 1024 / 2), /* Size */
                    (ide_devices[i].Size / 1024 / 1024 / 2) ? "GB" : "MB",
                    ide_devices[i].Model);
            #endif
        }
}
static void ide_sleep(u8 drive) {
    ide_write(ide_devices[drive].Channel, ATA_REG_HDDEVSEL, 0xA0 | (ide_devices[drive].Drive << 4));
    pit_timer_wait_ms(1);

    ide_write(ide_devices[drive].Channel, ATA_REG_COMMAND, ATA_CMD_SLEEP);
    pit_timer_wait_ms(1);
}

u8 ide_ata_access(u8 direction, u8 drive, u32 lba, u8 numsects, u16 selector, u32 edi) {
    u8 lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */, dma /* 0: No DMA, 1: DMA */, cmd;
    u8 lba_io[6];
    u32  channel      = ide_devices[drive].Channel; // Read the Channel.
    u32  slavebit      = ide_devices[drive].Drive; // Read the Drive [Master/Slave]
    u32  bus = channels[channel].base; // Bus Base, like 0x1F0 which is also data port.
    u32  words      = 256; // Almost every ATA drive has a sector-size of 512-byte.
    u16 cyl, i;
    u8 head, sect, err;

    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = (ide_irq_invoked = 0x0) + 0x02);

    if (lba >= 0x10000000) { // Sure Drive should support LBA in this case, or you are
                                // giving a wrong LBA.
        // LBA48:
        lba_mode  = 2;
        lba_io[0] = (lba & 0x000000FF) >> 0;
        lba_io[1] = (lba & 0x0000FF00) >> 8;
        lba_io[2] = (lba & 0x00FF0000) >> 16;
        lba_io[3] = (lba & 0xFF000000) >> 24;
        lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
        head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
    } else if (ide_devices[drive].Capabilities & 0x200)  { // Drive supports LBA?
        // LBA28:
        lba_mode  = 1;
        lba_io[0] = (lba & 0x00000FF) >> 0;
        lba_io[1] = (lba & 0x000FF00) >> 8;
        lba_io[2] = (lba & 0x0FF0000) >> 16;
        lba_io[3] = 0; // These Registers are not used here.
        lba_io[4] = 0; // These Registers are not used here.
        lba_io[5] = 0; // These Registers are not used here.
        head      = (lba & 0xF000000) >> 24;
    } else {
        // CHS:
        lba_mode  = 0;
        sect      = (lba % 63) + 1;
        cyl       = (lba + 1  - sect) / (16 * 63);
        lba_io[0] = sect;
        lba_io[1] = (cyl >> 0) & 0xFF;
        lba_io[2] = (cyl >> 8) & 0xFF;
        lba_io[3] = 0;
        lba_io[4] = 0;
        lba_io[5] = 0;
        head      = (lba + 1  - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
    }
    dma = 0; // We don't support DMA

    while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) HALT();

    if (lba_mode == 0)
        ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head); // Drive & CHS.
    else
        ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head); // Drive & LBA

    // (V) Write Parameters;
    if (lba_mode == 2) {
        ide_write(channel, ATA_REG_SECCOUNT1,   0);
        ide_write(channel, ATA_REG_LBA3,   lba_io[3]);
        ide_write(channel, ATA_REG_LBA4,   lba_io[4]);
        ide_write(channel, ATA_REG_LBA5,   lba_io[5]);
    }
    ide_write(channel, ATA_REG_SECCOUNT0,   numsects);
    ide_write(channel, ATA_REG_LBA0,   lba_io[0]);
    ide_write(channel, ATA_REG_LBA1,   lba_io[1]);
    ide_write(channel, ATA_REG_LBA2,   lba_io[2]);

    if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;   
    if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;   
    if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
    if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
    if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
    if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
    if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
    ide_write(channel, ATA_REG_COMMAND, cmd);  

    if (dma)
        if (direction == 0);
            // DMA Read.
        else;
            // DMA Write.
    else
        if (direction == 0)
            // PIO Read.
        for (i = 0; i < numsects; i++) {
            if ((err = ide_polling(channel, 1)))
                return err; // Polling, set error and exit if there is.
            // asm("pushw %es");
            asm("mov %%ax, %%es" : : "a"(selector));
            asm("cld; rep insw" : : "c"(words), "d"(bus), "D"(edi)); // Receive Data.
            // asm("popw %es");
            edi += (words*2);
        } else {
        // PIO Write.
            for (i = 0; i < numsects; i++) {
                ide_polling(channel, 0); // Polling.
                // asm("pushw %ds");
                asm("mov %%ax, %%ds"::"a"(selector));
                asm("rep outsw"::"c"(words), "d"(bus), "S"(edi)); // Send Data
                // asm("popw %ds");
                edi += (words*2);
            }
            ide_write(channel, ATA_REG_COMMAND, (char []) {   ATA_CMD_CACHE_FLUSH,
                            ATA_CMD_CACHE_FLUSH,
                            ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
            ide_polling(channel, 0); // Polling.
        }

    return numsects; // Easy, isn't it?
}

void ide_wait_irq() {
    while (!ide_irq_invoked) HALT();
    ide_irq_invoked = 0;
}
void ide_irq() {
    ide_irq_invoked = 1;
}

u8 ide_atapi_read(u8 drive, u32 lba, u8 numsects, u16 selector, u32 edi) {
    u32   channel  = ide_devices[drive].Channel;
    u32   slavebit = ide_devices[drive].Drive;
    u32   bus      = channels[channel].base;
    u32   words    = 1024; // Sector Size. ATAPI drives have a sector size of 2048 bytes.
    u8  err;
    int i;

    ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = ide_irq_invoked = 0x0);

    atapi_packet[ 0] = ATAPI_CMD_READ;
    atapi_packet[ 1] = 0x0;
    atapi_packet[ 2] = (lba >> 24) & 0xFF;
    atapi_packet[ 3] = (lba >> 16) & 0xFF;
    atapi_packet[ 4] = (lba >> 8) & 0xFF;
    atapi_packet[ 5] = (lba >> 0) & 0xFF;
    atapi_packet[ 6] = 0x0;
    atapi_packet[ 7] = 0x0;
    atapi_packet[ 8] = 0x0;
    atapi_packet[ 9] = numsects;
    atapi_packet[10] = 0x0;
    atapi_packet[11] = 0x0;

    ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);

    for (int i = 0; i < 4; i++) ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns.

    ide_write(channel, ATA_REG_FEATURES, 0); // PIO mode.

    ide_write(channel, ATA_REG_LBA1, (words * 2) & 0xFF);   // Lower Byte of Sector Size.
    ide_write(channel, ATA_REG_LBA2, (words * 2) >> 8);   // Upper Byte of Sector Size.

    ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);      // Send the Command.

    // (VII): Waiting for the driver to finish or return an error code:
    // ------------------------------------------------------------------
    if ((err = ide_polling(channel, 1))) return err;         // Polling and return if error.

    // (VIII): Sending the packet data:
    // ------------------------------------------------------------------
    asm("rep   outsw" : : "c"(6), "d"(bus), "S"(atapi_packet));   // Send Packet Data

    for (i = 0; i < numsects; i++) {
        ide_wait_irq();                  // Wait for an IRQ.
        if ((err = ide_polling(channel, 1)))
            return err;      // Polling and return if error.
        // asm("pushw %es");
        asm("mov %%ax, %%es"::"a"(selector));
        asm("rep insw"::"c"(words), "d"(bus), "D"(edi));// Receive Data.
        // asm("popw %es");
        edi += (words * 2);
    }
    // (X): Waiting for an IRQ:
    // ------------------------------------------------------------------
    ide_wait_irq();

    // (XI): Waiting for BSY & DRQ to clear:
    while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ)) HALT();

    return 0; // Easy, ... Isn't it?
}

u8 package[1];

void ide_read_sectors(u8 drive, u8 numsects, u32 lba, u16 es, u32 edi) {
    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0) package[0] = 0x1;      // Drive Not Found!

    // 2: Check if inputs are valid:
    // ==================================
    else if (((lba + numsects) > ide_devices[drive].Size) && (ide_devices[drive].Type == IDE_ATA))
        package[0] = 0x2;                     // Seeking to invalid position.

    // 3: Read in PIO Mode through Polling & IRQs:
    // ============================================
    else {
        u8 err;
        if (ide_devices[drive].Type == IDE_ATA)
            err = ide_ata_access(ATA_READ, drive, lba, numsects, es, edi);
        else if (ide_devices[drive].Type == IDE_ATAPI)
            for (int i = 0; i < numsects; i++)
                err = ide_atapi_read(drive, lba + i, 1, es, edi + (i*2048));
        package[0] = ide_match_error(drive, err);
    }
}
// package[0] is an entry of an array. It contains the Error Code.

void ide_write_sectors(u8 drive, u8 numsects, u32 lba, u16 es, u32 edi) {

    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0)
        package[0] = 0x1;      // Drive Not Found!
    // 2: Check if inputs are valid:
    // ==================================
    else if (((lba + numsects) > ide_devices[drive].Size) && (ide_devices[drive].Type == IDE_ATA))
        package[0] = 0x2;                     // Seeking to invalid position.
    // 3: Read in PIO Mode through Polling & IRQs:
    // ============================================
    else {
        u8 err;
        if (ide_devices[drive].Type == IDE_ATA)
            err = ide_ata_access(ATA_WRITE, drive, lba, numsects, es, edi);
        else if (ide_devices[drive].Type == IDE_ATAPI)
            err = 4; // Write-Protected.
        package[0] = ide_match_error(drive, err);
    }
}

void ide_atapi_eject(u8 drive) {
    u32   channel      = ide_devices[drive].Channel;
    u32   slavebit      = ide_devices[drive].Drive;
    u32   bus      = channels[channel].base;
    u32   words      = 2048 / 2;               // Sector Size in Words.
    u8  err = 0;
    ide_irq_invoked = 0;

    // 1: Check if the drive presents:
    // ==================================
    if (drive > 3 || ide_devices[drive].Reserved == 0)
        package[0] = 0x1;      // Drive Not Found!
    // 2: Check if drive isn't ATAPI:
    // ==================================
    else if (ide_devices[drive].Type == IDE_ATA)
        package[0] = 20;         // Command Aborted.
    // 3: Eject ATAPI Driver:
    // ============================================
    else {
        // Enable IRQs:
        ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = ide_irq_invoked = 0x0);

        // (I): Setup SCSI Packet:
        // ------------------------------------------------------------------
        atapi_packet[ 0] = ATAPI_CMD_EJECT;
        atapi_packet[ 1] = 0x00;
        atapi_packet[ 2] = 0x00;
        atapi_packet[ 3] = 0x00;
        atapi_packet[ 4] = 0x02;
        atapi_packet[ 5] = 0x00;
        atapi_packet[ 6] = 0x00;
        atapi_packet[ 7] = 0x00;
        atapi_packet[ 8] = 0x00;
        atapi_packet[ 9] = 0x00;
        atapi_packet[10] = 0x00;
        atapi_packet[11] = 0x00;

        // (II): Select the Drive:
        // ------------------------------------------------------------------
        ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);

        // (III): Delay 400 nanosecond for select to complete:
        // ------------------------------------------------------------------
        for(int i = 0; i < 4; i++)
            ide_read(channel, ATA_REG_ALTSTATUS); // Reading Alternate Status Port wastes 100ns.

        // (IV): Send the Packet Command:
        // ------------------------------------------------------------------
        ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);      // Send the Command.

        // (V): Waiting for the driver to finish or invoke an error:
        // ------------------------------------------------------------------
        err = ide_polling(channel, 1);            // Polling and stop if error.
        if (err) return;

        // (VI): Sending the packet data:
        // ------------------------------------------------------------------
        {
            asm("rep   outsw"::"c"(6), "d"(bus), "S"(atapi_packet));// Send Packet Data
            ide_wait_irq();                  // Wait for an IRQ.
            err = ide_polling(channel, 1);            // Polling and get error code.
            if (err == 3) err = 0; // DRQ is not needed here.
        }
        package[0] = ide_match_error(drive, err); // Return;
    }
}