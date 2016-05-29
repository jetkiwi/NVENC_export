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

#ifndef _NVENCODE_H264_H_
#define _NVENCODE_H264_H_

#include <CNVEncoder.h>

class CNvEncoderH264:public CNvEncoder
{
public:
    CNvEncoderH264();
    ~CNvEncoderH264();
protected:
    bool                                                 m_bMVC;
    unsigned int                                         m_dwViewId;
    unsigned int                                         m_dwPOC;
    unsigned int                                         m_dwFrameNumSyntax;
    unsigned int                                         m_dwIDRPeriod;
    unsigned int                                         m_dwNumRefFrames[2];
    unsigned int                                         m_dwFrameNumInGOP;
    unsigned int                                         m_uMaxHeight;
    unsigned int                                         m_uMaxWidth;
    unsigned int                                         m_uCurHeight;
    unsigned int                                         m_uCurWidth;
	NV_ENC_H264_SEI_PAYLOAD								 m_sei_user_payload;     // SEI: user encoder settings
	std::string											 m_sei_user_payload_str; // SEI: encoder-settings converted to text-msg

	// SSE2 register mask values for the format-conversion functions
	//  yuv444 : for converting 32bpp packed-pixel 4:4:4 
	//           into planar 4:4:4 (8bpp per plane)
	__m128i m_mask_yuv444_y[4];
	__m128i m_mask_yuv444_u[4];
	__m128i m_mask_yuv444_v[4];

	//  uyvy422 : for converting 16bpp packed-pixel 4:2:2 -> planar NV12
	//            (8bpp luma plane, plus combined UV plane)
	__m128i m_mask_uyvy422_y[2];
	__m128i m_mask_uyvy422_uv[2];

	__m128i m_mask_yuyv422_y[2];
	__m128i m_mask_yuyv422_uv[2];

	// color conversion functions
	void _convert_YUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
		const uint32_t width, const uint32_t height,
		const __m128i * const src_yuv[3],
		const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m128i, 16-bytes)
	    __m128i dest_nv12_luma[], __m128i dest_nv12_chroma[],
		const uint32_t dstStride // stride (in units of __m128i, 16-bytes)
	);

	void _convert_YUV420toNV12(	const uint32_t width, const uint32_t height,
		const unsigned char * const src_yuv[3], const uint32_t src_stride[3],
		unsigned char dest_nv12_luma[], uint16_t dest_nv12_chroma[],
		const uint32_t dstStride
	);

	void _convert_YUV444toY444( // convert packed-pixel(Y444) into planar(4:4:4)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1)
		const uint32_t src_444[],  // pointer to input (YUV444 packed) surface
		const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1)
		unsigned char  dest_y[],   // pointer to output Y-plane
		unsigned char  dest_u[],   // pointer to output U-plane
		unsigned char  dest_v[]    // pointer to output V-plane
	);

	void _convert_YUV444toY444_ssse3( // convert packed-pixel(Y444) into planar(4:4:4)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		const __m128i src_444[],  // pointer to input (YUV444 packed) surface
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_u[],   // pointer to output U-plane
		__m128i dest_v[]    // pointer to output V-plane
	);

	void _convert_YUV422toNV12( // convert packed-pixel(Y422) into 2-plane(NV12)
		const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1)
		const uint32_t src_422[],  // pointer to input (YUV422 packed) surface [2 pixels per 32bits]
		const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1)
		unsigned char  dest_y[],   // pointer to output Y-plane
		unsigned char  dest_uv[]   // pointer to output UV-plane
	);

	void _convert_YUV422toNV12_ssse3( // convert packed-pixel(Y422) into 2-plane(NV12)
		const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		const __m128i src_444[],  // pointer to input (YUV444 packed) surface
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_uv[]  // pointer to output U-plane
	);

public:
    virtual HRESULT                                      InitializeEncoder();
    virtual HRESULT                                      InitializeEncoderH264( NV_ENC_CONFIG_H264_VUI_PARAMETERS *pvui );
	virtual HRESULT                                      ReconfigureEncoder(EncodeConfig EncoderReConfig);
    virtual HRESULT                                      EncodeFrame(EncodeFrameConfig *pEncodeFrame, bool bFlush=false);
	virtual HRESULT                                      EncodeFramePPro(EncodeFrameConfig *pEncodeFrame, const bool bFlush);
    virtual HRESULT                                      EncodeCudaMemFrame(EncodeFrameConfig *pEncodeFrame, CUdeviceptr oFrame[], bool bFlush=false);
    virtual HRESULT                                      DestroyEncoder();
};

#endif
