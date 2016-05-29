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
// Platform independent calling convention function decorations
//---------------------------------------------------------------------------

#ifndef _COMMON_INCLUDE_CALLING_CONVENTIONS_H_
#define _COMMON_INCLUDE_CALLING_CONVENTIONS_H_

#if defined(NV_BUILD_TOOLCHAIN_MSVC) || defined(_MSC_VER)
    #define NV_CALL_CRT_CALLBACK __cdecl
    #define NV_CALL_APP_ENTRY    __cdecl
    #define NV_CALL_CONV_COM     __stdcall
#else
    #define NV_CALL_CRT_CALLBACK
    #define NV_CALL_APP_ENTRY
    #define NV_CALL_CONV_COM
#endif

#if defined(NV_TARGET_OS_VXWORKS)
    #define MAIN() \
        int main(int argc, const char** argv);                              \
        int NV_CALL_APP_ENTRY mainEntry(int nargs, ...)                     \
        {                                                                   \
            int i, ret, argc = nargs + 1;                                   \
            va_list argptr;                                                 \
            char** argv;                                                    \
                                                                            \
            if (argc < 1) {                                                 \
                argc = 1;                                                   \
            } else {                                                        \
                va_start(argptr, nargs);                                    \
            }                                                               \
                                                                            \
            argv = (char**)malloc(argc * sizeof(char*));                    \
            if (!argv) {                                                    \
                fprintf(                                                    \
                    stderr,                                                 \
                    "\nFailed to allocate space for program arguments\n");  \
                return -1;                                                  \
            }                                                               \
                                                                            \
            argv[0] = "mainEntry";                                          \
            for (i = 1; i < argc; ++i) {                                    \
                argv[i] = va_arg(argptr, char*);                            \
            }                                                               \
                                                                            \
            ret = main(argc, (const char**)argv);                           \
            free(argv);                                                     \
            return ret;                                                     \
        }                                                                   \
        int main(int argc, const char** argv)
#else
    #define MAIN() \
        int NV_CALL_APP_ENTRY main(int argc, const char** argv)
#endif

#endif
