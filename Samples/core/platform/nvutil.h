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

#ifndef _CODECS_INCLUDE_NVUTIL_H_
#define _CODECS_INCLUDE_NVUTIL_H_

#include "NvCallingConventions.h"
#include "NvCriticalSection.h"
#include "NvSystemClock.h"

#ifdef __cplusplus
extern "C" {
#endif

// General purpose
#ifndef FALSE
#define TRUE    1
#define FALSE   0
#endif

// 64 bit reference time type
#ifdef _MSC_VER
typedef __int64 NVTIME;
#else
typedef long long NVTIME;
#endif

// Enum's for error values
typedef enum {
    NV_SUCCESS = 0,
    NV_FAIL = 1,
    NV_NOT_FINISHED,
    NV_ERROR_ALLOC = 0x10000,
    NV_ERROR_FREE,
} NvStatus;

// Enum's for NVDebugOut client field
typedef enum {
    NV_CLIENT_AUDDEC = 0,
    NV_CLIENT_VIDDEC,
    NV_CLIENT_SPDEC,
    NV_CLIENT_L21DEC,
    NV_CLIENT_AC3ADEC,
    NV_CLIENT_MPGADEC,
    NV_CLIENT_MPGVDEC,
    NV_CLIENT_DVDSPDEC,
    NV_CLIENT_PCMADEC,
    NV_CLIENT_DXVA,
    NV_CLIENT_DTSADEC,
    NV_CLIENT_DHPAENC,
    NV_CLIENT_DPLADEC,
    NV_CLIENT_NAVMAIN,
    NV_CLIENT_NAVAUDIO,
    NV_CLIENT_NAVVIDEO,
    NV_CLIENT_NAVSP,
    NV_CLIENT_MPGDEMUX,
    NV_CLIENT_MPGMUX,
    NV_CLIENT_AUDENC,
    NV_CLIENT_MPGAENC,
    NV_CLIENT_VIDENC,
    NV_CLIENT_MPGVENC,
    NV_CLIENT_AUDIOFX,
    NV_CLIENT_ASCALER,
    NV_CLIENT_MUXFILTR,
    NV_CLIENT_NOTRACE,
    NV_CLIENT_DVDVIDEO,
    NV_CLIENT_NVAVENC,
    NV_CLIENT_TVRATING,
    NV_CLIENT_TSINFO,
    NV_CLIENT_MP3ADEC,
    NV_CLIENT_WAVADEC,
    NV_CLIENT_WMAADEC,
    NV_CLIENT_H264DEC,
    NV_CLIENT_CRYPT,
    NV_CLIENT_DEBLOCK,
    NV_CLIENT_DVDNAV,
    NV_NUM_CLIENTS
} NV_CLIENTS;

// Enum's for NVDebugOut type field
typedef enum {
    NV_TYPE_TIMING,  // Timing and performance measurements
    NV_TYPE_TRACE,   // General step point call tracing
    NV_TYPE_MEMORY,  // Memory and object allocation/destruction
    NV_TYPE_LOCKING, // Locking/unlocking of critical sections
    NV_TYPE_ERROR,   // Debug error notification
    NV_TYPE_CUSTOM1,
    NV_TYPE_CUSTOM2,
    NV_TYPE_CUSTOM3,

    NV_NUM_TYPES
} NV_TYPES;

// Enum's for NVDebugOut level field
typedef enum {
    NV_LEVEL_MAJOR = 1,
    NV_LEVEL_MINOR = 2,
    NV_LEVEL_DETAIL = 3
} NV_LEVELS;

// Logs debug messages to either a file or debug window and/or a console window
void NVDebugRegister(char * modulename, char * clientname, NV_CLIENTS client);
void NVDebugReRegister(char * modulename, char * clientname, NV_CLIENTS client);
void NVDebugUnregister(char * modulename, char * clientname, NV_CLIENTS client);
void NVDebugOut(NV_CLIENTS client, NV_TYPES type, NV_LEVELS level, char * szFormat, ...);
void NVTimeOut1(NV_CLIENTS client, NV_TYPES type, NV_LEVELS level, char * text, int ms1);
void NVTimeOut2(NV_CLIENTS client, NV_TYPES type, NV_LEVELS level, char * text, int ms1, int ms2);
void NVTimeOut3(NV_CLIENTS client, NV_TYPES type, NV_LEVELS level, char * text, int ms1, int ms2, int ms3);

// Trace functions
#ifdef NVTRACE
#define NVDRC(_x_) NVDebugRegister _x_
#define NVDRRC(_x_) NVDebugReRegister _x_
#define NVDUC(_x_) NVDebugUnregister _x_
#define NVDPF(_x_) NVDebugOut _x_
#define NVDTS1(_x_) NVTimeOut1 _x_
#define NVDTS2(_x_) NVTimeOut2 _x_
#define NVDTS3(_x_) NVTimeOut3 _x_
#else
#define NVDRC(_x_)
#define NVDRRC(_x_)
#define NVDUC(_x_)
#define NVDPF(_x_)
#define NVDTS1(_x_)
#define NVDTS2(_x_)
#define NVDTS3(_x_)
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define ARRAY_ELEMENT_COUNT(_array) (sizeof(_array) / sizeof(_array[0]))

typedef void* CORE_HANDLE;

#ifdef __cplusplus
}
#endif

#endif
