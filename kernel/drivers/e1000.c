#include <drivers/e1000.h>
#include <drivers/pci.h>
#include <arch/x86_64/irq.h>
#include <mem/paging.h>
#include <mem/physical_mem.h>
#include <mem.h>
#include "drivers/vga.h"
#include "terminal/printf.h"

static volatile u32 *mmio_base = NULL;
static u8 mac[6];

static e1000_rx_desc_t *rx_descs_phys = NULL;
static e1000_tx_desc_t *tx_descs_phys = NULL;
static u8         *rx_bufs_phys  = NULL;
static u8         *tx_buf_phys   = NULL;

static e1000_rx_desc_t *rx_descs = NULL;
static e1000_tx_desc_t *tx_descs = NULL;
static u8         *rx_bufs  = NULL;
static u8         *tx_buf   = NULL;

static u32 rx_tail = 0;
static u32 tx_tail = 0;

static u32 e1000_read(u32 reg) {
    return mmio_base[reg / 4];
}

static void e1000_write(u32 reg, u32 val) {
    mmio_base[reg / 4] = val;
}

static u32 eeprom_read(u8 addr) {
    e1000_write(E1000_EERD, (1u) | ((u32)addr << 8));
    u32 v;
    do { v = e1000_read(E1000_EERD); } while (!(v & (1u << 4)));
    return (v >> 16) & 0xFFFF;
}

static void read_mac(void) {
    u32 lo = eeprom_read(0);
    u32 mid = eeprom_read(1);
    u32 hi = eeprom_read(2);
    mac[0] = lo & 0xFF;
    mac[1] = (lo >> 8) & 0xFF;
    mac[2] = mid & 0xFF;
    mac[3] = (mid >> 8) & 0xFF;
    mac[4] = hi & 0xFF;
    mac[5] = (hi >> 8) & 0xFF;
}

static void rx_init(void) {
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        u64 buf_phys = (u64)(uintptr_t)(rx_bufs_phys + i * E1000_RX_BUF_SIZE);
        rx_descs[i].addr   = buf_phys;
        rx_descs[i].status = 0;
    }

    u64 base = (u64)(uintptr_t)rx_descs_phys;
    e1000_write(E1000_RDBAL, (u32)(base & 0xFFFFFFFF));
    e1000_write(E1000_RDBAH, (u32)(base >> 32));
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);
    rx_tail = 0;

    e1000_write(E1000_RCTL,
        E1000_RCTL_EN  |
        E1000_RCTL_BAM |
        E1000_RCTL_BSIZE_2048 |
        E1000_RCTL_SECRC);
}

static void tx_init(void) {
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        tx_descs[i].addr   = 0;
        tx_descs[i].status = E1000_TXD_STAT_DD;
    }

    u64 base = (u64)(uintptr_t)tx_descs_phys;
    e1000_write(E1000_TDBAL, (u32)(base & 0xFFFFFFFF));
    e1000_write(E1000_TDBAH, (u32)(base >> 32));
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);
    tx_tail = 0;

    e1000_write(E1000_TCTL,
        E1000_TCTL_EN  |
        E1000_TCTL_PSP |
        (15u << E1000_TCTL_CT_SHIFT)   |
        (63u << E1000_TCTL_COLD_SHIFT));

    e1000_write(E1000_TIPG, 0x0060200A);
}

static volatile int rx_pending = 0;

static void e1000_irq_handler(registers_t *regs) {
    (void)regs;
    u32 icr = e1000_read(E1000_ICR);

    if (icr & E1000_ICR_LSC) {
        u32 status = e1000_read(E1000_STATUS);
        printf("e1000: link %s\n", (status & 2) ? "up" : "down");
    }

    if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXO)) {
        rx_pending = 1;
    }
}

bool e1000_init(u8 bus, u8 slot, u8 func) {
    u32 bar0 = pci_readl(bus, slot, func, 0x10) & ~0xFu;

    set_printf_color(VGA_COLOR_LIGHT_RED);
    if (!bar0) {
        printf("e1000: BAR0 is zero\n");
        return false;
    }

    u64 virt = mmio_map((u64)bar0, 0x20000); // TODO: Get the bar size
    if (!virt) {
        printf("e1000: mmio_map failed\n");
        return false;
    }
    mmio_base = (volatile u32 *)virt;

    u16 cmd = pci_readw(bus, slot, func, 0x04);
    pci_writew(bus, slot, func, 0x04, cmd | (1u << 1) | (1u << 2));

    /* reset */
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);
    for (volatile int i = 0; i < 100000; i++);
    if (e1000_read(E1000_CTRL) & E1000_CTRL_RST) {
        printf("e1000: reset timed out\n");
        return false;
    }

    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU | E1000_CTRL_ASDE);

    /* clear multicast table */
    for (int i = 0; i < 128; i++)
        e1000_write(E1000_MTA + i * 4, 0);

    rx_descs_phys = (e1000_rx_desc_t *)allocate_blocks(1);
    tx_descs_phys = (e1000_tx_desc_t *)allocate_blocks(1);
    rx_bufs_phys  = (u8 *)allocate_blocks(E1000_NUM_RX_DESC);
    tx_buf_phys   = (u8 *)allocate_blocks(1);

    if (!rx_descs_phys || !tx_descs_phys || !rx_bufs_phys || !tx_buf_phys) {
        printf("e1000: failed to allocate DMA buffers\n");
        return false;
    }

    /* First 16 MB is identity-mapped, so virt == phys */
    rx_descs = rx_descs_phys;
    tx_descs = tx_descs_phys;
    rx_bufs  = rx_bufs_phys;
    tx_buf   = tx_buf_phys;

    read_mac();
    set_printf_color(VGA_COLOR_LIGHT_GREY);
    #ifdef DEBUG
        printf("  e1000: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    #endif

    /* write MAC into receive address register 0 */
    e1000_write(E1000_RAL,
        (u32)mac[0] |
        (u32)mac[1] << 8  |
        (u32)mac[2] << 16 |
        (u32)mac[3] << 24);
    e1000_write(E1000_RAH,
        (u32)mac[4] |
        (u32)mac[5] << 8  |
        E1000_RAH_AV);

    rx_init();
    tx_init();

    /* unmask TX done, RX timer, link change, RX overrun */
    e1000_write(E1000_IMS,
        E1000_ICR_TXDW |
        E1000_ICR_RXT0 |
        E1000_ICR_LSC  |
        E1000_ICR_RXO);

    u8 irq_line = pci_readb(bus, slot, func, 0x3C);
    irq_install_handler(irq_line, e1000_irq_handler,0x9);

    #ifdef DEBUG
        printf("  e1000: init done, IRQ %d\n", irq_line);
    #endif

    set_printf_color(VGA_COLOR_WHITE);

    return true;
}

int e1000_send(const void *data, u16 len) {
    if (!mmio_base) return -1;
    if (len > E1000_TX_BUF_SIZE) return -1;

    /* wait for descriptor to be free */
    int timeout = 100000;
    while (!(tx_descs[tx_tail].status & E1000_TXD_STAT_DD)) {
        if (--timeout == 0) return -1;
    }

    /* Copy data into physical TX buffer */
    memcpy(tx_buf, data, len);

    u64 buf_phys = (u64)(uintptr_t)tx_buf;
    tx_descs[tx_tail].addr   = buf_phys;
    tx_descs[tx_tail].length = len;
    tx_descs[tx_tail].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    tx_descs[tx_tail].status = 0;

    tx_tail = (tx_tail + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_TDT, tx_tail);

    return 0;
}

void e1000_get_mac(u8 out[6]) {
    for (int i = 0; i < 6; i++)
        out[i] = mac[i];
}

int e1000_receive(u8 *buf, u16 *len) {
    if (!mmio_base) return -1;

    if (rx_descs[rx_tail].status & E1000_RXD_STAT_DD) {
        *len = rx_descs[rx_tail].length;
        u8 *src = rx_bufs + rx_tail * E1000_RX_BUF_SIZE;
        for (u16 i = 0; i < *len; i++)
            buf[i] = src[i];

        rx_descs[rx_tail].status = 0;
        e1000_write(E1000_RDT, rx_tail);
        rx_tail = (rx_tail + 1) % E1000_NUM_RX_DESC;
        return 0;
    }
    return -1;
}