#ifndef _crepackyuv__h
#define _crepackyuv__h

#include "stdint.h"
#include <emmintrin.h> // Visual Studio 2005 MMX/SSE/SSE2 compiler intrinsics
#include <tmmintrin.h> // Visual Studio 2005 SSSE3 compiler intrinsics

class CRepackyuv
{

protected:
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
public:
	void _convert_YUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
		const uint32_t width, const uint32_t height,
		const __m128i * const src_yuv[3],
		const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m128i, 16-bytes)
		__m128i dest_nv12_luma[], __m128i dest_nv12_chroma[],
		const uint32_t dstStride // stride (in units of __m128i, 16-bytes)
		);

	void _convert_YUV420toNV12(const uint32_t width, const uint32_t height,
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
	CRepackyuv();
	~CRepackyuv();
};

#endif // #ifndef _crepackyuv__h