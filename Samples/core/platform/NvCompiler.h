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
// Platform independent compiler keywords
//---------------------------------------------------------------------------

#ifndef _NV_COMPILER_H
#define _NV_COMPILER_H

#if defined WIN32 || defined _WIN32

#define __nvforceinline __forceinline
#define __NVFUNCTION__ __FUNCTION__

#elif defined __APPLE__ || defined __MACOSX

#define __nvforceinline inline
#define __NVFUNCTION__ __func__

#elif defined __linux || defined NV_UNIX

#define __nvforceinline inline
#define __NVFUNCTION__ __func__

#else

#error Compiler keywords unknown for this platform.

#endif

#endif // _NV_COMPILER_H
