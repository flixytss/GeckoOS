#ifndef _ISR_H
#define _ISR_H

#include "sys/types.h"

typedef struct registers
{
    /* Saved by pushaq macro (isr.s) */
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;

    /* Pushed by stub */
    u64 int_no;
    u64 err_code;

    /* Pushed by the CPU automatically */
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} registers_t;

typedef void (*isr_t)(registers_t*);

void register_interrupt_handler(u8 n, isr_t handler);

// Interrupts
#define INT_DIVIDE          0x00 // Division error
#define INT_DEBUGEXC        0x01 // Debug exception
#define INT_NMI             0x02 // NMI interruption
#define INT_BREAKPOINT      0x03 // Breakpoint
#define INT_OVERFLOW        0x04 // Overflow
#define INT_BOUNDEXCEE      0x05 // BOUND Range Exceeded 
#define INT_INVINS          0x06 // Invalid Opcode
#define INT_DEVNOTAV        0x07 // Device Not Available
#define INT_DOBLEFAULT      0x08 // Double Fault
#define INT_COPROC          0x09 // Coprocessor Segment Overrun (reserved)
#define INT_INVTSS          0x0A // Invalid TSS
#define INT_SEGNOTPRESENT   0x0B // Segment not present
#define INT_SSEGFAULT       0x0C // Stack-Segment Fault 
#define INT_GENERALPROT     0x0D // General protection
#define INT_PAGEFAULT       0x0E // Page fault
#define INT_INTELRESERVED   0x0F // Intel reserved
#define INT_FPUF            0x10 // x87 FPU Floating-Point Error (Math Fault)
#define INT_ALIGNMENT       0x11 // Alignment check
#define INT_MACHINECHECK    0x12 // Machine Check
#define INT_SIMDEXP         0x13 // SIMD Floating-Point Exception
#define INT_VIRTEXC         0x14 // Virtualization Exception
#define INT_CPE             0x15 // Control Protection Exception

void ud_exception_handler(registers_t* regs);

#endif