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

//---------------------------------------------------------------------------
//
// encodeUtils.h
//
//---------------------------------------------------------------------------

#ifndef _ENCODE_UTILS_H
#define _ENCODE_UTILS_H

#include "nvEncodeAPI.h"

#if defined __linux
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <unistd.h>
#endif

#include <string.h>
#include "NvTypes.h"

#define PRINTERR(message) \
    fprintf(stderr, (message)); \
    fprintf(stderr, "\n-> @ %s, line %d\n", __FILE__, __LINE__);

#if defined (WIN32) || defined (_WIN32)
#define LICENSE_FILE "C:\\NvEncodeSDKLicense.bin"
#else
#define LICENSE_FILE "~/NvEncodeSDKLicense.bin"
#endif


inline bool IsYV12PLFormat(NV_ENC_BUFFER_FORMAT dwFormat)
{
   if (dwFormat == NV_ENC_BUFFER_FORMAT_YV12_PL)
   {
       return true;
   }
   else
       return false;
}

inline bool IsNV12PLFormat(NV_ENC_BUFFER_FORMAT dwFormat)
{
    if (dwFormat == NV_ENC_BUFFER_FORMAT_NV12_PL)
    {
        return true;
    }
    else
    {
        return false;
    }
}

inline bool IsNV12Tiled16x16Format(NV_ENC_BUFFER_FORMAT dwFormat)
{
    if (dwFormat == NV_ENC_BUFFER_FORMAT_NV12_TILED16x16)
    {
        return true;
    }
    else
    {
        return false;
    }
}

inline bool IsNV12Tiled64x16Format(NV_ENC_BUFFER_FORMAT dwFormat)
{
    if (dwFormat == NV_ENC_BUFFER_FORMAT_NV12_TILED64x16)
    {
        return true;
    }
    else
    {
        return false;
    }
}

inline bool IsYUV444PLFormat(NV_ENC_BUFFER_FORMAT dwFormat)
{
    if (dwFormat == NV_ENC_BUFFER_FORMAT_YUV444_PL)
    {
        return true;
    }
    else
    {
        return false;
    }
}

inline bool IsYUV444Tiled16x16Format(NV_ENC_BUFFER_FORMAT dwFormat)
{
    if (dwFormat == NV_ENC_BUFFER_FORMAT_YUV444_TILED16x16) 
    {
        return true;
    }
    else
    {
        return false;
    }
}
inline bool IsYUV444Tiled64x16Format(NV_ENC_BUFFER_FORMAT dwFormat)
{
    if (dwFormat == NV_ENC_BUFFER_FORMAT_YUV444_TILED64x16)
    {
        return true;
    }
    else
    {
        return false;
    }
}

inline bool IsNV12Format(NV_ENC_BUFFER_FORMAT dwFormat)
{
   if ((dwFormat == NV_ENC_BUFFER_FORMAT_NV12_PL) ||
       (dwFormat == NV_ENC_BUFFER_FORMAT_NV12_TILED16x16) ||
       (dwFormat == NV_ENC_BUFFER_FORMAT_NV12_TILED64x16))
   {
       return true;
   }
   else
       return false;
}

inline bool IsYV12Format(NV_ENC_BUFFER_FORMAT dwFormat)
{
   if ((dwFormat == NV_ENC_BUFFER_FORMAT_YV12_PL) ||
       (dwFormat == NV_ENC_BUFFER_FORMAT_YV12_TILED16x16) ||
       (dwFormat == NV_ENC_BUFFER_FORMAT_YV12_TILED64x16))
   {
       return true;
   }
   else
       return false;
}

inline bool IsTiled16x16Format(NV_ENC_BUFFER_FORMAT dwFormat)
{
   if ((dwFormat == NV_ENC_BUFFER_FORMAT_NV12_TILED16x16) ||
       (dwFormat == NV_ENC_BUFFER_FORMAT_YV12_TILED16x16) ||
       (dwFormat == NV_ENC_BUFFER_FORMAT_IYUV_TILED16x16))
   {
       return true;
   }
   else
   {
       return false;
   }
}

inline void CvtToTiled16x16(unsigned char *tile, unsigned char *src, 
                            unsigned int width, unsigned int height, 
                            unsigned int srcStride, unsigned int dstStride)
{
    unsigned int tileNb = 0;
    unsigned int x,y;
    unsigned int offs;
    for (  y = 0 ; y < height ; y++ )
    {
        for ( x = 0 ; x < width; x++ )
        {
            tileNb = x/16 + (y/16) * dstStride/16;

            offs = tileNb * 256;
            offs += (y % 16) * 16 + (x % 16);
            tile[offs]   =  src[srcStride*y + x];
        }
    }
}

inline void getTiled16x16Sizes( int width, int height, bool frame , int &luma_size, int &chroma_size )
{
   int  bl_width, bl_height;
    
   if ( frame ) {
        bl_width  = (width + 15)/16;            
        bl_height = (height + 15)/16;
        luma_size =  bl_width * bl_height * 16*16;

        bl_height = (height/2 + 15)/16;
        chroma_size =   bl_width * bl_height * 16*16;
   }
   else {        
        bl_width  = (width + 15)/16;             
        bl_height = (height/2 + 15)/16;
        luma_size =  bl_width * bl_height * 16*16;
            
        bl_height = (height/4 + 15)/16;
        chroma_size =  bl_width * bl_height * 16*16;
   }    
}

inline void convertYUVpitchtoNV12tiled16x16( unsigned char *yuv_luma, unsigned char *yuv_cb, unsigned char *yuv_cr,
                                            unsigned char *tiled_luma, unsigned char *tiled_chroma,
                                            int width, int height, int srcStride, int dstStride )
{
    int tileNb, offs;
    int x,y;

    if(srcStride<0) srcStride = width;
    if(dstStride<0) dstStride = width;

    for (  y = 0 ; y < height ; y++){
        for ( x= 0 ; x < width; x++){
             tileNb = x/16 + (y/16) * dstStride/16;

             offs = tileNb * 256;
             offs += (y % 16) * 16 + (x % 16);
             tiled_luma[offs]   =  yuv_luma[(srcStride*y) + x];
        }
    }

    for (  y = 0 ; y < height/2 ; y++){
        for ( x= 0 ; x < width; x = x+2 ){
            tileNb = x/16 + (y/16) * dstStride/16;

            offs = tileNb * 256;
            offs += (y % 16) * 16 + (x % 16);
            tiled_chroma[offs]   =  yuv_cb[(srcStride/2)*y + x/2];
            tiled_chroma[offs+1] =  yuv_cr[(srcStride/2)*y + x/2];
        }
    }
}

inline void convertYUVpitchtoNV12( unsigned char *yuv_luma, unsigned char *yuv_cb, unsigned char *yuv_cr,
                            unsigned char *nv12_luma, unsigned char *nv12_chroma,
                            int width, int height , int srcStride, int dstStride)
{
    int y;
    int x;
    if (srcStride == 0)
        srcStride = width;
    if (dstStride == 0)
        dstStride = width;

    for ( y = 0 ; y < height ; y++)
    {
        memcpy( nv12_luma + (dstStride*y), yuv_luma + (srcStride*y) , width );
    }

    for ( y = 0 ; y < height/2 ; y++)
    {
        for ( x= 0 ; x < width; x=x+2)
        {
            nv12_chroma[(y*dstStride) + x] =    yuv_cb[((srcStride/2)*y) + (x >>1)];
            nv12_chroma[(y*dstStride) +(x+1)] = yuv_cr[((srcStride/2)*y) + (x >>1)];
        }
    }
}

inline
void convertYUVpitchtoYUV444tiled16x16(unsigned char *yuv_luma, unsigned char *yuv_cb, unsigned char *yuv_cr,
                                       unsigned char *tiled_luma, unsigned char *tiled_cb, unsigned char *tiled_cr,
                                       int width, int height, int srcStride, int dstStride )
{
    int tileNb, offs;
    int x,y;

    if(srcStride<0) srcStride = width;
    if(dstStride<0) dstStride = width;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
             tileNb = x/16 + (y/16) * dstStride/16;
             offs = tileNb * 256;
             offs += (y % 16) * 16 + (x % 16);
             tiled_luma[offs]   =  yuv_luma[(srcStride*y) + x];
             tiled_cb  [offs]   =  yuv_cb  [(srcStride*y) + x];
             tiled_cr  [offs]   =  yuv_cr  [(srcStride*y) + x];
        }
    }
}

inline
void convertYUVpitchtoYUV444(unsigned char *yuv_luma, unsigned char *yuv_cb, unsigned char *yuv_cr,
                             unsigned char *surf_luma, unsigned char *surf_cb, unsigned char *surf_cr,
                             int width, int height, int srcStride, int dstStride)
{
    if(srcStride<0)
        srcStride = width;
    if(dstStride<0)
        dstStride = width;

    for(int h = 0; h < height; h++)
    {
        memcpy(surf_luma + dstStride * h, yuv_luma + srcStride * h, width);
        memcpy(surf_cb   + dstStride * h, yuv_cb   + srcStride * h, width);
        memcpy(surf_cr   + dstStride * h, yuv_cr   + srcStride * h, width);
    }
}

#endif
