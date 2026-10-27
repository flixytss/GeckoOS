#pragma once

#include "sys/types.h"

void start_pit_timer(u32 frequency);
void pit_timer_wait_s(u64 ticks);
void pit_timer_wait_ms(u32 ms);

extern volatile u64 pit_timer;

#define PITHZ 1000