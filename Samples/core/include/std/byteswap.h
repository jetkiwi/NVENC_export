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
// byteswap.h
//
// Replacement for #include <byteswap.h>
// Used to fixup and deficiencies in the platform headers
//
//---------------------------------------------------------------------------

#ifndef _COMMON_INCLUDE_STD_BYTESWAP_H_
#define _COMMON_INCLUDE_STD_BYTESWAP_H_

#if defined(NV_TARGET_OS_LINUX)

#include <byteswap.h>

#else

#define bswap_16(_x_) ( \
    (((_x_) & 0xff) << 8) | \
    (((_x_) >> 8) & 0xff) \
)

#define bswap_32(_x_) ( \
    (((_x_) >> 24) & 0x000000ffU) | \
    (((_x_) >>  8) & 0x0000ff00U) | \
    (((_x_) <<  8) & 0x00ff0000U) | \
    (((_x_) << 24) & 0xff000000U) \
)

#endif

#endif
