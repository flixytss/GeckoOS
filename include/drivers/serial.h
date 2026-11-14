#pragma once

#include <gk/io.h>

int serial_init();
int serial_can_send(void);
void serial_putc(char c);