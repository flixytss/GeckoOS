#include "arch/x86_64/isr.h"
#include "drivers/vga.h"
#include "terminal/terminal.h"
#include "drivers/framebuffer.h"
#include "terminal/printf.h"
#include "sys/types.h"
#include <stdbool.h>
#include <stddef.h>
#include <mem.h>
#include <mem/physical_mem.h>
#include <mem/paging.h>

static page_table_t *current_pml4 = NULL;

#define MMIO_VIRT_BASE 0xFFFFFFFF90000000ULL
static u64 mmio_virt_next = MMIO_VIRT_BASE;

page_table_t *alloc_table(void)
{
    page_table_t *t = (page_table_t *)allocate_blocks(1);
    if (t) memset(t, 0, sizeof(page_table_t));
    return t;
}

static pte_t *get_pte(page_table_t *pml4, u64 virt, bool create)
{
    pte_t *pml4e = &pml4->entries[PML4_INDEX(virt)];
    if (!pte_is_present(*pml4e)) {
        if (!create) return NULL;
        page_table_t *pdpt = alloc_table();
        if (!pdpt) return NULL;
        *pml4e = (u64)pdpt | PTE_PRESENT | PTE_WRITABLE;
    }

    page_table_t *pdpt = (page_table_t *)pte_get_phys(*pml4e);
    pte_t *pdpte = &pdpt->entries[PDPT_INDEX(virt)];
    if (!pte_is_present(*pdpte)) {
        if (!create) return NULL;
        page_table_t *pd = alloc_table();
        if (!pd) return NULL;
        *pdpte = (u64)pd | PTE_PRESENT | PTE_WRITABLE;
    }

    page_table_t *pd = (page_table_t *)pte_get_phys(*pdpte);
    pte_t *pde = &pd->entries[PD_INDEX(virt)];
    if (!pte_is_present(*pde)) {
        if (!create) return NULL;
        page_table_t *pt = alloc_table();
        if (!pt) return NULL;
        *pde = (u64)pt | PTE_PRESENT | PTE_WRITABLE;
    }

    page_table_t *pt = (page_table_t *)pte_get_phys(*pde);
    return &pt->entries[PT_INDEX(virt)];
}

static void flush_tlb(u64 virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

page_table_t *vmm_get_pml4(void)
{
    return current_pml4;
}

bool vmm_set_pml4(page_table_t *pml4)
{
    assert(pml4 != NULL);
    current_pml4 = pml4;
    asm volatile("mov %0, %%cr3" : : "r"((u64)pml4) : "memory");
    return true;
}

bool vmm_map(page_table_t *pml4, u64 phys, u64 virt, u64 flags)
{
    pte_t *pte = get_pte(pml4, virt, true);
    if (!pte) return false;
    *pte = (phys & PTE_ADDR_MASK) | (flags & ~PTE_ADDR_MASK) | PTE_PRESENT;
    flush_tlb(virt);
    return true;
}

void vmm_unmap(page_table_t *pml4, u64 virt)
{
    pte_t *pte = get_pte(pml4, virt, false);
    if (!pte) return;
    *pte = 0;
    flush_tlb(virt);
}

u64 vmm_get_phys(page_table_t *pml4, u64 virt)
{
    pte_t *pte = get_pte(pml4, virt, false);
    if (!pte || !pte_is_present(*pte)) return 0;
    return pte_get_phys(*pte);
}

bool vmm_init(void)
{
    page_table_t *pml4 = alloc_table();
    if (!pml4) return false;

    for (u64 phys = 0; phys < 0x1000000; phys += PAGE_SIZE) {
        if (!vmm_map(pml4, phys, phys, PTE_PRESENT | PTE_WRITABLE))
            return false;
    }

    if (g_fb_addr) {
        u64 fb_phys_start = g_fb_addr & ~((u64)PAGE_SIZE - 1);
        u64 fb_size       = (u64)g_fb_height * g_fb_pitch;
        u64 fb_pages      = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE + 1;
        for (u64 i = 0; i < fb_pages; i++) {
            u64 p = fb_phys_start + i * PAGE_SIZE;
            if (!vmm_map(pml4, p, p,
                         PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE))
                return false;
        }
    }

    u64 kernel_phys = 0x100000;
    for (u64 off = 0; off < 0x400000; off += PAGE_SIZE) {
        if (!vmm_map(pml4, kernel_phys + off, VIRT + off,
                     PTE_PRESENT | PTE_WRITABLE))
            return false;
    }

    return vmm_set_pml4(pml4);
}

u64 mmio_map(u64 phys, u64 size)
{
    page_table_t *pml4 = vmm_get_pml4();
    if (!pml4) return 0;

    u64 phys_aligned = phys & ~(PAGE_SIZE - 1);
    u64 offset       = phys - phys_aligned;
    u64 pages        = (size + offset + PAGE_SIZE - 1) / PAGE_SIZE;

    u64 virt_base = mmio_virt_next;
    mmio_virt_next += pages * PAGE_SIZE;

    for (u64 i = 0; i < pages; i++) {
        if (!vmm_map(pml4,
                     phys_aligned + i * PAGE_SIZE,
                     virt_base    + i * PAGE_SIZE,
                     PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE))
            return 0;
    }

    return virt_base + offset;
}

int mmio_unmap(u64 virt, u64 size) {
    page_table_t *pml4 = vmm_get_pml4();
    if (!pml4) return -1;
    
    u64 virt_aligned = virt & ~(PAGE_SIZE - 1);
    u64 offset = virt - virt_aligned;
    u64 pages = (size + offset + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (u64 i = 0; i < pages; i++) {
        // Unmap the page (set entry to 0)
        vmm_unmap(pml4, virt_aligned + i * PAGE_SIZE) ;
    }
    
    return 0;
}

// Panic (Page fault)

void page_fault(registers_t* regs) {
    u64 faulting_address;
    asm volatile("movq %%cr2, %0" : "=r" (faulting_address));

    int present   = !(regs->err_code & 0x1); // Page not present
    int rw = regs->err_code & 0x2;           // Write operation?
    int us = regs->err_code & 0x4;           // Processor was in user-mode?
    int reserved = regs->err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
    int id = regs->err_code & 0x10;          // Caused by an instruction fetch?
    ((void)id);

    print("Page fault! ( ");
    if (present) {print("present ");}
    if (rw) {print("read-only ");}
    if (us) {print("user-mode ");}
    if (reserved) {print("reserved ");}
    print(") at ");
    printf("0x%p\nHalting...", faulting_address);
    for (;;) asm("hlt");
} 