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

#include<sstream>
#include <include/videoFormats.h>
#include <CNVEncoderH264.h>
#include <xcodeutil.h>
#include <emmintrin.h> // Visual Studio 2005 MMX/SSE/SSE2 compiler intrinsics

#include <helper_cuda_drvapi.h>    // helper file for CUDA Driver API calls and error checking
#include <include/helper_nvenc.h>

#include <nvapi.h> // NVidia NVAPI - functions to query system-info (eg. version of Geforce driver)

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
inline void CNVEncoderH264::_convertYUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
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

inline void CNvEncoderH264::_convert_YUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
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

inline void CNvEncoderH264::_convert_YUV420toNV12( // convert planar(YV12) into planar(NV12)
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

inline void CNvEncoderH264::_convert_YUV444toY444( // convert packed-pixel(Y444) into planar(4:4:4)
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1)
	const uint32_t src_444[],  // pointer to input (YUV444 packed) surface
	const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1)
    unsigned char  dest_y[],   // pointer to output Y-plane
    unsigned char  dest_u[],   // pointer to output U-plane
    unsigned char  dest_v[]    // pointer to output V-plane
)
{
    for ( unsigned y = 0, yout = height-1; y < height; ++y, --yout)
    {
        for ( unsigned x= 0; x < width; ++x)
        {
			dest_y[ dst_stride*yout + x] = src_444[ src_stride*y + x] >> 16;

			dest_u[ dst_stride*yout + x] = src_444[ src_stride*y + x] >> 8;
			dest_v[ dst_stride*yout + x] = src_444[ src_stride*y + x];
        }
//		memset( (void *)&dest_444_1[dst_stride*yout], 77, width );
//		memset( (void *)&dest_444_2[dst_stride*yout], 77, width );
    }
}

inline void CNvEncoderH264::_convert_YUV444toY444_ssse3( // convert packed-pixel(Y444) into planar(4:4:4)
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	const __m128i src_444[],  // pointer to input (YUV444 packed) surface
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
    __m128i dest_y[],   // pointer to output Y-plane
    __m128i dest_u[],   // pointer to output U-plane
    __m128i dest_v[]    // pointer to output V-plane
)
{

	__m128i out_y, out_u, out_v, temp_y, temp_u, temp_v, in_444;

	const uint32_t width_div_4 = (width + 3) >> 2;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const uint32_t adj_src_stride = src_stride - width_div_4;   // adjusted src-stride
	const uint32_t adj_dst_stride = dst_stride + width_div_16;  // adjusted dest-stride

	const __m128i * src_ptr = src_444;
	uint32_t dst_offset = dst_stride*(height - 1);

    for ( uint32_t y = 0; y < height; ++y)
    {
//      for ( uint32_t x = 0, yout=height-1; x < width_div_4; x += 4, --yout)
		for ( uint32_t x = 0; x < width_div_4; x += 4, ++dst_offset)
        {
			//in_444 = src_444[src_stride*y + x];
			in_444 = src_ptr[0];
			out_y = _mm_shuffle_epi8( in_444, m_mask_yuv444_y[0] );
			out_u = _mm_shuffle_epi8( in_444, m_mask_yuv444_u[0] );
			out_v = _mm_shuffle_epi8( in_444, m_mask_yuv444_v[0] );

//			in_444 = src_444[src_stride*y + x + 1];
			in_444 = src_ptr[1];
			temp_y = _mm_shuffle_epi8( in_444, m_mask_yuv444_y[1] );
			temp_u = _mm_shuffle_epi8( in_444, m_mask_yuv444_u[1] );
			temp_v = _mm_shuffle_epi8( in_444, m_mask_yuv444_v[1] );
			out_y  = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u  = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v  = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

//			in_444 = src_444[src_stride*y + x + 2];
			in_444 = src_ptr[2];
			temp_y = _mm_shuffle_epi8( in_444, m_mask_yuv444_y[2] );
			temp_u = _mm_shuffle_epi8( in_444, m_mask_yuv444_u[2] );
			temp_v = _mm_shuffle_epi8( in_444, m_mask_yuv444_v[2] );
			out_y  = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u  = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v  = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

//			in_444 = src_444[src_stride*y + x + 3];
			in_444 = src_ptr[3];
			temp_y = _mm_shuffle_epi8( in_444, m_mask_yuv444_y[3] );
			temp_u = _mm_shuffle_epi8( in_444, m_mask_yuv444_u[3] );
			temp_v = _mm_shuffle_epi8( in_444, m_mask_yuv444_v[3] );
			out_y  = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u  = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v  = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

			//dest_y[ dst_stride*yout + (x>>2)] = out_y;
			dest_y[ dst_offset ] = out_y;
			dest_u[ dst_offset ] = out_u;
			dest_v[ dst_offset ] = out_v;
			src_ptr +=4;
        } // for x

		// Move src_ptr down by 1 scanline 
		src_ptr += adj_src_stride;

		// Move dst_offset up by 1 scanline 
		dst_offset -= adj_dst_stride;
    } // for y
}

inline void CNvEncoderH264::_convert_YUV422toNV12( // convert packed-pixel(Y422) into 2-plane(NV12)
	const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1)
	const uint32_t src_422[],  // pointer to input (YUV422 packed) surface [2 pixels per 32bits]
	const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1)
    unsigned char  dest_y[],   // pointer to output Y-plane
    unsigned char  dest_uv[]   // pointer to output UV-plane
)
{
	// Choose the src_422's color-format: UYVY or YUYV (this affects byte-ordering)
	const uint8_t luma_shift_x0 = mode_uyvy ? 8 : 0;
	const uint8_t luma_shift_x1 = mode_uyvy ? 24 : 16;
	const uint8_t u_shift_x = mode_uyvy ? 0  : 8;
	const uint8_t v_shift_x = mode_uyvy ? 16 : 24;

	// temp vars
	uint32_t src[2];
	uint16_t u[2], v[2];

	// input format: UYVY
	//  Byte# -->
	//     0     1   2    3    4    5    6    7
	//    ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
	//   | U01| Y0 | V01| Y1 | U23| Y2 | V23| Y3 | U45| Y4 | ...
	//   :                   :                   :
	//   |<----- 32 bits --->| <--- 32 bits ---->|
	//    _________ _________  ________ _________ _________
	//   | pixel#0 | pixel#1 | pixel#2 | pixel#3 | pixel#4 |
	//
	//
	for(uint32_t y = 0; y < height; y += 2) {
		for(uint32_t x = 0; x < width; x += 2, --y ) {
			// Each x-iteration, process:
			//     horizontal-pair of pixels {x, x+1} from scanline(y)
			//     horizontal-pair of pixels {x, x+1} from scanline(y+1)

			// scanline y: read 2 pixels {x,x+1}
			src[0] = src_422[ (src_stride>>1)*y + (x>>1)];// 2 pixels: [pixel x, x+1]

			// Get the chroma(U,V) from current scanline(y)
			u[0] = (src[0] >> u_shift_x) & 0xFF;
			v[0] = (src[0] >> v_shift_x) & 0xFF;

			// Copy the luma(Y) component to output
			dest_y[ (dst_stride * y) + x  ] = src[0] >> luma_shift_x0; // pixel x
			dest_y[ (dst_stride * y) + x+1] = src[0] >> luma_shift_x1; // pixel x+1

			++y; // advance to next scanline

			// scanline y+1: read 2 pixels {x,x+1}
			src[1] = src_422[ (src_stride>>1)*y + (x>>1)];// 2 pixels: [pixel x, x+1]
			
			// Copy the luma(Y) component to output
			dest_y[ (dst_stride * y) + x  ] = src[1] >> luma_shift_x0; // pixel x
			dest_y[ (dst_stride * y) + x+1] = src[1] >> luma_shift_x1; // pixel x+1

			// Chroma: take the average of scanline(y) and scanline(y+1)'s chroma pixel
			//    Because in the vertical direction, the output-format (NV12) has half 
			//    the chroma-samples as the input format (YUV422)

			// Get the chroma(U,V) from scanline(y+1)
			u[1] = (src[1] >> u_shift_x) & 0xFF;
			v[1] = (src[1] >> v_shift_x) & 0xFF;

			// average the chroma(U,V) samples together
			dest_uv[ dst_stride * (y>>1) + x  ] = (u[0] + u[1]) >> 1;
			dest_uv[ dst_stride * (y>>1) + x+1] = (v[0] + v[1]) >> 1;
		} // for x
	} // for y
}

inline void CNvEncoderH264::_convert_YUV422toNV12_ssse3( // convert packed-pixel(Y422) into 2-plane(NV12)
	const bool   mode_uyvy,    // chroma-order: true=UYVY, false=YUYV
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	const __m128i src_422[],  // pointer to input (YUV422 packed) surface
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
    __m128i dest_y[],   // pointer to output Y-plane
    __m128i dest_uv[]  // pointer to output U-plane
)
{
	const uint32_t src_stride_m128 = src_stride >> 3;// distance: scanline(x) to (x+1) [units of __m128i]
	const uint32_t dst_stride_m128 = dst_stride >> 4;// distance: scanline(x) to (x+1) [units of __m128i]

	// temp vars
	__m128i src[2];   // scanline #y
	__m128i src1[2];  // scanline #y+1
	__m128i uv[2];
	__m128i tmp_y, out_y, tmp_uv, tmp_uv_yp1, out_uv;
	// input format: UYVY
	//  Byte# -->
	//     0     1   2    3    4    5    6    7
	//    ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
	//   | U01| Y0 | V01| Y1 | U23| Y2 | V23| Y3 | U45| Y4 | ...
	//   :                   :                   :
	//   |<----- 32 bits --->| <--- 32 bits ---->|
	//
	//
	const uint32_t width_div_8 = width >> 3;

	// data-pointers
	const __m128i *src_ptr_y;  // source-pointer to source-pixel {x,y}
	const __m128i *src_ptr_yp1;// source-pointer to source-pixel {x,y+1} (next scanline)
	__m128i *dst_ptr_y;  // pointer to destination (Y)luma-plane: pixel{x,y}
	__m128i *dst_ptr_yp1;// pointer to destination (Y)luma-plane: pixel{x,y+1} (next scanline)
	__m128i *dst_ptr_uv; // pointer to destination (UV)chroma-plane: 
	                     //    corresponding to the 4 luma-pixels {x..x+1,y..y+1}

	// end-of scanline address-increment values (for moving to start of next scanline)
	const uint32_t src_ptr_y_inc2 = (src_stride_m128<<1) - width_div_8;
	const uint32_t dst_ptr_y_inc2 = (dst_stride_m128<<1) - (width_div_8>>1);
	const uint32_t dst_ptr_uv_inc = dst_stride_m128 - (width_div_8>>1);

	// setup the pointers to 
	src_ptr_y   = src_422;
	src_ptr_yp1 = src_422 + src_stride_m128;

	dst_ptr_y   = dest_y;
	dst_ptr_yp1 = dest_y  + dst_stride_m128;
	dst_ptr_uv  = dest_uv;

	// Choose the src_422's color-format: UYVY or YUYV (this affects byte-ordering)
	const __m128i * const mask_y  = mode_uyvy ? m_mask_uyvy422_y  : m_mask_yuyv422_y;
	const __m128i * const mask_uv = mode_uyvy ? m_mask_uyvy422_uv : m_mask_yuyv422_uv;

	for(uint32_t y = 0; y < height; y += 2) {
		for(uint32_t x = 0; x < width_div_8; x += 2 ) {
			// Each x-iteration, process two bundles of 16 pixels (32 pixels total):
			//    @ [scanline y  ]:  pixels {x .. x+15}
			//    @ [scanline y+1]:  pixels {x .. x+15}
			
			// scanline y: read src-pixels
			//src[0] = src_422[ src_stride_m128*y + x    ];// scanline y, [pixel x ... x+7]
			//src[1] = src_422[ src_stride_m128*y + x + 1];// scanline y, [pixel x+8 ... x+15]
			src[0] = *(src_ptr_y    );
			src[1] = *(src_ptr_y + 1);
			src_ptr_y += 2; // advance +16 horizontal YUV422 pixels (32 bytes total)

			// scanline y: Copy the luma(Y) component to output
			tmp_y = _mm_shuffle_epi8( src[0], mask_y[0] );
			out_y = _mm_shuffle_epi8( src[1], mask_y[1] );

			out_y = _mm_or_si128(out_y, tmp_y);  // out_y = out_y | temp_y;
			//dest_y[dst_stride_m128*y + (x>>1)] = out_y;
			(*dst_ptr_y) = out_y;
			++dst_ptr_y;// (destination Y-plane): advance +16 horizontal luma-samples (16 bytes)

			// scanline y: Copy the chroma(UV) components to temp
			uv[0] = _mm_shuffle_epi8( src[0], mask_uv[0] );
			uv[1] = _mm_shuffle_epi8( src[1], mask_uv[1] );
			tmp_uv = _mm_or_si128( uv[0], uv[1] ); // tmp = uv[0] | uv[1]

			// scanline y+1: read src-pixels
			//src1[0] = src_422[ src_stride_m128*(y+1) + x    ];// scanline y+1, [pixel x ... x+7]
			//src1[1] = src_422[ src_stride_m128*(y+1) + x + 1];// scanline y+1, [pixel x+8 ... x+15]
			src1[0] = *(src_ptr_yp1    );
			src1[1] = *(src_ptr_yp1 + 1);
			src_ptr_yp1 += 2; // advance +16 horizontal YUV422 pixels (32 bytes total)

			tmp_y = _mm_shuffle_epi8( src1[0], mask_y[0] );
			out_y = _mm_shuffle_epi8( src1[1], mask_y[1] );

			// scanline y+1: Copy the chroma(Y) components to temp
			out_y = _mm_or_si128(out_y, tmp_y);  // out_y = out_y | temp_y;
			//dest_y[dst_stride_m128*(y+1) + (x>>1)] = out_y;
			(*dst_ptr_yp1) = out_y;
			++dst_ptr_yp1;// (destination Y-plane): advance +16 horizontal luma-samples (16 bytes)

			// scanline y+1: Copy the chroma(UV) components to temp
			uv[0] = _mm_shuffle_epi8( src1[0], mask_uv[0] );
			uv[1] = _mm_shuffle_epi8( src1[1], mask_uv[1] );
			tmp_uv_yp1 = _mm_or_si128( uv[0], uv[1] ); // tmp = uv[0] | uv[1]
			
			// combine the UV samples from scanlines (y) and (y+1) --
			//    average them together.
			out_uv = _mm_avg_epu8( tmp_uv, tmp_uv_yp1);

			//dest_uv[dst_stride_m128*(y>>1) + (x>>1)] = out_uv;
			(*dst_ptr_uv) = out_uv;
			++dst_ptr_uv;// (destination UV-plane): advance +8 UV-samples (16 bytes)
		} // for x

		src_ptr_y   += src_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)
		src_ptr_yp1 += src_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)

		dst_ptr_y   += dst_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)
		dst_ptr_yp1 += dst_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)

		dst_ptr_uv  += dst_ptr_uv_inc;// advance +2 scanlines (1 row of UV-samples)
	} // for y
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
	typedef __declspec(align(16)) union {
	    __m128i  xmm;   // the SSE2 register
	    uint8_t  b[16]; // byte arrangement
	    uint16_t w[8];  // word arrangement
	    uint32_t d[4];  // dword arrangement
	    uint64_t q[2];  // qword arrangement
	} m128_u;

	m128_u mask_y[4], mask_u[4], mask_v[4];

    m_uMaxHeight = 0;
    m_uMaxWidth = 0;
    m_uCurHeight = 0;
    m_uCurWidth = 0;
    m_dwFrameNumInGOP = 0;
	memset( (void *) &m_sei_user_payload, 0, sizeof(m_sei_user_payload) );

	////////////////////
	//
	// YUV444
	//

	// The masks are applied per source pixel 'bundle'
	// a bundle of pixels is 16 horizontal pixels {x .. x+15}
	//
	// mask[0] applies to pixels {  x ..  x+3}
	// mask[1] applies to pixels { x+4 .. x+7}
	// mask[2] applies to pixels { x+8 .. x+11}
	// mask[3] applies to pixels { x+12.. x+15}

	memset( (void *) &mask_y, 0x80, sizeof(mask_y) );
	memset( (void *) &mask_u, 0x80, sizeof(mask_u) );
	memset( (void *) &mask_v, 0x80, sizeof(mask_v) );

	for(uint32_t i = 0; i < 4; ++i ) {
		mask_v[i].b[ (i<<2) + 0] = 0;
		mask_v[i].b[ (i<<2) + 1] = 4;
		mask_v[i].b[ (i<<2) + 2] = 8;
		mask_v[i].b[ (i<<2) + 3] = 12;

		mask_u[i].b[ (i<<2) + 0] = 1;
		mask_u[i].b[ (i<<2) + 1] = 5;
		mask_u[i].b[ (i<<2) + 2] = 9;
		mask_u[i].b[ (i<<2) + 3] = 13;

		mask_y[i].b[ (i<<2) + 0] = 2;
		mask_y[i].b[ (i<<2) + 1] = 6;
		mask_y[i].b[ (i<<2) + 2] = 10;
		mask_y[i].b[ (i<<2) + 3] = 14;

		m_mask_yuv444_y[i] = mask_y[i].xmm;
		m_mask_yuv444_u[i] = mask_u[i].xmm;
		m_mask_yuv444_v[i] = mask_v[i].xmm;
	} // for i

	memset( (void *) &mask_y, 0x80, sizeof(mask_y) );
	memset( (void *) &mask_u, 0x80, sizeof(mask_u) );
	memset( (void *) &mask_v, 0x80, sizeof(mask_v) );

	////////////////////
	//
	// UYVY - 16bpp packed YUV 4:2:2
	//

	// The masks are applied per source pixel 'bundle'
	// a bundle of pixels is 16 horizontal pixels {x .. x+15}
	// mask[0] applies to lower 8 pixels { x..x+7}
	// mask[1] applies to upper 8 pixels { x+8..x+15}

	for(uint32_t i = 0; i < 8; ++i ) {
		mask_y[0].b[i  ] = (i<<1) + 1;
//		mask_y[0].b[i+8] = 0x80; // clear

//		mask_y[1].b[i  ] = 0x80;
		mask_y[1].b[i+8] = (i<<1) + 1;
	}
	m_mask_uyvy422_y[0] = mask_y[0].xmm;
	m_mask_uyvy422_y[1] = mask_y[1].xmm;

	//  byte# ->
	// 0  1  2  3  4  5  6  7 : 8 
	// U0    V0    U1    V1   : U2          source (UYVY 16bpp packed-pixels)
	//    Y0    Y1    Y2    Y3:    Y4
	//
	// byte# ->
	// 0  1  2  3  4  5  6  7 : 8
	//
	// U0 V0 U1 V1 U2 V2 U3 V3:U4 V4 U5 V5 U6 V6 U7 V7  destination (NV12 chroma-plane)
	//  #0  :  #1 :  #2 :  #3 : #4  : #5  : #6  :  #7 :
	// .....:.....:.....:.....:.....:.....:.....:.....:


	for(uint32_t i = 0; i < 4; ++i ) {
		// mask0: get 4 U/V-pixels from source  {x..x+3}
		mask_u[0].b[(i<<1)    ] = (i<<2);
		mask_u[0].b[(i<<1) + 1] = (i<<2) + 2;

		// mask1 : get another 4 U/V-pixels from source {x+4..x+7}
		// --------
		mask_u[1].b[(i<<1) + 8] = (i<<2);
		mask_u[1].b[(i<<1) + 9] = (i<<2) + 2;
	}
	m_mask_uyvy422_uv[0] = mask_u[0].xmm;
	m_mask_uyvy422_uv[1] = mask_u[1].xmm;

	///////////////
	//
	// YUYV - 16bpp packed YUV 4:2:2
	//
	
	//  byte# ->
	// 0  1  2  3  4  5  6  7 : 8 
	//    U0    V0    U1    V1:    U2      source (YUYV 16bpp packed-pixels)
	// Y0    Y1    Y2    Y3   : Y4
	//
	// byte# ->
	// 0  1  2  3  4  5  6  7 : 8
	//
	// U0 V0 U1 V1 U2 V2 U3 V3:U4 V4 U5 V5 U6 V6 U7 V7  destination (NV12 chroma-plane)
	//  #0  :  #1 :  #2 :  #3 : #4  : #5  : #6  :  #7 :
	// .....:.....:.....:.....:.....:.....:.....:.....:

	for(uint32_t i = 0; i < 8; ++i ) {
		mask_y[0].b[i  ] = (i<<1);
//		mask_y[0].b[i+8] = 0x80; // clear

//		mask_y[1].b[i  ] = 0x80; // clear
		mask_y[1].b[i+8] = (i<<1);
	}
	m_mask_yuyv422_y[0] = mask_y[0].xmm;
	m_mask_yuyv422_y[1] = mask_y[1].xmm;
	
	for(uint32_t i = 0; i < 4; ++i ) {
		// mask0: get 4 U/V-pixels from source  {x..x+3}
		mask_u[0].b[(i<<1)    ] = (i<<2) + 1;
		mask_u[0].b[(i<<1) + 1] = (i<<2) + 3;

		// mask1 : get another 4 U/V-pixels from source {x+4..x+7}
		// --------
		mask_u[1].b[(i<<1) + 8] = (i<<2) + 1;
		mask_u[1].b[(i<<1) + 9] = (i<<2) + 3;
	}
	m_mask_yuyv422_uv[0] = mask_u[0].xmm;
	m_mask_yuyv422_uv[1] = mask_u[1].xmm;
}

CNvEncoderH264::~CNvEncoderH264()
{
	DestroyEncoder();

	if ( m_sei_user_payload.payload != NULL )
		delete [] m_sei_user_payload.payload;
}

HRESULT CNvEncoderH264::InitializeEncoder()
{
    return E_FAIL;
}

HRESULT CNvEncoderH264::InitializeEncoderH264(NV_ENC_CONFIG_H264_VUI_PARAMETERS *pvui)
{
	static const uint8_t x264_sei_uuid[16] = // X264's unregistered_user SEI
	{   // random ID number generated according to ISO-11578
		0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9, 0x48, 0xb7,
		0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xef
	};

    HRESULT hr           = S_OK;
    int numFrames        = 0;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    bool bMVCEncoding    = m_stEncoderInput.profile == NV_ENC_H264_PROFILE_STEREO ? true : false;
    m_bAsyncModeEncoding = ((m_stEncoderInput.syncMode==0) ? true : false);
	string            s; // text-buffer
	ostringstream   oss; // text-buffer to generate encoder-settings

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

	// user_SEI: (1) Create 16-byte UUID header (this is x264's uuid)
	for( unsigned i = 0; i < sizeof(x264_sei_uuid)/sizeof(x264_sei_uuid[0]); ++i ) 
		oss << static_cast<char>(x264_sei_uuid[i]);

	// user_SEI: (2) start putting NVENC's encoder-settings
    CUresult        cuResult = CUDA_SUCCESS;
    CUdevice        cuDevice = 0;
	char            gpu_name[100];
	checkCudaErrors(cuDeviceGet(&cuDevice, m_deviceID));
	checkCudaErrors(cuDeviceGetName(gpu_name, 100, cuDevice));

	// Get the Geforce driver-version using NVAPI -
	//   NVENC functionality is a hardware+firmware implementation, so it is important
	//   to report both the GPU-hardware and the Geforce driver revision.
	NvU32             NVidia_DriverVersion;
	NvAPI_ShortString szBuildBranchString;
	NvAPI_Status      nvs = NvAPI_SYS_GetDriverAndBranchVersion( &NVidia_DriverVersion, szBuildBranchString);

	//oss << "x264 - core 141 - H.264/MPEG-4 AVC codec - Copyleft 2003-2012 - " << __DATE__ "}, NVENC API " << std::dec << NVENCAPI_MAJOR_VERSION
	oss << "CNvEncoderH264[" << __DATE__  << ", NVENC API "
		<< std::dec << NVENCAPI_MAJOR_VERSION << "."
		<< std::dec << NVENCAPI_MINOR_VERSION << "]"
		<< gpu_name;
	if ( nvs == NVAPI_OK )
		oss << " (driver " << szBuildBranchString << "," << std::dec 
			<< static_cast<unsigned>(NVidia_DriverVersion)  << ")";
	else
		oss << " (driver ???)"; // unknown driver version
	oss	<< " - options: ";

	oss << " / PROFILE=" << std::dec << m_stEncoderInput.profile;
	// NVENC PRESET - print the index-value instead of the actual GUID (which isn't really informative)
	desc_nv_enc_preset_names.value2string(m_stPresetIdx, s);
	oss << ",PRESET=" << std::dec << m_stPresetIdx;
	oss << "(" << s << ")"; // show ascii-name of the preset

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

	//////////////////////////////////////////////////
	//
	// user_SEI: (3) more NVENC's encoder-settings
	//

		oss << " / rateMode=";
		
		switch( m_stInitEncParams.encodeConfig->rcParams.rateControlMode ) {
			case NV_ENC_PARAMS_RC_CONSTQP:        /**< Constant QP mode */
				oss << "CONSTQP(I:P:B)="
					<< std::dec << m_stEncoderInput.qpI << ":"
					<< std::dec << m_stEncoderInput.qpP << ":"
					<< std::dec << m_stEncoderInput.qpB;
				break;

			case NV_ENC_PARAMS_RC_VBR:            /**< Variable bitrate mode */
				oss << "VBR(avg:max)="
					<< std::dec << m_stEncoderInput.avgBitRate << ":"
					<< std::dec << m_stEncoderInput.peakBitRate;
				break;

			case NV_ENC_PARAMS_RC_CBR:            /**< Constant bitrate mode */
				oss << "CBR(avg)="
					<< std::dec << m_stEncoderInput.avgBitRate;
				break;

			case NV_ENC_PARAMS_RC_VBR_MINQP:      /**< Variable bitrate mode with MinQP */
				// ASSUME min_qp_ena is set!
				oss << "VBR_MINQP(avg:max)="
					<< std::dec << m_stEncoderInput.avgBitRate << ":"
					<< std::dec << m_stEncoderInput.peakBitRate;
				break;

			case NV_ENC_PARAMS_RC_2_PASS_QUALITY: /**< Multi pass CBR encoding optimized for image quality and works only with low latency mode */
				oss << "2_PASS_QUALITY(avg)="
					<< std::dec << m_stEncoderInput.avgBitRate;
				break;

			case NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP: /**< Multi pass CBR encoding optimized for maintaining frame size and works only with low latency mode */
				oss << "2_PASS_FRAMESIZE_CAP(avg)="
					<< std::dec << m_stEncoderInput.avgBitRate;
				break;

			case NV_ENC_PARAMS_RC_2_PASS_VBR: /**< Multi pass VBR encoding for higher quality */
				oss << "2_PASS_VBR(avg:max)="
					<< std::dec << m_stEncoderInput.avgBitRate << ":"
					<< std::dec << m_stEncoderInput.peakBitRate;
				break;
			default:
				break;
		}

		switch( m_stInitEncParams.encodeConfig->rcParams.rateControlMode ) {
			case NV_ENC_PARAMS_RC_CONSTQP:        /**< Constant QP mode */
			case NV_ENC_PARAMS_RC_VBR_MINQP:      /**< Variable bitrate mode with MinQP */
			case NV_ENC_PARAMS_RC_2_PASS_QUALITY: /**< Multi pass encoding optimized for image quality and works only with low latency mode */
			case NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP: /**< Multi pass encoding optimized for maintaining frame size and works only with low latency mode */
				if ( m_stEncoderInput.initial_qp_ena ) {
					oss << ",IniQP(I:P:B)="
						<< std::dec << m_stEncoderInput.initial_qpI << ":"
						<< std::dec << m_stEncoderInput.initial_qpP << ":"
						<< std::dec << m_stEncoderInput.initial_qpB;
				}

				if ( m_stEncoderInput.min_qp_ena ) {
					oss << ",MinQP(I:P:B)="
						<< std::dec << m_stEncoderInput.min_qpI << ":"
						<< std::dec << m_stEncoderInput.min_qpP << ":"
						<< std::dec << m_stEncoderInput.min_qpB;
				}

				if ( m_stEncoderInput.max_qp_ena ) {
					oss << ",MaxQP(I:P:B)="
						<< std::dec << m_stEncoderInput.max_qpI << ":"
						<< std::dec << m_stEncoderInput.max_qpP << ":"
						<< std::dec << m_stEncoderInput.max_qpB;
				}
				break;
		}

		// (for ConstQP and MinQP only), report adaptive-quantization
		//  actually, report it for everything, 'cause I don't know its effect
		if (m_stInitEncParams.encodeConfig->rcParams.enableAQ )
			oss << ",AQ"; // adaptive quantization

#define ADD_ENCODECONFIG_RCPARAM_2_OSS2(var,name) \
	oss << " / " << name << "=" << std::dec << (unsigned) m_stInitEncParams.encodeConfig->rcParams. ## var
#define ADD_ENCODECONFIG_RCPARAM_2_OSS( var ) ADD_ENCODECONFIG_RCPARAM_2_OSS2(var,#var) 

		ADD_ENCODECONFIG_RCPARAM_2_OSS2(vbvBufferSize,"vbvBS");
		ADD_ENCODECONFIG_RCPARAM_2_OSS2(vbvInitialDelay,"vbvID");

	//
	// user_SEI: (3) more NVENC's encoder-settings
	//
	//////////////////////////////////////////////////

        m_stInitEncParams.encodeConfig->frameIntervalP       = m_stEncoderInput.numBFrames + 1;
        m_stInitEncParams.encodeConfig->gopLength            = (m_stEncoderInput.gopLength > 0) ?  m_stEncoderInput.gopLength : 30;
        m_stInitEncParams.encodeConfig->monoChromeEncoding   = m_stEncoderInput.monoChromeEncoding;
        m_stInitEncParams.encodeConfig->frameFieldMode       = m_stEncoderInput.FieldEncoding ;
        m_stInitEncParams.encodeConfig->mvPrecision          = m_stEncoderInput.mvPrecision;

#define ADD_ENCODECONFIG_2_OSS2( var, name ) \
	oss << " / " << name << "=" << std::dec << (unsigned) m_stInitEncParams.encodeConfig-> ## var
#define ADD_ENCODECONFIG_2_OSS(var) ADD_ENCODECONFIG_2_OSS2(var,#var)

#define ADD_ENCODECONFIG_2_OSS2_if_nz( var, name ) \
	if ( m_stInitEncParams.encodeConfig-> ## var ) \
		oss << " / " << name << "=" << std::dec << (unsigned) m_stInitEncParams.encodeConfig-> ## var

#define ADD_ENCODECONFIG_2_OSS_if_nz( var ) ADD_ENCODECONFIG_2_OSS2_if_nz(var,#var)

		ADD_ENCODECONFIG_2_OSS(frameIntervalP,"IntervalP");
		if ( m_stEncoderInput.numBFrames )
			oss << " (BFrames=" << std::dec << m_stEncoderInput.numBFrames << ")";
		ADD_ENCODECONFIG_2_OSS2(gopLength,"gop");
		ADD_ENCODECONFIG_2_OSS2_if_nz(monoChromeEncoding,"mono");
		ADD_ENCODECONFIG_2_OSS2(frameFieldMode,"frameMode");
		ADD_ENCODECONFIG_2_OSS2(mvPrecision,"mv");

        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.disableDeblockingFilterIDC = m_stEncoderInput.disable_deblocking; // alawys enable deblk filter for h264
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.adaptiveTransformMode      = (m_stEncoderInput.profile >= NV_ENC_H264_PROFILE_HIGH) ? m_stEncoderInput.adaptive_transform_mode : NV_ENC_H264_ADAPTIVE_TRANSFORM_AUTOSELECT;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.fmoMode                    = m_stEncoderInput.enableFMO;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.bdirectMode                = m_stEncoderInput.numBFrames > 0 ? m_stEncoderInput.bdirectMode : NV_ENC_H264_BDIRECT_MODE_DISABLE;
//        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.bdirectMode                = m_stEncoderInput.numBFrames > 0 ? NV_ENC_H264_BDIRECT_MODE_TEMPORAL : NV_ENC_H264_BDIRECT_MODE_DISABLE;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputAUD                  = m_stEncoderInput.aud_enable;
//      m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.entropyCodingMode        = (m_stEncoderInput.profile > NV_ENC_H264_PROFILE_BASELINE) ? m_stEncoderInput.vle_entropy_mode : NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.idrPeriod                = m_stInitEncParams.encodeConfig->gopLength ;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.level                    = m_stEncoderInput.level;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.numTemporalLayers        = m_stEncoderInput.numlayers;
/*
		if (m_stEncoderInput.svcTemporal)
        {
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.enableTemporalSVC = 1;
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig.basePriorityID           = 0;
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.h264Extension.svcTemporalConfig.numTemporalLayers = m_stEncoderInput.numlayers;;
        }
*/

		// NVENC 4.0 API
		// -------------
		// Geforce 340.52 WHQL driver quirk on Geforce 750TI (GM107):
		//
		// Setting the chromaFromatIDC to NV_ENC_BUFFER_FORMAT_NV12_TILED64x16 result in the NVENC-driver
		// using the 4:4:4 chroma-format! (Not sure if this was intentional or a bug.)
		//
		// Don't need to specify 'separate color planes'; 
		if ( IsYUV444Format( m_stEncoderInput.chromaFormatIDC ) ) {
			//m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC = NV_ENC_BUFFER_FORMAT_YUV444_PL;// error
			m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC = m_stEncoderInput.useChroma444hack ?
				NV_ENC_BUFFER_FORMAT_NV12_TILED64x16 :  // hack: NVENC interprets this value as YUV444?!?
				m_stEncoderInput.chromaFormatIDC;
		}
		else {
			m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC = NV_ENC_BUFFER_FORMAT_NV12_PL;
		}

#define ADD_ENCODECONFIGH264_2_OSS2(var,name) \
	oss << " / " << name << "=" << std::dec << (unsigned) m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config. ## var
#define ADD_ENCODECONFIGH264_2_OSS( var ) ADD_ENCODECONFIGH264_2_OSS2(var,#var)

#define ADD_ENCODECONFIGH264_2_OSS2_if_nz(var,name) \
	if ( m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config. ## var ) \
		oss << " / " << name << "=" << std::dec << (unsigned) m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config. ## var
#define ADD_ENCODECONFIGH264_2_OSS_if_nz(var,name) ADD_ENCODECONFIGH264_2_OSS2_if_nz(var,#var)

		desc_nv_enc_buffer_format_names.value2string(
			m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.chromaFormatIDC, s
		);
		ADD_ENCODECONFIGH264_2_OSS2(chromaFormatIDC,"chroma");
		//oss << "(" << s << ")"; // show ascii-name of the chromaFormatIDC
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(separateColourPlaneFlag,"sepCPF");
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(disableDeblockingFilterIDC,"disDFIDC");
		ADD_ENCODECONFIGH264_2_OSS2(adaptiveTransformMode,"adaTM");
		ADD_ENCODECONFIGH264_2_OSS_if_nz(fmoMode);
		ADD_ENCODECONFIGH264_2_OSS(bdirectMode);
		ADD_ENCODECONFIGH264_2_OSS_if_nz(outputAUD);
		//ADD_ENCODECONFIGH264_2_OSS(entropyCodingMode);
		ADD_ENCODECONFIGH264_2_OSS(idrPeriod);
		ADD_ENCODECONFIGH264_2_OSS(level);
		//ADD_ENCODECONFIGH264_2_OSS(numTemporalLayers);

        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputBufferingPeriodSEI = m_stEncoderInput.output_sei_BufferPeriod;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputPictureTimingSEI   = m_stEncoderInput.output_sei_PictureTime;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.hierarchicalPFrames      = !! m_stEncoderInput.hierarchicalP;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.hierarchicalBFrames      = !! m_stEncoderInput.hierarchicalB;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.disableSPSPPS            = !! m_stEncoderInput.outBandSPSPPS;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.outputFramePackingSEI    = m_stEncoderInput.stereo3dMode!= NV_ENC_STEREO_PACKING_MODE_NONE ? 1 : 0;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.stereoMode               = (NV_ENC_STEREO_PACKING_MODE)m_stEncoderInput.stereo3dMode;
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.separateColourPlaneFlag  = m_stEncoderInput.separateColourPlaneFlag;// set to 1 to enable 4:4:4 mode
        m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.entropyCodingMode        = (m_stEncoderInput.profile > NV_ENC_H264_PROFILE_BASELINE) ? m_stEncoderInput.vle_entropy_mode : NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC;
        if (m_stEncoderInput.max_ref_frames>0) 
             m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.maxNumRefFrames     = m_stEncoderInput.max_ref_frames;
        if ( pvui != NULL )
            m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.h264VUIParameters = *pvui;

		ADD_ENCODECONFIGH264_2_OSS2_if_nz(outputBufferingPeriodSEI,"outBPSEI");
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(outputPictureTimingSEI,"outPTSEI");
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(hierarchicalPFrames,"hierP");
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(hierarchicalBFrames,"hierB");
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(disableSPSPPS,"disSPSPPS");
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(outputFramePackingSEI,"outFPSEI");
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(stereoMode,"stereo");
		ADD_ENCODECONFIGH264_2_OSS2(entropyCodingMode,"entCM");
		ADD_ENCODECONFIGH264_2_OSS2(maxNumRefFrames,"maxRef");

		// NVENC API 3
		m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.enableVFR = m_stEncoderInput.enableVFR ? 1 : 0;
		m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.sliceMode      = 3;
		m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.sliceModeData  = m_stEncoderInput.numSlices;

		ADD_ENCODECONFIGH264_2_OSS2_if_nz(enableVFR,"enaVFR");
		ADD_ENCODECONFIGH264_2_OSS(sliceMode);
		ADD_ENCODECONFIGH264_2_OSS(sliceModeData);

		// NVENC API 4
		m_stInitEncParams.encodeConfig->encodeCodecConfig.h264Config.qpPrimeYZeroTransformBypassFlag = m_stEncoderInput.qpPrimeYZeroTransformBypassFlag;
		ADD_ENCODECONFIGH264_2_OSS2_if_nz(qpPrimeYZeroTransformBypassFlag,"qpPrimeYZero");
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

		// Query and save the reported hardware capabilities for this NVENC-instance.
		QueryEncoderCaps( m_nv_enc_caps );
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
    
	///////
	//
	// transfer the encoder-settings to m_sei_user_payload
	//

	m_sei_user_payload_str = oss.str();
	//printf( "m_sei_user_payload(%0u) = '%s'\n", m_sei_user_payload_str.length(), m_sei_user_payload_str.c_str() );

	m_sei_user_payload.payloadType = 5;// Annex D : Type 5 = 'user data unregistered'
	m_sei_user_payload.payloadSize = m_sei_user_payload_str.length();// fill in later
	if ( m_sei_user_payload.payload != NULL )
		delete [] m_sei_user_payload.payload;

	m_sei_user_payload.payload = new uint8_t[ m_sei_user_payload.payloadSize ];
	memcpy( (char *)m_sei_user_payload.payload, m_sei_user_payload_str.c_str(), m_sei_user_payload.payloadSize );

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
//    m_stEncodePicParams.codecPicParams.h264PicParams.h264ExtPicParams.mvcPicParams.viewID = pEncodeFrame->viewId;    
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
HRESULT CNvEncoderH264::EncodeFramePPro(
	EncodeFrameConfig *pEncodeFrame,
	const bool bFlush
)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    HRESULT hr = S_OK;
    NV_ENC_MAP_INPUT_RESOURCE mapRes = {0};

	// (for 4:2:0 encoding): input is YUV422 packed format
	const bool input_yuv422 = pEncodeFrame->ppro_pixelformat_is_uyvy422 ||
		pEncodeFrame->ppro_pixelformat_is_yuyv422;
	const bool input_yuv420 = pEncodeFrame->ppro_pixelformat_is_yuv420;
	const bool input_yuv444 = pEncodeFrame->ppro_pixelformat_is_yuv444;

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

	// Test the stride-values and memory-pointers for 16-byte alignment.
	// If *everything* is 16-byte aligned, then we will use the faster SSE2-function
	//    ...otherwise must use the slower non-sse2 function
	bool is_xmm_aligned = true;

	// check both the src-stride and src-surfaces for *any* unaligned param
	unsigned src_plane_count = input_yuv420 ?
		3 : // YUV 4:2:0 planar (3 planes)
		1;  // YUV444 or YUV422 (packed pixel, 1 plane only)

	for( unsigned i = 0; i < src_plane_count; ++i ) {
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

	// IsNV12Tiled16x16Format (bunch of sqaures)
	//convertYUVpitchtoNV12tiled16x16(pLuma, pChromaU, pChromaV,pInputSurface, pInputSurfaceCh, dwWidth, dwHeight, dwWidth, lockedPitch);
    //(IsNV12PLFormat(pInput->bufferFmt))  (Luma plane intact, chroma planes broken)
//	if ( IsYUV444Format(pInput->bufferFmt) ) {
	if ( IsYUV444Format(m_stEncoderInput.chromaFormatIDC) ) {
		// input = YUV 4:4:4
		//
		// Convert the source-video (YUVA_4444 32bpp packed-pixel) into 
		// planar format 4:4. (NVENC only accepts YUV444 3-plane format)
		//

		if ( is_xmm_aligned ) {
			_convert_YUV444toY444_ssse3( // Streaming SSE3 version of converter
				dwWidth, dwHeight, (pEncodeFrame->stride[0] >> 2) >> 2,
				reinterpret_cast<__m128i *>(pEncodeFrame->yuv[0]),
				lockedPitch >> 4,
				reinterpret_cast<__m128i *>(pInputSurface),    // output Y
				reinterpret_cast<__m128i *>(pInputSurfaceCh),  // output U
				reinterpret_cast<__m128i *>(pInputSurfaceCh + (dwSurfHeight*lockedPitch)) // output V
			);
		}
		else {
			// not XMM aligned- call the non-SSE function
			_convert_YUV444toY444(  // non-SSE version (slow)
				dwWidth, dwHeight, pEncodeFrame->stride[0] >> 2,
				reinterpret_cast<uint32_t *>(pEncodeFrame->yuv[0]),
				lockedPitch,
				pInputSurface,    // output Y
				pInputSurfaceCh,  // output U
				pInputSurfaceCh + (dwSurfHeight*lockedPitch) // output V
			);
		} // if ( is_xmm_aligned )
	} // if (IsYUV444Format(m_stEncoderInput.chromaFormatIDC) )

	if ( IsNV12Format(m_stEncoderInput.chromaFormatIDC) ) {
		
		if ( input_yuv420) {
			// Note, PPro handed us YUV4:2:0 (YV12) data, and NVENC only accepts 
			// 4:2:0 pixel-data in the NV12_planar format (2 planes.)
			//
			// ... So we must convert the source-frame from YUV420 -> NV12

			/////////////////////////////
			//
			// Select which YUV420 -> NV12 conversion algorithm to use
			//

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

				_convert_YUV420toNV12_sse2( // faster sse2 version (requires 16-byte data-alignment)
					dwWidth, dwHeight,
					xmm_src_yuv,
					xmm_src_stride,
					xmm_dst_luma, xmm_dst_chroma, 
					xmm_dst_stride
				);
			}
			else {
				// not XMM-aligned: call the plain (non-SSE2) function
				_convert_YUV420toNV12(  // plain (non-SSE2) version, slower
					dwWidth, dwHeight,
					pEncodeFrame->yuv,
					pEncodeFrame->stride,
					pInputSurface, reinterpret_cast<uint16_t*>(pInputSurfaceCh), 
					lockedPitch
				);
			}
		} ///////////////// if ( input_yuv420)
		else if (input_yuv422) {
			// Note, PPro handed us YUV4:2:2 (16bpp packed) data, and NVENC only accepts 
			// 4:2:0 pixel-data in the NV12_planar format (2 planes.)
			//
			// ... So we must convert the source-frame from YUV422 -> NV12

			/////////////////////////////
			//
			// Select which YUV420 -> NV12 conversion algorithm to use
			//
			if ( is_xmm_aligned )
				_convert_YUV422toNV12_ssse3(  // non-SSE version (slow)
					pEncodeFrame->ppro_pixelformat_is_uyvy422, // chroma-order: true=UYVY, false=YUYV
					dwWidth, dwHeight, pEncodeFrame->stride[0] >> 1,
					reinterpret_cast<__m128i *>(pEncodeFrame->yuv[0]),
					lockedPitch,
					reinterpret_cast<__m128i *>(pInputSurface),    // output Y
					reinterpret_cast<__m128i *>(pInputSurfaceCh)  // output UV
				);
			else
				_convert_YUV422toNV12(  // non-SSE version (slow)
					pEncodeFrame->ppro_pixelformat_is_uyvy422, // chroma-order: true=UYVY, false=YUYV
					dwWidth, dwHeight, pEncodeFrame->stride[0] >> 1,
					reinterpret_cast<uint32_t *>(pEncodeFrame->yuv[0]),
					lockedPitch,
					pInputSurface,    // output Y
					pInputSurfaceCh  // output UV
				);

		} ///////////////// if (input_yuv422)
		else {
			// TODO ERROR: if it wasn't YUV420, and not YUV422,
			//  then PremierePro gave us something we can't handle.
			// ABORT
		}

	} // if ( IsNV12Format(m_stEncoderInput.chromaFormatIDC) )

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
//    m_stEncodePicParams.codecPicParams.h264PicParams.h264ExtPicParams.mvcPicParams.viewID = pEncodeFrame->viewId;    
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

	// embed encoder-settings (text-string) into the encoded videostream
	if ( m_sei_user_payload_str.length() ) { // m_sei_user_payload.payloadSize ) {
		m_stEncodePicParams.codecPicParams.h264PicParams.seiPayloadArrayCnt = 1;
		m_stEncodePicParams.codecPicParams.h264PicParams.seiPayloadArray = &m_sei_user_payload;

		// Delete the payload.  This way, our user-sei is only embedded into the *first* frame
		// of the output-bitstream, and nothing subsequent.  While we really should mebed
		// it in every frame, that would bloat the output filesize, and MediaInfo only
		// needs the user-sei in the first-frame to display the info. 
		m_sei_user_payload_str.clear();
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
			if ( IsYUV444Format( m_stEncoderInput.chromaFormatIDC ) )
	            result = cuMemcpyDtoD((CUdeviceptr)pInput->pExtAlloc, oDecodedFrame[0], pInput->dwCuPitch*pInput->dwHeight*1);
			else
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

//    m_stEncodePicParams.codecPicParams.h264PicParams.h264ExtPicParams.mvcPicParams.viewID = pEncodeFrame->viewId;    
    m_stEncodePicParams.encodePicFlags = 0;
    m_stEncodePicParams.inputTimeStamp = 0;
    m_stEncodePicParams.inputDuration = 0;

	// embed encoder-settings (text-string) into the encoded videostream
	if ( m_sei_user_payload_str.length() ) { // m_sei_user_payload.payloadSize ) {
		m_stEncodePicParams.codecPicParams.h264PicParams.seiPayloadArrayCnt = 1;
		m_stEncodePicParams.codecPicParams.h264PicParams.seiPayloadArray = &m_sei_user_payload;

		// Delete the payload.  This way, our user-sei is only embedded into the *first* frame
		// of the output-bitstream, and nothing subsequent.  While we really should mebed
		// it in every frame, that would bloat the output filesize, and MediaInfo only
		// needs the user-sei in the first-frame to display the info. 
		m_sei_user_payload_str.clear();
	}

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
