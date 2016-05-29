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
// ctype.h
//
// Replacement for #include <ctype.h>
// Used to fixup and deficiencies in the platform headers
//
//---------------------------------------------------------------------------

#ifndef _COMMON_INCLUDE_STD_CTYPE_H_
#define _COMMON_INCLUDE_STD_CTYPE_H_

#include <ctype.h>

#if defined(NV_BUILD_TOOLCHAIN_RVCT)
#define isascii(c) (((c) & ~0x7f) == 0)
#endif

#endif
