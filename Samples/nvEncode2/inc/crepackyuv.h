#ifndef _crepackyuv__h
#define _crepackyuv__h

#include "stdint.h"
#include <emmintrin.h> // Visual Studio 2005 MMX/SSE/SSE2 compiler intrinsics
#include <pmmintrin.h> // Visual Studio 2005 SSE3 compiler intrinsics
#include <tmmintrin.h> // Visual Studio 2005 SSSE3 compiler intrinsics
#include <immintrin.h> // Visual Studio 2010 AVX compiler intrinsics

class CRepackyuv
{

protected:
	// CPU-characteristics
	bool    m_cpu_has_avx;  // flag: CPU supports AVX256 instructions (Intel Sandy Bridge 2011)
	bool    m_cpu_has_avx2; // flag: CPU supports AVX2   instructions (Intel Haswell      2013)
	bool    m_cpu_has_ssse3;// flag: CPU supports Streaming SSE3 instructions (Intel Conroe 2006)

	// SSE2 shuffle-mask values for the format-conversion functions
	//  yuv444 : for converting 32bpp packed-pixel 4:4:4 
	//           into planar 4:4:4 (8bpp per plane)
	__m128i m128_shuffle_yuv444_y[4];
	__m128i m128_shuffle_yuv444_u[4];
	__m128i m128_shuffle_yuv444_v[4];
	__m256i m256_shuffle_yuv444_y[2];// AVX2 version of mask
	__m256i m256_shuffle_yuv444_u[2];// AVX2 version of mask
	__m256i m256_shuffle_yuv444_v[2];// AVX2 version of mask
	__m256i m256_permc_yuv444;// permute-control to fixup mm256_shuffle_epi8

	//  uyvy422 : for converting 16bpp packed-pixel 4:2:2 -> planar NV12
	//            (8bpp luma plane, plus combined UV plane)
	__m128i m128_shuffle_uyvy422_y[2];
	__m128i m128_shuffle_uyvy422_uv[2];

	__m128i m128_shuffle_yuyv422_y[2];
	__m128i m128_shuffle_yuyv422_uv[2];

	__m256i m256_shuffle_uyvy422_y[2];
	__m256i m256_shuffle_uyvy422_uv[2];

	__m256i m256_shuffle_yuyv422_y[2];
	__m256i m256_shuffle_yuyv422_uv[2];

	// RGB32f : for converting 128bpp RGB float32 packed -> YUV444
	//          (8bpp luma plane)
	__m128i m128_rgb32fyuv_reorder; 
	__m256i m256_rgb32fyuv_reorder;

	// After a pixel is color-space converted from RGB -> YUV,
	// the offsets below are are added to fix their origin-point.
	__m128i m128_rgb32fyuv_offset0255;  // offset for full-scale YUV
	__m128i m128_rgb32fyuv_offset16240; // offset for 16-235 limited scale YUV
	__m256i m256_rgb32fyuv_offset0255;  // offset for full-scale YUV
	__m256i m256_rgb32fyuv_offset16240; // offset for 16-235 limited scale YUV

	// PC full-scale RGB->YUV conversion coefficients
	__m256  m256_rgbf_y_601;// 8 floats (AVX register)
	__m256  m256_rgbf_u_601;
	__m256  m256_rgbf_v_601;
	__m256  m256_rgbf_y_709;
	__m256  m256_rgbf_u_709;
	__m256  m256_rgbf_v_709;

	// video-scale RGB->YUV conversion coefficients
	__m256  m256_rgbv_y_601;// 8 floats
	__m256  m256_rgbv_u_601;
	__m256  m256_rgbv_v_601;
	__m256  m256_rgbv_y_709;
	__m256  m256_rgbv_u_709;
	__m256  m256_rgbv_v_709;

	// PC full-scale RGB->YUV conversion coefficients
	__m128  m128_rgbf_y_601;// 4 floats (SSE2 register)
	__m128  m128_rgbf_u_601;
	__m128  m128_rgbf_v_601;
	__m128  m128_rgbf_y_709;
	__m128  m128_rgbf_u_709;
	__m128  m128_rgbf_v_709;

	// video-scale RGB->YUV conversion coefficients
	__m128  m128_rgbv_y_601;// 4 floats
	__m128  m128_rgbv_u_601;
	__m128  m128_rgbv_v_601;
	__m128  m128_rgbv_y_709;
	__m128  m128_rgbv_u_709;
	__m128  m128_rgbv_v_709;

	// Pixel-extraction masks for YUV444 -> NV12
	// (used by rgb32f -> nv12 conversion)
	__m128i m128_shuffle_y16to8[4], m128_shuffle_uv16to8[2];
	__m256i m256_shuffle_y16to8[4], m256_shuffle_uv16to8[2];
	__m256i m256_permc_y16to8[4], m256_permc_uv16to8[2];// permute-control fixups

	// Permute-masks for undoing the shuffle built-in to AVX2 int-ops
	//    In the RGBtoNV12_avx2 function, two avx2 pack-instructions are executed
	//    back-to-back, with the comulative side-effect of two permute operations
	//    (permute4x64( x, _mm_shuffle(3,1,2,0) ).  permc_pack2 is a mask for
	//    the permutevar8x32 op, to restore the pixels to the correct-order.
	const __m256i m256_permc_pack2 = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

	// color conversion functions
public:

	void convert_YUV420toNV12(
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
//		const unsigned char * const src_yuv[3],
		unsigned char * const src_yuv[3],
		const uint32_t src_stride[3], // Y/U/V distance: #pixels from scanline(x) to scanline(x+1) [units of uint8_t]
		unsigned char dest_nv12_luma[],  // pointer to output Y-plane
		unsigned char dest_nv12_chroma[],// pointer to output chroma-plane (combined UV)
		const uint32_t dstStride      // distance: #pixels from scanline(x) to scanline(x+1) [units of uint8_t]
		                              //  (same value is used for both Y-plane and UV-plane)
		);

	void convert_YUV444toY444( // convert packed-pixel(Y444) into planar(4:4:4)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1) [units of uint8_t]
		const uint8_t  src_444[],  // pointer to input (YUV444 packed) surface
		const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1) [units of uint8_t]
		unsigned char  dest_y[],   // pointer to output Y-plane
		unsigned char  dest_u[],   // pointer to output U-plane
		unsigned char  dest_v[]    // pointer to output V-plane
		);

	void convert_YUV422toNV12(  // convert packed-pixel(Y422) into 2-plane(NV12)
		const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1) [uints of uint8_t]
		const uint8_t  src_422[],  // pointer to input (YUV422 packed) surface [2 pixels per 32bits]
		const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1) [uints of uint8_t]
		unsigned char  dest_y[],   // pointer to output Y-plane
		unsigned char  dest_uv[]   // pointer to output UV-plane
		);

protected:
	void _convert_YUV420toNV12_avx2( // convert planar(YV12) into planar(NV12)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const __m256i * const src_yuv[3],
		const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m256i, 32-bytes)
		__m256i dest_nv12_luma[], __m256i dest_nv12_chroma[],
		const uint32_t dstStride // stride (in units of __m256i, 32-bytes)
		);

	void _convert_YUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
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

	void _convert_YUV444toY444_avx2( // convert packed-pixel(Y444) into planar(4:4:4)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
		const __m256i src_444[],  // pointer to input (YUV444 packed) surface
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
		const __m128i  src_422[],  // pointer to input (YUV422 packed) surface
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_uv[]  // pointer to output U-plane
		);

	void _convert_YUV422toNV12_avx2( // convert packed-pixel(Y422) into 2-plane(NV12)
		const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
		const __m256i  src_422[],  // pointer to input (YUV422 packed) surface
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
		__m256i dest_y[],   // pointer to output Y-plane
		__m256i dest_uv[]  // pointer to output U-plane
		);

public:
	void convert_RGBFtoY444( // convert packed(RGB f32) into packed(YUV 8bpp)
		const bool     use_bt709,     // color-space select
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of uint8_t]
		const uint8_t  src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of uint8_t]
		uint8_t dest_y[],   // pointer to output Y-plane
		uint8_t dest_u[],   // pointer to output U-plane
		uint8_t dest_v[]    // pointer to output V-plane
		);

	void convert_RGBFtoNV12( // convert packed(RGB f32) into packed(YUV f32)
		const bool     use_bt709,     // color-space select: false=bt601, true=bt709
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of uint8_t]
		const uint8_t  src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of uint8_t]
		uint8_t dest_y[],   // pointer to output Y-plane
		uint8_t dest_uv[]   // pointer to output UV-plane
	);

protected:
	void _convert_RGBFtoY444_ssse3( // convert packed(RGB f32) into packed(YUV 8bpp)
		const bool     use_bt709,     // color-space select
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		const __m128   src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_u[],   // pointer to output U-plane
		__m128i dest_v[]    // pointer to output V-plane
		);

	void _convert_RGBFtoY444_avx2( // convert packed(RGB f32) into packed(YUV 8bpp)
		const bool     use_bt709,     // color-space select: false=bt601, true=bt709
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m256]
		const __m256   src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_u[],   // pointer to output U-plane
		__m128i dest_v[]    // pointer to output V-plane
		);

	void _convert_RGBFtoY444_avx( // convert packed(RGB f32) into packed(YUV 8bpp)
		const bool     use_bt709,     // color-space select: false=bt601, true=bt709
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m256]
		const __m256   src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_u[],   // pointer to output U-plane
		__m128i dest_v[]    // pointer to output V-plane
		);

	void _convert_RGBFtoNV12_ssse3( // convert packed(RGB f32) into packed(YUV f32)
		const bool     use_bt709,     // color-space select: false=bt601, true=bt709
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
		const __m128   src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_uv[]   // pointer to output UV-plane
		);

	void _convert_RGBFtoNV12_avx( // convert packed(RGB f32) into packed(YUV f32)
		const bool     use_bt709,     // color-space select: false=bt601, true=bt709
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
		const __m256   src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
		__m128i dest_y[],   // pointer to output Y-plane
		__m128i dest_uv[]   // pointer to output UV-plane
		);

	void _convert_RGBFtoNV12_avx2( // convert packed(RGB f32) into packed(YUV f32)
		const bool     use_bt709,     // color-space select: false=bt601, true=bt709
		const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
		const uint32_t width,      // X-dimension (#pixels)
		const uint32_t height,     // Y-dimension (#pixels)
		const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
		const __m256   src_rgb[],  // source RGB 32f plane (128 bits per pixel)
		const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
		__m256i dest_y[],   // pointer to output Y-plane
		__m256i dest_uv[]   // pointer to output UV-plane
		);

protected:
	typedef enum {
		SELECT_COLOR_Y = 0,
		SELECT_COLOR_U = 1,
		SELECT_COLOR_V = 2
	} select_color_t;

	void _avx_init(); // initialize values stored in AVX-registers
	inline __m256 get_rgb2yuv_coeff_matrix256(
		const bool use_bt709,
		const bool use_fullscale,
		const select_color_t select_color // 0==Y, 1==U, 2==V
		) const
	{
		if (use_bt709) {
			switch (select_color) {
			case SELECT_COLOR_Y: return use_fullscale ? m256_rgbf_y_709 : m256_rgbv_y_709;
				break;
			case SELECT_COLOR_U: return use_fullscale ? m256_rgbf_u_709 : m256_rgbv_u_709;
				break;
			default: return use_fullscale ? m256_rgbf_v_709 : m256_rgbv_v_709;
				break;
			} // switch
		}
		else {
			switch (select_color) {
			case SELECT_COLOR_Y: return use_fullscale ? m256_rgbf_y_601 : m256_rgbv_y_601;
				break;
			case SELECT_COLOR_U: return use_fullscale ? m256_rgbf_u_601 : m256_rgbv_u_601;
				break;
			default: return use_fullscale ? m256_rgbf_v_601 : m256_rgbv_v_601;
				break;
			} // switch
		}
	}
	
	inline __m128 get_rgb2yuv_coeff_matrix128(
		const bool use_bt709,
		const bool use_fullscale,
		const select_color_t select_color // 0==Y, 1==U, 2==V
		) const
	{
		if (use_bt709) {
			switch (select_color) {
			case SELECT_COLOR_Y: return use_fullscale ? m128_rgbf_y_709 : m128_rgbv_y_709;
				break;
			case SELECT_COLOR_U: return use_fullscale ? m128_rgbf_u_709 : m128_rgbv_u_709;
				break;
			default: return use_fullscale ? m128_rgbf_v_709 : m128_rgbv_v_709;
			} // switch
		}
		else {
			switch (select_color) {
			case SELECT_COLOR_Y: return use_fullscale ? m128_rgbf_y_601 : m128_rgbv_y_601;
				break;
			case SELECT_COLOR_U: return use_fullscale ? m128_rgbf_u_601 : m128_rgbv_u_601;
				break;
			default: return use_fullscale ? m128_rgbf_v_601 : m128_rgbv_v_601;
				break;
			} // switch
		}
	}


	// CPU control flags
	// -----------------
	// By default, all forms of SSE (SSE2, SSSE3) are enabled
	//   This requires an Intel Core 2 Duo (2006) or later CPU,
	//   or an AMD Bulldozer (2011) or later CPU

	bool m_allow_avx;  // allow AVX  (Intel Sandy Bridge 2011)
	bool m_allow_avx2; // allow AVX2 (Intel Haswell 2013)

public:
	CRepackyuv();
	~CRepackyuv();

	bool get_cpu_allow_avx() const { return m_allow_avx; };
	bool get_cpu_allow_avx2() const { return m_allow_avx2; };
	bool set_cpu_allow_avx(bool flag); // sets control-flag, allow_avx
	bool set_cpu_allow_avx2(bool flag);// sets control-flag, allow_avx2
};

#endif // #ifndef _crepackyuv__h