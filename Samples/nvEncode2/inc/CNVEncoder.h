/*
 * Copyright 1993-2013 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

/**
* \file CNVEncoder.h
* \brief CNVEncoder is the Class interface for the Hardware Encoder (NV Encode API)
* \date 2011 
*  This file contains the CNvEncoder class declaration and data structures
*/
#ifndef CNVEncoder_h 
#define CNVEncoder_h 

#pragma once

#if defined __linux || defined __APPLE__ || defined __MACOSX
  #ifndef NV_UNIX
  #define NV_UNIX
  #endif
#endif

#if defined(WIN32) || defined(_WIN32) || defined(WIN64)
  #ifndef NV_WINDOWS
  #define NV_WINDOWS
  #endif
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#if defined (NV_WINDOWS)
    #include <windows.h>
    #include <d3d9.h>
    #include <d3d10_1.h>
    #include <d3d11.h>
#endif

#if defined (NV_UNIX)
    #include <stdio.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <string.h>
    #include <unistd.h>

    #include <dlfcn.h>
    #include <pthread.h>
    #include "threads/NvPthreadABI.h"
#endif

#include "include/NvTypes.h"
#include "threads/NvThreadingClasses.h"
#include "nvEncodeAPI.h"
#include <cuviddec.h> // cudaVideoChromaFormat

#include <stdint.h>
#include "guidutil2.h"

#include <cuda.h>
#include "crepackyuv.h"  // _convert_YUV420toNV12(), _convert_YUV444toY444, ...

#define MAX_ENCODERS 16

#ifndef max
#define max(a,b) (a > b ? a : b) 
#endif

#define MAX_INPUT_QUEUE  32
#define MAX_OUTPUT_QUEUE 32
#define SET_VER(configStruct, type) {configStruct.version = type##_VER;}

// {00000000-0000-0000-0000-000000000000}
static const GUID  NV_ENC_H264_PROFILE_INVALID_GUID =
{ 0x0000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

static const GUID NV_ENC_PRESET_GUID_NULL = 
{ 0x0000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };

#if defined (NV_WINDOWS)
#define NVENCAPI __stdcall
#elif defined (NV_UNIX)
#define NVENCAPI 
typedef void * HINSTANCE;
typedef S32  HRESULT;
typedef void * HANDLE;

#define FILE_CURRENT             SEEK_CUR
#define FILE_BEGIN               SEEK_SET
#define INVALID_SET_FILE_POINTER (-1)
#define S_OK                     (0)
#define E_FAIL                   (-1)
#endif

// =========================================================================================
// Encode Codec GUIDS supported by the NvEncodeAPI interface.
// =========================================================================================

#define array_length(x) (sizeof(x)/sizeof(x[0])) // number of entries in a 1-D array
#define GUID_ENTRY_I(i) {i ## _GUID, string(""#i "_GUID"), i, string(""#i"")}
#define GUID_ENTRY(guid,i) {guid ## _GUID, string(""#guid"_GUID"), i, string(""#i"")}
//#define GUID_ENTRY_V(v) {i ## _GUID, string(""#i "_GUID"), i, string(""#i"")}

const GUID NO_GUID_GUID =
{ 0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } };
const guidutil::inttype NO_GUID = 1;

typedef struct {
	//
	// Collection of all NV_ENC_CAPS properties of an NVENC codec.
	//
	// (These properties are reported by the NVENC-hardware)
	int value_NV_ENC_CAPS_NUM_MAX_BFRAMES;
    int value_NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES;
    int value_NV_ENC_CAPS_SUPPORT_FIELD_ENCODING;
    int value_NV_ENC_CAPS_SUPPORT_MONOCHROME;
    int value_NV_ENC_CAPS_SUPPORT_FMO;
    int value_NV_ENC_CAPS_SUPPORT_QPELMV;
    int value_NV_ENC_CAPS_SUPPORT_BDIRECT_MODE;
    int value_NV_ENC_CAPS_SUPPORT_CABAC;
    int value_NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM;
    //int value_NV_ENC_CAPS_SUPPORT_STEREO_MVC;
    int value_NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS;
    int value_NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES;
    int value_NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES;
    int value_NV_ENC_CAPS_LEVEL_MAX;
    int value_NV_ENC_CAPS_LEVEL_MIN;
    int value_NV_ENC_CAPS_SEPARATE_COLOUR_PLANE;
    int value_NV_ENC_CAPS_WIDTH_MAX;
    int value_NV_ENC_CAPS_HEIGHT_MAX;
    int value_NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC;
    int value_NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE;
    int value_NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE;
    int value_NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP;
    int value_NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE;
    int value_NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK;
    int value_NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING;
    int value_NV_ENC_CAPS_SUPPORT_INTRA_REFRESH;
    int value_NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE;
    int value_NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE;
    int value_NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION;
    int value_NV_ENC_CAPS_PREPROC_SUPPORT;
    int value_NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT;
	int value_NV_ENC_CAPS_MB_NUM_MAX;
	int value_NV_ENC_CAPS_MB_PER_SEC_MAX;
	int value_NV_ENC_CAPS_SUPPORT_YUV444_ENCODE;
	int value_NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE;
} nv_enc_caps_s;

typedef struct {
    int param;
    char name[256];
} param_desc;

const param_desc framefieldmode_names[] = 
{
    { 0,                                    "Invalid Frame/Field Mode" },
    { NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME, "Frame Mode"               },
    { NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD, "Field Mode"               },
    { NV_ENC_PARAMS_FRAME_FIELD_MODE_MBAFF, "MB adaptive frame/field"  }
};


const NV_ENC_PARAMS_FRAME_FIELD_MODE MODE_FRAME = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;
const NV_ENC_PARAMS_FRAME_FIELD_MODE MODE_FIELD = NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD;
const NV_ENC_PARAMS_FRAME_FIELD_MODE MODE_MBAFF = NV_ENC_PARAMS_FRAME_FIELD_MODE_MBAFF;
/*
const st_guid_entry table_nv_enc_params_frame_mode_names[] = {
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_FRAME_FIELD_MODE_MBAFF)
};*/
// kludge: shorten these names so that they don't 
//         scroll off the edge of the Premiere plugin dialog-box
const st_guid_entry table_nv_enc_params_frame_mode_names[] = {
	GUID_ENTRY(NO_GUID, MODE_FRAME),
	GUID_ENTRY(NO_GUID, MODE_FIELD),
	GUID_ENTRY(NO_GUID, MODE_MBAFF)
};

const param_desc ratecontrol_names[] =  // updated for NVENC SDK 4.0 (Aug 2014)
{
    { NV_ENC_PARAMS_RC_CONSTQP,                 "Constant QP Mode"                        },
    { NV_ENC_PARAMS_RC_VBR,                     "VBR (Variable Bitrate)"                  },
    { NV_ENC_PARAMS_RC_CBR,                     "CBR (Constant Bitrate)"                  },
    { 3,                                        "Invalid Rate Control Mode"               },
    { NV_ENC_PARAMS_RC_VBR_MINQP,               "VBR_MINQP (Variable Bitrate with MinQP)" },
    { 5,                                        "Invalid Rate Control Mode"               },
    { 6,                                        "Invalid Rate Control Mode"               },
    { 7,                                        "Invalid Rate Control Mode"               },
    { NV_ENC_PARAMS_RC_2_PASS_QUALITY,          "Two-Pass Prefered Quality Bitrate"       },
    { 9,                                        "Invalid Rate Control Mode"               },
    { 10,                                       "Invalid Rate Control Mode"               },
    { 11,                                       "Invalid Rate Control Mode"               },
    { 12,                                       "Invalid Rate Control Mode"               },
    { 13,                                       "Invalid Rate Control Mode"               },
    { 14,                                       "Invalid Rate Control Mode"               },
    { 15,                                       "Invalid Rate Control Mode"               },
    { NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP,    "Two-Pass Prefered Frame Size Bitrate"    },
    { 17,                                       "Invalid Rate Control Mode"               },
    { 18,                                       "Invalid Rate Control Mode"               },
    { 19,                                       "Invalid Rate Control Mode"               },
    { 20,                                       "Invalid Rate Control Mode"               },
    { 21,                                       "Invalid Rate Control Mode"               },
    { 22,                                       "Invalid Rate Control Mode"               },
    { 23,                                       "Invalid Rate Control Mode"               },
    { 24,                                       "Invalid Rate Control Mode"               },
    { 25,                                       "Invalid Rate Control Mode"               },
    { 26,                                       "Invalid Rate Control Mode"               },
    { 27,                                       "Invalid Rate Control Mode"               },
    { 28,                                       "Invalid Rate Control Mode"               },
    { 29,                                       "Invalid Rate Control Mode"               },
    { 30,                                       "Invalid Rate Control Mode"               },
    { 31,                                       "Invalid Rate Control Mode"               },
    { NV_ENC_PARAMS_RC_2_PASS_VBR,              "Two-Pass (Variable Bitrate)"             }
};

const st_guid_entry table_nv_enc_ratecontrol_names[] = {
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_RC_CONSTQP),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_RC_VBR),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_RC_CBR),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_RC_VBR_MINQP),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_RC_2_PASS_QUALITY),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP),
	GUID_ENTRY(NO_GUID, NV_ENC_PARAMS_RC_2_PASS_VBR)
};

const param_desc encode_picstruct_names[] = 
{
    { 0,                                    "0 = Invalid Picture Struct"                },
    { NV_ENC_PIC_STRUCT_FRAME,              "1 = Progressive Frame"                     },
    { NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM,   "2 = Top Field interlaced frame"            },
    { NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP,   "3 = Bottom Field first inerlaced frame"    },
};

/**
 *  * Input picture type
 *   */
const param_desc encode_picture_types[] =
{
    { NV_ENC_PIC_TYPE_P,             "0 = Forward predicted"                              },
    { NV_ENC_PIC_TYPE_B,             "1 = Bi-directionally predicted picture"             },
    { NV_ENC_PIC_TYPE_I,             "2 = Intra predicted picture"                        },
    { NV_ENC_PIC_TYPE_IDR,           "3 = IDR picture"                                    }, 
    { NV_ENC_PIC_TYPE_BI,            "4 = Bi-directionally predicted with only Intra MBs" },
    { NV_ENC_PIC_TYPE_SKIPPED,       "5 = Picture is skipped"                             },
    { NV_ENC_PIC_TYPE_INTRA_REFRESH, "6 = First picture in intra refresh cycle"           },
    { NV_ENC_PIC_TYPE_UNKNOWN,       "0xFF = Picture type unknown"                        } 
};

/**
 *  * Motion vector precisions
 *   */
const param_desc encode_precision_mv[] =
{
    { NV_ENC_MV_PRECISION_DEFAULT,     "0 = Default (drive selects quarterpel"   },       /**<Driver selects QuarterPel motion vector precision by default*/
    { NV_ENC_MV_PRECISION_FULL_PEL,    "1 = Full-Pel    Motion Vector precision" },
    { NV_ENC_MV_PRECISION_HALF_PEL,    "2 = Half-Pel    Motion Vector precision" }, 
    { NV_ENC_MV_PRECISION_QUARTER_PEL, "3 = Quarter-Pel Motion Vector precision" },
};

const st_guid_entry table_nv_enc_mv_precision_names[] = {
	GUID_ENTRY(NO_GUID, NV_ENC_MV_PRECISION_DEFAULT),
	GUID_ENTRY(NO_GUID, NV_ENC_MV_PRECISION_FULL_PEL),
	GUID_ENTRY(NO_GUID, NV_ENC_MV_PRECISION_HALF_PEL),
	GUID_ENTRY(NO_GUID, NV_ENC_MV_PRECISION_QUARTER_PEL)
};

typedef struct {
    GUID id;
    char name[256];
    unsigned int  value;
} guid_desc;

const guid_desc codec_names[] = 
{
    { NV_ENC_CODEC_H264_GUID, "Invalid Codec Setting" , 0},
    { NV_ENC_CODEC_H264_GUID, "Invalid Codec Setting" , 1},
    { NV_ENC_CODEC_H264_GUID, "Invalid Codec Setting" , 2},
    { NV_ENC_CODEC_H264_GUID, "Invalid Codec Setting" , 3},
    { NV_ENC_CODEC_H264_GUID, "H.264 Codec"           , 4},
	{ NV_ENC_CODEC_HEVC_GUID, "H.265 Codec"           , 5}
};

typedef enum { // updated for NVENC SDK 5.0 (Dec 2014)
	NV_ENC_CODEC_PROFILE_AUTOSELECT = 0,
    NV_ENC_H264_PROFILE_BASELINE= 66 ,
    NV_ENC_H264_PROFILE_MAIN    = 77 ,
    NV_ENC_H264_PROFILE_HIGH    = 100,
    NV_ENC_H264_PROFILE_STEREO  = 128,
	NV_ENC_H264_PROFILE_HIGH_444 = 244,
	NV_ENC_H264_PROFILE_CONSTRAINED_HIGH = 257,
	NV_ENC_HEVC_PROFILE_MAIN     = 300
} enum_NV_ENC_H264_PROFILE;

const guid_desc codecprofile_names[] =  // updated for NVENC SDK 5.0 (Dec 2014)
{
	{ NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID,     "H.264/H.EVC Auto", NV_ENC_CODEC_PROFILE_AUTOSELECT },
    { NV_ENC_H264_PROFILE_BASELINE_GUID,        "H.264 Baseline", NV_ENC_H264_PROFILE_BASELINE },
    { NV_ENC_H264_PROFILE_MAIN_GUID,            "H.264 Main Profile", NV_ENC_H264_PROFILE_MAIN },
    { NV_ENC_H264_PROFILE_HIGH_GUID,            "H.264 High Profile", NV_ENC_H264_PROFILE_HIGH },
    { NV_ENC_H264_PROFILE_STEREO_GUID,          "H.264 Stereo Profile", NV_ENC_H264_PROFILE_STEREO },
    { NV_ENC_H264_PROFILE_HIGH_444_GUID,        "H.264 444 Profile", NV_ENC_H264_PROFILE_HIGH_444 }, // NVENC 4.0 API
    { NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID,"H.264 Constrained High Profile", NV_ENC_H264_PROFILE_CONSTRAINED_HIGH },
	{ NV_ENC_HEVC_PROFILE_MAIN_GUID,            "H.265 Main Profile", NV_ENC_HEVC_PROFILE_MAIN } // NVENC 5.0 API
};

const guid_desc preset_names[] =  // updated for NVENC SDK 4.0 (Aug 2014)
{
    { NV_ENC_PRESET_DEFAULT_GUID,                               "Default Preset",  0},
    { NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID,                   "Low Latancy Default Preset", 1 },
    { NV_ENC_PRESET_HP_GUID,                                    "High Performance (HP) Preset", 2},
    { NV_ENC_PRESET_HQ_GUID,                                    "High Quality (HQ) Preset", 3 },
    { NV_ENC_PRESET_BD_GUID,                                    "Blue Ray Preset", 4 },
    { NV_ENC_PRESET_LOW_LATENCY_HQ_GUID,                        "Low Latancy High Quality (HQ) Preset", 5 },
    { NV_ENC_PRESET_LOW_LATENCY_HP_GUID,                        "Low Latancy High Performance (HP) Preset", 6 },
    { NV_ENC_PRESET_GUID_NULL,                                  "Reserved Preset", 7},
    { NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID,                      "Lossless Default Preset", 8 },
    { NV_ENC_PRESET_LOSSLESS_HP_GUID,                           "Lossless (HP) Preset", 9 }
};


typedef enum {
	NV_ENC_CODEC_H264  = 4,
	NV_ENC_CODEC_HEVC  = 5
} enum_NV_ENC_CODEC;

const st_guid_entry table_nv_enc_codec_names[] = {
	GUID_ENTRY_I( NV_ENC_CODEC_H264  ),
	GUID_ENTRY_I( NV_ENC_CODEC_HEVC  )
};

// table_nv_enc_profile_names[] must be aligned with codecprofile_names[]
// (i.e. entries must be listed in the exact same order
const st_guid_entry table_nv_enc_profile_names[] = {
	GUID_ENTRY_I( NV_ENC_CODEC_PROFILE_AUTOSELECT),
	GUID_ENTRY_I( NV_ENC_H264_PROFILE_BASELINE ),
	GUID_ENTRY_I( NV_ENC_H264_PROFILE_MAIN ),
	GUID_ENTRY_I( NV_ENC_H264_PROFILE_HIGH ),
	GUID_ENTRY_I( NV_ENC_H264_PROFILE_STEREO ),
	GUID_ENTRY_I( NV_ENC_H264_PROFILE_HIGH_444 ),
	GUID_ENTRY_I( NV_ENC_H264_PROFILE_CONSTRAINED_HIGH ),
	GUID_ENTRY_I( NV_ENC_HEVC_PROFILE_MAIN )
};

typedef enum
{
    NV_ENC_PRESET_DEFAULT                   =0, // Default Preset
    NV_ENC_PRESET_LOW_LATENCY_DEFAULT       =1, // Low Latancy Default Preset", 1 },
    NV_ENC_PRESET_HP                        =2, // High Performance (HP) Preset
    NV_ENC_PRESET_HQ                        =3, // High Quality (HQ) Preset
    NV_ENC_PRESET_BD                        =4, // Blue Ray Preset
    NV_ENC_PRESET_LOW_LATENCY_HQ            =5, // Low Latancy High Quality (HQ) Preset
    NV_ENC_PRESET_LOW_LATENCY_HP            =6, // Low Latancy High Performance (HP) Preset
    NV_ENC_PRESET_LOSSLESS_DEFAULT          =8,
    NV_ENC_PRESET_LOSSLESS_HP               =9
} enum_NV_ENC_PRESET;

const st_guid_entry table_nv_enc_preset_names[] = {
	GUID_ENTRY_I( NV_ENC_PRESET_DEFAULT ),
	GUID_ENTRY_I( NV_ENC_PRESET_LOW_LATENCY_DEFAULT ),
	GUID_ENTRY_I( NV_ENC_PRESET_HP ),
	GUID_ENTRY_I( NV_ENC_PRESET_HQ ),
	GUID_ENTRY_I( NV_ENC_PRESET_BD ), 
	GUID_ENTRY_I( NV_ENC_PRESET_LOW_LATENCY_HQ ), 
	GUID_ENTRY_I( NV_ENC_PRESET_LOW_LATENCY_HP ),
	GUID_ENTRY_I( NV_ENC_PRESET_LOSSLESS_DEFAULT ),
    GUID_ENTRY_I( NV_ENC_PRESET_LOSSLESS_HP )
};

const st_guid_entry table_nv_enc_buffer_format_names[] = {
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_UNDEFINED          ) ,    /**< Undefined buffer format. */

    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_NV12_PL            ) ,    /**< Semi-Planar YUV [UV interleaved] allocated as serial 2D buffer. */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_NV12_TILED16x16    ) ,    /**< Semi-Planar YUV [UV interleaved] allocated as 16x16 tiles.      */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_NV12_TILED64x16    ) ,    /**< Semi-Planar YUV [UV interleaved] allocated as 64x16 tiles.      */

    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_YV12_PL            ) ,   /**< Planar YUV [YUV separate planes] allocated as serial 2D buffer. */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_YV12_TILED16x16    ) ,   /**< Planar YUV [YUV separate planes] allocated as 16x16 tiles.      */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_YV12_TILED64x16    ) ,   /**< Planar YUV [YUV separate planes] allocated as 64x16 tiles.      */

    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_IYUV_PL            ) ,  /**< Packed YUV [YUV separate bytes per pixel] allocated as serial 2D buffer. */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_IYUV_TILED16x16    ) ,  /**< Packed YUV [YUV separate bytes per pixel] allocated as 16x16 tiles.      */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_IYUV_TILED64x16    ) ,  /**< Packed YUV [YUV separate bytes per pixel] allocated as 64x16 tiles.      */

    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_YUV444_PL          ) , /**< Planar YUV [YUV separate bytes per pixel] allocated as serial 2D buffer. */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_YUV444_TILED16x16  ) , /**< Planar YUV [YUV separate bytes per pixel] allocated as 16x16 tiles.      */
    GUID_ENTRY( NO_GUID, NV_ENC_BUFFER_FORMAT_YUV444_TILED64x16  ) , /**< Planar YUV [YUV separate bytes per pixel] allocated as 64x16 tiles.      */
};

const st_guid_entry table_cudaVideoChromaFormat_names[] = {
	GUID_ENTRY( NO_GUID, cudaVideoChromaFormat_Monochrome ),
	GUID_ENTRY( NO_GUID, cudaVideoChromaFormat_420        ),
	GUID_ENTRY( NO_GUID, cudaVideoChromaFormat_422        ),
	GUID_ENTRY( NO_GUID, cudaVideoChromaFormat_444        )
};

const st_guid_entry table_nv_enc_level_h264_names[] = {
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_AUTOSELECT         ) ,
    
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_1             ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_1b            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_11            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_12            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_13            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_2             ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_21            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_22            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_3             ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_31            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_32            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_4             ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_41            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_42            ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_5             ) ,
    GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_H264_51            )
};

const st_guid_entry table_nv_enc_level_hevc_names[] = {
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_AUTOSELECT         ),

	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_1             ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_2             ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_21            ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_3             ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_31            ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_4             ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_41            ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_5             ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_51            ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_52            ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_6             ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_61            ),
	GUID_ENTRY( NO_GUID, NV_ENC_LEVEL_HEVC_62            )
};

const st_guid_entry table_nv_enc_tier_hevc_names[] = {
	GUID_ENTRY( NO_GUID, NV_ENC_TIER_HEVC_MAIN           ),
    GUID_ENTRY( NO_GUID, NV_ENC_TIER_HEVC_HIGH           )
};

/**
*  HEVC CU SIZE
*/
const st_guid_entry table_nv_enc_hevc_cusize_names[] = {
	GUID_ENTRY( NO_GUID, NV_ENC_HEVC_CUSIZE_AUTOSELECT ),
	GUID_ENTRY( NO_GUID, NV_ENC_HEVC_CUSIZE_8x8        ),
	GUID_ENTRY( NO_GUID, NV_ENC_HEVC_CUSIZE_16x16      ),
	GUID_ENTRY( NO_GUID, NV_ENC_HEVC_CUSIZE_32x32      ),
	GUID_ENTRY( NO_GUID, NV_ENC_HEVC_CUSIZE_64x64      )
};

const st_guid_entry table_nv_enc_h264_fmo_names[] = {
	GUID_ENTRY( NO_GUID, NV_ENC_H264_FMO_AUTOSELECT          ) ,          /**< FMO usage is auto selected by the encoder driver */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_FMO_ENABLE              ) ,          /**< Enable FMO */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_FMO_DISABLE             )            /**< Disble FMO */
};

const st_guid_entry table_nv_enc_h264_entropy_coding_mode_names[] = {
    GUID_ENTRY( NO_GUID, NV_ENC_H264_ENTROPY_CODING_MODE_AUTOSELECT ) ,   /**< Entropy coding mode is auto selected by the encoder driver */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_ENTROPY_CODING_MODE_CABAC      ) ,   /**< Entropy coding mode is CABAC */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC      )     /**< Entropy coding mode is CAVLC */
};

const st_guid_entry table_nv_enc_adaptive_transform_names[] = {
    GUID_ENTRY( NO_GUID, NV_ENC_H264_ADAPTIVE_TRANSFORM_AUTOSELECT ) ,   /**< Adaptive Transform 8x8 mode is auto selected by the encoder driver*/
    GUID_ENTRY( NO_GUID, NV_ENC_H264_ADAPTIVE_TRANSFORM_DISABLE    ) ,   /**< Adaptive Transform 8x8 mode disabled */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_ADAPTIVE_TRANSFORM_ENABLE     )     /**< Adaptive Transform 8x8 mode should be used */
};

const st_guid_entry table_nv_enc_stereo_packing_mode_names[] = {
    GUID_ENTRY( NO_GUID, NV_ENC_STEREO_PACKING_MODE_NONE             ) ,  /**< No Stereo packing required */
    GUID_ENTRY( NO_GUID, NV_ENC_STEREO_PACKING_MODE_CHECKERBOARD     ) ,  /**< Checkerboard mode for packing stereo frames */
    GUID_ENTRY( NO_GUID, NV_ENC_STEREO_PACKING_MODE_COLINTERLEAVE    ) ,  /**< Column Interleave mode for packing stereo frames */
    GUID_ENTRY( NO_GUID, NV_ENC_STEREO_PACKING_MODE_ROWINTERLEAVE    ) ,  /**< Row Interleave mode for packing stereo frames */
    GUID_ENTRY( NO_GUID, NV_ENC_STEREO_PACKING_MODE_SIDEBYSIDE       ) ,  /**< Side-by-side mode for packing stereo frames */
    GUID_ENTRY( NO_GUID, NV_ENC_STEREO_PACKING_MODE_TOPBOTTOM        ) ,  /**< Top-Bottom mode for packing stereo frames */
    GUID_ENTRY( NO_GUID, NV_ENC_STEREO_PACKING_MODE_FRAMESEQ         )    /**< Frame Sequential mode for packing stereo frames */
};

const st_guid_entry table_nv_enc_h264_bdirect_mode_names[] = {
    GUID_ENTRY( NO_GUID, NV_ENC_H264_BDIRECT_MODE_AUTOSELECT ),          /**< BDirect mode is auto selected by the encoder driver */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_BDIRECT_MODE_DISABLE    ),          /**< Disable BDirect mode */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_BDIRECT_MODE_TEMPORAL   ),          /**< Temporal BDirect mode */
    GUID_ENTRY( NO_GUID, NV_ENC_H264_BDIRECT_MODE_SPATIAL    )           /**< Spatial BDirect mode */
};


inline bool compareGUIDs(const GUID &guid1, const GUID &guid2)
{
    if (guid1.Data1    == guid2.Data1 &&
        guid1.Data2    == guid2.Data2 &&
        guid1.Data3    == guid2.Data3 &&
        guid1.Data4[0] == guid2.Data4[0] &&
        guid1.Data4[1] == guid2.Data4[1] &&
        guid1.Data4[2] == guid2.Data4[2] &&
        guid1.Data4[3] == guid2.Data4[3] &&
        guid1.Data4[4] == guid2.Data4[4] &&
        guid1.Data4[5] == guid2.Data4[5] &&
        guid1.Data4[6] == guid2.Data4[6] &&
        guid1.Data4[7] == guid2.Data4[7]) {
        return true;
    }
    return false;
}

inline void printGUID(int i, GUID *id) 
{
    printf("GUID[%d]: %08X-%04X-%04X-%08X", i, id->Data1, id->Data2, id->Data3, *((unsigned int *)id->Data4));
}

inline void printPresetGUIDName(GUID guid)
{
    int loopCnt = sizeof(preset_names)/ sizeof(guid_desc);
    for (int cnt = 0; cnt < loopCnt; cnt++)
    {
        if (compareGUIDs(preset_names[cnt].id, guid))
        {
            printf(" \"%s\"\n", preset_names[cnt].name);
        }
    }
}

inline void printProfileGUIDName(GUID guid)
{
    int loopCnt = sizeof(codecprofile_names)/ sizeof(guid_desc);
    for (int cnt = 0; cnt < loopCnt; cnt++)
    {
        if (compareGUIDs(codecprofile_names[cnt].id, guid))
        {
            printf(" \"%s\"\n", codecprofile_names[cnt].name);
        }
    }
}

typedef enum _NvEncodeCompressionStd
{
    NV_ENC_Unknown=-1,
    NV_ENC_H264=4,      // 14496-10 (AVC)
	NV_ENC_H265=5       // HEVC 
}NvEncodeCompressionStd;

typedef enum _NvEncodeInterfaceType
{
    NV_ENC_DX9=0,
    NV_ENC_DX11=1,
    NV_ENC_CUDA=2, // On Linux, CUDA is the only NVENC interface available
    NV_ENC_DX10=3,
} NvEncodeInterfaceType;

const param_desc nvenc_interface_names[] =
{
    { NV_ENC_DX9,   "DirectX9"  },
    { NV_ENC_DX11,  "DirectX11" },
    { NV_ENC_CUDA,  "CUDA"      },
    { NV_ENC_DX10,  "DirectX10" }
};

struct EncodeConfig
{
	bool                      enabled;// (for nvEncode2 application); this encoder is enabled
    NvEncodeCompressionStd    codec;  // H.264, VC-1, MPEG-2, etc.
    unsigned int              profile;// Base Profile, Main Profile, High Profile, etc.
    unsigned int              width;  // video-resolution: X
    unsigned int              height; // video-resolution: Y
    unsigned int              frameRateNum;// fps (Numerator  )
    unsigned int              frameRateDen;// fps (Denominator)
    unsigned int              darRatioX;   // Display Aspect Ratio: X
    unsigned int              darRatioY;   // Display Aspect Ratio: Y
    unsigned int              avgBitRate;  // Average Bitrate (for CBR, VBR rateControls)
    unsigned int              peakBitRate; // Peak BitRate (for VBR rateControl)
    unsigned int              gopLength;   // (#frames) per Group-of-Pictures
    unsigned int              numBFrames;  // Max# B-frames
    NV_ENC_PARAMS_FRAME_FIELD_MODE FieldEncoding;// Field-encoding mode (field/Progressive/MBAFF)
    unsigned int              rateControl; // 0= QP, 1= CBR. 2= VBR

	// parameters for ConstQP rateControl-mode
    unsigned int              qp;  // ConstQP (this one isn't used?)
    unsigned int              qpI; // constQP Quality: I-frame (for ConstQP rateControl)
    unsigned int              qpP; // constQP Quality: P-frame (...)
    unsigned int              qpB; // constQP Quality: B-frame

	// parameters for VariableQP rateControl-mode
    bool                      min_qp_ena;// flag: enable min_qp?
    unsigned int              min_qpI; // min_QP Quality: I-frame
    unsigned int              min_qpP; // min_QP Quality: P-frame (...)
    unsigned int              min_qpB; // min_QP Quality: B-frame
    bool                      max_qp_ena;// flag: enable max_qp?
    unsigned int              max_qpI; // max_QP Quality: I-frame
    unsigned int              max_qpP; // max_QP Quality: P-frame (...)
    unsigned int              max_qpB; // max_QP Quality: B-frame
    bool                      initial_qp_ena;// flag: enable initial_qp?
    unsigned int              initial_qpI; // initial_QP Quality: I-frame
    unsigned int              initial_qpP; // initial_QP Quality: P-frame (...)
    unsigned int              initial_qpB; // initial_QP Quality: B-frame

    NV_ENC_H264_FMO_MODE      enableFMO;   // flexible macroblock ordering (Baseline profile)
    unsigned int              application; // 0=default, 1= HP, 2= HQ, 3=VC, 4=WIDI, 5=Wigig, 6=FlipCamera, 7=BD, 8=IPOD
    FILE                     *fOutput; // file output pointer
    int                       hierarchicalP;// enable hierarchial P
    int                       hierarchicalB;// enable hierarchial B
    int                       svcTemporal; //
    unsigned int              numlayers;
    int                       outBandSPSPPS;
    unsigned int              viewId;  // (stereo/MVC only) view ID
    unsigned int              numEncFrames;// #frames to encode (not used)
    int                       stereo3dMode;
    int                       stereo3dEnable;
	unsigned int              sliceMode; // always use 3 (divide a picture into <sliceModeData> slices)
    int                       sliceModeData;
	unsigned int              vbvBufferSize;
    unsigned int              vbvInitialDelay;
    unsigned int              level;     // encode level setting (41 = bluray)
	unsigned int              idr_period;// I-frame don't re-use period
    NV_ENC_H264_ENTROPY_CODING_MODE vle_entropy_mode;// (high-profile only) enable CABAC
    //NV_ENC_BUFFER_FORMAT      chromaFormatIDC;// chroma format (typo, wrong typedef?)
	cudaVideoChromaFormat     chromaFormatIDC;// chroma format
	unsigned int              separateColourPlaneFlag; // (for YUV444 only)
    unsigned int              output_sei_BufferPeriod;
    NV_ENC_MV_PRECISION       mvPrecision; // 1=FULL_PEL, 2=HALF_PEL, 3= QUARTER_PEL
    int                       output_sei_PictureTime;

    unsigned int              aud_enable;
    unsigned int              report_slice_offsets;
    unsigned int              enableSubFrameWrite;
    unsigned int              disableDeblock;
    unsigned int              disable_ptd;
    NV_ENC_H264_ADAPTIVE_TRANSFORM_MODE adaptive_transform_mode;
	NV_ENC_H264_BDIRECT_MODE  bdirectMode;// 0=auto, 1=disable, 2 = temporal, 3=spatial
    int                       preset;
    unsigned int              maxWidth;
    unsigned int              maxHeight;
    unsigned int              curWidth;
    unsigned int              curHeight;
    int                       syncMode; // 1==async mode, 0==sync mode
    NvEncodeInterfaceType     interfaceType;// select iftype: DirectX9, DirectX10, DirectX11, or CUDA
    int                       disableCodecCfg;
    unsigned int              useMappedResources;// enable 
    unsigned int              max_ref_frames; // maximum #reference frames (2 required for B-frames)
    unsigned int              monoChromeEncoding;// monoChrome encoding

	unsigned int              enableLTR;    // long term reference frames (only supported in HEVC)
	unsigned int              ltrNumFrames;
	unsigned int              ltrTrustMode;

	//NVENC API v3.0
	unsigned int              enableVFR; // enable variable frame rate mode

	//NVENC API v4.0
	unsigned int              qpPrimeYZeroTransformBypassFlag;// (for lossless encoding: set to 1, else set 0)
	unsigned int              enableAQ; // enable adaptive quantization

	//NVENC API v5.0
	unsigned int              tier;      // (HEVC only) encode tier setting (0 = main)
	NV_ENC_HEVC_CUSIZE        minCUsize; // (HEVC only) minimum coding unit size
	NV_ENC_HEVC_CUSIZE        maxCUsize; // (HEVC only) maximum coding unit size

	void print(string &stringout) const;
};

struct EncodeInputSurfaceInfo
{
    unsigned int      dwWidth;
    unsigned int      dwHeight;
    unsigned int      dwLumaOffset;
    unsigned int      dwChromaOffset;
    void              *hInputSurface;
    unsigned int      lockedPitch;    // #bytes from scanline[y] to scanline[y+1]
    NV_ENC_BUFFER_FORMAT bufferFmt;   // framebuffer pixelformat (NV12, Y444, etc.)
    void              *pExtAlloc;     // framebuffer in CUDA Device Memory
    unsigned char     *pExtAllocHost; // framebuffer in HostMemory
    unsigned int      dwCuPitch;
    NV_ENC_INPUT_RESOURCE_TYPE type;  
    void              *hRegisteredHandle; 
};

struct EncodeOutputBuffer
{
    unsigned int     dwSize;
    unsigned int     dwBitstreamDataSize;
    void             *hBitstreamBuffer;
    HANDLE           hOutputEvent;
    bool             bWaitOnEvent;
    void             *pBitstreamBufferPtr;
    bool             bEOSFlag;
    bool             bDynResChangeFlag;
};

struct EncoderThreadData
{
    EncodeOutputBuffer      *pOutputBfr; 
    EncodeInputSurfaceInfo  *pInputBfr;
};

#define DYN_DOWNSCALE 1
#define DYN_UPSCALE   2

struct EncodeFrameConfig
{
    unsigned char *yuv[3];
    unsigned int stride[3];
    unsigned int width;
    unsigned int height;
    unsigned int fieldPicflag;
    unsigned int topField;
    unsigned int viewId;
    unsigned int dynResChangeFlag;
    unsigned int newWidth;
    unsigned int newHeight;
    unsigned int dynBitrateChangeFlag;
	uint32_t     ppro_pixelformat; // for EncodeFramePPro() [used by Adobe apps only]
	bool         ppro_pixelformat_is_yuv420;
	bool         ppro_pixelformat_is_uyvy422;
	bool         ppro_pixelformat_is_yuyv422;
	bool         ppro_pixelformat_is_yuv444;
};

struct FrameThreadData
{
    HANDLE        hInputYUVFile;
    unsigned int  dwFileWidth;
    unsigned int  dwFileHeight;
    unsigned int  dwSurfWidth;
    unsigned int  dwSurfHeight;
    unsigned int  dwFrmIndex;
    void          *pYUVInputFrame; 
};

struct EncoderGPUInfo
{
    char gpu_name[256];
    unsigned int device;
};

class CNvEncoderThread;

// The main Encoder Class interface
class CNvEncoder
{
public:
    CNvEncoder();
    virtual ~CNvEncoder();
    CUcontext                                            m_cuContext; // videodecoder needs to share this context

	//
	// Some hooks for Premiere Pro Plugin
	//
	// fwrite_callback_t : a function-pointer to the stdio function: fwrite(),
	//    the last argument '*privateData' is so that the CNvEncoder can pass a pointer to the plugin's data-struct
	//    out to the fwrite_callback function.  I.e., so the fwrite_callback function has the necessary context
	//    to be able to access the correct Premiere FileHandle.
	//
	void                                                *m_privateData; // private-data for Premiere Pro Plugin
	typedef size_t (*fwrite_callback_t)(void * _Str, size_t _Size, size_t _Count, FILE * _File, void *privateData);

public: // temp-hack
	// most of these vars should be protected, but leave them public so Premiere Plugin has easy access
	// (too lazy to friend all the necessary functions)

    void                                                *m_hEncoder;
	int unsigned                                         m_deviceID; // GPU inedex (CUDA DeviceID)
#if defined (NV_WINDOWS)
    IDirect3D9*                                          m_pD3D;
    IDirect3DDevice9*                                    m_pD3D9Device;
    ID3D10Device*                                        m_pD3D10Device;
    ID3D11Device*                                        m_pD3D11Device;
#endif
    unsigned int                                         m_dwEncodeGUIDCount;// Number of Encoding GUIDs (codecs)
	GUID                                                *m_stEncodeGUIDArray;// List of Encoding GUIDs (H264, VC-1, MPEG-2, etc.)
    GUID                                                 m_stEncodeGUID;     // codec choice (H264, VC1, MPEG-2, etc.)
    unsigned int                                         m_dwCodecProfileGUIDCount; // List of Encoding Profiles (base, main, high ,etc.)
    GUID                                                 m_stCodecProfileGUID;// profile choice (H264: base, main, high, etc.)
    GUID                                                *m_stCodecProfileGUIDArray;
    unsigned int                                         m_stPresetIdx;      // encoding preset (index#)
    GUID                                                 m_stPresetGUID;     // encoding preset choice
    unsigned int                                         m_encodersAvailable;
    unsigned int                                         m_dwInputFmtCount;
    NV_ENC_BUFFER_FORMAT                                *m_pAvailableSurfaceFmts; // input framebuffer formats (NV12, Y444, etc.)
    unsigned int                                         m_dwCodecPresetGUIDCount; // #entries in m_st_CodecPresetGUIDArray
    GUID                                                *m_stCodecPresetGUIDArray; // list of support presets
    NV_ENC_BUFFER_FORMAT                                 m_dwInputFormat;
    NV_ENC_INITIALIZE_PARAMS                             m_stInitEncParams;
    NV_ENC_RECONFIGURE_PARAMS                            m_stReInitEncParams;
    NV_ENC_CONFIG                                        m_stEncodeConfig;
    NV_ENC_PRESET_CONFIG                                 m_stPresetConfig;
    NV_ENC_PIC_PARAMS                                    m_stEncodePicParams;
    bool                                                 m_bEncoderInitialized;
    EncodeConfig                                         m_stEncoderInput;
    EncodeInputSurfaceInfo                               m_stInputSurface[MAX_INPUT_QUEUE];
    EncodeOutputBuffer                                   m_stBitstreamBuffer[MAX_OUTPUT_QUEUE];
    CNvQueue<EncodeInputSurfaceInfo*, MAX_INPUT_QUEUE>   m_stInputSurfQueue;
    CNvQueue<EncodeOutputBuffer*, MAX_OUTPUT_QUEUE>      m_stOutputSurfQueue;
    unsigned int                                         m_dwMaxSurfCount;
    unsigned int                                         m_dwCurrentSurfIdx;
    unsigned int                                         m_dwFrameWidth;
    unsigned int                                         m_dwFrameHeight;
    unsigned int                                         m_uRefCount;
    FILE                                                *m_fOutput;
    FILE                                                *m_fInput;
    CNvEncoderThread                                    *m_pEncoderThread;
    unsigned char                                       *m_pYUV[3];
    bool                                                 m_bAsyncModeEncoding; // only avialable on Windows Platforms
	bool                                                 m_useExternalContext;
    unsigned char                                        m_pUserData[128];
    NV_ENC_SEQUENCE_PARAM_PAYLOAD                        m_spspps;
    EncodeOutputBuffer                                   m_stEOSOutputBfr; 
    CNvQueue<EncoderThreadData, MAX_OUTPUT_QUEUE>        m_pEncodeFrameQueue;

public:
    virtual void                                         UseExternalCudaContext(const CUcontext context, const unsigned int deviceID);
    virtual HRESULT                                      OpenEncodeSession(const EncodeConfig encodeConfig, const unsigned int deviceID, NVENCSTATUS &nvencstatus);

	// QueryEncodeSession() : opens a new encode-session to get its capabilities and return it to the caller.
	//                        automatically closes the encode-session.
	//                        *This method should NOT be called if an Encodesession is already open!*
	virtual NVENCSTATUS                                  QueryEncodeSession(const unsigned int deviceID, nv_enc_caps_s &nv_enc_caps, const bool destroy_on_exit = true);
	virtual NVENCSTATUS                                  QueryEncodeSessionCodec(const unsigned int deviceID, const NvEncodeCompressionStd codec, nv_enc_caps_s &nv_enc_caps);
    virtual HRESULT                                      InitializeEncoder() = 0;
    //virtual HRESULT                                      InitializeEncoderH264( NV_ENC_CONFIG_H264_VUI_PARAMETERS *pvui ) = 0;
	virtual HRESULT                                      InitializeEncoderCodec(void * const p) = 0;
	//virtual HRESULT                                      EncodeFrame(EncodeFrameConfig *pEncodeFrame, bool bFlush = false) = 0;
	virtual HRESULT                                      EncodeFramePPro(EncodeFrameConfig *pEncodeFrame, const bool bFlush) = 0;
    virtual HRESULT                                      EncodeCudaMemFrame(EncodeFrameConfig *pEncodeFrame, CUdeviceptr oFrame[], const unsigned int oFrame_pitch, bool bFlush=false) = 0;
    virtual HRESULT                                      DestroyEncoder() = 0;
   
    virtual HRESULT                                      CopyBitstreamData(EncoderThreadData stThreadData);
    virtual HRESULT                                      CopyFrameData(FrameThreadData stFrameData);

	HRESULT                                              QueryEncodeCaps(NV_ENC_CAPS caps_type, int *p_nCapsVal);
	void PrintGUID( const GUID &guid) const;
	void PrintEncodeFormats(string &s) const;
	void PrintEncodeProfiles(string &s) const;
	void PrintEncodePresets(string &s) const;
	void PrintBufferFormats(string &s) const; // print list of supported _NV_ENC_BUFFER_FORMAT

	// Query all NV_ENC_CAP properties of an already opened EncodeSession.
	HRESULT  QueryEncoderCaps( nv_enc_caps_s &nv_enc_caps ); // reutrn caps for currently configured codec (m_stEncodeGUID)
	HRESULT  _QueryEncoderCaps(const GUID &codecGUID, nv_enc_caps_s &nv_enc_caps);// return caps for user-requested codecGUID
	
	// Set the codec-type (H264 or HEVC): this must be called after InitializeEncoder(),
	// and before OpenEncodeSession()
	bool SetCodec(const enum_NV_ENC_CODEC codec);
	enum_NV_ENC_CODEC GetCodec() const;

	// initEncoderConfig - initialize p_nvEncoderConfig to default values for encoding H.264 video
	//    Arguments
	//     p_nvEncoderConfig    valid pointer to NvEncodeConfig struct
	void initEncoderConfig(EncodeConfig *p_EncoderConfig); // (For PPro plugin, initialize the caller supplied p_nvEncoderConfig)
    HRESULT                                              GetPresetConfig(const int iPresetIdx);	

	// the fwrite_callback() is a caller supplied function that implements receives the compressed bitstream
	// from the output of the nvEncode-API. (Typically, this data is written to a file.)
	void                                                 Register_fwrite_callback( fwrite_callback_t callback );
	CRepackyuv                                           m_Repackyuv;

protected:
#if defined (NV_WINDOWS) // Windows uses Direct3D or CUDA to access NVENC
    HRESULT                                              InitD3D9(unsigned int deviceID = 0);
    HRESULT                                              InitD3D10(unsigned int deviceID = 0);
    HRESULT                                              InitD3D11(unsigned int deviceID = 0);
#endif
    HRESULT                                              InitCuda(unsigned int deviceID = 0);
    HRESULT                                              AllocateIOBuffers(unsigned int dwInputWidth, unsigned int dwInputHeight, unsigned int maxFrmCnt);
    HRESULT                                              ReleaseIOBuffers();

    unsigned char*                                       LockInputBuffer(void * hInputSurface, unsigned int *pLockedPitch);
    HRESULT                                              UnlockInputBuffer(void * hInputSurface);
	unsigned int                                         GetCodecType(const GUID &encodeGUID) const;
	GUID                                                 GetCodecGUID(const NvEncodeCompressionStd codec) const;
    unsigned int                                         GetCodecProfile(const GUID &encodeGUID) const;
//  HRESULT                                              GetPresetConfig(int iPresetIdx);

    HRESULT                                              FlushEncoder();
    HRESULT                                              ReleaseEncoderResources();
    HRESULT                                              WaitForCompletion();

    NV_ENC_REGISTER_RESOURCE                             m_RegisteredResource;
    nv_enc_caps_s                                        m_nv_enc_caps; // capabilities of currently initialized encoder
//	const cls_convert_guid<uint32_t> &m_preset_desc;// ( table_nv_enc_profiles, array_length(table_nv_enc_profiles) );
//	const cls_convert_guid<uint32_t> &m_profile_desc; // = o_profile_desc; // ( table_nv_enc_profiles, array_length(table_nv_enc_profiles) );
//	const cls_convert_guid<uint32_t> &m_codec_desc;// = o_codec_desc; // 
//	const cls_convert_guid  &m_preset_desc;// ( table_nv_enc_profiles, array_length(table_nv_enc_profiles) );
//	const cls_convert_guid &m_profile_desc; // = o_profile_desc; // ( table_nv_enc_profiles, array_length(table_nv_enc_profiles) );
//	const cls_convert_guid &m_codec_desc;// = o_codec_desc; // 
	void DestroyEncodeSession();

	// fwrite_callback - function pointer to caller-supplied fwrite() implementation
	//  When any part of the encoder needs to write to a file, it will execute the user-supplied callback
	//size_t (*fwrite_callback)(_In_count_x_(_Size*_Count) const void * _Str, size_t _Size, size_t _Count, FILE * _File);
	fwrite_callback_t m_fwrite_callback;

public:
    NV_ENCODE_API_FUNCTION_LIST*                         m_pEncodeAPI;
    HINSTANCE                                            m_hinstLib;
    bool                                                 m_bEncodeAPIFound;

	const static uint32_t MAX_QP = 51; // H264/HEVC qunatization (QP) : maximum allowed value
};// class CNvEncoder

class CNvEncoderThread: public CNvThread
{
public:
    CNvEncoderThread(CNvEncoder* pOwner, U32 dwMaxQueuedSamples)
    :   CNvThread("Encoder Output Thread")
    ,   m_pOwner(pOwner)
    ,   m_dwMaxQueuedSamples(dwMaxQueuedSamples)
    {
        // Empty constructor
    }

    // Overridden virtual functions
    virtual bool ThreadFunc();
   // virtual bool ThreadFini();

    bool QueueSample(EncoderThreadData &sThreadData);
    int GetCurrentQCount()                      { return m_pEncoderQueue.GetCount(); }
    bool IsIdle()                               { return m_pEncoderQueue.GetCount() == 0; }
    bool IsQueueFull()                          { return m_pEncoderQueue.GetCount() >= m_dwMaxQueuedSamples; }

protected:
    CNvEncoder* const m_pOwner;
    CNvQueue<EncoderThreadData, MAX_OUTPUT_QUEUE> m_pEncoderQueue;
    U32 m_dwMaxQueuedSamples;
};

// NVEncodeAPI entry point
#if defined(NV_WINDOWS)
typedef NVENCSTATUS (__stdcall *MYPROC)(NV_ENCODE_API_FUNCTION_LIST*); 
#else
typedef NVENCSTATUS (*MYPROC)(NV_ENCODE_API_FUNCTION_LIST*); 
#endif

	// =========================================================================================
	// Encode Codec GUIDS supported by the NvEncodeAPI interface.
	// =========================================================================================

	#define CREATE_NV_ENC_PARAM_DESCRIPTOR(x) \
		const cls_convert_guid desc_ ## x = cls_convert_guid( table_ ## x, array_length( table_ ## x ) );

	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_params_frame_mode_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_ratecontrol_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_mv_precision_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_codec_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_profile_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_preset_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_buffer_format_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( cudaVideoChromaFormat_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_level_h264_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_level_hevc_names)
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_tier_hevc_names)
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_h264_fmo_names)
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_hevc_cusize_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_h264_entropy_coding_mode_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_adaptive_transform_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_stereo_packing_mode_names )
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_h264_bdirect_mode_names )

#endif // CNVEncoder_h 