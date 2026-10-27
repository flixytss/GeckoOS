
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static inline void outb(u16 port, u8 val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(u16 port, u16 val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret; 
}

static inline u16 inw(u16 port) {
    u16 ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

struct mbr_partition_t
{
	u8 flags;
	u32 chr_start: 24;
	u8 type;
	u32 chr_end: 24;
	u32 lba;
	u32 size;
};
typedef struct mbr_partition_t mbr_t[4];

void _entry()
{
	u16 *mem = (void*)0x100000;
	void (*jmp)(void) = (void*)0x100000;
	// read kernel
	while (inb(0x1F7) & 0x80);
	outb(0x1F6, 0xE0);
	outb(0x1F2, 0); //256 sectors
	outb(0x1F3, 16); //8kb is bootloader
	outb(0x1F4, 0);
	outb(0x1F5, 0);
	outb(0x1F7, 0x20);
	while (inb(0x1F7) & 0x80);
	for (int s = 0; s < 256; s++)
	{
		while (!(inb(0x1F7) & 0x08));
		for (int i = 0; i < 256; i++)
		{
			mem[s*256+i] = inw(0x1F0);
		}
	}
	jmp();
}
