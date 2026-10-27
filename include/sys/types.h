/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/*
 *	POSIX Standard: 2.6 Primitive System Data Types	<sys/types.h>
 */


#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H 1

/* ssize_t: signed counterpart of size_t */
#ifndef __ssize_t_defined
typedef long ssize_t;
# define __ssize_t_defined
#endif

/* off_t: file offset */
#ifndef __off_t_defined
typedef long long off_t;
# define __off_t_defined
#endif

/* pid_t, uid_t, gid_t — placeholder types */
typedef int  pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;

typedef unsigned char      u8;
typedef char               i8;
typedef unsigned int       u32;
typedef int                i32;
typedef unsigned long long u64;
typedef long long          i64;
typedef unsigned short     u16;
typedef short              i16;

#ifndef _UINTPTR_T
typedef __UINTPTR_TYPE__ uintptr_t;
#define _UINTPTR_T
#endif

#ifndef _INTPTR_T
#ifndef __intptr_t_defined
typedef __INTPTR_TYPE__ intptr_t;
#define __intptr_t_defined
#define _INTPTR_T
#endif
#endif

typedef unsigned long size_t;
typedef long          ptrdiff_t;

typedef __INTMAX_TYPE__  intmax_t;
typedef __UINTMAX_TYPE__ uintmax_t;

typedef unsigned char      uint_fast8_t;
typedef char               int_fast8_t;
typedef unsigned int       uint_fast32_t;
typedef int                int_fast32_t;
typedef unsigned long long uint_fast64_t;
typedef long long          int_fast64_t;
typedef unsigned short     uint_fast16_t;
typedef short              int_fast16_t;

#define UINT32_C(x) ((u32)x)

#define NULL ((void*)0)

#define U64_MAX 

#endif /* _SYS_TYPES_H */