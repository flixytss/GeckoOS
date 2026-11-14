#pragma once

#include <sys/types.h>

#define pause() asm volatile("pause")
#define cli() asm volatile("cli")
#define sti() asm volatile("sti")
#define halt() asm volatile("hlt")

void outb(u16 port, u8 val);
void outw(u16 port, u16 val);
void outl(u16 port, u32 val);

u8 inb(u16 port);
u16 inw(u16 port);
u32 inl(u16 port);

void insl(u16 port, void *buffer, u32 count);