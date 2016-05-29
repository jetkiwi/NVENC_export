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
// NvAssert.cpp
//
// Assert that always works regardless of Debug/Release or existence of NDEBUG.
//
//---------------------------------------------------------------------------

#ifndef _NV_ASSERT_H
#define _NV_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#define _NV_ABORT \
    (*((volatile int*)0) -= 1)

#if defined __cplusplus
#    define _ASSERT_VOID_CAST static_cast<void>
#else
#    define _ASSERT_VOID_CAST (void)
#endif

#define NV_ABORT \
    _ASSERT_VOID_CAST(_NV_ABORT)

#define NV_ASSERT(expr) \
    ( \
        _ASSERT_VOID_CAST( \
            (expr) ? 0 : ( \
                fprintf(stderr, "%s:%d: NV_ASSERT failed: %s\n", __FILE__, __LINE__, #expr) && \
                _NV_ABORT \
            ) \
        ) \
    )

#endif
