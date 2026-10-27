#include "drivers/pit.h"
#include "ports.h"
#include <arch/x86_64/irq.h>
#include "sys/types.h"
#include <terminal/printf.h>
#include <drivers/acpi.h>

volatile u64 pit_timer = 0;

void pit_timer_wait_s(u64 ticks);

static void timer_irq(registers_t* _) {
    pit_timer++;
}

void start_pit_timer(u32 frequency) {
    irq_install_handler(0, timer_irq, 0);

    u32 divisor = 1193180 / frequency;

    outb(0x43, 0x36);

    u8 l = (u8)(divisor & 0xFF);
    u8 h = (u8)((divisor >> 8) & 0xFF );

    outb(0x40, l);
    outb(0x40, h);
} 

void pit_timer_wait_s(u64 s) {
    unsigned long eticks;

    eticks = pit_timer + (s * PITHZ);
    while(pit_timer < eticks) HALT();
}
void pit_timer_wait_ms(u32 ms) {
    unsigned long eticks;

    eticks = pit_timer + ms;
    while(pit_timer < eticks) HALT();
}