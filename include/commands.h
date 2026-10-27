#ifndef COMMANDS_H
#define COMMANDS_H

#include "sys/types.h"

typedef struct {
    char *name;
    void (*func)(u8 color);
} Command;

// System
static void cmd_help(u8 color);
static void cmd_hello(u8 color);
static void cmd_contributors(u8 color);
static void cmd_clear(u8 color);
static void cmd_version(u8 color);
static void cmd_chars(u8 color);
static void cmd_uptime(u8 color);
static void cmd_meminfo(u8 color);
static void cmd_lspci(u8 color);

// Keyboard
static void cmd_setkeyswe(u8 color);
static void cmd_setkeyus(u8 color);
static void cmd_setkeyuk(u8 color);

// Timer / power
static void cmd_sleep5(u8 color);
static void cmd_print_ticks(u8 color);
static void cmd_reboot(u8 color);

// Scripting
static void cmd_gk(u8 color);
static void cmd_gk_run_file(const char* filename, u8 color);

// Filesystem
static void cmd_fsmount(u8 color);
static void cmd_ls(u8 color);
static void cmd_cat(u8 color);
static void cmd_fsinfo(u8 color);
static void cmd_touch(u8 color);
static void cmd_rm(u8 color);
static void cmd_cp(u8 color);
static void cmd_mv(u8 color);
static void cmd_mkdir(u8 color);
static void cmd_echo(u8 color);
static void cmd_write(u8 color);
static void cmd_runelf(u8 color);
static void cmd_showdrives(u8 color);
static void cmd_mounts(u8 color);
static void cmd_cd(u8 color);

// Network
static void cmd_ping(u8 color);

// Processes
static void cmd_processes(u8 color);

// usb
static void cmd_show_usb(u8 color);
static void cmd_show_usb_info(u8 color);

// Dispatcher
static int streq(unsigned char *a, char *b);
void run_command(unsigned char *input, u8 color);

#endif