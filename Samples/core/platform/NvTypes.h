/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

//---------------------------------------------------------------------------
//! \file NvTypes.h
//! \brief Platform independent types.
//!
//! NvTypes.h contains the base types used to construct more abstract data
//! types. This file is necessary because different machines and compilers
//! may apply a different number of bits precision to the C/C++ types \c int,
//! \c unsigned, \c short, \c signed and \c char. Use the platform independent
//! types defined in this module in most if not all places. Use of \c int, and
//! other built in types should only be used within an algorithm where compiled
//! speed is more important.
//---------------------------------------------------------------------------

#ifndef _NV_TYPES_H
#define _NV_TYPES_H

#if defined LINUX || defined __linux || defined NV_UNIX || defined INTEGRITY || defined __CC_ARM || defined TENSILICA

typedef unsigned long long  U64;
typedef signed long long    S64;
typedef unsigned int        U32;
typedef signed int          S32;
typedef unsigned short      U16;
typedef signed short        S16;
typedef unsigned char       U8;
typedef signed char         S8;

#define U64_MAX 18446744073709551615ULL
#define U64_MIN 0
#define S64_MAX 9223372036854775807LL
#define S64_MIN (-S64_MAX - 1)
#define U32_MAX 4294967295UL
#define U32_MIN 0
#define S32_MAX 2147483647L
#define S32_MIN (-S32_MAX - 1)
#define U16_MAX 65535
#define U16_MIN 0
#define S16_MAX 32767
#define S16_MIN (-S16_MAX - 1)
#define U8_MAX  255
#define U8_MIN  0
#define S8_MAX  127
#define S8_MIN  (-S8_MAX - 1)

#elif defined QNX || defined VXWORKS //FIXME: verify all the data types

typedef unsigned long long int U64;
typedef signed long long int   S64;
typedef unsigned int        U32;
typedef signed int          S32;
typedef unsigned short      U16;
typedef signed short        S16;
typedef unsigned char       U8;
typedef signed char         S8;

#define U64_MAX 18446744073709551615ULL
#define U64_MIN 0
#define S64_MAX 9223372036854775807LL
#define S64_MIN (-S64_MAX - 1)
#define U32_MAX 4294967295UL
#define U32_MIN 0
#define S32_MAX 2147483647L
#define S32_MIN (-S32_MAX - 1)
#define U16_MAX 65535
#define U16_MIN 0
#define S16_MAX 32767
#define S16_MIN (-S16_MAX - 1)
#define U8_MAX  255
#define U8_MIN  0
#define S8_MAX  127
#define S8_MIN  (-S8_MAX - 1)

#elif defined UNDER_CE

typedef unsigned __int64    U64;
typedef signed __int64      S64;
typedef unsigned int        U32;
typedef signed int          S32;
typedef unsigned short      U16;
typedef signed short        S16;
typedef unsigned char       U8;
typedef signed char         S8;

#define U64_MAX 18446744073709551615 /* WARNING! WINCE doesn't allow "ULL" suffix - this value may be truncated */
#define U64_MIN 0
#define S64_MAX 9223372036854775807LL
#define S64_MIN (-S64_MAX - 1)
#define U32_MAX 4294967295UL
#define U32_MIN 0
#define S32_MAX 2147483647L
#define S32_MIN (-S32_MAX - 1)
#define U16_MAX 65535
#define U16_MIN 0
#define S16_MAX 32767
#define S16_MIN (-S16_MAX - 1)
#define U8_MAX  255
#define U8_MIN  0
#define S8_MAX  127
#define S8_MIN  (-S8_MAX - 1)

#elif defined WIN32 || defined _WIN32

typedef unsigned __int64    U64;
typedef signed __int64      S64;
typedef unsigned int        U32;
typedef signed int          S32;
typedef unsigned short      U16;
typedef signed short        S16;
typedef unsigned char       U8;
typedef signed char         S8;

#define U64_MAX 18446744073709551615ULL
#define U64_MIN 0
#define S64_MAX 9223372036854775807LL
#define S64_MIN (-S64_MAX - 1)
#define U32_MAX 4294967295UL
#define U32_MIN 0
#define S32_MAX 2147483647L
#define S32_MIN (-S32_MAX - 1)
#define U16_MAX 65535
#define U16_MIN 0
#define S16_MAX 32767
#define S16_MIN (-S16_MAX - 1)
#define U8_MAX  255
#define U8_MIN  0
#define S8_MAX  127
#define S8_MIN  (-S8_MAX - 1)

#elif defined __APPLE__ || defined __MACOSX

typedef unsigned long long  U64;
typedef signed long long    S64;
typedef unsigned int        U32;
typedef signed int          S32;
typedef unsigned short      U16;
typedef signed short        S16;
typedef unsigned char       U8;
typedef signed char         S8;

#define U64_MAX 18446744073709551615ULL
#define U64_MIN 0
#define S64_MAX 9223372036854775807LL
#define S64_MIN (-S64_MAX - 1)
#define U32_MAX 4294967295UL
#define U32_MIN 0
#define S32_MAX 2147483647L
#define S32_MIN (-S32_MAX - 1)
#define U16_MAX 65535
#define U16_MIN 0
#define S16_MAX 32767
#define S16_MIN (-S16_MAX - 1)
#define U8_MAX  255
#define U8_MIN  0
#define S8_MAX  127
#define S8_MIN  (-S8_MAX - 1)

#else

#error Unknown platform.

// The following is for documentation only.

//! \brief Unsigned 64 bits.
//! Use sparingly since some platforms may emulate this.
typedef unsigned long long  U64;

//! \brief Signed 64 bits.
//! Use sparingly since some platforms may emulate this.
typedef signed long long    S64;

//! \brief Unsigned 32 bits.
typedef unsigned int        U32;

//! \brief Signed 32 bits.
typedef signed int          S32;

//! \brief Unsigned 16 bits.
typedef unsigned short      U16;

//! \brief Signed 16 bits.
typedef signed short        S16;

//! \brief Unsigned 8 bits.
//! Note that some platforms char is signed and on other it is unsigned. Use
//! of U8 or S8 is highly recommended.
typedef unsigned char       U8;

//! \brief Signed 8 bits.
//! Note that some platforms char is signed and on other it is unsigned. Use
//! of U8 or S8 is highly recommended.
typedef signed char         S8;

//! \brief Maximum value of a U64.
#define U64_MAX 18446744073709551615

//! \brief Minimum value of a U64.
#define U64_MIN 0

//! \brief Maximum value of an S64.
#define S64_MAX 9223372036854775807

//! \brief Minimum value of an S64.
#define S64_MIN -9223372036854775808

//! \brief Maximum value of a U32.
#define U32_MAX 4294967295

//! \brief Minimum value of a U32.
#define U32_MIN 0

//! \brief Maximum value of an S32.
#define S32_MAX 2147483647

//! \brief Minimum value of an S32.
#define S32_MIN -2147483648

//! \brief Maxmimum value of a U16.
#define U16_MAX 65535

//! \brief Minimum value of a U16.
#define U16_MIN 0

//! \brief Maximum value of an S16.
#define S16_MAX 32767

//! \brief Minimum value of an S16.
#define S16_MIN -32768

//! \brief Maximum value of a U8.
#define U8_MAX  255

//! \brief Minimum value of a U8.
#define U8_MIN  0

//! \brief Maximum value of an S8.
#define S8_MAX  127

//! \brief Minimum value of an S8.
#define S8_MIN  -128

#endif

//! \brief UTF-8 single character.
//! Used for Unicode strings formatted in UTF-8.
typedef U8  UTF8;

//! \brief UTF-16 single character.
//! Used for Unicode strings formatted in UTF-16.
typedef U16 UTF16;

typedef int BOOL;

#endif
