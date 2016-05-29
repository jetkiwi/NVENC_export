#include <cstring>   // memset()

#include "crepackyuv.h"

CRepackyuv::CRepackyuv()
{
	typedef __declspec(align(16)) union {
		__m128i  xmm;   // the SSE2 register
		uint8_t  b[16]; // byte arrangement
		uint16_t w[8];  // word arrangement
		uint32_t d[4];  // dword arrangement
		uint64_t q[2];  // qword arrangement
	} m128_u;

	m128_u mask_y[4], mask_u[4], mask_v[4];

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

	for (uint32_t i = 0; i < 8; ++i) {
		mask_y[0].b[i] = (i << 1) + 1;
		//		mask_y[0].b[i+8] = 0x80; // clear

		//		mask_y[1].b[i  ] = 0x80;
		mask_y[1].b[i + 8] = (i << 1) + 1;
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


	for (uint32_t i = 0; i < 4; ++i) {
		// mask0: get 4 U/V-pixels from source  {x..x+3}
		mask_u[0].b[(i << 1)] = (i << 2);
		mask_u[0].b[(i << 1) + 1] = (i << 2) + 2;

		// mask1 : get another 4 U/V-pixels from source {x+4..x+7}
		// --------
		mask_u[1].b[(i << 1) + 8] = (i << 2);
		mask_u[1].b[(i << 1) + 9] = (i << 2) + 2;
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

	for (uint32_t i = 0; i < 8; ++i) {
		mask_y[0].b[i] = (i << 1);
		//		mask_y[0].b[i+8] = 0x80; // clear

		//		mask_y[1].b[i  ] = 0x80; // clear
		mask_y[1].b[i + 8] = (i << 1);
	}
	m_mask_yuyv422_y[0] = mask_y[0].xmm;
	m_mask_yuyv422_y[1] = mask_y[1].xmm;

	for (uint32_t i = 0; i < 4; ++i) {
		// mask0: get 4 U/V-pixels from source  {x..x+3}
		mask_u[0].b[(i << 1)] = (i << 2) + 1;
		mask_u[0].b[(i << 1) + 1] = (i << 2) + 3;

		// mask1 : get another 4 U/V-pixels from source {x+4..x+7}
		// --------
		mask_u[1].b[(i << 1) + 8] = (i << 2) + 1;
		mask_u[1].b[(i << 1) + 9] = (i << 2) + 3;
	}
	m_mask_yuyv422_uv[0] = mask_u[0].xmm;
	m_mask_yuyv422_uv[1] = mask_u[1].xmm;
}

CRepackyuv::~CRepackyuv()
{
}

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
inline void CRepackyuv::_convertYUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
	const uint32_t width, const uint32_t height,
	const __m128i * const src_yuv[3],
	const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m128i, 16-bytes)
	__m128i dest_nv12_luma[], __m128i dest_nv12_chroma[],
	const uint32_t dstStride // stride (in units of __m128i, 16-bytes)
	)
{
	const uint32_t half_height = (height + 1) >> 1;  // round_up( height / 2 )
	const uint32_t width_div_16 = (width + 15) >> 4; // round_up( width / 16 )

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
	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0; x < width_div_16; ++x) {
			// copy 16 horizontal luma pixels (128-bits) per inner-loop pass
			dest_nv12_luma[y*dstStride + x] = src_yuv[0][src_stride[0] * y + x];
		} // for x
	} // for y


	// copy the chroma portion of the framebuffer
	for (unsigned y = 0; y < half_height; ++y) {
		for (unsigned x = 0, x_div2 = 0; x < width_div_16;) {
			// copy 16 horizontal chroma pixels per inner-loop pass
			// each chroma-pixel is 16-bits {Cb,Cr}
			// (Cb,Cr) pixel x.. x+7

			// Generate 8 output pixels [n..n+7]
			// Read the lower 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			chroma_pixels0 = _mm_unpacklo_epi8(src_yuv[1][src_stride[1] * y + x_div2], src_yuv[2][src_stride[2] * y + x_div2]);// even
			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels0;

			// Generate 8 more output pixels [n+8..n+15]
			// Read the upper 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			++x;
			chroma_pixels1 = _mm_unpackhi_epi8(src_yuv[1][src_stride[1] * y + x_div2], src_yuv[2][src_stride[2] * y + x_div2]);// odd
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

void CRepackyuv::_convert_YUV420toNV12_sse2( // convert planar(YV12) into planar(NV12)
	const uint32_t width, const uint32_t height,
	const __m128i * const src_yuv[3],
	const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m128i, 16-bytes)
	__m128i dest_nv12_luma[], __m128i dest_nv12_chroma[],
	const uint32_t dstStride // stride (in units of __m128i, 16-bytes)
	)
{
	const uint32_t half_height = (height + 1) >> 1;  // round_up( height / 2 )
	const uint32_t width_div_16 = (width + 15) >> 4; // round_up( width / 16 )

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
	for (unsigned y = 0; y < height; y++) {

		for (unsigned x = 0, ptr_dst_y_plus_x = ptr_dst_y, ptr_src0_x_plus_x = ptr_src0_x; x < width_div_16; ++x) {
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

	for (unsigned y = 0; y < half_height; ++y) {
		for (unsigned x = 0, ptr_dst_y_plus_x = ptr_dst_y, ptr_src1_x_plus_xdiv2 = ptr_src1_x, ptr_src2_x_plus_xdiv2 = ptr_src2_x; x < width_div_16; x += 2)
		{
			// copy 16 horizontal chroma pixels per inner-loop pass
			// each chroma-pixel is 16-bits {Cb,Cr}
			// (Cb,Cr) pixel x.. x+7

			// Generate 8 output pixels [n..n+7]
			// Read the lower 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			//			chroma_pixels0 = _mm_unpacklo_epi8( src_yuv[1][src_stride[1]*y + x_div2], src_yuv[2][src_stride[2]*y + x_div2] );// even
			//			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels0;
			//			chroma_pixels0 = _mm_unpacklo_epi8( src_yuv[1][ptr_src1_x + x_div2], src_yuv[2][ptr_src2_x + x_div2] );// even
			chroma_pixels0 = _mm_unpacklo_epi8(src_yuv[1][ptr_src1_x_plus_xdiv2], src_yuv[2][ptr_src2_x_plus_xdiv2]);// even
			//			dest_nv12_chroma[ptr_dst_y + x] = chroma_pixels;
			dest_nv12_chroma[ptr_dst_y_plus_x] = chroma_pixels0;


			// Generate 8 more output pixels [n+8..n+15]
			// Read the upper 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			++ptr_dst_y_plus_x;
			//			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][src_stride[1]*y + x_div2], src_yuv[2][src_stride[2]*y + x_div2] );// odd
			//			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels1;
			//			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][ptr_src1_x + x_div2], src_yuv[2][ptr_src2_x + x_div2] );// odd
			chroma_pixels1 = _mm_unpackhi_epi8(src_yuv[1][ptr_src1_x_plus_xdiv2], src_yuv[2][ptr_src2_x_plus_xdiv2]);// odd
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

void CRepackyuv::_convert_YUV420toNV12( // convert planar(YV12) into planar(NV12)
	const uint32_t width, const uint32_t height,
	const unsigned char * const src_yuv[3], const uint32_t src_stride[3],
	unsigned char dest_nv12_luma[], uint16_t dest_nv12_chroma[],
	const uint32_t dstStride // stride in #bytes (divided by 2)
	)
{
	const uint32_t half_height = (height + 1) >> 1;  // round_up( height / 2)
	const uint32_t half_width = (width + 1) >> 1;  // round up( width / 2)
	const uint32_t dstStride_div_2 = (dstStride + 1) >> 1;// round_up( dstStride / 2)
	const uint32_t dstStride_div_4 = (dstStride + 3) >> 2;// round_up( dstStride / 4)
	uint16_t chroma_pixel; // 1 chroma (Cb+Cr) pixel

	// This loop is the *standard* (failsafe) version of the YUV420->NV12 converter.
	// It is slow and inefficient. If possible, don't use this; use the sse2-version 
	// instead...

	for (unsigned y = 0; y < height; y++)
	{
		memcpy(dest_nv12_luma + (dstStride*y), src_yuv[0] + (src_stride[0] * y), width);
	}

	// optimized access loop -
	// Our expectation is that the source and destination scanlines start on dword-aligned
	// addresses, and that there is sufficient stride in between adjacent scanlines to
	// allow some 'overshoot' past the right-edge of each scanline.

	for (unsigned y = 0; y < half_height; ++y)
	{
		//dest_nv12 = reinterpret_cast<uint32_t *>(dest_nv12_chroma + y*dstStride);

		for (unsigned x = 0; x < half_width; x = x + 2)
		{
			// (Cb,Cr) pixel (x ,  y)
			chroma_pixel = src_yuv[1][src_stride[1] * y + x] |  // Cb
				(src_yuv[2][src_stride[2] * y + x] << 8); // Cr

			dest_nv12_chroma[(y*dstStride_div_2) + x] = chroma_pixel;

			// (Cb,Cr) pixel (x+1, y)
			chroma_pixel = src_yuv[1][src_stride[1] * y + x + 1] |  // Cb
				(src_yuv[2][src_stride[2] * y + x + 1] << 8); // Cr

			dest_nv12_chroma[(y*dstStride_div_2) + x + 1] = chroma_pixel;
		}
	}
}

void CRepackyuv::_convert_YUV444toY444( // convert packed-pixel(Y444) into planar(4:4:4)
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
	for (unsigned y = 0, yout = height - 1; y < height; ++y, --yout)
	{
		for (unsigned x = 0; x < width; ++x)
		{
			dest_y[dst_stride*yout + x] = src_444[src_stride*y + x] >> 16;

			dest_u[dst_stride*yout + x] = src_444[src_stride*y + x] >> 8;
			dest_v[dst_stride*yout + x] = src_444[src_stride*y + x];
		}
		//		memset( (void *)&dest_444_1[dst_stride*yout], 77, width );
		//		memset( (void *)&dest_444_2[dst_stride*yout], 77, width );
	}
}

void CRepackyuv::_convert_YUV444toY444_ssse3( // convert packed-pixel(Y444) into planar(4:4:4)
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

	for (uint32_t y = 0; y < height; ++y)
	{
		//      for ( uint32_t x = 0, yout=height-1; x < width_div_4; x += 4, --yout)
		for (uint32_t x = 0; x < width_div_4; x += 4, ++dst_offset)
		{
			//in_444 = src_444[src_stride*y + x];
			in_444 = src_ptr[0];
			out_y = _mm_shuffle_epi8(in_444, m_mask_yuv444_y[0]);
			out_u = _mm_shuffle_epi8(in_444, m_mask_yuv444_u[0]);
			out_v = _mm_shuffle_epi8(in_444, m_mask_yuv444_v[0]);

			//			in_444 = src_444[src_stride*y + x + 1];
			in_444 = src_ptr[1];
			temp_y = _mm_shuffle_epi8(in_444, m_mask_yuv444_y[1]);
			temp_u = _mm_shuffle_epi8(in_444, m_mask_yuv444_u[1]);
			temp_v = _mm_shuffle_epi8(in_444, m_mask_yuv444_v[1]);
			out_y = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

			//			in_444 = src_444[src_stride*y + x + 2];
			in_444 = src_ptr[2];
			temp_y = _mm_shuffle_epi8(in_444, m_mask_yuv444_y[2]);
			temp_u = _mm_shuffle_epi8(in_444, m_mask_yuv444_u[2]);
			temp_v = _mm_shuffle_epi8(in_444, m_mask_yuv444_v[2]);
			out_y = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

			//			in_444 = src_444[src_stride*y + x + 3];
			in_444 = src_ptr[3];
			temp_y = _mm_shuffle_epi8(in_444, m_mask_yuv444_y[3]);
			temp_u = _mm_shuffle_epi8(in_444, m_mask_yuv444_u[3]);
			temp_v = _mm_shuffle_epi8(in_444, m_mask_yuv444_v[3]);
			out_y = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

			//dest_y[ dst_stride*yout + (x>>2)] = out_y;
			dest_y[dst_offset] = out_y;
			dest_u[dst_offset] = out_u;
			dest_v[dst_offset] = out_v;
			src_ptr += 4;
		} // for x

		// Move src_ptr down by 1 scanline 
		src_ptr += adj_src_stride;

		// Move dst_offset up by 1 scanline 
		dst_offset -= adj_dst_stride;
	} // for y
}

void CRepackyuv::_convert_YUV422toNV12( // convert packed-pixel(Y422) into 2-plane(NV12)
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
	const uint8_t u_shift_x = mode_uyvy ? 0 : 8;
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
	for (uint32_t y = 0; y < height; y += 2) {
		for (uint32_t x = 0; x < width; x += 2, --y) {
			// Each x-iteration, process:
			//     horizontal-pair of pixels {x, x+1} from scanline(y)
			//     horizontal-pair of pixels {x, x+1} from scanline(y+1)

			// scanline y: read 2 pixels {x,x+1}
			src[0] = src_422[(src_stride >> 1)*y + (x >> 1)];// 2 pixels: [pixel x, x+1]

			// Get the chroma(U,V) from current scanline(y)
			u[0] = (src[0] >> u_shift_x) & 0xFF;
			v[0] = (src[0] >> v_shift_x) & 0xFF;

			// Copy the luma(Y) component to output
			dest_y[(dst_stride * y) + x] = src[0] >> luma_shift_x0; // pixel x
			dest_y[(dst_stride * y) + x + 1] = src[0] >> luma_shift_x1; // pixel x+1

			++y; // advance to next scanline

			// scanline y+1: read 2 pixels {x,x+1}
			src[1] = src_422[(src_stride >> 1)*y + (x >> 1)];// 2 pixels: [pixel x, x+1]

			// Copy the luma(Y) component to output
			dest_y[(dst_stride * y) + x] = src[1] >> luma_shift_x0; // pixel x
			dest_y[(dst_stride * y) + x + 1] = src[1] >> luma_shift_x1; // pixel x+1

			// Chroma: take the average of scanline(y) and scanline(y+1)'s chroma pixel
			//    Because in the vertical direction, the output-format (NV12) has half 
			//    the chroma-samples as the input format (YUV422)

			// Get the chroma(U,V) from scanline(y+1)
			u[1] = (src[1] >> u_shift_x) & 0xFF;
			v[1] = (src[1] >> v_shift_x) & 0xFF;

			// average the chroma(U,V) samples together
			dest_uv[dst_stride * (y >> 1) + x] = (u[0] + u[1]) >> 1;
			dest_uv[dst_stride * (y >> 1) + x + 1] = (v[0] + v[1]) >> 1;
		} // for x
	} // for y
}

void CRepackyuv::_convert_YUV422toNV12_ssse3( // convert packed-pixel(Y422) into 2-plane(NV12)
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
	const uint32_t src_ptr_y_inc2 = (src_stride_m128 << 1) - width_div_8;
	const uint32_t dst_ptr_y_inc2 = (dst_stride_m128 << 1) - (width_div_8 >> 1);
	const uint32_t dst_ptr_uv_inc = dst_stride_m128 - (width_div_8 >> 1);

	// setup the pointers to 
	src_ptr_y = src_422;
	src_ptr_yp1 = src_422 + src_stride_m128;

	dst_ptr_y = dest_y;
	dst_ptr_yp1 = dest_y + dst_stride_m128;
	dst_ptr_uv = dest_uv;

	// Choose the src_422's color-format: UYVY or YUYV (this affects byte-ordering)
	const __m128i * const mask_y = mode_uyvy ? m_mask_uyvy422_y : m_mask_yuyv422_y;
	const __m128i * const mask_uv = mode_uyvy ? m_mask_uyvy422_uv : m_mask_yuyv422_uv;

	for (uint32_t y = 0; y < height; y += 2) {
		for (uint32_t x = 0; x < width_div_8; x += 2) {
			// Each x-iteration, process two bundles of 16 pixels (32 pixels total):
			//    @ [scanline y  ]:  pixels {x .. x+15}
			//    @ [scanline y+1]:  pixels {x .. x+15}

			// scanline y: read src-pixels
			//src[0] = src_422[ src_stride_m128*y + x    ];// scanline y, [pixel x ... x+7]
			//src[1] = src_422[ src_stride_m128*y + x + 1];// scanline y, [pixel x+8 ... x+15]
			src[0] = *(src_ptr_y);
			src[1] = *(src_ptr_y + 1);
			src_ptr_y += 2; // advance +16 horizontal YUV422 pixels (32 bytes total)

			// scanline y: Copy the luma(Y) component to output
			tmp_y = _mm_shuffle_epi8(src[0], mask_y[0]);
			out_y = _mm_shuffle_epi8(src[1], mask_y[1]);

			out_y = _mm_or_si128(out_y, tmp_y);  // out_y = out_y | temp_y;
			//dest_y[dst_stride_m128*y + (x>>1)] = out_y;
			(*dst_ptr_y) = out_y;
			++dst_ptr_y;// (destination Y-plane): advance +16 horizontal luma-samples (16 bytes)

			// scanline y: Copy the chroma(UV) components to temp
			uv[0] = _mm_shuffle_epi8(src[0], mask_uv[0]);
			uv[1] = _mm_shuffle_epi8(src[1], mask_uv[1]);
			tmp_uv = _mm_or_si128(uv[0], uv[1]); // tmp = uv[0] | uv[1]

			// scanline y+1: read src-pixels
			//src1[0] = src_422[ src_stride_m128*(y+1) + x    ];// scanline y+1, [pixel x ... x+7]
			//src1[1] = src_422[ src_stride_m128*(y+1) + x + 1];// scanline y+1, [pixel x+8 ... x+15]
			src1[0] = *(src_ptr_yp1);
			src1[1] = *(src_ptr_yp1 + 1);
			src_ptr_yp1 += 2; // advance +16 horizontal YUV422 pixels (32 bytes total)

			tmp_y = _mm_shuffle_epi8(src1[0], mask_y[0]);
			out_y = _mm_shuffle_epi8(src1[1], mask_y[1]);

			// scanline y+1: Copy the chroma(Y) components to temp
			out_y = _mm_or_si128(out_y, tmp_y);  // out_y = out_y | temp_y;
			//dest_y[dst_stride_m128*(y+1) + (x>>1)] = out_y;
			(*dst_ptr_yp1) = out_y;
			++dst_ptr_yp1;// (destination Y-plane): advance +16 horizontal luma-samples (16 bytes)

			// scanline y+1: Copy the chroma(UV) components to temp
			uv[0] = _mm_shuffle_epi8(src1[0], mask_uv[0]);
			uv[1] = _mm_shuffle_epi8(src1[1], mask_uv[1]);
			tmp_uv_yp1 = _mm_or_si128(uv[0], uv[1]); // tmp = uv[0] | uv[1]

			// combine the UV samples from scanlines (y) and (y+1) --
			//    average them together.
			out_uv = _mm_avg_epu8(tmp_uv, tmp_uv_yp1);

			//dest_uv[dst_stride_m128*(y>>1) + (x>>1)] = out_uv;
			(*dst_ptr_uv) = out_uv;
			++dst_ptr_uv;// (destination UV-plane): advance +8 UV-samples (16 bytes)
		} // for x

		src_ptr_y += src_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)
		src_ptr_yp1 += src_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)

		dst_ptr_y += dst_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)
		dst_ptr_yp1 += dst_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)

		dst_ptr_uv += dst_ptr_uv_inc;// advance +2 scanlines (1 row of UV-samples)
	} // for y
}
