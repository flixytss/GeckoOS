#ifndef MOUSE_H
#define MOUSE_H

#include "arch/x86_64/isr.h"

#define MOUSE_CMD_RESET                    0xFF
#define MOUSE_CMD_RESEND                   0xFE
#define MOUSE_CMD_SET_DEFAULTS             0xF6
#define MOUSE_CMD_DISABLE_PACKET_STREAMING 0xF5
#define MOUSE_CMD_ENABLE_PACKET_STREAMING  0xF4
#define MOUSE_CMD_SAMPLE_RATE              0xF3
#define MOUSE_CMD_MOUSE_ID                 0xF2
#define MOUSE_CMD_REQUEST_SINGLE_PACKET    0xEB
#define MOUSE_CMD_STATUS                   0xE9
#define MOUSE_CMD_RESOLUTION               0xE8
#define MOUSE_CMD_CMD                      0xD4 //COmmand to ask for writing command response is 0XFA acknowledge byte

#define MOUSE_ACKNOWLEDGE 0xFA

typedef struct {
    u8 left_button : 1;
    u8 right_button : 1;
    u8 middle_button : 1;
    u8 always_1 : 1;
    u8 x_sign : 1;
    u8 y_sign : 1;
    u8 x_overflow : 1;
    u8 y_overflow : 1;
} MOUSE_STATUS;

void mouse_init();

int mouse_getx();
int mouse_gety();

typedef struct {
    i16 x, y;
    i8 scroll;
    u8 buttons;
    u8 event_type; // 0=move, 1=button, 2=scroll
} mouse_event_data_t;

// intializes the mouse
void mouse_init();
// the handler to be attached at irq 12
void mouse_handler(registers_t *r);

typedef void (*mouse_event_callback)(mouse_event_data_t md);

// Function to register a callback
void register_mouse_callback(mouse_event_callback callback);

// Function that will trigger the callback
void simulate_mouse_event(mouse_event_data_t md);

static mouse_event_callback mouse_event_run = 0;

void set_mouse_rate(u8 rate);

u8 mouse_read();
#endif