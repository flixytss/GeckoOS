#include <gk/io.h>
#include <terminal/printf.h>

#define PORT 0x3f8

static int serial_initialized = 0;

int serial_init() {
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    if(inb(PORT + 0) != 0xAE)
        return -1;

    outb(PORT + 4, 0x0F);

    serial_initialized = 1;
    return 0;
}

int serial_can_send(void)
    { return (inb(PORT + 5) & 0x20) != 0; }

void serial_putc(char c) {
    if (!serial_initialized)
        return;
    while (!serial_can_send()) halt();
    outb(PORT, c);
}