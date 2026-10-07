#pragma once

// just a bunch of linux errnos to globalize errors

#define ENOENT  2
#define EIO     5
#define ENXIO   6
#define EBADF   9
#define EAGAIN  11
#define EWOULDBLOCK  11
#define ENOMEM  12
#define ENOTBLK 15
#define EBUSY   16
#define EEXIST  17
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define EFBIG   27
#define ENOSPC  28
#define ESPIPE  29
#define ENAMETOOLONG 36
#define ENOTEMPTY    39
#define ENODATA 61
#define ETIME   62
#define ENETDOWN     100
#define ENETUNREACH  101
#define ENOBUFS 105
#define EHWPOISON    133

#ifdef DEBUG
    #include "terminal/printf.h"

    static const char* serrnos[] = {
        [2]  = "No such file or directory",
        [5]  = "Input/output error",
        "No such device or address",
        [9]  = "Bad file descriptor",
        [11] = "Resource temporarily unavailable",
        "Cannot allocate memory",
        [15] = "Block device required",
        "Device or resource busy",
        "File exists",
        [19] = "No such device",
        "Not a directory",
        "Is a directory",
        "Invalid argument",
        [27] = "File too large",
        "No space left on device",
        "Illegal seek",
        [36] = "File name too long",
        [39] = "Directory not empty",
        [61] = "No data available",
        "Timer expired",
        [100] = "Network is down",
        "Network is unreachable",
        [105] = "No buffer space available",
        [133] = "Memory page has hardware error"
    };


    static inline void perror(int errno) {
        if (serrnos[-errno]) printf("%s\n", serrnos[-errno]);
    }
#endif