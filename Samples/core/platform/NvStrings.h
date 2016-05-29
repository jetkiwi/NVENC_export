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
// Platform independent string manipulation function decorations
//---------------------------------------------------------------------------

#ifndef _NV_STRINGS_H
#define _NV_STRINGS_H

#include <string.h>

#if defined WIN32 || defined _WIN32

__inline int NvStringCaseCmp(const char* s1, const char* s2) { return _stricmp(s1, s2); }

#elif defined __APPLE__ || defined __MACOSX || defined __linux

__inline int NvStringCaseCmp(const char* s1, const char* s2) { return strcasecmp(s1, s2); }

#else

#error String manipulation functions unknown for this platform.

#endif

#endif // _NV_STRINGS_H
