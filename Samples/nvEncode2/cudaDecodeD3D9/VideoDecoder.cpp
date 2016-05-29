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

#include "VideoDecoder.h"

#include "FrameQueue.h"
#include <cstring>
#include <cassert>
#include <string>

// CUDA utilities and system includes
#include <helper_functions.h>
#include <helper_cuda_drvapi.h>    // helper file for CUDA Driver API calls and error checking

VideoDecoder::VideoDecoder(const CUVIDEOFORMAT &rVideoFormat,
                           CUcontext &rContext,
                           cudaVideoCreateFlags eCreateFlags,
                           CUvideoctxlock &vidCtxLock)
    : m_VidCtxLock(vidCtxLock)
{
    // get a copy of the CUDA context
    m_Context          = rContext;
    m_VideoCreateFlags = eCreateFlags;

    printf("> VideoDecoder::cudaVideoCreateFlags = <%d>", (int)eCreateFlags);

    switch (eCreateFlags)
    {
        case cudaVideoCreate_Default:
            printf("Default (VP)\n");
            break;

        case cudaVideoCreate_PreferCUDA:
            printf("Use CUDA decoder\n");
            break;

        case cudaVideoCreate_PreferDXVA:
            printf("Use DXVA decoder\n");
            break;

        case cudaVideoCreate_PreferCUVID:
            printf("Use CUVID decoder\n");
            break;

        default:
            printf("Unknown value\n");
            break;
    }

    printf("\n");

    // Validate video format.  These are the currently supported formats via NVCUVID
    assert(cudaVideoCodec_MPEG1 == rVideoFormat.codec ||
           cudaVideoCodec_MPEG2 == rVideoFormat.codec ||
           cudaVideoCodec_MPEG4 == rVideoFormat.codec ||
           cudaVideoCodec_VC1   == rVideoFormat.codec ||
           cudaVideoCodec_H264  == rVideoFormat.codec ||
           cudaVideoCodec_H264_SVC  == rVideoFormat.codec ||
           cudaVideoCodec_H264_MVC  == rVideoFormat.codec ||
           cudaVideoCodec_JPEG  == rVideoFormat.codec ||
           cudaVideoCodec_HEVC  == rVideoFormat.codec ||
           cudaVideoCodec_YUV420== rVideoFormat.codec ||
           cudaVideoCodec_YV12  == rVideoFormat.codec ||
           cudaVideoCodec_NV12  == rVideoFormat.codec ||
           cudaVideoCodec_YUYV  == rVideoFormat.codec ||
           cudaVideoCodec_UYVY  == rVideoFormat.codec);

    assert(cudaVideoChromaFormat_Monochrome == rVideoFormat.chroma_format ||
           cudaVideoChromaFormat_420        == rVideoFormat.chroma_format ||
           cudaVideoChromaFormat_422        == rVideoFormat.chroma_format ||
           cudaVideoChromaFormat_444        == rVideoFormat.chroma_format);

    // Fill the decoder-create-info struct from the given video-format struct.
    memset(&oVideoDecodeCreateInfo_, 0, sizeof(CUVIDDECODECREATEINFO));
    // Create video decoder
    oVideoDecodeCreateInfo_.CodecType           = rVideoFormat.codec;
    oVideoDecodeCreateInfo_.ulWidth             = rVideoFormat.coded_width;
    oVideoDecodeCreateInfo_.ulHeight            = rVideoFormat.coded_height;
    oVideoDecodeCreateInfo_.ulNumDecodeSurfaces = FrameQueue::cnMaximumSize;

    // Allocate the equivalent of 64M pixels worth of surfaces
	//   (this is more than enough for H264 high-profile L5.2)
    while (oVideoDecodeCreateInfo_.ulNumDecodeSurfaces * rVideoFormat.coded_width * rVideoFormat.coded_height > 64*1024*1024)
    {
        oVideoDecodeCreateInfo_.ulNumDecodeSurfaces--;
    }

    oVideoDecodeCreateInfo_.ChromaFormat        = rVideoFormat.chroma_format;
    oVideoDecodeCreateInfo_.OutputFormat        = cudaVideoSurfaceFormat_NV12;
//    oVideoDecodeCreateInfo_.DeinterlaceMode     = cudaVideoDeinterlaceMode_Adaptive;
    oVideoDecodeCreateInfo_.DeinterlaceMode     = cudaVideoDeinterlaceMode_Weave;

    // No scaling.
	// ulTargetWidth influences the pitch of the (2D) output surfaces.
	//  the GPU-driver will always allocate a 2D-framebuffer large enough to accomodate the
	//  the user-specified output-size.  We can use this to our advantage by rounding the
	//  ulTargetWidth up to a multiple of 4096 (to match the NVENC encoder's input-surface pitch.)
    oVideoDecodeCreateInfo_.ulTargetWidth       = oVideoDecodeCreateInfo_.ulWidth;
    oVideoDecodeCreateInfo_.ulTargetHeight      = oVideoDecodeCreateInfo_.ulHeight;
    oVideoDecodeCreateInfo_.ulNumOutputSurfaces = MAX_FRAME_COUNT;  // We won't simultaneously map more than 8 surfaces
    oVideoDecodeCreateInfo_.ulCreationFlags     = m_VideoCreateFlags;
    oVideoDecodeCreateInfo_.vidLock             = vidCtxLock;

    oVideoDecodeCreateInfo_.display_area.left   = rVideoFormat.display_area.left;
    oVideoDecodeCreateInfo_.display_area.top    = rVideoFormat.display_area.top;
    oVideoDecodeCreateInfo_.display_area.right  = rVideoFormat.display_area.right;
    oVideoDecodeCreateInfo_.display_area.bottom = rVideoFormat.display_area.bottom;

    // create the decoder
	CUresult oresult = cuvidCreateDecoder(&oDecoder_, &oVideoDecodeCreateInfo_);

	// if the attempt failed, then retry with CUVID
	if ( oresult != CUDA_SUCCESS && oVideoDecodeCreateInfo_.ulCreationFlags == cudaVideoCreate_PreferDXVA ) {
		printf( "cuvidCreateDecoder() failed for ulCreationFlags==%0u, retrying with cudaVideoCreate_PreferCUVID\n",
			oVideoDecodeCreateInfo_.ulCreationFlags );
		oVideoDecodeCreateInfo_.ulCreationFlags = cudaVideoCreate_PreferCUVID;
		checkCudaErrors(cuvidCreateDecoder(&oDecoder_, &oVideoDecodeCreateInfo_));
	}
}

VideoDecoder::~VideoDecoder()
{
    checkCudaErrors(cuvidDestroyDecoder(oDecoder_));
}

cudaVideoCodec
VideoDecoder::codec()
const
{
    return oVideoDecodeCreateInfo_.CodecType;
}

cudaVideoChromaFormat
VideoDecoder::chromaFormat()
const
{
    return oVideoDecodeCreateInfo_.ChromaFormat;
}

unsigned long
VideoDecoder::maxDecodeSurfaces()
const
{
    return oVideoDecodeCreateInfo_.ulNumDecodeSurfaces;
}

unsigned long
VideoDecoder::frameWidth()
const
{
    return oVideoDecodeCreateInfo_.ulWidth;
}

unsigned long
VideoDecoder::frameHeight()
const
{
    return oVideoDecodeCreateInfo_.ulHeight;
}

unsigned long
VideoDecoder::targetWidth()
const
{
    return oVideoDecodeCreateInfo_.ulTargetWidth;
}

unsigned long
VideoDecoder::targetHeight()
const
{
    return oVideoDecodeCreateInfo_.ulTargetHeight;
}

void
VideoDecoder::decodePicture(CUVIDPICPARAMS *pPictureParameters, CUcontext *pContext)
{
	// for debug
    // Handle CUDA picture decode (this actually calls the hardware VP/CUDA to decode video frames)
    CUresult oResult = cuvidDecodePicture(oDecoder_, pPictureParameters);
/* for debug only
    const CUVIDMPEG2PICPARAMS &mpeg2 = pPictureParameters->CodecSpecific.mpeg2;          // Also used for MPEG-1
    const CUVIDH264PICPARAMS  &h264  = pPictureParameters->CodecSpecific.h264;
    const CUVIDVC1PICPARAMS   &vc1   = pPictureParameters->CodecSpecific.vc1;
    const CUVIDMPEG4PICPARAMS &mpeg4 = pPictureParameters->CodecSpecific.mpeg4;
    const CUVIDJPEGPICPARAMS  &jpeg  = pPictureParameters->CodecSpecific.jpeg;
    const CUVIDHEVCPICPARAMS  &hevc  = pPictureParameters->CodecSpecific.hevc;
*/
	checkCudaErrors( oResult );
}

void
VideoDecoder::mapFrame(int iPictureIndex, CUdeviceptr *ppDevice, unsigned int *pPitch, CUVIDPROCPARAMS *pVideoProcessingParameters)
{
    CUresult oResult = cuvidMapVideoFrame(oDecoder_,
                                          iPictureIndex,
                                          ppDevice,
                                          pPitch, pVideoProcessingParameters);
	checkCudaErrors( oResult );
    assert(0 != *ppDevice);
    assert(0 != *pPitch);
}

void
VideoDecoder::unmapFrame(CUdeviceptr pDevice)
{
    CUresult oResult = cuvidUnmapVideoFrame(oDecoder_, pDevice);
	checkCudaErrors( oResult );
}

