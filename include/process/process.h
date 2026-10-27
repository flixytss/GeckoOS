#pragma once
#include <arch/x86_64/isr.h>
#include <mem/paging.h>
#include "sys/types.h"

typedef enum {
    INVALID,
    SLEEPING,
    ACTIVE,
} Proc_State;

typedef struct Process Process;
typedef struct Thread  Thread;

typedef Process process_t;
typedef Thread  thread_t;

typedef struct Thread {
    Process    *parent;
    void       *stack;
    void       *stack_limit;
    void       *kernel_stack;
    u32    priority;
    Proc_State  state;
    u64    pgm_buf;
    u64    pgm_size;
    registers_t regs;
} Thread;

typedef struct Process {
    u32      id;
    u32      priority;
    page_table_t *pml4;
    Proc_State    state;
    Thread        threads[5];
    u32      thread_count;
} Process;

extern u32        nr_processes;

static void    switch_task(void);
u32       create_process(void *entry_point, u64 entry_to, void *data_point, u64 data_to);
static void    execute_process(Process *proc);