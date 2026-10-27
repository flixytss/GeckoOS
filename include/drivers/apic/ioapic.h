// Advanced programamble interrupt card or smth
// the thing we use for input and output and we have the io one and the local
// one the local is for interacting between cores i think
#pragma once

#include <stdbool.h>
#include "sys/types.h"

#define IOAPIC_IOREGSEL 0x00
#define IOAPIC_IOWIN    0x10

#define IOAPIC_REG_ID      0x00
#define IOAPIC_REG_VER     0x01
#define IOAPIC_REG_ARB     0x02
#define IOAPIC_REDTBL_BASE 0x10

// Redirection entry flags
#define IOAPIC_REDTBL_MASKED            (1 << 16)
#define IOAPIC_REDTBL_TRIGGER_LEVEL     (1 << 15)
#define IOAPIC_REDTBL_REMOTE_IRR        (1 << 14)
#define IOAPIC_REDTBL_POLARITY          (1 << 13)
#define IOAPIC_REDTBL_DELIVERY_PENDING  (1 << 12)
#define IOAPIC_REDTBL_DEST_MODE         (1 << 11) // 0=physical, 1=logical
#define IOAPIC_REDTBL_DELIVERY_MASK     (7 << 8)
#define IOAPIC_REDTBL_DELIVERY_FIXED    (0 << 8)
#define IOAPIC_REDTBL_DELIVERY_LOWEST   (1 << 8)
#define IOAPIC_REDTBL_DELIVERY_SMI      (2 << 8)
#define IOAPIC_REDTBL_DELIVERY_NMI      (4 << 8)
#define IOAPIC_REDTBL_DELIVERY_INIT     (5 << 8)
#define IOAPIC_REDTBL_DELIVERY_EXTINT   (7 << 8)
#define IOAPIC_REDTBL_VECTOR_MASK       0xFF
#define IOAPIC_REGSEL                   0x00
#define IOAPIC_WIN                      0x10
#define IOAPIC_REDTBL_DEST_MODE_PHYS    0x00000000 // Physical destination
#define IOAPIC_REDTBL_DEST_MODE_LOGICAL 0x00000800 // Logical destination

#define IOAPIC_REDTBL_DELIVERY_STATUS 0x00001000 // Read-only: delivery status

#define IOAPIC_REDTBL_PIN_POLARITY_ACTIVE_HIGH 0x00000000
#define IOAPIC_REDTBL_PIN_POLARITY_ACTIVE_LOW  0x00002000

#define IOAPIC_REDTBL_TRIGGER_EDGE 0x00000000

#define IOAPIC_FLAGS_DEFAULT                                                   \
    (IOAPIC_REDTBL_DELIVERY_FIXED | IOAPIC_REDTBL_DEST_MODE_PHYS |             \
     IOAPIC_REDTBL_PIN_POLARITY_ACTIVE_HIGH | IOAPIC_REDTBL_TRIGGER_EDGE)
// Edge-triggered, active-high (keyboard, PS/2 mouse, legacy PCI)
#define FLAGS_EDGE_HIGH                                                        \
    (IOAPIC_REDTBL_DELIVERY_FIXED | IOAPIC_REDTBL_TRIGGER_EDGE |               \
     IOAPIC_REDTBL_PIN_POLARITY_ACTIVE_HIGH)

// Level-triggered, active-low (most PCI devices)
#define FLAGS_LEVEL_LOW                                                        \
    (IOAPIC_REDTBL_DELIVERY_FIXED | IOAPIC_REDTBL_TRIGGER_LEVEL |              \
     IOAPIC_REDTBL_PIN_POLARITY_ACTIVE_LOW)

// NMI for watchdog/timer
#define FLAGS_NMI                                                              \
    (IOAPIC_REDTBL_DELIVERY_NMI | IOAPIC_REDTBL_TRIGGER_EDGE |                 \
     IOAPIC_REDTBL_PIN_POLARITY_ACTIVE_HIGH)

i32 init();

u32 cpuReadIoApic(void *ioapicaddr, u32 reg);
void cpuWriteIoApic(void *ioapicaddr, u32 reg, u32 value);

void ioapic_init();

u32 ioapic_read(u32 reg);
void ioapic_write(u32 reg, u32 value);

void ioapic_mask_irq(u32 irq, bool masked);
void ioapic_redirect_irq(int gsi, int vector, u16 flags);