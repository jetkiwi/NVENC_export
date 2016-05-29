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
// math.h
//
// Replacement for #include <math.h>
// Used to fixup and deficiencies in the platform headers
//
//---------------------------------------------------------------------------

#ifndef _COMMON_INCLUDE_STD_MATH_H_
#define _COMMON_INCLUDE_STD_MATH_H_

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(NV_BUILD_TOOLCHAIN_MSVC) || defined(NV_TARGET_OS_VXWORKS)
double log2(double d);
#endif

#ifdef __cplusplus
}
#endif

#endif
