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

#include <include/videoFormats.h>
#include <CNVEncoderH264.h>
#include <xcodeutil.h>
#include <emmintrin.h> // Visual Studio 2005 MMX/SSE/SSE2 compiler intrinsics

#include <helper_cuda_drvapi.h>    // helper file for CUDA Driver API calls and error checking
#include <include/helper_nvenc.h>

#ifndef INFINITE
#define INFINITE UINT_MAX
#endif

#pragma warning (disable:4311)

// uncomment the following line to use the optimized version of yuv420tonv12
#define NO_MULT_INDEX // use the 'no mutiply-on-index' optimized version of yuv420tonv12

#ifndef NO_MULT_INDEX
// _convertYUV420toNV12_sse2(
//		width,  // horizontal #pixels (REQUIREMENT: multiple of 32)
//		height, // vertical #pixels (REQUIREMENT: multple of 2)
//		src_stride,  // units of 128bits (must be multiple of 2)
//					 // 'source stride' is measurement of the distance between start of scanline#(y) 
//					 //  and start of scanline#(y+1), in the source-surface
//		dstStride,  // destination-surface in units of 128bits (must be multiple fo 2)
inline void _convertYUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
	const uint32_t width, const uint32_t height,
	const __m128i * const src_yuv[3],
	const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m128i, 16-bytes)
    __m128i dest_nv12_luma[], __m128i dest_nv12_chroma[],
	const uint32_t dstStride // stride (in units of __m128i, 16-bytes)
)
{
	const uint32_t half_height = (height+1) >> 1;  // round_up( height / 2 )
	const uint32_t width_div_16 = (width+15) >> 4; // round_up( width / 16 )

	__m128i chroma_pixels1, chroma_pixels0;  // 8 horizontal chroma (Cb+Cr) pixels:{ x+0 .. x+7 }
	//
	// This function converts a source-image that is formatted as YUV4:2:0 (3-plane),
	// into an output-image that is formatted as NV12 (2-plane).

	// Loop optimization:
	// ------------------
	// each scanline is copied from left -> right.  Each SSE2 instruction copies
	// 16-pixels, which means we may 'overshoot' the right-edge of the picture
	// boundary.  This is ok, since we assume the source & dest framebuffers
	// are allocated with sufficiently large stride to tolerate our overshooting.

	// copy the luma portion of the framebuffer
    for ( unsigned y = 0; y < height ; y++) {
		for( unsigned x = 0; x < width_div_16; ++x ) {
			// copy 16 horizontal luma pixels (128-bits) per inner-loop pass
			dest_nv12_luma[y*dstStride + x] = src_yuv[0][ src_stride[0]*y + x];
		} // for x
	} // for y
	

	// copy the chroma portion of the framebuffer
    for ( unsigned y = 0; y < half_height ; ++y) {
        for ( unsigned  x= 0, x_div2 = 0; x < width_div_16;) {
			// copy 16 horizontal chroma pixels per inner-loop pass
			// each chroma-pixel is 16-bits {Cb,Cr}
			// (Cb,Cr) pixel x.. x+7

			// Generate 8 output pixels [n..n+7]
			// Read the lower 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			chroma_pixels0 = _mm_unpacklo_epi8( src_yuv[1][src_stride[1]*y + x_div2], src_yuv[2][src_stride[2]*y + x_div2] );// even
			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels0;

			// Generate 8 more output pixels [n+8..n+15]
			// Read the upper 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			++x;
			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][src_stride[1]*y + x_div2], src_yuv[2][src_stride[2]*y + x_div2] );// odd
			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels1;

			++x;
			++x_div2; // x_div2 = x << 1;
        } // for x
	} // for y
}

#else // NO_MULT_INDEX

	// semi-optimized convert-function: 'no multiply-on-index' 
	//	This function avoids IMUL instructions by replacing the framebuffer array-index operator ([])
	//  with manual pointer-arithmetic.  Not sure if this really helps with VC++ 2010's compiler.

inline void _convertYUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
	const uint32_t width, const uint32_t height,
	const __m128i * const src_yuv[3],
	const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m128i, 16-bytes)
    __m128i dest_nv12_luma[], __m128i dest_nv12_chroma[],
	const uint32_t dstStride // stride (in units of __m128i, 16-bytes)
)
{
	const uint32_t half_height = (height+1) >> 1;  // round_up( height / 2 )
	const uint32_t width_div_16 = (width+15) >> 4; // round_up( width / 16 )

	__m128i chroma_pixels1, chroma_pixels0;  // 8 horizontal chroma (Cb+Cr) pixels:{ x+0 .. x+7 }
	//
	// This function converts a source-image that is formatted as YUV4:2:0 (3-plane),
	// into an output-image that is formatted as NV12 (2-plane).

	// Loop optimization:
	// ------------------
	// each scanline is copied from left -> right.  Each SSE2 instruction copies
	// 16-pixels, which means we may 'overshoot' the right-edge of the picture
	// boundary.  This is ok, since we assume the source & dest framebuffers
	// are allocated with sufficiently large stride to tolerate our overshooting.
	uint32_t ptr_src0_x, ptr_src1_x, ptr_src2_x;
	uint32_t ptr_dst_y;

	ptr_dst_y = 0;
	ptr_src0_x = 0;

	// copy the luma portion of the framebuffer
    for ( unsigned y = 0; y < height ; y++) {
		
		for( unsigned x = 0, ptr_dst_y_plus_x = ptr_dst_y, ptr_src0_x_plus_x = ptr_src0_x; x < width_div_16; ++x ) {
			// copy 16 horizontal luma pixels (128-bits) per inner-loop pass
//			dest_nv12_luma[y*dstStride + x] = src_yuv[0][ src_stride[0]*y + x];
//			dest_nv12_luma[ptr_dst_y + x] = src_yuv[0][ ptr_src0_x + x];
			dest_nv12_luma[ptr_dst_y_plus_x] = src_yuv[0][ptr_src0_x_plus_x];
			++ptr_dst_y_plus_x;
			++ptr_src0_x_plus_x;
		} // for x

		ptr_dst_y += dstStride; //y * dstStride;
		ptr_src0_x += src_stride[0]; //src_stride[0]*y;

	} // for y
	

	// copy the chroma portion of the framebuffer
	ptr_dst_y = 0; //y * dstStride;
	ptr_src1_x = 0; //src_stride[1]*y;
	ptr_src2_x = 0; //src_stride[2]*y;

    for ( unsigned y = 0; y < half_height ; ++y) {
        for ( unsigned  x= 0, ptr_dst_y_plus_x = ptr_dst_y, ptr_src1_x_plus_xdiv2 = ptr_src1_x, ptr_src2_x_plus_xdiv2 = ptr_src2_x; x < width_div_16; x += 2)
        {
			// copy 16 horizontal chroma pixels per inner-loop pass
			// each chroma-pixel is 16-bits {Cb,Cr}
			// (Cb,Cr) pixel x.. x+7

			// Generate 8 output pixels [n..n+7]
			// Read the lower 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
//			chroma_pixels0 = _mm_unpacklo_epi8( src_yuv[1][src_stride[1]*y + x_div2], src_yuv[2][src_stride[2]*y + x_div2] );// even
//			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels0;
//			chroma_pixels0 = _mm_unpacklo_epi8( src_yuv[1][ptr_src1_x + x_div2], src_yuv[2][ptr_src2_x + x_div2] );// even
			chroma_pixels0 = _mm_unpacklo_epi8( src_yuv[1][ptr_src1_x_plus_xdiv2], src_yuv[2][ptr_src2_x_plus_xdiv2] );// even
//			dest_nv12_chroma[ptr_dst_y + x] = chroma_pixels;
			dest_nv12_chroma[ptr_dst_y_plus_x] = chroma_pixels0;


			// Generate 8 more output pixels [n+8..n+15]
			// Read the upper 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			++ptr_dst_y_plus_x;
//			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][src_stride[1]*y + x_div2], src_yuv[2][src_stride[2]*y + x_div2] );// odd
//			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels1;
//			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][ptr_src1_x + x_div2], src_yuv[2][ptr_src2_x + x_div2] );// odd
			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][ptr_src1_x_plus_xdiv2], src_yuv[2][ptr_src2_x_plus_xdiv2] );// odd
//			dest_nv12_chroma[ptr_dst_y + x] = chroma_pixels1;
			dest_nv12_chroma[ptr_dst_y_plus_x] = chroma_pixels1;

			++ptr_dst_y_plus_x;
			++ptr_src1_x_plus_xdiv2;
			++ptr_src2_x_plus_xdiv2;

        } // for x

		ptr_dst_y += dstStride; //y * dstStride;
		ptr_src1_x += src_stride[1]; //src_stride[1]*y;
		ptr_src2_x += src_stride[2]; //src_stride[2]*y;

	} // for y
}
#endif // NO_MULT_INDEX

inline void _convertYUV420toNV12( // convert planar(YV12) into planar(NV12)
	const uint32_t width, const uint32_t height,
	const unsigned char * const src_yuv[3], const uint32_t src_stride[3],
    unsigned char dest_nv12_luma[], uint16_t dest_nv12_chroma[],
	const uint32_t dstStride // stride in #bytes (divided by 2)
)
{
	const uint32_t half_height = (height +1)>> 1;  // round_up( height / 2)
	const uint32_t half_width  = (width +1) >> 1;  // round up( width / 2)
	const uint32_t dstStride_div_2 = (dstStride +1) >> 1;// round_up( dstStride / 2)
	const uint32_t dstStride_div_4 = (dstStride +3) >> 2;// round_up( dstStride / 4)
	uint16_t chroma_pixel; // 1 chroma (Cb+Cr) pixel

	// This loop is the *standard* (failsafe) version of the YUV420->NV12 converter.
	// It is slow and inefficient. If possible, don't use this; use the sse2-version 
	// instead...

    for ( unsigned y = 0; y < height ; y++)
    {
        memcpy( dest_nv12_luma + (dstStride*y), src_yuv[0] + (src_stride[0]*y), width );
    }
	
	// optimized access loop -
	// Our expectation is that the source and destination scanlines start on dword-aligned
	// addresses, and that there is sufficient stride in between adjacent scanlines to
	// allow some 'overshoot' past the right-edge of each scanline.

    for ( unsigned y = 0; y < half_height ; ++y)
    {
		//dest_nv12 = reinterpret_cast<uint32_t *>(dest_nv12_chroma + y*dstStride);

        for ( unsigned  x= 0 ; x < half_width; x = x + 2)
        {
			// (Cb,Cr) pixel (x ,  y)
			chroma_pixel = src_yuv[1][src_stride[1]*y + x] |  // Cb
				(src_yuv[2][src_stride[2]*y + x] << 8); // Cr

			dest_nv12_chroma[(y*dstStride_div_2) + x] = chroma_pixel;

			// (Cb,Cr) pixel (x+1, y)
			chroma_pixel = src_yuv[1][src_stride[1]*y + x+1] |  // Cb
				(src_yuv[2][src_stride[2]*y + x+1] << 8); // Cr

			dest_nv12_chroma[(y*dstStride_div_2) + x+1] = chroma_pixel;
        }
    }
}

inline void _convertYUV444toY444( // convert packed-pixel(Y444) into planar(4:4:4)
	const uint32_t width, const uint32_t height,
	const uint32_t src_stride, // in #pixels (not bytes)
	const uint32_t src_444[], 
	const uint32_t dst_stride,
    unsigned char dest_444[]
)
{
	unsigned char * const dest_444_1 = dest_444 + dst_stride*height;
	unsigned char * const dest_444_2 = dest_444_1 + dst_stride*height;

    for ( unsigned y = 0, yout = height-1; y < height; ++y, --yout)
    {
        for ( unsigned x= 0; x < width; ++x)
        {
			dest_444[ dst_stride*yout + x] = src_444[ src_stride*y + x] >> 16;

			dest_444_1[ dst_stride*yout + x] = src_444[ src_stride*y + x] >> 8;
			dest_444_2[ dst_stride*yout + x] = src_444[ src_stride*y + x];
        }
//		memset( (void *)&dest_444_1[dst_stride*yout], 77, width );
//		memset( (void *)&dest_444_2[dst_stride*yout], 77, width );
    }
}


/**
* \file CNVEncoderH264.cpp
* \brief CNVEncoderH264 is the Class interface for the Hardware Encoder (NV Encode API H.264)
* \date 2011 
*  This file contains the CNvEncoderH264 class declaration and data structures
*/


// H264 Encoder
CNvEncoderH264::CNvEncoderH264()
{
    m_uMaxHeight = 0;
    m_uMaxWidth = 0;
    m_uCurHeight = 0;
    m_uCurWidth = 0;
    m_dwFrameNumInGOP = 0;
}

CNvEncoderH264::~CNvEncoderH264()
{
	DestroyEncoder();
}

HRESULT CNvEncoderH264::InitializeEncoder()
{
    return E_FAIL;
}

HRESULT CNvEncoderH264::InitializeEncoderH264(NV_ENC_CONFIG_H264_VUI_PARAMETERS *pvui)
{
    HRESULT hr           = S_OK;
    int numFrames        = 0;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    bool bMVCEncoding    = m_stEncoderInput.profile == NV_ENC_H264_PROFILE_STEREO ? true : false;
    m_bAsyncModeEncoding = ((m_stEncoderInput.syncMode==0) ? true : false);

    m_uMaxHeight         = m_stEncoderInput.maxHeight;
    m_uMaxWidth          = m_stEncoderInput.maxWidth;
    m_dwFrameWidth       = m_stEncoderInput.width;
    m_dwFrameHeight      = m_stEncoderInput.height;

    memset(&m_stInitEncParams, 0, sizeof(NV_ENC_INITIALIZE_PARAMS));
    SET_VER(m_stInitEncParams, NV_ENC_INITIALIZE_PARAMS);
    m_stInitEncParams.encodeConfig = &m_stEncodeConfig;
    SET_VER(m_stEncodeConfig, NV_ENC_CONFIG);
    m_stInitEncParams.darHeight           = m_stEncoderInput.darRatioY;
    m_stInitEncParams.darWidth            = m_stEncoderInput.darRatioX;
//    m_stInitEncParams.encodeHeight        = m_uMaxHeight;
//    m_stInitEncParams.encodeWidth         = m_uMaxWidth;
    m_stInitEncParams.encodeHeight        = m_dwFrameHeight;
    m_stInitEncParams.encodeWidth         = m_dwFrameWidth;
    
    m_uCurHeight                          = m_stInitEncParams.encodeHeight;
    m_uCurWidth                           = m_stInitEncParams.encodeWidth;
    
    m_stInitEncParams.maxEncodeHeight     = m_uMaxHeight;
    m_stInitEncParams.maxEncodeWidth      = m_uMaxWidth;

    m_stInitEncParams.frameRateNum        = m_stEncoderInput.frameRateNum;
    m_stInitEncParams.frameRateDen        = m_stEncoderInput.frameRateDen;
    //Fix me add theading model
    m_stInitEncParams.enableEncodeAsync   = m_bAsyncModeEncoding;
    m_stInitEncParams.enablePTD           = !m_stEncoderInput.disable_ptd;
    m_stInitEncParams.reportSliceOffsets  = m_stEncoderInput.report_slice_offsets;
    m_stInitEncParams.enableSubFrameWrite = m_stEncoderInput.enableSubFrameWrite;
    m_stInitEncParams.encodeGUID          = m_stEncodeGUID;
    m_stInitEncParams.presetGUID          = m_stPresetGUID;

    if (m_stEncoderInput.disableCodecCfg == 0)
    {
        m_stInitEncParams.encodeConfig->profileGUID                  = m_stCodecProfileGUID;
        m_stInitEncParams.encodeConfig->rcParams.averageBitRate      = m_stEncoderInput.avgBitRate;
        m_stInitEncParams.encodeConfig->rcParams.maxBitRate          = m_stEncoderInput.peakBitRate;
        m_stInitEncParams.encodeConfig->rcParams.constQP.qpIntra     = m_stEncoderInput.qpI;
        m_stInitEncParams.encodeConfig->rcParams.constQP.qpInterP    = m_stEncoderInput.qpP;
        m_stInitEncParams.encodeConfig->rcParams.constQP.qpInterB    = m_stEncoderInput.qpB;
		m_stInitEncParams.encodeConfig->rcParams.enableMinQP       = m_stEncoderInput.min_qp_ena;
        m_stInitEncParams.encodeConfig->rcParams.minQP.qpIntra     = m_stEncoderInput.min_qpI;
        m_stInitEncParams.encodeConfig->rcParams.minQP.qpInterP    = m_stEncoderInput.min_qpP;
        m_stInitEncParams.encodeConfig->rcParams.minQP.qpInterB    = m_stEncoderInput.min_qpB;
		m_stInitEncParams.encodeConfig->rcParams.enableMaxQP       = m_stEncoderInput.max_qp_ena;
        m_stInitEncParams.encodeConfig->rcParams.maxQP.qpIntra     = m_stEncoderInput.max_qpI;
        m_stInitEncParams.encodeConfig->rcParams.maxQP.qpInterP    = m_stEncoderInput.max_qpP;
        m_stInitEncParams.encodeConfig->rcParams.maxQP.qpInterB    = m_stEncoderInput.max_qpB;
		m_stInitEncParams.encodeConfig->rcParams.enableInitialRCQP    = m_stEncoderInput.initial_qp_ena;
        m_stInitEncParams.encodeConfig->rcParams.initialRCQP.qpIntra  = m_stEncoderInput.initial_qpI;
        m_stInitEncParams.encodeConfig->rcParams.initialRCQP.qpInterP = m_stEncoderInput.initial_qpP;
        m_stInitEncParams.encodeConfig->rcParams.initialRCQP.qpInterB = m_stEncoderInput.initial_qpB;

        m_stInitEncParams.encodeConfig->rcParams.rateControlMode     = (NV_ENC_PARAMS_RC_MODE)m_stEncoderInput.rateControl;
		m_stInitEncParams.encodeConfig->rcParams.vbvBufferSize       =  m_stEncoderInput.vbvBufferSize;
		m_stInitEncParams.encodeConfig->rcParams.vbvInitialDelay     =  m_stEncoderInput.vbvInitialDelay;


        m_stInitEncParams.encodeConfig->frameIntervalP       = m_stEncoderInput.numBFrames + 1;
        m_stInitEncParams.encodeConfig->gopLength            = (m_stEncoderInput.gopLength > 0) ?  m_stEncoderInput.gopLength : 30;
        m_stInitEncParams.encodeConfig->monoChromeEncoding   = 0;
        m_stInitEncParams.encodeConfig->frameFieldMode       = m_stEncoderInput.FieldEncoding ;
        m_stInitEncParams.encodeConfig->mvPrecision          = m_stEncoderInput.mvPrecision;
        
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.disableDeblockingFilterIDC = m_stEncoderInput.disable_deblocking; // alawys enable deblk filter for h264
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.adaptiveTransformMode      = m_stEncoderInput.profile == NV_ENC_H264_PROFILE_HIGH ? m_stEncoderInput.adaptive_transform_mode : NV_ENC_H264_ADAPTIVE_TRANSFORM_AUTOSELECT;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.fmoMode                    = m_stEncoderInput.enableFMO;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.bdirectMode                = m_stEncoderInput.numBFrames > 0 ? m_stEncoderInput.bdirectMode : NV_ENC_H264_BDIRECT_MODE_DISABLE;
//        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.bdirectMode                = m_stEncoderInput.numBFrames > 0 ? NV_ENC_H264_BDIRECT_MODE_TEMPORAL : NV_ENC_H264_BDIRECT_MODE_DISABLE;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputAUD                  = m_stEncoderInput.aud_enable;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.entropyCodingMode        = (m_stEncoderInput.profile > NV_ENC_H264_PROFILE_BASELINE) ? m_stEncoderInput.vle_entropy_mode : NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.idrPeriod                = m_stInitEncParams.encodeConfig->gopLength ;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.level                    = m_stEncoderInput.level;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.numTemporalLayers        = m_stEncoderInput.numlayers;
        if (m_stEncoderInput.svcTemporal)
        {
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.enableTemporalSVC = 1;
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig.basePriorityID           = 0;
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig.numTemporalLayers = m_stEncoderInput.numlayers;;
        }
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputBufferingPeriodSEI = m_stEncoderInput.output_sei_BufferPeriod;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputPictureTimingSEI   = m_stEncoderInput.output_sei_PictureTime;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.hierarchicalPFrames      = !! m_stEncoderInput.hierarchicalP;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.hierarchicalBFrames      = !! m_stEncoderInput.hierarchicalB;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.disableSPSPPS            = !! m_stEncoderInput.outBandSPSPPS;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputFramePackingSEI    = m_stEncoderInput.stereo3dMode!= NV_ENC_STEREO_PACKING_MODE_NONE ? 1 : 0;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.stereoMode               = (NV_ENC_STEREO_PACKING_MODE)m_stEncoderInput.stereo3dMode;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.separateColourPlaneFlag  = (m_stEncoderInput.chromaFormatIDC >= NV_ENC_BUFFER_FORMAT_YUV444_PL) ?
			m_stEncoderInput.separate_color_plane : // set to 1 to enable 4:4:4 mode
		    0;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.entropyCodingMode        = (m_stEncoderInput.profile > NV_ENC_H264_PROFILE_BASELINE) ? m_stEncoderInput.vle_entropy_mode : NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC;
        if (m_stEncoderInput.max_ref_frames>0) 
             m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.maxNumRefFrames     = m_stEncoderInput.max_ref_frames;
        if ( pvui != NULL )
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.h264VUIParameters = *pvui;

		// NVENC API 3
		m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.enableVFR = m_stEncoderInput.enableVFR ? 1 : 0;
		m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.sliceMode      = 3;
		m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.sliceModeData  = m_stEncoderInput.numSlices;
    }

    // Initialize the Encoder
    nvStatus = m_pEncodeAPI->nvEncInitializeEncoder(m_hEncoder, &m_stInitEncParams);
    if (nvStatus == NV_ENC_SUCCESS)
    {
        if (m_stEncoderInput.outBandSPSPPS > 0)
        {
            printf("\n");
            SET_VER(m_spspps, NV_ENC_SEQUENCE_PARAM_PAYLOAD);
            m_spspps.spsppsBuffer = new unsigned char [1024];
            m_spspps.inBufferSize = 1024;
            m_spspps.outSPSPPSPayloadSize = new unsigned int[1];
            nvStatus = m_pEncodeAPI->nvEncGetSequenceParams(m_hEncoder, &m_spspps);
            assert(nvStatus == NV_ENC_SUCCESS);
            if (nvStatus == NV_ENC_SUCCESS)
            {
                (*m_fwrite_callback)(m_spspps.spsppsBuffer, 1, *m_spspps.outSPSPPSPayloadSize, m_fOutput, m_privateData);
                printf(">> outSPSPPS PayloadSize = %d, Payload=", *m_spspps.outSPSPPSPayloadSize);

                for (int i=0; i < sizeof(*m_spspps.outSPSPPSPayloadSize) ; i++) 
                    printf("%x", ((unsigned char *)m_spspps.spsppsBuffer)[i]);

                printf("\n");
            }
        }

        // Allocate IO buffers -
        //    Note, here the surface must be allocated to the source-video's *coded height* and *coded width*,
        // rather than the display height & width.  If we use the display-height & width, then the chroma portion
        // of the decoded Frames will most likely be offset improperly (resulting in in a color-misalignment.)
        // Example:
        //      Source video coded-width x height = 1920 x 1088 (Coded dimensions VC1/MPEG2/MPEG4/H264)
        //      Source video displayed dimensions = 1920 x 1080 
        //      ...AllocateIOBuffers( 1920, 1088, ...);
        //
		//   In interlaced encoding mode, the encoder receives input as a whole frames (i.e. a pair of fields),
		//                                so the full height is still used.

        unsigned int dwPicHeight = m_uMaxHeight;
        int numMBs = ((m_dwFrameWidth + 15)/16) * ((dwPicHeight + 15)/16);
        int NumIOBuffers = m_stEncoderInput.numBFrames + 4 + 1;
		/*
		if ( numMBs < 8160)   // less than 1920x1088
			NumIOBuffers = m_stEncoderInput.numBFrames + 4 + 1;
		else if ( numMBs < 16320 ) // between 1920x1088 and 2560x...
			NumIOBuffers = 16;
		else
			NumIOBuffers = 9;
		*/
        //AllocateIOBuffers(m_dwFrameWidth, dwPicHeight, NumIOBuffers);
        AllocateIOBuffers(m_uMaxWidth, dwPicHeight, NumIOBuffers);
        hr = S_OK;
    }
    else
        hr = E_FAIL;

    // intialize output thread
    if (hr == S_OK && !m_pEncoderThread)
    {
        m_pEncoderThread = new CNvEncoderThread(reinterpret_cast<CNvEncoder*>(this), MAX_OUTPUT_QUEUE);
        if (!m_pEncoderThread)
        {
            hr = E_FAIL;
        }
        else
        {
            m_pEncoderThread->ThreadStart();
        }
    }
    
    if (hr == S_OK)
        m_bEncoderInitialized = true;

    return hr;
}

HRESULT
CNvEncoderH264::ReconfigureEncoder(EncodeConfig EncoderReConfig)
{
    // Initialize the Encoder
    memcpy(&m_stEncoderInput ,&EncoderReConfig, sizeof(EncoderReConfig));
    m_stInitEncParams.encodeHeight        =  EncoderReConfig.height;
    m_stInitEncParams.encodeWidth         =  EncoderReConfig.width;
    m_stInitEncParams.darWidth            =  EncoderReConfig.width;
    m_stInitEncParams.darHeight           =  EncoderReConfig.height;

    m_stInitEncParams.frameRateNum        =  EncoderReConfig.frameRateNum;
    m_stInitEncParams.frameRateDen        =  EncoderReConfig.frameRateDen;
    //m_stInitEncParams.presetGUID          = m_stPresetGUID;
    m_stInitEncParams.encodeConfig->rcParams.maxBitRate         = EncoderReConfig.peakBitRate;
    m_stInitEncParams.encodeConfig->rcParams.averageBitRate     = EncoderReConfig.avgBitRate;
    m_stInitEncParams.encodeConfig->frameFieldMode              = EncoderReConfig.FieldEncoding ? NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD : NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME ;
    m_stInitEncParams.encodeConfig->rcParams.vbvBufferSize      = EncoderReConfig.vbvBufferSize;
    m_stInitEncParams.encodeConfig->rcParams.vbvInitialDelay    = EncoderReConfig.vbvInitialDelay;
    m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.disableSPSPPS = 0;
    memcpy( &m_stReInitEncParams.reInitEncodeParams, &m_stInitEncParams, sizeof(m_stInitEncParams));
    SET_VER(m_stReInitEncParams, NV_ENC_RECONFIGURE_PARAMS);
    m_stReInitEncParams.resetEncoder    = true;
    NVENCSTATUS nvStatus = m_pEncodeAPI->nvEncReconfigureEncoder(m_hEncoder, &m_stReInitEncParams);
    return nvStatus;
}

//
// neither nvenc_export nor nvEncode2 uses the vanilla EncodeFrame() function
//
HRESULT CNvEncoderH264::EncodeFrame(EncodeFrameConfig *pEncodeFrame, bool bFlush)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    HRESULT hr = S_OK;
    NV_ENC_MAP_INPUT_RESOURCE mapRes = {0};

    if (bFlush)
    {
        FlushEncoder();
        return S_OK;
    }

    if (!pEncodeFrame)
    {
        return E_FAIL;
    }

    EncodeInputSurfaceInfo  *pInput;
    EncodeOutputBuffer      *pOutputBitstream;

    if (!m_stInputSurfQueue.Remove(pInput, INFINITE))
    {
        assert(0);
    }


    if (!m_stOutputSurfQueue.Remove(pOutputBitstream, INFINITE))
    {
        assert(0);
    }

    unsigned int lockedPitch = 0;
    // encode width and height
    unsigned int dwWidth =  m_uMaxWidth; //m_stEncoderInput.width;
    unsigned int dwHeight = (pEncodeFrame->fieldPicflag == 2) ? m_uMaxHeight >> 1 :m_uMaxHeight;//m_stEncoderInput.height;
    // Align 32 as driver does the same
    unsigned int dwSurfWidth  = (dwWidth + 0x1f) & ~0x1f;
    unsigned int dwSurfHeight = (dwHeight + 0x1f) & ~0x1f;
    unsigned char *pLuma    = pEncodeFrame->yuv[0];
    unsigned char *pChromaU = pEncodeFrame->yuv[1];
    unsigned char *pChromaV = pEncodeFrame->yuv[2];
    unsigned char *pInputSurface = NULL;
    unsigned char *pInputSurfaceCh = NULL;
    

    if (m_stEncoderInput.useMappedResources)
    {
        if (m_stEncoderInput.interfaceType == NV_ENC_CUDA)
        {
            lockedPitch = pInput->dwCuPitch;
            pInputSurface = pInput->pExtAllocHost;
            pInputSurfaceCh = pInputSurface + (dwSurfHeight*pInput->dwCuPitch);
        }
#if defined(NV_WINDOWS)
        if (m_stEncoderInput.interfaceType == NV_ENC_DX9)
        {
            D3DLOCKED_RECT lockRect = {0};
            IDirect3DSurface9 *pSurf = (IDirect3DSurface9 *)(pInput->pExtAlloc);
            HRESULT hr1 = pSurf->LockRect(&lockRect, NULL, 0);
            if (hr1 == S_OK)
            {
                pInputSurface = (unsigned char *)lockRect.pBits;
                lockedPitch = lockRect.Pitch;
            }
            else
            {
                return hr1;
            }
        }
#endif
    }
    else
    {
        pInputSurface = LockInputBuffer(pInput->hInputSurface, &lockedPitch);
        pInputSurfaceCh = pInputSurface + (dwSurfHeight*lockedPitch);
    }

    if (IsYV12PLFormat(pInput->bufferFmt))
    {
       for(unsigned int h = 0; h < dwHeight; h++)
        {
            memcpy(&pInputSurface[lockedPitch * h], &pLuma[pEncodeFrame->stride[0] * h], dwWidth);
        }

        for(unsigned int h = 0; h < dwHeight/2; h++)
        {
            memcpy(&pInputSurfaceCh[h * (lockedPitch/2)] , &pChromaV[h * pEncodeFrame->stride[2]], dwWidth/2);
        }
        pInputSurfaceCh = pInputSurface + (dwSurfHeight*lockedPitch) +  ((dwSurfHeight * lockedPitch)>>2);

        for(unsigned int h = 0; h < dwHeight/2; h++)
        {
            memcpy(&pInputSurfaceCh[h * (lockedPitch/2)] , &pChromaU[h * pEncodeFrame->stride[1]], dwWidth/2);
        }
    }
    else if (IsNV12Tiled16x16Format(pInput->bufferFmt))
    {
        convertYUVpitchtoNV12tiled16x16(pLuma, pChromaU, pChromaV,pInputSurface, pInputSurfaceCh, dwWidth, dwHeight, dwWidth, lockedPitch);
    }
    else if (IsNV12PLFormat(pInput->bufferFmt))
    {
        pInputSurfaceCh = pInputSurface + (pInput->dwHeight*lockedPitch);
        convertYUVpitchtoNV12(pLuma, pChromaU, pChromaV,pInputSurface, pInputSurfaceCh, dwWidth, dwHeight, dwWidth, lockedPitch);
    }
    else if (IsYUV444Tiled16x16Format(pInput->bufferFmt))
    {
        unsigned char *pInputSurfaceCb = pInputSurface   + (dwSurfHeight * lockedPitch);
        unsigned char *pInputSurfaceCr = pInputSurfaceCb + (dwSurfHeight * lockedPitch);
        convertYUVpitchtoYUV444tiled16x16(pLuma, pChromaU, pChromaV, pInputSurface, pInputSurfaceCb, pInputSurfaceCr, dwWidth, dwHeight, dwWidth, lockedPitch);
    }
    else if (IsYUV444PLFormat(pInput->bufferFmt))
    {
        unsigned char *pInputSurfaceCb = pInputSurface   + (pInput->dwHeight * lockedPitch);
        unsigned char *pInputSurfaceCr = pInputSurfaceCb + (pInput->dwHeight * lockedPitch);
        convertYUVpitchtoYUV444(pLuma, pChromaU, pChromaV, pInputSurface, pInputSurfaceCb, pInputSurfaceCr, dwWidth, dwHeight, dwWidth, lockedPitch);
    }

    // CUDA or DX9 interop with NVENC
    if (m_stEncoderInput.useMappedResources)
    {
        // Here we copy from Host to Device Memory (CUDA)
        if (m_stEncoderInput.interfaceType == NV_ENC_CUDA)
        {
            cuCtxPushCurrent(m_cuContext); // Necessary to bind the 
            CUcontext cuContextCurr;
                
            CUresult result = cuMemcpyHtoD((CUdeviceptr)pInput->pExtAlloc, pInput->pExtAllocHost, pInput->dwCuPitch*pInput->dwHeight*3/2);
            cuCtxPopCurrent(&cuContextCurr);
        }
#if defined(NV_WINDOWS)
        // TODO: Grab a pointer GPU Device Memory (DX9) and then copy the result
        if (m_stEncoderInput.interfaceType == NV_ENC_DX9)
        {
            IDirect3DSurface9 *pSurf = (IDirect3DSurface9 *)pInput->pExtAlloc;
            pSurf->UnlockRect();
        }
#endif
        SET_VER(mapRes, NV_ENC_MAP_INPUT_RESOURCE);
        mapRes.registeredResource  = pInput->hRegisteredHandle;
        nvStatus = m_pEncodeAPI->nvEncMapInputResource(m_hEncoder, &mapRes);
        pInput->hInputSurface = mapRes.mappedResource;
    }
    else // here we just pass the frame in system memory to NVENC
    {
        UnlockInputBuffer(pInput->hInputSurface);
    }

    memset(&m_stEncodePicParams, 0, sizeof(m_stEncodePicParams));
    SET_VER(m_stEncodePicParams, NV_ENC_PIC_PARAMS);
    m_stEncodePicParams.inputBuffer = pInput->hInputSurface;
    m_stEncodePicParams.bufferFmt = pInput->bufferFmt;
    m_stEncodePicParams.inputWidth = pInput->dwWidth;
    m_stEncodePicParams.inputHeight = pInput->dwHeight;
    m_stEncodePicParams.outputBitstream = pOutputBitstream->hBitstreamBuffer;
    m_stEncodePicParams.completionEvent = m_bAsyncModeEncoding == true ? pOutputBitstream->hOutputEvent : NULL;
    m_stEncodePicParams.pictureStruct = (pEncodeFrame->fieldPicflag == 2) ? 
		(pEncodeFrame->topField ? NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM: NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP): 
		NV_ENC_PIC_STRUCT_FRAME;
    m_stEncodePicParams.codecPicParams.h264PicParams.h264ExtPicParams.mvcPicParams.viewID = pEncodeFrame->viewId;    
    m_stEncodePicParams.encodePicFlags = 0;
    m_stEncodePicParams.inputTimeStamp = 0;
    m_stEncodePicParams.inputDuration = 0;

    if(!m_stInitEncParams.enablePTD)
    {
        m_stEncodePicParams.codecPicParams.h264PicParams.refPicFlag = 1;
        //m_stEncodePicParams.codecPicParams.h264PicParams.frameNumSyntax = m_dwFrameNumInGOP;
        m_stEncodePicParams.codecPicParams.h264PicParams.displayPOCSyntax = 2*m_dwFrameNumInGOP;
        m_stEncodePicParams.pictureType = ((m_dwFrameNumInGOP % m_stEncoderInput.gopLength) == 0) ? NV_ENC_PIC_TYPE_IDR : NV_ENC_PIC_TYPE_P;
    }

    // Handling Dynamic Resolution Changing    
    if (pEncodeFrame->dynResChangeFlag)
    {
        m_stEncodePicParams.encodePicFlags = (NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_DYN_RES_CHANGE);
        m_stEncodePicParams.newEncodeWidth  = m_uCurWidth  = pEncodeFrame->newWidth;
        m_stEncodePicParams.newEncodeHeight = m_uCurHeight = pEncodeFrame->newHeight;
    }

    // Handling Dynamic Bitrate Change
    {
        if (pEncodeFrame->dynBitrateChangeFlag == DYN_DOWNSCALE)
        {
            m_stEncodePicParams.encodePicFlags      =  m_stEncodePicParams.encodePicFlags | NV_ENC_PIC_FLAG_DYN_BITRATE_CHANGE;
            m_stEncodePicParams.rcParams.maxBitRate          = (m_stInitEncParams.encodeConfig->rcParams.maxBitRate * 5) / 10;
            m_stEncodePicParams.rcParams.averageBitRate      = (m_stInitEncParams.encodeConfig->rcParams.averageBitRate * 5) / 10;
        }

        if (pEncodeFrame->dynBitrateChangeFlag == DYN_UPSCALE)
        {
            m_stEncodePicParams.encodePicFlags      = m_stEncodePicParams.encodePicFlags | NV_ENC_PIC_FLAG_DYN_BITRATE_CHANGE;
            m_stEncodePicParams.rcParams.maxBitRate          = m_stInitEncParams.encodeConfig->rcParams.maxBitRate * 2 ;
            m_stEncodePicParams.rcParams.averageBitRate      = m_stInitEncParams.encodeConfig->rcParams.averageBitRate * 2 ;
        }
    }

    if ((m_bAsyncModeEncoding == false) && 
        (m_stInitEncParams.enablePTD == 1))
    {
        EncoderThreadData stThreadData;
        stThreadData.pOutputBfr = pOutputBitstream;
        stThreadData.pInputBfr = pInput;
        stThreadData.pOutputBfr->bDynResChangeFlag = pEncodeFrame->dynResChangeFlag == 1 ? 1 : 0;
        pOutputBitstream->bWaitOnEvent = false;
        m_pEncodeFrameQueue.Add(stThreadData);
    }
    nvStatus = m_pEncodeAPI->nvEncEncodePicture(m_hEncoder, &m_stEncodePicParams);
    
    m_dwFrameNumInGOP++;
    if ((m_bAsyncModeEncoding == false) && 
        (m_stInitEncParams.enablePTD == 1))
    {        
        if (nvStatus == NV_ENC_SUCCESS)
        {
            EncoderThreadData stThreadData;
            while (m_pEncodeFrameQueue.Remove(stThreadData, 0))
            {
                m_pEncoderThread->QueueSample(stThreadData);
            }
        }
        else
        {
            assert(nvStatus == NV_ENC_ERR_NEED_MORE_INPUT);
        }
    }
    else
    {
        if (nvStatus == NV_ENC_SUCCESS)
        {
            EncoderThreadData stThreadData;
            stThreadData.pOutputBfr = pOutputBitstream;
            stThreadData.pInputBfr = pInput;
            pOutputBitstream->bWaitOnEvent = true;
            stThreadData.pOutputBfr->bDynResChangeFlag = pEncodeFrame->dynResChangeFlag == 1 ? 1 : 0;
            // Queue o/p Sample
            if (!m_pEncoderThread->QueueSample(stThreadData))
            {
                assert(0);
            }
        }
        else
        {
            assert(0);
        }
    }
    return hr;
}

//
//  EncodeFramePPro() - called by the Premiere Pro Plugin 
//
//     we accept frames in either of the following formats:
//         (1) YV12(4:2:0) planar-format
//         (2) YUV444 packed (32bpp per pixel)   [TODO, not supported]
HRESULT CNvEncoderH264::EncodeFramePPro(EncodeFrameConfig *pEncodeFrame, const bool bFlush)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    HRESULT hr = S_OK;
    NV_ENC_MAP_INPUT_RESOURCE mapRes = {0};

    if (bFlush)
    {
        FlushEncoder();
        return S_OK;
    }

    if (!pEncodeFrame)
    {
        return E_FAIL;
    }

    EncodeInputSurfaceInfo  *pInput;
    EncodeOutputBuffer      *pOutputBitstream;

    if (!m_stInputSurfQueue.Remove(pInput, INFINITE))
    {
        assert(0);
    }


    if (!m_stOutputSurfQueue.Remove(pOutputBitstream, INFINITE))
    {
        assert(0);
    }

    // encode width and height
    unsigned int dwWidth =  m_uMaxWidth; //m_stEncoderInput.width;
    unsigned int dwHeight = m_uMaxHeight;//m_stEncoderInput.height;
    // Align 32 as driver does the same
    unsigned int dwSurfWidth  = (dwWidth + 0x1f) & ~0x1f;
    unsigned int dwSurfHeight = (dwHeight + 0x1f) & ~0x1f;
    unsigned char *pLuma    = pEncodeFrame->yuv[0];
    unsigned char *pChromaU = pEncodeFrame->yuv[1];
    unsigned char *pChromaV = pEncodeFrame->yuv[2];
    unsigned char *pInputSurface = NULL;
    unsigned char *pInputSurfaceCh = NULL;
    unsigned int lockedPitch = dwSurfWidth;
    
/* // PPro plugin doesn't use mapped resources
    if (m_stEncoderInput.useMappedResources)
    {
        if (m_stEncoderInput.interfaceType == NV_ENC_CUDA)
        {
            lockedPitch = pInput->dwCuPitch;
            pInputSurface = pInput->pExtAllocHost;
            pInputSurfaceCh = pInputSurface + (dwSurfHeight*pInput->dwCuPitch);
        }
#if defined(NV_WINDOWS)
        if (m_stEncoderInput.interfaceType == NV_ENC_DX9)
        {
            D3DLOCKED_RECT lockRect = {0};
            IDirect3DSurface9 *pSurf = (IDirect3DSurface9 *)(pInput->pExtAlloc);
            HRESULT hr1 = pSurf->LockRect(&lockRect, NULL, 0);
            if (hr1 == S_OK)
            {
                pInputSurface = (unsigned char *)lockRect.pBits;
                lockedPitch = lockRect.Pitch;
            }
            else
            {
                return hr1;
            }
        }
#endif
    }
    else */
    {
		// Premiere Pro executes this path
        pInputSurface = LockInputBuffer(pInput->hInputSurface, &lockedPitch);
        pInputSurfaceCh = pInputSurface + (dwSurfHeight*lockedPitch);
    }

	// IsNV12Tiled16x16Format (bunch of sqaures)
	//convertYUVpitchtoNV12tiled16x16(pLuma, pChromaU, pChromaV,pInputSurface, pInputSurfaceCh, dwWidth, dwHeight, dwWidth, lockedPitch);
    //(IsNV12PLFormat(pInput->bufferFmt))  (Luma plane intact, chroma planes broken)
	if ( pInput->bufferFmt >= NV_ENC_BUFFER_FORMAT_YUV444_PL ) {
		// input = YUV 4:4:4
		//
		// NVENC only accepts 4:4:4 pixel-data in the YUV444 planar format (3 planes)
		_convertYUV444toY444( 
			dwWidth, dwHeight, pEncodeFrame->stride[0] >> 2,
			reinterpret_cast<uint32_t *>(pEncodeFrame->yuv[0]),
			lockedPitch,
			pInputSurface
		);
	}
	else {
		// input = YUV 4:2:0
		//
		// NVENC only accepts 4:2:0 pixel-data in the NV12_planar format (2 planes)

		/////////////////////////////
		//
		// Select which YUV420 -> NV12 conversion algorithm to use
		//

		//
		// Test the stride-values and memory-pointers for 16-byte alignment.
		// If *everything* is 16-byte aligned, then we will use the faster SSE2-function
		//    ...otherwise must use the slower non-sse2 function
		bool is_xmm_aligned = true;

		// Note: is any of this really necessary?  Premiere Pro CS6 requires x64 capability,
		//       heavily uses SSE2+ instruction-set optimization, so it's probably a
		//       foregone conclusion that everything internal to PPro is already SSE-friendly.

		// check both the src-stride and src-surfaces for *any* unaligned param
		for( unsigned i = 0; (i < 3) ; ++i ) {
			if ( reinterpret_cast<uint64_t>(pEncodeFrame->yuv[i]) & 0xF ) 
				is_xmm_aligned = false;
			else if ( pEncodeFrame->stride[i] & 0xF )
				is_xmm_aligned = false;
		}

		// Now check both the dest-stride and dest-surfaces for *any* unaligned param
		if ( reinterpret_cast<uint64_t>(pInputSurface) & 0xF )
			is_xmm_aligned = false;
		else if ( reinterpret_cast<uint64_t>(pInputSurfaceCh) & 0xF )
			is_xmm_aligned = false;
		else if ( lockedPitch & 0xF )
			is_xmm_aligned = false;

		if ( is_xmm_aligned ) {
			__m128i      *xmm_src_yuv[3];
			unsigned int xmm_src_stride[3];// stride in units of 128bits (i.e. 'stride==1' means 16 bytes)
			__m128i      *xmm_dst_luma;
			__m128i      *xmm_dst_chroma;
			unsigned int xmm_dst_stride;   // destination stride (units of 128 bits)

			for(unsigned i = 0; i < 3; ++i ) {
				xmm_src_yuv[i]    = reinterpret_cast<__m128i *>(pEncodeFrame->yuv[i]);
				xmm_src_stride[i] = pEncodeFrame->stride[i] >> 4;
			}
			xmm_dst_luma   = reinterpret_cast<__m128i *>(pInputSurface);
			xmm_dst_chroma = reinterpret_cast<__m128i *>(pInputSurfaceCh);
			xmm_dst_stride = lockedPitch >> 4;

			_convertYUV420toNV12_sse2( // faster sse2 version (requires 16-byte data-alignment)
				dwWidth, dwHeight,
				xmm_src_yuv,
				xmm_src_stride,
				xmm_dst_luma, xmm_dst_chroma, 
				xmm_dst_stride
			);
		}
		else {
			_convertYUV420toNV12(  // plain (non-SSE2) version, slower
				dwWidth, dwHeight,
				pEncodeFrame->yuv,
				pEncodeFrame->stride,
				pInputSurface, reinterpret_cast<uint16_t*>(pInputSurfaceCh), 
				lockedPitch
			);
		}
	} /////////////////

    // CUDA or DX9 interop with NVENC
/* // PPro plugin doesn't use mapped resources
    if (m_stEncoderInput.useMappedResources)
    {
        // Here we copy from Host to Device Memory (CUDA)
        if (m_stEncoderInput.interfaceType == NV_ENC_CUDA)
        {
            cuCtxPushCurrent(m_cuContext); // Necessary to bind the 
            CUcontext cuContextCurr;
                
            CUresult result = cuMemcpyHtoD((CUdeviceptr)pInput->pExtAlloc, pInput->pExtAllocHost, pInput->dwCuPitch*pInput->dwHeight*3/2);
            cuCtxPopCurrent(&cuContextCurr);
        }
#if defined(NV_WINDOWS)
        // TODO: Grab a pointer GPU Device Memory (DX9) and then copy the result
        if (m_stEncoderInput.interfaceType == NV_ENC_DX9)
        {
            IDirect3DSurface9 *pSurf = (IDirect3DSurface9 *)pInput->pExtAlloc;
            pSurf->UnlockRect();
        }
#endif
        SET_VER(mapRes, NV_ENC_MAP_INPUT_RESOURCE);
        mapRes.registeredResource  = pInput->hRegisteredHandle;
        nvStatus = m_pEncodeAPI->nvEncMapInputResource(m_hEncoder, &mapRes);
        pInput->hInputSurface = mapRes.mappedResource;
    }
    else // here we just pass the frame in system memory to NVENC
*/
    {
        UnlockInputBuffer(pInput->hInputSurface);
    }

    memset(&m_stEncodePicParams, 0, sizeof(m_stEncodePicParams));
    SET_VER(m_stEncodePicParams, NV_ENC_PIC_PARAMS);
    m_stEncodePicParams.inputBuffer = pInput->hInputSurface;
    m_stEncodePicParams.bufferFmt = pInput->bufferFmt;
    m_stEncodePicParams.inputWidth = pInput->dwWidth;
    m_stEncodePicParams.inputHeight = pInput->dwHeight;
    m_stEncodePicParams.outputBitstream = pOutputBitstream->hBitstreamBuffer;
    m_stEncodePicParams.completionEvent = m_bAsyncModeEncoding == true ? pOutputBitstream->hOutputEvent : NULL;
    m_stEncodePicParams.pictureStruct = pEncodeFrame->fieldPicflag ?
		(pEncodeFrame->topField ? NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM : NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP) :
		NV_ENC_PIC_STRUCT_FRAME;
    m_stEncodePicParams.codecPicParams.h264PicParams.h264ExtPicParams.mvcPicParams.viewID = pEncodeFrame->viewId;    
    m_stEncodePicParams.encodePicFlags = 0;
    m_stEncodePicParams.inputTimeStamp = 0;
    m_stEncodePicParams.inputDuration = 0;

    if(!m_stInitEncParams.enablePTD)
    {
        m_stEncodePicParams.codecPicParams.h264PicParams.refPicFlag = 1;
        //m_stEncodePicParams.codecPicParams.h264PicParams.frameNumSyntax = m_dwFrameNumInGOP;
        m_stEncodePicParams.codecPicParams.h264PicParams.displayPOCSyntax = 2*m_dwFrameNumInGOP;
        m_stEncodePicParams.pictureType = ((m_dwFrameNumInGOP % m_stEncoderInput.gopLength) == 0) ? NV_ENC_PIC_TYPE_IDR : NV_ENC_PIC_TYPE_P;
    }

    // Don't allow Dynamic Resolution Changing (not supported in PPro)
	assert (!pEncodeFrame->dynResChangeFlag);

    // Handling Dynamic Bitrate Change (don't need this for PPro)
	assert( pEncodeFrame->dynBitrateChangeFlag != DYN_DOWNSCALE);

    assert(pEncodeFrame->dynBitrateChangeFlag != DYN_UPSCALE);

    if ((m_bAsyncModeEncoding == false) && 
        (m_stInitEncParams.enablePTD == 1))
    {
        EncoderThreadData stThreadData;
        stThreadData.pOutputBfr = pOutputBitstream;
        stThreadData.pInputBfr = pInput;
        stThreadData.pOutputBfr->bDynResChangeFlag = pEncodeFrame->dynResChangeFlag == 1 ? 1 : 0;
        pOutputBitstream->bWaitOnEvent = false;
        m_pEncodeFrameQueue.Add(stThreadData);
    }

    nvStatus = m_pEncodeAPI->nvEncEncodePicture(m_hEncoder, &m_stEncodePicParams);
    
    m_dwFrameNumInGOP++;
    if ((m_bAsyncModeEncoding == false) && 
        (m_stInitEncParams.enablePTD == 1))
    {        
        if (nvStatus == NV_ENC_SUCCESS)
        {
            EncoderThreadData stThreadData;
            while (m_pEncodeFrameQueue.Remove(stThreadData, 0))
            {
                m_pEncoderThread->QueueSample(stThreadData);
            }
        }
        else
        {
            assert(nvStatus == NV_ENC_ERR_NEED_MORE_INPUT);
        }
    }
    else
    {
        if (nvStatus == NV_ENC_SUCCESS)
        {
            EncoderThreadData stThreadData;
            stThreadData.pOutputBfr = pOutputBitstream;
            stThreadData.pInputBfr = pInput;
            pOutputBitstream->bWaitOnEvent = true;
            stThreadData.pOutputBfr->bDynResChangeFlag = pEncodeFrame->dynResChangeFlag == 1 ? 1 : 0;
            // Queue o/p Sample
            if (!m_pEncoderThread->QueueSample(stThreadData))
            {
                assert(0);
            }
        }
        else
        {
            assert(0);
        }
    }
    return hr;
}

//
// nvEncode2 (the command-line transcoder) calls EncodeCudaMemFrame() function
//
HRESULT CNvEncoderH264::EncodeCudaMemFrame(EncodeFrameConfig *pEncodeFrame, CUdeviceptr oDecodedFrame[3], bool bFlush)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    HRESULT hr = S_OK;
    NV_ENC_MAP_INPUT_RESOURCE mapRes = {0};

    if (bFlush)
    {
        FlushEncoder();
        return S_OK;
    }

    if (!pEncodeFrame)
    {
        return E_FAIL;
    }

    EncodeInputSurfaceInfo  *pInput;
    EncodeOutputBuffer      *pOutputBitstream;

    if (!m_stInputSurfQueue.Remove(pInput, INFINITE))
    {
        assert(0);
    }


    if (!m_stOutputSurfQueue.Remove(pOutputBitstream, INFINITE))
    {
        assert(0);
    }

    unsigned int lockedPitch = 0;
    // encode width and height
    unsigned int dwWidth =  m_uMaxWidth; //m_stEncoderInput.width;
    unsigned int dwHeight = m_uMaxHeight;//m_stEncoderInput.height;
    // Align 32 as driver does the same
    unsigned int dwSurfWidth  = (dwWidth + 0x1f) & ~0x1f;
    unsigned int dwSurfHeight = (dwHeight + 0x1f) & ~0x1f;
    //unsigned char *pLuma    = pEncodeFrame->yuv[0];
    //unsigned char *pChromaU = pEncodeFrame->yuv[1];
    //unsigned char *pChromaV = pEncodeFrame->yuv[2];
    unsigned char *pInputSurface = NULL;
    unsigned char *pInputSurfaceCh = NULL;
    

    // CUDA or DX9 interop with NVENC
    if (m_stEncoderInput.useMappedResources)
    {
        // Here we copy from Host to Device Memory (CUDA)
        if (m_stEncoderInput.interfaceType == NV_ENC_CUDA)
        {
            CUresult result;
            cuCtxPushCurrent(m_cuContext); // Necessary to bind the 
            CUcontext cuContextCurr;
            result = cuMemcpyDtoD((CUdeviceptr)pInput->pExtAlloc, oDecodedFrame[0], pInput->dwCuPitch*pInput->dwHeight*3/2);
            checkCudaErrors(result);
            cuCtxPopCurrent(&cuContextCurr);
        }
//#if defined(NV_WINDOWS)
//        // TODO: Grab a pointer GPU Device Memory (DX9) and then copy the result
//        if (m_stEncoderInput.interfaceType == NV_ENC_DX9)
//        {
//            IDirect3DSurface9 *pSurf = (IDirect3DSurface9 *)pInput->pExtAlloc;
//            pSurf->UnlockRect();
//        }
//#endif
        SET_VER(mapRes, NV_ENC_MAP_INPUT_RESOURCE);
        mapRes.registeredResource  = pInput->hRegisteredHandle;
        nvStatus = m_pEncodeAPI->nvEncMapInputResource(m_hEncoder, &mapRes);
        pInput->hInputSurface = mapRes.mappedResource;
    }
    else // here we just pass the frame in system memory to NVENC
    {
        printf("CNvEncoderH264::EncodeCudaMemFrame ERROR !useMappedResources\n");
        UnlockInputBuffer(pInput->hInputSurface);
    }

    memset(&m_stEncodePicParams, 0, sizeof(m_stEncodePicParams));
    SET_VER(m_stEncodePicParams, NV_ENC_PIC_PARAMS);
    m_stEncodePicParams.inputBuffer = pInput->hInputSurface;
    m_stEncodePicParams.bufferFmt = pInput->bufferFmt;
    m_stEncodePicParams.inputWidth = pInput->dwWidth;
    m_stEncodePicParams.inputHeight = pInput->dwHeight;
    m_stEncodePicParams.outputBitstream = pOutputBitstream->hBitstreamBuffer;
    m_stEncodePicParams.completionEvent = m_bAsyncModeEncoding == true ? pOutputBitstream->hOutputEvent : NULL;
    if ( m_stEncoderInput.FieldEncoding == NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME ) {
        // progressive-video encoding mode
        m_stEncodePicParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	}
	else {
        // interlaced-video encoding mode
        // In interlaced-mode, NVENC requires interlaced-input, even if the frames are progressive
        // pEncodeFrame->fieldPicflag ?
        m_stEncodePicParams.pictureStruct = pEncodeFrame->topField ? 
            NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM : NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP;
    }

    m_stEncodePicParams.codecPicParams.h264PicParams.h264ExtPicParams.mvcPicParams.viewID = pEncodeFrame->viewId;    
    m_stEncodePicParams.encodePicFlags = 0;
    m_stEncodePicParams.inputTimeStamp = 0;
    m_stEncodePicParams.inputDuration = 0;

    if(!m_stInitEncParams.enablePTD)
    {
        m_stEncodePicParams.codecPicParams.h264PicParams.refPicFlag = 1;
        //m_stEncodePicParams.codecPicParams.h264PicParams.frameNumSyntax = m_dwFrameNumInGOP;
        m_stEncodePicParams.codecPicParams.h264PicParams.displayPOCSyntax = 2*m_dwFrameNumInGOP;
        m_stEncodePicParams.pictureType = ((m_dwFrameNumInGOP % m_stEncoderInput.gopLength) == 0) ? NV_ENC_PIC_TYPE_IDR : NV_ENC_PIC_TYPE_P;
    }

    // Handling Dynamic Resolution Changing    
    if (pEncodeFrame->dynResChangeFlag)
    {
        m_stEncodePicParams.encodePicFlags = (NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_DYN_RES_CHANGE);
        m_stEncodePicParams.newEncodeWidth  = m_uCurWidth  = pEncodeFrame->newWidth;
        m_stEncodePicParams.newEncodeHeight = m_uCurHeight = pEncodeFrame->newHeight;
    }

    // Handling Dynamic Bitrate Change
    {
        if (pEncodeFrame->dynBitrateChangeFlag == DYN_DOWNSCALE)
        {
            m_stEncodePicParams.encodePicFlags      =  m_stEncodePicParams.encodePicFlags | NV_ENC_PIC_FLAG_DYN_BITRATE_CHANGE;
            m_stEncodePicParams.rcParams.maxBitRate          = (m_stInitEncParams.encodeConfig->rcParams.maxBitRate * 5) / 10;
            m_stEncodePicParams.rcParams.averageBitRate      = (m_stInitEncParams.encodeConfig->rcParams.averageBitRate * 5) / 10;
        }

        if (pEncodeFrame->dynBitrateChangeFlag == DYN_UPSCALE)
        {
            m_stEncodePicParams.encodePicFlags      = m_stEncodePicParams.encodePicFlags | NV_ENC_PIC_FLAG_DYN_BITRATE_CHANGE;
            m_stEncodePicParams.rcParams.maxBitRate          = m_stInitEncParams.encodeConfig->rcParams.maxBitRate * 2 ;
            m_stEncodePicParams.rcParams.averageBitRate      = m_stInitEncParams.encodeConfig->rcParams.averageBitRate * 2 ;
        }
    }

    if ((m_bAsyncModeEncoding == false) && 
        (m_stInitEncParams.enablePTD == 1))
    {
        EncoderThreadData stThreadData;
        stThreadData.pOutputBfr = pOutputBitstream;
        stThreadData.pInputBfr = pInput;
        stThreadData.pOutputBfr->bDynResChangeFlag = pEncodeFrame->dynResChangeFlag == 1 ? 1 : 0;
        pOutputBitstream->bWaitOnEvent = false;
        m_pEncodeFrameQueue.Add(stThreadData);
    }
    nvStatus = m_pEncodeAPI->nvEncEncodePicture(m_hEncoder, &m_stEncodePicParams);
    
    m_dwFrameNumInGOP++;
    if ((m_bAsyncModeEncoding == false) && 
        (m_stInitEncParams.enablePTD == 1))
    {        
        if (nvStatus == NV_ENC_SUCCESS)
        {
            EncoderThreadData stThreadData;
            while (m_pEncodeFrameQueue.Remove(stThreadData, 0))
            {
                m_pEncoderThread->QueueSample(stThreadData);
            }
        }
        else
        {
			if (nvStatus != NV_ENC_ERR_NEED_MORE_INPUT) {
				checkNVENCErrors(nvStatus);
			}
            assert(nvStatus == NV_ENC_ERR_NEED_MORE_INPUT);
        }
    }
    else
    {
        if (nvStatus == NV_ENC_SUCCESS)
        {
            EncoderThreadData stThreadData;
            stThreadData.pOutputBfr = pOutputBitstream;
            stThreadData.pInputBfr = pInput;
            pOutputBitstream->bWaitOnEvent = true;
            stThreadData.pOutputBfr->bDynResChangeFlag = pEncodeFrame->dynResChangeFlag == 1 ? 1 : 0;
            // Queue o/p Sample
            if (!m_pEncoderThread->QueueSample(stThreadData))
            {
                assert(0);
            }
        }
        else
        {
			checkNVENCErrors(nvStatus);
            assert(0);
        }
    }
    return hr;
}

HRESULT CNvEncoderH264::DestroyEncoder()
{
    HRESULT hr = S_OK;
    // common
    hr = ReleaseEncoderResources();
    return hr;
}
