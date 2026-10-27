//Ember2819, with light assistance from Claude Sonnet 4.6 on debugging.
// All code was manually reviewed and tested.
#include "terminal/printf.h"
#include <drivers/ata.h>
#include <ports.h>
#include "sys/types.h"
#include <drivers/drives.h>
#include <terminal/terminal.h>

static int drive_present[4] = {0, 0, 0, 0};

static void ata_delay(void) {
    for (int i = 0; i < 4; i++) inb(ATA_PRIMARY_CTRL);
}

static int ata_wait_not_busy(void) {
    u32 timeout = 1000000;
    while (timeout--) {
        u8 status = inb(ATA_PRIMARY_BASE + ATA_REG_STATUS);
        if (status == 0xFF) return -1;
        if (!(status & ATA_SR_BSY)) return 0;
    }
    return -1;
}

static int ata_poll_drq(void) {
    u32 timeout = 100000;
    while (timeout--) {
        u8 status = inb(ATA_PRIMARY_BASE + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return -1;
        if (status & ATA_SR_DRQ) return 0;
    }
    return -1;
}

static int ata_kdrive_read_sectors(struct kdrive_t *drive, unsigned int lba,
                                    unsigned int count, u8 *buf)
{
    return ata_read_sectors(drive->userdata1, lba, count, buf);
}

static ssize_t ata_kdrive_write_sectors(struct kdrive_t *drive, size_t lba,
                                         size_t count, const u8 *buf)
{
    return ata_write_sectors(drive->userdata1, lba, count, buf);
}

u16 busses_by_drive[] = {
    ATA_PRIMARY_BASE,
    ATA_PRIMARY_BASE,
    ATA_SECONDARY_BASE,
    ATA_SECONDARY_BASE
};

static int ata_check_drive(int drive, char bus) /* BUS == 0: Depending on the drive, the bus will be 1 or 2 */ {
    drive_present[drive] = 0;

    u16 base = bus == 0 ? busses_by_drive[drive] : (bus ? ATA_SECONDARY_BASE : ATA_PRIMARY_BASE);
    u8 select = ((drive & 1) == 0) ? ATA_SELECT_SLAVE : ATA_SELECT_MASTER;

    // Select drive and wait
    outb(base + ATA_REG_HDDEVSEL, select);
    for (int i = 0; i < 5; i++) ata_delay();

    // Check status — 0xFF means no drive on bus
    u8 status = inb(base + ATA_REG_STATUS);
    if (status == 0xFF) return 0;

    // Wait for not busy
    if (ata_wait_not_busy() != 0) return 0;

    // Try to read sector 0 as a presence test
    u8 lba_sel = (drive ? ATA_LBA_SLAVE : ATA_LBA_MASTER);
    outb(base + ATA_REG_HDDEVSEL, lba_sel);
    ata_delay();
    outb(base + ATA_REG_SECCOUNT, 1);
    outb(base + ATA_REG_LBA_LO,   0);
    outb(base + ATA_REG_LBA_MID,  0);
    outb(base + ATA_REG_LBA_HI,   0);
    outb(base + ATA_REG_COMMAND,   ATA_CMD_READ_PIO);

    ata_delay();

    if (ata_wait_not_busy() != 0) return 0;

    status = inb(base + ATA_REG_STATUS);
    if (status & ATA_SR_ERR || !(status & ATA_SR_DRQ)) return 0;

    // Drain the data register
    for (int i = 0; i < 256; i++)
        inw(base + ATA_REG_DATA);

    struct kdrive_t kdrive = {};
    kdrive.userdata1   = drive;
    kdrive.sector_size = 512;
    kdrive.read  = (kdrive_read_sectors)ata_kdrive_read_sectors;
    kdrive.write = (kdrive_write_sectors)ata_kdrive_write_sectors;
    npf_snprintf_(kdrive.name, 64, "ATA Drive %d", drive + bus);
    drive_present[drive + bus] = 1;
    register_kdrive(kdrive);

    return 1;
}

void ata_init(void) {
    for (u8 i = 0; i < 2; i++) {
        for (u8 y = 0; y < 2; y++) {
            ata_check_drive(y, i);
        }
    }
}

int ata_drive_present(int drive) {
    if (drive < 0 || drive > 1) return 0;
    return drive_present[drive];
}

int ata_read_sectors(int drive, u32 lba, u8 count, u8 *buf) {
    if (!drive_present[drive]) return -1;
    if (ata_wait_not_busy() != 0) return -1;

    u8 lba_select = (drive ? ATA_LBA_SLAVE : ATA_LBA_MASTER)
                         | ((lba >> 24) & 0x0F);

    outb(ATA_PRIMARY_BASE + ATA_REG_HDDEVSEL, lba_select);
    ata_delay();
    outb(ATA_PRIMARY_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_LO,   (u8)(lba));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_MID,  (u8)(lba >> 8));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_HI,   (u8)(lba >> 16));
    outb(ATA_PRIMARY_BASE + ATA_REG_COMMAND,   ATA_CMD_READ_PIO);

    u16 *p = (u16 *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_not_busy() != 0) return -1;
        if (ata_poll_drq() != 0) return -1;
        for (int i = 0; i < 256; i++) {
            p[s * 256 + i] = inw(ATA_PRIMARY_BASE + ATA_REG_DATA);
        }
    }
    return count;
}

int ata_write_sectors(int drive, u32 lba, u8 count, const u8 *buf) {
    if (!drive_present[drive]) return -1;
    if (ata_wait_not_busy() != 0) return -1;

    u8 lba_select = (drive ? ATA_LBA_SLAVE : ATA_LBA_MASTER)
                         | ((lba >> 24) & 0x0F);

    outb(ATA_PRIMARY_BASE + ATA_REG_HDDEVSEL, lba_select);
    ata_delay();
    outb(ATA_PRIMARY_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_LO,   (u8)(lba));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_MID,  (u8)(lba >> 8));
    outb(ATA_PRIMARY_BASE + ATA_REG_LBA_HI,   (u8)(lba >> 16));
    outb(ATA_PRIMARY_BASE + ATA_REG_COMMAND,   ATA_CMD_WRITE_PIO);

    const u16 *p = (const u16 *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_not_busy() != 0) return -1;
        if (ata_poll_drq() != 0) return -1;
        for (int i = 0; i < 256; i++) {
            outw(ATA_PRIMARY_BASE + ATA_REG_DATA, p[s * 256 + i]);
        }
        outb(ATA_PRIMARY_BASE + ATA_REG_COMMAND, ATA_CMD_FLUSH);
        if (ata_wait_not_busy() != 0) return -1;
    }
    return 0;
}
