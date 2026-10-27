#include <colors.h>
#include <arch/x86_64/idt.h>
#include <mem.h>
#include <ports.h>
#include <terminal/terminal.h>
#include <arch/x86_64/irq.h>

idt_entry_t idt_entries[256];
idt_ptr_t   idt_ptr;

void idt_set_gate(u8 num, u64 base, u16 sel, u8 flags)
{
    idt_entries[num].base_lo  =  base        & 0xFFFF;
    idt_entries[num].base_mid = (base >> 16) & 0xFFFF;
    idt_entries[num].base_hi  = (base >> 32) & 0xFFFFFFFF;

    idt_entries[num].sel    = sel;
    idt_entries[num].ist    = 0;
    idt_entries[num].flags  = flags;
    idt_entries[num].zero   = 0;
}

void init_idt()
{
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base  = (u64)&idt_entries;

    memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);

    /* Remap PIC: IRQs 0-15 → IDT vectors 32-47 */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);

    idt_set_gate( 0, (u64)isr0 , 0x08, 0x8E);
    idt_set_gate( 1, (u64)isr1 , 0x08, 0x8E);
    idt_set_gate( 2, (u64)isr2 , 0x08, 0x8E);
    idt_set_gate( 3, (u64)isr3 , 0x08, 0x8E);
    idt_set_gate( 4, (u64)isr4 , 0x08, 0x8E);
    idt_set_gate( 5, (u64)isr5 , 0x08, 0x8E);
    idt_set_gate( 6, (u64)isr6 , 0x08, 0x8E);
    idt_set_gate( 7, (u64)isr7 , 0x08, 0x8E);
    idt_set_gate( 8, (u64)isr8 , 0x08, 0x8E);
    idt_set_gate( 9, (u64)isr9 , 0x08, 0x8E);
    idt_set_gate(10, (u64)isr10, 0x08, 0x8E);
    idt_set_gate(11, (u64)isr11, 0x08, 0x8E);
    idt_set_gate(12, (u64)isr12, 0x08, 0x8E);
    idt_set_gate(13, (u64)isr13, 0x08, 0x8E);
    idt_set_gate(14, (u64)isr14, 0x08, 0x8E);
    idt_set_gate(15, (u64)isr15, 0x08, 0x8E);
    idt_set_gate(16, (u64)isr16, 0x08, 0x8E);
    idt_set_gate(17, (u64)isr17, 0x08, 0x8E);
    idt_set_gate(18, (u64)isr18, 0x08, 0x8E);
    idt_set_gate(19, (u64)isr19, 0x08, 0x8E);
    idt_set_gate(20, (u64)isr20, 0x08, 0x8E);
    idt_set_gate(21, (u64)isr21, 0x08, 0x8E);
    idt_set_gate(22, (u64)isr22, 0x08, 0x8E);
    idt_set_gate(23, (u64)isr23, 0x08, 0x8E);
    idt_set_gate(24, (u64)isr24, 0x08, 0x8E);
    idt_set_gate(25, (u64)isr25, 0x08, 0x8E);
    idt_set_gate(26, (u64)isr26, 0x08, 0x8E);
    idt_set_gate(27, (u64)isr27, 0x08, 0x8E);
    idt_set_gate(28, (u64)isr28, 0x08, 0x8E);
    idt_set_gate(29, (u64)isr29, 0x08, 0x8E);
    idt_set_gate(30, (u64)isr30, 0x08, 0x8E);
    idt_set_gate(31, (u64)isr31, 0x08, 0x8E);

    idt_flush((u64)&idt_ptr);
    STI();
}