#pragma once

extern int serial_initialized;

int serial_init();
int serial_can_send(void);
void serial_putc(char c);