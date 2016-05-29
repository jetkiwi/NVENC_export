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

/// \file NvSystemClock.h
/// \brief Abstraction layer for SystemClock.
///
/// Copyright(c) 2003 NVIDIA Corporation.

#ifndef _NV_OSAL_NV_SYSTEM_CLOCK_H
#define _NV_OSAL_NV_SYSTEM_CLOCK_H

#include <include/NvTypes.h>

#if defined WIN32 || defined _WIN32
#include <windows.h>
__inline void NVSleep(int ms) { Sleep(ms); }

#elif defined __APPLE__ || defined __MACOSX || defined __linux || defined NV_UNIX

#include <unistd.h>
__inline void NVSleep(int ms) { usleep(ms*1000); }

#else

#error NVSleep function is unknown for this platform.

#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Return the system time in microseconds (i.e. 1/1000000 of a second units)
U64 NvGetSystemClockMicrosecs();

#ifdef __cplusplus
}
#endif

#endif //_NV_OSAL_NV_SYSTEM_CLOCK_H
