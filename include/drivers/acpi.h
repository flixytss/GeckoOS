#pragma once

#include "arch/x86_64/isr.h"
#include "sys/types.h"

struct acpi_rsdp_v1 {
    char signature[8];
    u8 checksum;
    char oem_id[6];
    u8 revision;
    u32 rsdt_addr;
} __attribute__((packed));

struct acpi_rsdp_v2 {
    struct acpi_rsdp_v1 v1;
    u32 length;
    u64 xsdt_addr;
    u8 extended_checksum;
    u8 reserved[3];
} __attribute__((packed));

typedef struct acpi_header {
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32 oem_revision;
    u32 creator_id;
    u32 creator_revision;
}__attribute__((packed)) acpi_header_t;

typedef struct acpi_madt {
    struct acpi_header header;
    u32 lapic_addr;
    u32 flags;
    // entries follow
} __attribute__((packed)) acpit_madt_t;

struct madt_entry_header {
    u8 type;
    u8 length;
} __attribute__((packed));

struct madt_ioapic {
    u8 type;      // 1
    u8 length;    // 12
    u8 ioapic_id;
    u8 reserved;
    u32 addr;
    u32 gsi_base;
} __attribute__((packed));

struct madt_iso {
    u8 type;      // 2
    u8 length;    // 10
    u8 bus;
    u8 irq;
    u32 gsi;
    u16 flags;
} __attribute__((packed));

typedef struct {
    acpi_header_t header;
    u32 firmwareControl;
    u32 dsdt;
    u8 reserved;
    u8 preferredPMProfile;
    u16 sciInterrupt;
    u32 smiCommandPort;
    u8 acpiEnable;
    u8 acpiDisable;
    u8 s4BiosReq;
    u8 pStateCnt;
    u32 pm1aEventBlk;
    u32 pm1bEventBlk;
    u32 pm1aControlBlk;
    u32 pm1bControlBlk;
    u32 pm2ControlBlk;
    u32 pmTimerBlk;
    u32 gpe0Blk;
    u32 gpe1Blk;
    u8 pm1EventLength;
    u8 pm1ControlLength;
    u8 pm2ControlLength;
    u8 pmTimerLength;
    u8 gpe0BlkLength;
    u8 gpe1BlkLength;
    u8 gpe1Base;
    u8 cStateControl;
    u16 worstC2Latency;
    u16 worstC3Latency;
    u16 flushSize;
    u16 flushStride;
    u8 dutyOffset;
    u8 dutyWidth;
    u8 dayAlrm;
    u8 monAlrm;
    u8 century;
    u16 bootArchFlags;
    u8 reserved2;
    u32 flags;
    u8 resetReg[12];
    u8 resetValue;
    u16 armBootArchFlags;
    u8 fadtMinorVersion;
    u64 xFirmwareControl;
    u64 xDsdt;
    u8 xPm1aEventBlk[12];
    u8 xPm1bEventBlk[12];
    u8 xPm1aControlBlk[12];
    u8 xPm1bControlBlk[12];
    u8 xPm2ControlBlk[12];
    u8 xPmTimerBlk[12];
    u8 xGpe0Blk[12];
    u8 xGpe1Blk[12];
    u8 sleepControlReg[12];
    u8 sleepStatusReg[12];
    u64 hypervisorVendorId;
} __attribute__((__packed__)) acpi_fadt_t;

extern uintptr_t acpi_lapic_base;
extern uintptr_t acpi_ioapic_base;
int acpi_init();

extern struct madt_iso isos[32];
extern int isos_num;

void acpi_irq_handler(registers_t* regs);
int shutdown();