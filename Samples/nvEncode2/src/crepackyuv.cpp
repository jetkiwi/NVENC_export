#include <cstring>   // memset()

#include "crepackyuv.h"
#include "cpuid_ssse3.h"

CRepackyuv::CRepackyuv()
{
	////////////////////
	//
	// YUV444
	//

	// The masks are applied per source pixel 'bundle'
	// a bundle of pixels is 16 horizontal pixels {x .. x+15}
	//
	// shuffle[0] applies to pixels {  x ..  x+3}
	// shuffle[1] applies to pixels { x+4 .. x+7}
	// shuffle[2] applies to pixels { x+8 .. x+11}
	// shuffle[3] applies to pixels { x+12.. x+15}

	memset( (void *)m128_shuffle_yuv444_y, 0x80, sizeof(m128_shuffle_yuv444_y) );
	memset( (void *)m128_shuffle_yuv444_u, 0x80, sizeof(m128_shuffle_yuv444_u) );
	memset( (void *)m128_shuffle_yuv444_v, 0x80, sizeof(m128_shuffle_yuv444_v) );
	//
	//   | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14| 15|
	//                   :               :               :
	//    Y0  V0  U0  A0 |Y1  V1  U1  A1 |Y2  V2  U2  A2 | Y3 <<-- source
	//    |              : |             : |             : |
	//    |   +------------+               V               |
	//    |   |   +------------------------+               V
	//    |   |   |   +------------------------------------+
	//    V   V   V   V
	//    Y0  Y1  Y2  Y3   <<-- output   (from shuffle[0])

	for(uint32_t i = 0; i < 4; ++i ) {
		// Each mask-register #i (for y,u,v channels) pulls in 4 pixels
		m128_shuffle_yuv444_v[i].m128i_u8[(i << 2) + 0] = 0;// Y-component of pixel#[x]
		m128_shuffle_yuv444_v[i].m128i_u8[(i << 2) + 1] = 4;// Y-component of pixel#[x+1]
		m128_shuffle_yuv444_v[i].m128i_u8[(i << 2) + 2] = 8;// Y-component of pixel#[x+2]
		m128_shuffle_yuv444_v[i].m128i_u8[(i << 2) + 3] = 12;// Y-component of pixel#[x+3]

		m128_shuffle_yuv444_u[i].m128i_u8[(i << 2) + 0] = 1;// U-component of pixel#[x]
		m128_shuffle_yuv444_u[i].m128i_u8[(i << 2) + 1] = 5;// U-component of pixel#[x+1]
		m128_shuffle_yuv444_u[i].m128i_u8[(i << 2) + 2] = 9;// U-component of pixel#[x+2]
		m128_shuffle_yuv444_u[i].m128i_u8[(i << 2) + 3] = 13;// U-component of pixel#[x+3]

		m128_shuffle_yuv444_y[i].m128i_u8[(i << 2) + 0] = 2;// V-component of pixel#[x]
		m128_shuffle_yuv444_y[i].m128i_u8[(i << 2) + 1] = 6;// V-component of pixel#[x+1]
		m128_shuffle_yuv444_y[i].m128i_u8[(i << 2) + 2] = 10;// V-component of pixel#[x+2]
		m128_shuffle_yuv444_y[i].m128i_u8[(i << 2) + 3] = 14;// V-component of pixel#[x+3]
	} // for i

	////////////////////
	//
	// UYVY - 16bpp packed YUV 4:2:2
	//

	// The masks are applied per source pixel 'bundle'
	// a bundle of pixels is 16 horizontal pixels {x .. x+15}
	// shuffle[0] applies to lower 8 pixels { x..x+7}
	// shuffle[1] applies to upper 8 pixels { x+8..x+15}

	memset((void *)&m128_shuffle_uyvy422_y, 0x80, sizeof(m128_shuffle_uyvy422_y));
	for (uint32_t i = 0; i < 8; ++i) {
		m128_shuffle_uyvy422_y[0].m128i_u8[i    ] = (i << 1) + 1;// get pixels#[x..x+7]
		m128_shuffle_uyvy422_y[1].m128i_u8[i + 8] = (i << 1) + 1;// get pixels#[x+8..x+15]
	}


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
	memset((void *)m128_shuffle_uyvy422_uv, 0x80, sizeof(m128_shuffle_uyvy422_uv));

	for (uint32_t i = 0; i < 4; ++i) {
		// mask0: get 4 U/V-pixels from source  {x..x+3}
		m128_shuffle_uyvy422_uv[0].m128i_u8[(i << 1)    ] = (i << 2);    // U
		m128_shuffle_uyvy422_uv[0].m128i_u8[(i << 1) + 1] = (i << 2) + 2;// V

		// mask1 : get another 4 U/V-pixels from source {x+4..x+7}
		// --------
		m128_shuffle_uyvy422_uv[1].m128i_u8[(i << 1) + 8] = (i << 2);    // U
		m128_shuffle_uyvy422_uv[1].m128i_u8[(i << 1) + 9] = (i << 2) + 2;// V
	}

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
	memset((void *)m128_shuffle_yuyv422_y, 0x80, sizeof(m128_shuffle_yuyv422_y));
	memset((void *)m128_shuffle_yuyv422_uv, 0x80, sizeof(m128_shuffle_yuyv422_uv));

	for (uint32_t i = 0; i < 8; ++i) {
		m128_shuffle_yuyv422_y[0].m128i_u8[i    ] = (i << 1);
		m128_shuffle_yuyv422_y[1].m128i_u8[i + 8] = (i << 1);
	}

	for (uint32_t i = 0; i < 4; ++i) {
		// mask0: get 4 U/V-pixels from source  {x..x+3}
		m128_shuffle_yuyv422_uv[0].m128i_u8[(i << 1)    ] = (i << 2) + 1;// U
		m128_shuffle_yuyv422_uv[0].m128i_u8[(i << 1) + 1] = (i << 2) + 3;// V

		// mask1 : get another 4 U/V-pixels from source {x+4..x+7}
		// --------
		m128_shuffle_yuyv422_uv[1].m128i_u8[(i << 1) + 8] = (i << 2) + 1;// U
		m128_shuffle_yuyv422_uv[1].m128i_u8[(i << 1) + 9] = (i << 2) + 3;// V
	}

#define MASK4F(d,a,b,c) \
	{ 255*(a), 255*(b), 255*(c), 255*(d)} // PC full-scale
#define MASK4V(d,a,b,c)  \
	{ 220*(a), 220*(b), 220*(c), 220*(d)} // video limited-scale

	// PC-full scale YUV->RGB coefficients (Bt601)
	//                         A   B         G         R
	m128_rgbf_y_601 = { MASK4F(0,  0.114,    0.587,    0.299) };
	m128_rgbf_u_601 = {	MASK4F(0,  0.436,   -0.28886, -0.14713) };
	m128_rgbf_v_601 = { MASK4F(0, -0.10001, -0.51499,  0.615) };

	// PC-full scale YUV->RGB coefficients (Bt709)
	//                         A   B         G         R
	m128_rgbf_y_709 = { MASK4F(0,  0.0722,   0.7152,   0.2126) };
	m128_rgbf_u_709 = { MASK4F(0,  0.436,   -0.33609, -0.09991) };
	m128_rgbf_v_709 = { MASK4F(0, -0.05639, -0.55861,  0.615) };

		/////////////////////////////////////////////////////

	// video-scale YUV->RGB coefficients (Bt601)
	//                         A   B         G         R
	m128_rgbv_y_601 = { MASK4V(0,  0.114,    0.587,    0.299) };
	m128_rgbv_u_601 = {	MASK4V(0,  0.436,   -0.28886, -0.14713) };
	m128_rgbv_v_601 = { MASK4V(0, -0.10001, -0.51499,  0.615) };

	// video-scale YUV->RGB coefficients (Bt709)
	//                         A   B         G         R
	m128_rgbv_y_709 = { MASK4V(0,  0.0722,   0.7152,   0.2126) };
	m128_rgbv_u_709 = { MASK4V(0,  0.436,   -0.33609, -0.09991) };
	m128_rgbv_v_709 = { MASK4V(0, -0.05639, -0.55861,  0.615) };

	// m128_rgb32fyuv_reorder: <shuffle_epi8 mask>
	//  Input Float#7  6  5  4  3  2  1  0
	//              0  V1 U1 Y1 0  V0 U0 Y0
	//
	// Output Float#7  6  5  4  3  2  1  0
	//              0  Y1 U1 V1 0  Y0 U0 V0
	for (uint32_t i = 0; i < 16; i += 4) {
		// For each 32bpp packed-pixel: reorder ARGB -> BGRA
		m128_rgb32fyuv_reorder.m128i_u8[i + 0] = 2 + i;// V0 from byte +2
		m128_rgb32fyuv_reorder.m128i_u8[i + 1] = 1 + i;// U0 from byte +1
		m128_rgb32fyuv_reorder.m128i_u8[i + 2] = 0 + i;// Y0 from byte +0
		m128_rgb32fyuv_reorder.m128i_u8[i + 3] = 0x80; // A0 (unused)
	}

	// For RGB32f -> YUV conversion
	//  Normalization offsets to reposition the Y/U/V sample origins
	m128_rgb32fyuv_offset0255 = { // PC/full-scale (full 8-bit: 0-255)
		{
			0,   0, // Y0 word0
			128, 0, // U0 word1
			128, 0, // V0 ...
			0,   0, // alpha (unused)
			0,   0, // Y1
			128, 0, // U1
			128, 0, // V1
			0,   0  // alpha (unused)
		}
	};

	// For RGB32f -> YUV conversion:
	//  Normalization offsets to reposition the Y/U/V sample origins
	m128_rgb32fyuv_offset16240 = { // video-scale (limited to 16-235)
		{
			16,  0, // Y0 word0
			128, 0, // U0 word1
			128, 0, // V0 ...
			0,   0, // alpha (unused)
			16,  0, // Y1
			128, 0, // U1
			128, 0, // V1
			0,   0  // alpha (unused)
		}
	};

	// For RGB32f -> YUV conversion
	for (uint32_t k = 0; k < 4; ++k) {
		// Repack 16-bit luma(Y) samples from multiple xmm registers into 1 reg
		memset((void *)&m128_shuffle_y16to8[k], 0x80, sizeof(__m128i));// mask for <_mm_shuffle_epi8>

		// for k=0:    move Y1, Y0 into words 1 & 0 (bits [31:16] and [15:00] respectively)
		// for k=1:    move Y3, Y2 into words 3 & 2 (bits [63:48] and [47:32] respectively)
		// for k=2:    move Y5, Y4 into words 5 & 4 (bits [95:80] and [79:64] respectively)
		// for k=3:    move Y7, Y6 into words 7 & 6 (bits [127:112] and [111:96] respectively)

		// move source word#[0]  to destination word#[k]
		m128_shuffle_y16to8[k].m128i_u8[(k << 2)    ] = 0;// Y0 (lower byte)
		m128_shuffle_y16to8[k].m128i_u8[(k << 2) + 1] = 1;// Y0 (upper byte)

		// move source word#[4]  to destination word#[k + 1]
		m128_shuffle_y16to8[k].m128i_u8[(k << 2) + 2] = 8;// Y1 (lower byte)
		m128_shuffle_y16to8[k].m128i_u8[(k << 2) + 3] = 9;// Y1 (upper byte)

	} // for k

	for (uint32_t k = 0; k < 2; ++k) {
		// Repack 16-bit chroma(UV) samples from multiple xmm regs into 1 reg

		// for k=0:    move V1, V0, U1, U0 into words 3,2,1,0 (respectively)
		// for k=1:    move V1, V0, U1, U0 into words 7,6,5,4 (respectively)

		m128_shuffle_uv16to8[k] = { {		// mask for <_mm_shuffle_epi8>
				0x80, 0x80, 0x80, 0x80,
				0x80, 0x80, 0x80, 0x80,
				0x80, 0x80, 0x80, 0x80,
				0x80, 0x80, 0x80, 0x80 } };

		// move source word#1  "U0"  to destination word#[0]
		// move source word#2  "V0"  to destination word#[2]
		// move source word#5  "U1"  to destination word#[1]
		// move source word#6  "V1"  to destination word#[3]
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 0] = 2;    // U0
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 1] = 3;
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 2] = 2 + 8;// U1
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 3] = 3 + 8;
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 4] = 4;    // V0
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 5] = 5;
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 6] = 4 + 8;// V1
		m128_shuffle_uv16to8[k].m128i_u8[(k << 3) + 7] = 5 + 8;
	} // for k

	// Move any initialization that requires AVX-instructions, to separate function.
	m_cpu_has_ssse3 = get_cpuinfo_has_ssse3();
	m_cpu_has_avx   = get_cpuinfo_has_avx();
	m_cpu_has_avx2  = get_cpuinfo_has_avx2();

	m_allow_avx  = m_cpu_has_avx;
	m_allow_avx2 = m_cpu_has_avx2;

	_avx_init(); // setup constants/masks for AVX-versions of functions
}

void 
CRepackyuv::_avx_init()
{
	if ( !m_cpu_has_avx )
		return;

#define MASK8F(h,e,f,g,d,a,b,c) \
	{ 255*(a), 255*(b), 255*(c), 255*(d), 255*(e), 255*(f), 255*(g), 255*(h)} // pc full-scale

#define MASK8V(h,e,f,g,d,a,b,c) \
	{ 220*(a), 220*(b), 220*(c), 220*(d), 220*(e), 220*(f), 220*(g), 220*(h)} // video-scale (16-235)
	
	// video-scale YUV->RGB coefficients (Bt709)
	// PC full-scale YUV->RGB coefficients (Bt601)
	//                         A      B        G         R      A     B         G         R
	m256_rgbf_y_601 = { MASK8F(0,  0.114,    0.587,    0.299,   0,  0.114,    0.587,    0.299) };
	m256_rgbf_u_601 = { MASK8F(0,  0.436,   -0.28886, -0.14713, 0,  0.436,   -0.28886, -0.14713) };
	m256_rgbf_v_601 = { MASK8F(0, -0.10001, -0.51499,  0.615,   0, -0.10001, -0.51499,  0.615) };

	// PC full-scale YUV->RGB coefficients (Bt709)
	//                         A      B        G         R      A     B         G         R
	m256_rgbf_y_709 = { MASK8F(0,  0.0722,   0.7152,   0.2126,  0,  0.0722,   0.7152,   0.2126) };
	m256_rgbf_u_709 = { MASK8F(0,  0.436,   -0.33609, -0.09991, 0,  0.436,   -0.33609, -0.09991) };
	m256_rgbf_v_709 = { MASK8F(0, -0.05639, -0.55861,  0.615,   0, -0.05639, -0.55861,  0.615) };

	// video-scale YUV->RGB coefficients (Bt601)
	//                         A      B        G         R      A     B         G         R
	m256_rgbv_y_601 = { MASK8V(0,  0.114,    0.587,    0.299,   0,  0.114,    0.587,    0.299) };
	m256_rgbv_u_601 = { MASK8V(0,  0.436,   -0.28886, -0.14713, 0,  0.436,   -0.28886, -0.14713) };
	m256_rgbv_v_601 = { MASK8V(0, -0.10001, -0.51499,  0.615,   0, -0.10001, -0.51499,  0.615) };

	// video-scale YUV->RGB coefficients (Bt709)
	//                         A      B        G         R      A     B         G         R
	m256_rgbv_y_709 = { MASK8V(0,  0.0722,   0.7152,   0.2126,  0,  0.0722,   0.7152,   0.2126) };
	m256_rgbv_u_709 = { MASK8V(0,  0.436,   -0.33609, -0.09991, 0,  0.436,   -0.33609, -0.09991) };
	m256_rgbv_v_709 = { MASK8V(0, -0.05639, -0.55861,  0.615,   0, -0.05639, -0.55861,  0.615) };

	for (uint32_t i = 0; i < 16; i += 4) {
		// For each 32bpp packed-pixel: reorder ARGB -> BGRA
		m256_rgb32fyuv_reorder.m256i_u8[i + 0] = 2 + i;// V0 from byte +2
		m256_rgb32fyuv_reorder.m256i_u8[i + 1] = 1 + i;// U0 from byte +1
		m256_rgb32fyuv_reorder.m256i_u8[i + 2] = 0 + i;// Y0 from byte +0
		m256_rgb32fyuv_reorder.m256i_u8[i + 3] = 0x80; // A0 (unused)

		// For each 32bpp packed-pixel: reorder ARGB -> BGRA
		m256_rgb32fyuv_reorder.m256i_u8[i + 16] = 18 + i;// V0 from byte +2
		m256_rgb32fyuv_reorder.m256i_u8[i + 17] = 17 + i;// U0 from byte +1
		m256_rgb32fyuv_reorder.m256i_u8[i + 18] = 16 + i;// Y0 from byte +0
		m256_rgb32fyuv_reorder.m256i_u8[i + 19] = 0x80; // A0 (unused)
	}

	// For RGB32f -> YUV conversion
	//  Normalization offsets to reposition the Y/U/V sample origins
	m256_rgb32fyuv_offset0255 = { // PC/full-scale (0-255)
		{
			0,   0, // Y0 word0
			128, 0, // U0 word1
			128, 0, // V0 ...
			0,   0, // alpha (unused)
			0,   0, // Y1
			128, 0, // U1
			128, 0, // V1
			0,   0, // alpha (unused)
			0,   0, // Y2 word0
			128, 0, // U2 word1
			128, 0, // V2 ...
			0,   0, // alpha (unused)
			0,   0, // Y3
			128, 0, // U3
			128, 0, // V3
			0,   0  // alpha (unused)
		}
	};

	// For RGB32f -> YUV conversion
	//  Normalization offsets to reposition the Y/U/V sample origins
	m256_rgb32fyuv_offset16240 = { // video-scale (16-235)
		{
			16,  0, // Y0 word0
			128, 0, // U0 word1
			128, 0, // V0 ...
			0,   0, // alpha (unused)
			16,  0, // Y1
			128, 0, // U1
			128, 0, // V1
			0,   0,  // alpha (unused)
			16,  0, // Y2 word0
			128, 0, // U2 word1
			128, 0, // V2 ...
			0,   0, // alpha (unused)
			16,  0, // Y3
			128, 0, // U3
			128, 0, // V3
			0,   0  // alpha (unused)
		}
	};

	// For RGB32f -> YUV conversion
	//   _mm256_shuffle_epi8 masks: repack Y-pixel information from
	//      multiple registers [packed-pixel data (64 bits per pixel)], into a single
	//      register (planar-format.)
	for (uint32_t k = 0; k < 4; ++k) {
		memset((void *)&m256_shuffle_y16to8[k], 0x80, sizeof(__m256i));// mask for <_mm256_shuffle_epi8>
	}
	// m256_shuffle_y16to8: the *intent* is to perform the following
	// 
	//     for k=0:    move Y3-Y0   into words 3,2,1,0     (bits [63:00])
	//     for k=1:    move Y7-Y4   into words 7,6,5,4     (bits [127:64])
	//     for k=2:    move Y11-Y8  into words 11,10,9,8   (bits [191:128])
	//     for k=3:    move Y15-Y12 into words 15,14,13,12 (bits [255:192])
	//
	// However, the avx256 shuffle swaps bits[191:128]<->[127:64], so it is
	// impossible to directly reach the above goal.  Instead, the shuffles
	// swap bits[191:128]<->[127:64].

	m256_shuffle_y16to8[0].m256i_u8[0    ] = 0;// Y0 (lower byte)  into dest word#0
	m256_shuffle_y16to8[0].m256i_u8[1    ] = 1;// Y0
	m256_shuffle_y16to8[0].m256i_u8[2    ] = 8;// Y1 (lower byte)  into dest word#1
	m256_shuffle_y16to8[0].m256i_u8[3    ] = 9;// Y1
	m256_shuffle_y16to8[0].m256i_u8[16+ 4] = 16 - 16;// Y2 (lower byte) into dest word#10 (instead of 8)
	m256_shuffle_y16to8[0].m256i_u8[16+ 5] = 17 - 16;// Y2
	m256_shuffle_y16to8[0].m256i_u8[16 +6] = 24 - 16;// Y3  into dest word#11 (instead of 9)
	m256_shuffle_y16to8[0].m256i_u8[16 +7] = 25 - 16;// Y3

	m256_shuffle_y16to8[1].m256i_u8[8 ] = 0;// Y0 (lower byte) into dest word#4
	m256_shuffle_y16to8[1].m256i_u8[9 ] = 1;// Y0
	m256_shuffle_y16to8[1].m256i_u8[10] = 8;// Y1 (lower byte) into dest word#5
	m256_shuffle_y16to8[1].m256i_u8[11] = 9;// Y1
	m256_shuffle_y16to8[1].m256i_u8[16 +12] = 16 - 16;// Y2 (lower byte) into dest word#14
	m256_shuffle_y16to8[1].m256i_u8[16 +13] = 17 - 16;// Y2
	m256_shuffle_y16to8[1].m256i_u8[16 +14] = 24 - 16;// Y3 into dest word #15
	m256_shuffle_y16to8[1].m256i_u8[16 +15] = 25 - 16;// Y3

	m256_shuffle_y16to8[2].m256i_u8[16 - 16] = 0;// Y0 (lower byte)
	m256_shuffle_y16to8[2].m256i_u8[17 - 16] = 1;// Y0
	m256_shuffle_y16to8[2].m256i_u8[18 - 16] = 8;// Y1 (lower byte)
	m256_shuffle_y16to8[2].m256i_u8[19 - 16] = 9;// Y1
	m256_shuffle_y16to8[2].m256i_u8[20] = 16 - 16;// Y2 (lower byte)
	m256_shuffle_y16to8[2].m256i_u8[21] = 17 - 16;// Y2
	m256_shuffle_y16to8[2].m256i_u8[22] = 24 - 16;// Y3
	m256_shuffle_y16to8[2].m256i_u8[23] = 25 - 16;// Y3

	m256_shuffle_y16to8[3].m256i_u8[24 - 16] = 0;// Y0 (lower byte)
	m256_shuffle_y16to8[3].m256i_u8[25 - 16] = 1;// Y0
	m256_shuffle_y16to8[3].m256i_u8[26 - 16] = 8;// Y1 (lower byte)
	m256_shuffle_y16to8[3].m256i_u8[27 - 16] = 9;// Y1
	m256_shuffle_y16to8[3].m256i_u8[28] = 16 - 16;// Y2 (lower byte)
	m256_shuffle_y16to8[3].m256i_u8[29] = 17 - 16;// Y2
	m256_shuffle_y16to8[3].m256i_u8[30] = 24 - 16;// Y3
	m256_shuffle_y16to8[3].m256i_u8[31] = 25 - 16;// Y3

	// The permute-masks to re-shuffle the out-of-order pixels into 
	// the correct, final lane(s)
	m256_permc_y16to8[0] = _mm256_set_epi32(7, 7, 7, 7, 7, 7, 5, 0);
	//                                                        U1 L0

	m256_permc_y16to8[1] = _mm256_set_epi32(0, 0, 0, 0, 7, 2, 0, 0);
	//                                                 U3 L2

	m256_permc_y16to8[2] = _mm256_set_epi32(7, 7, 5, 0, 7, 7, 7, 7);
	//                                            U1 L0

	m256_permc_y16to8[3] = _mm256_set_epi32(7, 2, 0, 0, 0, 0, 0, 0);
	//                                      U3 L2

	for (uint32_t k = 0; k < 2; ++k) {
		memset((void *)&m256_shuffle_uv16to8[k], 0x80, sizeof(__m256i));// mask for <_mm_shuffle_epi8>
	}

	// Repack 16-bit chroma(UV) samples from multiple xmm regs into 1 reg
	m256_shuffle_uv16to8[0].m256i_u8[0    ] = 2;    // U0
	m256_shuffle_uv16to8[0].m256i_u8[0 + 1] = 3;
	m256_shuffle_uv16to8[0].m256i_u8[0 + 2] = 2 + 8;// U1
	m256_shuffle_uv16to8[0].m256i_u8[0 + 3] = 3 + 8;
	m256_shuffle_uv16to8[0].m256i_u8[0 + 4] = 4;    // V0
	m256_shuffle_uv16to8[0].m256i_u8[0 + 5] = 5;
	m256_shuffle_uv16to8[0].m256i_u8[0 + 6] = 4 + 8;// V1
	m256_shuffle_uv16to8[0].m256i_u8[0 + 7] = 5 + 8;

	m256_shuffle_uv16to8[0].m256i_u8[16   ] = 2;    // U0
	m256_shuffle_uv16to8[0].m256i_u8[16+ 1] = 3;
	m256_shuffle_uv16to8[0].m256i_u8[16+ 2] = 2 + 8;// U1
	m256_shuffle_uv16to8[0].m256i_u8[16+ 3] = 3 + 8;
	m256_shuffle_uv16to8[0].m256i_u8[16+ 4] = 4;    // V0
	m256_shuffle_uv16to8[0].m256i_u8[16+ 5] = 5;
	m256_shuffle_uv16to8[0].m256i_u8[16+ 6] = 4 + 8;// V1
	m256_shuffle_uv16to8[0].m256i_u8[16+ 7] = 5 + 8;

	m256_shuffle_uv16to8[1].m256i_u8[8 + 0] = 2;    // U0
	m256_shuffle_uv16to8[1].m256i_u8[ 8+ 1] = 3;
	m256_shuffle_uv16to8[1].m256i_u8[ 8+ 2] = 2 + 8;// U1
	m256_shuffle_uv16to8[1].m256i_u8[ 8+ 3] = 3 + 8;
	m256_shuffle_uv16to8[1].m256i_u8[ 8+ 4] = 4;    // V0
	m256_shuffle_uv16to8[1].m256i_u8[ 8+ 5] = 5;
	m256_shuffle_uv16to8[1].m256i_u8[ 8+ 6] = 4 + 8;// V1
	m256_shuffle_uv16to8[1].m256i_u8[ 8+ 7] = 5 + 8;

	m256_shuffle_uv16to8[1].m256i_u8[24   ] = 2;    // U0
	m256_shuffle_uv16to8[1].m256i_u8[24+ 1] = 3;
	m256_shuffle_uv16to8[1].m256i_u8[24+ 2] = 2 + 8;// U1
	m256_shuffle_uv16to8[1].m256i_u8[24+ 3] = 3 + 8;
	m256_shuffle_uv16to8[1].m256i_u8[24+ 4] = 4;    // V0
	m256_shuffle_uv16to8[1].m256i_u8[24+ 5] = 5;
	m256_shuffle_uv16to8[1].m256i_u8[24+ 6] = 4 + 8;// V1
	m256_shuffle_uv16to8[1].m256i_u8[24+ 7] = 5 + 8;
	/*
	// Repack 16-bit chroma(UV) samples from multiple xmm regs into 1 reg

		// for k=0:    move V1, V0, U1, U0 into words 3,2,1,0 (respectively)
		// for k=1:    move V1, V0, U1, U0 into words 7,6,5,4 (respectively)

		memset((void *)&m256_shuffle_uv16to8[k], 0x80, sizeof(__m256i));// mask for <_mm_shuffle_epi8>

		// move source word#1  "U0"  to destination word#[0]
		// move source word#2  "V0"  to destination word#[2]
		// move source word#5  "U1"  to destination word#[1]
		// move source word#6  "V1"  to destination word#[3]
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 0] = 2;    // U0
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 1] = 3;
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 2] = 2 + 8;// U1
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 3] = 3 + 8;
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 4] = 4;    // V0
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 5] = 5;
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 6] = 4 + 8;// V1
		m256_shuffle_uv16to8[k].m256i_u8[(k << 3) + 7] = 5 + 8;
	} // for k
	*/
	////////////////////
	//
	// UYVY & YUYV - 16bpp packed YUV 4:2:2 (avx2)
	//

	//  byte# ->
	// 0  1  2  3  4  5  6  7 : 8 
	// U0    V0    U1    V1   : U2         source (UYVY 16bpp packed-pixels)
	//    Y0    Y1    Y2    Y3:    Y4
	//
	//    U0    V0    U1    V1:    U2      source (YUYV 16bpp packed-pixels)
	// Y0    Y1    Y2    Y3   : Y4

	// byte# ->
	// 0  1  2  3  4  5  6  7 : 8  9 10 11 12 13 14 15:
	//
	// U0 V0 U1 V1 U2 V2 U3 V3:U4 V4 U5 V5 U6 V6 U7 V7:  ....  destination (NV12 chroma-plane)
	//  #0  :  #1 :  #2 :  #3 : #4  : #5  : #6  :  #7 : #8  : #9  : #10 : #11 : #12 : #13 : #14 : #15 :
	// .....:.....:.....:.....:.....:.....:.....:.....:.....:.....:.....:.....:.....:.....:.....:.....:

	// The masks are applied per source pixel 'bundle'
	// a bundle of pixels is 16 horizontal pixels {x .. x+15}
	memset((void *)&m256_shuffle_uyvy422_y , 0x80, sizeof(m256_shuffle_uyvy422_y));
	memset((void *)&m256_shuffle_uyvy422_uv, 0x80, sizeof(m256_shuffle_uyvy422_uv));
	memset((void *)&m256_shuffle_yuyv422_y , 0x80, sizeof(m256_shuffle_yuyv422_y));
	memset((void *)&m256_shuffle_yuyv422_uv, 0x80, sizeof(m256_shuffle_yuyv422_uv));

	for (uint32_t i = 0; i < 8; ++i) {
		// Source bytes[x+0..x+15] are moved into dest bytes[0..7]
		m256_shuffle_uyvy422_y[0].m256i_u8[i] = (i << 1) + 1;
		m256_shuffle_yuyv422_y[0].m256i_u8[i] = (i << 1);

		// We want to move source bytes[16..31] into dest bytes[8..15],
		//   but we have to settle for dest bytes 16..23
		//    (We'll run a shuffle_epi32 to fixup this problem)
		m256_shuffle_uyvy422_y[0].m256i_u8[i + 16] = (i << 1) + 1;
		m256_shuffle_yuyv422_y[0].m256i_u8[i + 16] = (i << 1);

		// We want to move source bytes[32..47] into dest bytes[16..23],
		//   but we have to settle for dest bytes 8..15
		//    (We'll run a shuffle_epi32 to fixup this problem)
		m256_shuffle_uyvy422_y[1].m256i_u8[i + 8] = (i << 1) + 1;
		m256_shuffle_yuyv422_y[1].m256i_u8[i + 8] = (i << 1);

		// Source bytes[x+48..x+63] are moved into dest bytes[24..31]
		m256_shuffle_uyvy422_y[1].m256i_u8[i + 24] = (i << 1) + 1;
		m256_shuffle_yuyv422_y[1].m256i_u8[i + 24] = (i << 1);
	} // for i

	for (uint32_t i = 0; i < 4; ++i) {
		// get 4 U/V-pixels from source  {x..x+3}
		m256_shuffle_uyvy422_uv[0].m256i_u8[(i << 1)    ] = (i << 2)    ;// U
		m256_shuffle_uyvy422_uv[0].m256i_u8[(i << 1) + 1] = (i << 2) + 2;// V

		m256_shuffle_yuyv422_uv[0].m256i_u8[(i << 1)    ] = (i << 2) + 1;// U
		m256_shuffle_yuyv422_uv[0].m256i_u8[(i << 1) + 1] = (i << 2) + 3;// V

		// get another 4 U/V-pixels from source {x+4..x+7}
		m256_shuffle_uyvy422_uv[0].m256i_u8[(i << 1) + 16] = (i << 2);// U
		m256_shuffle_uyvy422_uv[0].m256i_u8[(i << 1) + 17] = (i << 2) + 2;// V

		m256_shuffle_yuyv422_uv[0].m256i_u8[(i << 1) + 16] = (i << 2) + 1;// U
		m256_shuffle_yuyv422_uv[0].m256i_u8[(i << 1) + 17] = (i << 2) + 3;// V

		// 4 U/V-pixels from source  {x+8..x+11}
		m256_shuffle_uyvy422_uv[1].m256i_u8[(i << 1) + 8] = (i << 2)    ;// U
		m256_shuffle_uyvy422_uv[1].m256i_u8[(i << 1) + 9] = (i << 2) + 2;// V

		m256_shuffle_yuyv422_uv[1].m256i_u8[(i << 1) + 8] = (i << 2) + 1;// U
		m256_shuffle_yuyv422_uv[1].m256i_u8[(i << 1) + 9] = (i << 2) + 3;// V

		// get another 4 U/V-pixels from source {x+12..x+15}
		m256_shuffle_uyvy422_uv[1].m256i_u8[(i << 1) + 24] = (i << 2);// U
		m256_shuffle_uyvy422_uv[1].m256i_u8[(i << 1) + 25] = (i << 2) + 2;// V

		m256_shuffle_yuyv422_uv[1].m256i_u8[(i << 1) + 24] = (i << 2) + 1;// U
		m256_shuffle_yuyv422_uv[1].m256i_u8[(i << 1) + 25] = (i << 2) + 3;// V
	} // for i

	////////////////////
	//
	// YUV444 (avx2 integer mm256 register)
	//

	// The masks are applied per source pixel 'bundle'
	// a bundle of pixels is 16 horizontal pixels {x .. x+15}
	//
	// mask[0] applies to pixels {  x ..  x+7}
	// mask[1] applies to pixels { x+8 .. x+16}

	memset((void *)&m256_shuffle_yuv444_y, 0x80, sizeof(m256_shuffle_yuv444_y));
	memset((void *)&m256_shuffle_yuv444_u, 0x80, sizeof(m256_shuffle_yuv444_u));
	memset((void *)&m256_shuffle_yuv444_v, 0x80, sizeof(m256_shuffle_yuv444_v));

	for (uint32_t i = 0; i < 2; ++i) {
		// Each mask-register #i (for y,u,v channels) pulls in 8 pixels

		// v[0] = src[0], v[1] = src[4], v[2] = src[8], v[3] = src[12] ...
		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 0] = 0; // (byte#0) read from lower 128-bits
		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 1] = 4;
		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 2] = 8;
		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 3] = 12;

		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 0 + 16] = 0;  // (byte#16) read from upper 128-bits
		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 1 + 16] = 4;
		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 2 + 16] = 8;
		m256_shuffle_yuv444_v[i].m256i_u8[(i << 2) + 3 + 16] = 12;

		// u[0] = src[1], u[1] = src[5], u[2] = src[9], u[3] = src[13] ...
		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 0] = 1; // (byte#1) read from lower 128-bits
		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 1] = 5;
		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 2] = 9;
		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 3] = 13;

		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 0 + 16] = 1; // (byte#17) read from upper 128-bits
		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 1 + 16] = 5;
		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 2 + 16] = 9;
		m256_shuffle_yuv444_u[i].m256i_u8[(i << 2) + 3 + 16] = 13;

		// y[0] = src[2], y[1] = src[6], y[2] = src[10], y[3] = src[14] ...
		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 0] = 2; // (byte#2) read from lower 128-bits
		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 1] = 6;
		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 2] = 10;
		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 3] = 14;

		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 0 + 16] = 2; // (byte#18) read from upper 128-bits
		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 1 + 16] = 6;
		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 2 + 16] = 10;
		m256_shuffle_yuv444_y[i].m256i_u8[(i << 2) + 3 + 16] = 14;
	} // for i

	// Permute-control word for _mm256_shuffle_epi8
	
	//                                    (unused)    (from lower 128-bits)
	//                                   VVVVVVVVVV     V     V
	m256_permc_yuv444 = _mm256_set_epi32(6, 6, 6, 6, 5, 1, 4, 0);
	//                                               ^     ^
	//                                               |     |
	//                                        (from upper 128-bits)


}

CRepackyuv::~CRepackyuv()
{
}

void CRepackyuv::convert_YUV420toNV12(
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	unsigned char * const src_yuv[3],
	const uint32_t src_stride[3], // Y/U/V distance: #pixels from scanline(x) to scanline(x+1) (units of bytes)
	unsigned char dest_nv12_luma[],  // pointer to output Y-plane
	unsigned char dest_nv12_chroma[], // pointer to output chroma-plane (combined UV)
	const uint32_t dstStride      // distance: #pixels from scanline(x) to scanline(x+1) (units of bytes)
	//  (same value is used for both Y-plane and UV-plane)
	)
{
	bool is_xmm_aligned = true;   // are addresses 16-byte aligned?

	// Check address-alignment of source_yuv plane(s)
	for (unsigned i = 0; i < 3; ++i) {
		if (reinterpret_cast<uint64_t>(src_yuv[i]) & 0xF)
			is_xmm_aligned = false;
		else if (src_stride[i] & 0xF)
			is_xmm_aligned = false;
	}

	if (reinterpret_cast<uint64_t>(dest_nv12_luma) & 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_nv12_chroma) & 0xF)
		is_xmm_aligned = false;
	else if (dstStride & 0xF)
		is_xmm_aligned = false;

	// #scanlines must be even
	if ( height & 1 )
		is_xmm_aligned = false;
	else if ( width & 0xF )     // horizontal framesize must be multiple of 16
		is_xmm_aligned = false;

	bool is_avx256_aligned = is_xmm_aligned && m_cpu_has_avx2 && m_allow_avx2;
	if (is_avx256_aligned) {
		// Check address-alignment of source_yuv plane(s)
		for (unsigned i = 0; i < 3; ++i) {
			if (reinterpret_cast<uint64_t>(src_yuv[i]) & 0x1F)
				is_avx256_aligned = false;
			else if (src_stride[i] & 0x1F)
				is_avx256_aligned = false;
		}

		if (reinterpret_cast<uint64_t>(dest_nv12_luma)& 0x1F)
			is_avx256_aligned = false;
		else if (reinterpret_cast<uint64_t>(dest_nv12_chroma)& 0x1F)
			is_avx256_aligned = false;
		else if (dstStride & 0x1F)
			is_avx256_aligned = false;
		else if (width & 0x1F)     // horizontal framesize must be multiple of 32
			is_avx256_aligned = false;
	}

	if (is_avx256_aligned) {
		__m256i      *xmm_src_yuv[3];
		unsigned int xmm_src_stride[3];// stride in units of 256bits (i.e. 'stride==1' means 16 bytes)
		__m256i      *xmm_dst_luma;
		__m256i      *xmm_dst_chroma;
		unsigned int xmm_dst_stride;   // destination stride (units of 256 bits)

		for (unsigned i = 0; i < 3; ++i) {
			xmm_src_yuv[i] = reinterpret_cast< __m256i *>(src_yuv[i]);
			xmm_src_stride[i] = src_stride[i] >> 5;// convert byte-units into __m256i units
		}
		xmm_dst_luma = reinterpret_cast<__m256i *>(dest_nv12_luma);
		xmm_dst_chroma = reinterpret_cast<__m256i *>(dest_nv12_chroma);
		xmm_dst_stride = dstStride >> 5; // convert byte-units into __m256i units

		_convert_YUV420toNV12_avx2( // AVX2 version (requires 32-byte data-alignment)
			width,      // X-dimension (#pixels)
			height,     // Y-dimension (#pixels)
			xmm_src_yuv,// pointer to source planes: Y, U, V
			xmm_src_stride, // Y/U/V distance: #pixels from scanline(x) to scanline(x+1)
			xmm_dst_luma, xmm_dst_chroma, // pointer to output Y-plane, UV-plane
			xmm_dst_stride  // distance: #pixels from scanline(x) to scanline(x+1)
			);
	}
	else if (is_xmm_aligned) {
		__m128i      *xmm_src_yuv[3];
		unsigned int xmm_src_stride[3];// stride in units of 128bits (i.e. 'stride==1' means 16 bytes)
		__m128i      *xmm_dst_luma;
		__m128i      *xmm_dst_chroma;
		unsigned int xmm_dst_stride;   // destination stride (units of 128 bits)

		for (unsigned i = 0; i < 3; ++i) {
			xmm_src_yuv[i] = reinterpret_cast< __m128i *>(src_yuv[i]);
			xmm_src_stride[i] = src_stride[i] >> 4;// convert byte-units into __m128i units
		}
		xmm_dst_luma = reinterpret_cast<__m128i *>(dest_nv12_luma);
		xmm_dst_chroma = reinterpret_cast<__m128i *>(dest_nv12_chroma);
		xmm_dst_stride = dstStride >> 4; // convert byte-units into __m128i units

		_convert_YUV420toNV12_sse2( // SSE2 version (requires 16-byte data-alignment)
			width,      // X-dimension (#pixels)
			height,     // Y-dimension (#pixels)
			xmm_src_yuv,// pointer to source planes: Y, U, V
			xmm_src_stride, // Y/U/V distance: #pixels from scanline(x) to scanline(x+1)
			xmm_dst_luma, xmm_dst_chroma, // pointer to output Y-plane, UV-plane
			xmm_dst_stride  // distance: #pixels from scanline(x) to scanline(x+1)
		);
	}
	else {
		_convert_YUV420toNV12(
			width,      // X-dimension (#pixels)
			height,     // Y-dimension (#pixels)
			src_yuv,
			src_stride, // Y/U/V distance: #pixels from scanline(x) to scanline(x+1)
			dest_nv12_luma,  // pointer to output Y-plane
			reinterpret_cast<uint16_t*>(dest_nv12_chroma), // output chroma-plane (combined UV)
			dstStride   // distance: #pixels from scanline(x) to scanline(x+1)
		);
	}
}

void CRepackyuv::_convert_YUV420toNV12_avx2( // convert planar(YV12) into planar(NV12)
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const __m256i * const src_yuv[3],
	const uint32_t src_stride[3], // stride for src_yuv[3] (in units of __m256i, 32-bytes)
	__m256i dest_nv12_luma[], __m256i dest_nv12_chroma[],
	const uint32_t dstStride // stride (in units of __m256i, 32-bytes)
	)
{
	const uint32_t half_height = (height + 1) >> 1;  // round_up( height / 2 )
	const uint32_t width_div_32 = (width + 31) >> 5; // round_up( width / 32 )

	__m256i src[2]; // temp-vars to hold permuted source-pixels
	__m256i chroma_pixels1, chroma_pixels0;  // 16 horizontal chroma (Cb+Cr) pixels:{ x+0 .. x+15 }
	//
	// This function converts a source-image that is formatted as YUV4:2:0 (3-plane),
	// into an output-image that is formatted as NV12 (2-plane).

	// Loop optimization:
	// ------------------
	// each scanline is copied from left -> right.  Each AVX2 instruction copies
	// 32-pixels, which means we may 'overshoot' the right-edge of the picture
	// boundary.  This is ok, since we assume the source & dest framebuffers
	// are allocated with sufficiently large stride to tolerate our overshooting.
	uint32_t ptr_src0_x, ptr_src1_x, ptr_src2_x;
	uint32_t ptr_dst_y;

	ptr_dst_y = 0;
	ptr_src0_x = 0;

	// copy the luma portion of the framebuffer
	for (unsigned y = 0; y < height; y++) {
		for (unsigned x = 0, ptr_dst_y_plus_x = ptr_dst_y, ptr_src0_x_plus_x = ptr_src0_x; x < width_div_32; ++x) {
			// copy 32 horizontal luma pixels (256-bits) per inner-loop pass
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
		for (unsigned x = 0, ptr_dst_y_plus_x = ptr_dst_y, ptr_src1_x_plus_xdiv2 = ptr_src1_x, ptr_src2_x_plus_xdiv2 = ptr_src2_x; x < width_div_32; x += 2)
		{
			// copy 32 horizontal chroma pixels per inner-loop pass
			// each chroma-pixel is 16-bits {Cb,Cr}
			// (Cb,Cr) pixel x.. x+7

			// Preprocess the src-pixels by swapping bits[191:128] <-> [127:64]
			//   (This cancels out the integrated permute in the AVX2 versions
			//    of unpacklo_epi8/unpackhi_epi8)
			src[0] = _mm256_permute4x64_epi64(src_yuv[1][ptr_src1_x_plus_xdiv2], _MM_SHUFFLE(3, 1, 2, 0));
			src[1] = _mm256_permute4x64_epi64(src_yuv[2][ptr_src2_x_plus_xdiv2], _MM_SHUFFLE(3, 1, 2, 0));

			// Generate 8 output pixels [n..n+7]
			// Read the lower 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			chroma_pixels0 = _mm256_unpacklo_epi8(src[0], src[1]);// even
			//			dest_nv12_chroma[ptr_dst_y + x] = chroma_pixels;

			//        H2  L0       H3  L1
			//  SRC 76543210 SRC FEDCBA98
			//      H2  L0       H3  L1
			//
			//  DST D5C49180  // punpacklow
			//  DST F7E6B3A2  // punpackhigh

			dest_nv12_chroma[ptr_dst_y_plus_x] = chroma_pixels0;
			++ptr_dst_y_plus_x;

			// Generate 8 more output pixels [n+8..n+15]
			// Read the upper 8-bytes of U/V plane {src_yuv[1], src_yuv[2]}
			//++ptr_dst_y_plus_x;
			//			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][src_stride[1]*y + x_div2], src_yuv[2][src_stride[2]*y + x_div2] );// odd
			//			dest_nv12_chroma[(y*dstStride) + x] = chroma_pixels1;
			//			chroma_pixels1 = _mm_unpackhi_epi8( src_yuv[1][ptr_src1_x + x_div2], src_yuv[2][ptr_src2_x + x_div2] );// odd
			chroma_pixels1 = _mm256_unpackhi_epi8(src[0], src[1]);// odd
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
	const uint32_t width, // : must be multiple of 16
	const uint32_t height, //: must be multiple of 2
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
	const uint32_t width,  // must be multiple of 2
	const uint32_t height, // must be multiple of 2
	const unsigned char * const src_yuv[3], const uint32_t src_stride[3],
	unsigned char dest_nv12_luma[], uint16_t dest_nv12_chroma[],
	const uint32_t dstStride // stride in #bytes
	)
{
	const uint32_t half_height = (height + 1) >> 1;  // round_up( height / 2)
	const uint32_t half_width = (width + 1) >> 1;  // round up( width / 2)
	const uint32_t dstStride_div_2 = (dstStride + 1) >> 1;// round_up( dstStride / 2)
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

void CRepackyuv::convert_YUV444toY444( // convert packed-pixel(Y444) into planar(4:4:4)
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1) [units of bytes]
	const uint8_t  src_444[],  // pointer to input (YUV444 packed) surface
	const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1) [units of bytes]
	unsigned char  dest_y[],   // pointer to output Y-plane
	unsigned char  dest_u[],   // pointer to output U-plane
	unsigned char  dest_v[]    // pointer to output V-plane
	)
{
	bool is_xmm_aligned = m_cpu_has_ssse3;// are addresses 16-byte aligned?

	// Check address-alignment of source_yuv plane(s)
	if (reinterpret_cast<uint64_t>(src_444) & 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_y)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_u)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_v)& 0xF)
		is_xmm_aligned = false;

	if (src_stride & 0xF)
		is_xmm_aligned = false;
	else if (dst_stride & 0xF)
		is_xmm_aligned = false;

	if (width & 0xF)     // horizontal framesize must be multiple of 16
		is_xmm_aligned = false;

	// check for AVX2 alignment
	bool is_avx2_aligned = is_xmm_aligned && m_cpu_has_avx2 && m_allow_avx2;
	if (is_avx2_aligned) {
		if (reinterpret_cast<uint64_t>(src_444)& 0x1F)
			is_avx2_aligned = false;
		else if (reinterpret_cast<uint64_t>(dest_y)& 0x1F)
			is_avx2_aligned = false;
		else if (reinterpret_cast<uint64_t>(dest_u)& 0x1F)
			is_avx2_aligned = false;
		else if (reinterpret_cast<uint64_t>(dest_v)& 0x1F)
			is_avx2_aligned = false;

		if (src_stride & 0x1F)
			is_avx2_aligned = false;
	}

	if (is_avx2_aligned) {
		_convert_YUV444toY444_avx2(
			width, height,
			src_stride >> 5, // src stride (units of _m256)
			reinterpret_cast<__m256i const *>(src_444),// pointer to input (YUV444 packed) surface
			dst_stride >> 4, // dest stride (units of _m128)
			reinterpret_cast<__m128i *>(dest_y),    // output Y
			reinterpret_cast<__m128i *>(dest_u),  // output U
			reinterpret_cast<__m128i *>(dest_v)
		);
	}
	else if (is_xmm_aligned) {
		_convert_YUV444toY444_ssse3(
			width, height,
			src_stride >> 4, // src stride (units of _m128)
			reinterpret_cast<__m128i const *>(src_444),// pointer to input (YUV444 packed) surface
			dst_stride >> 4, // dest stride (units of _m128)
			reinterpret_cast<__m128i *>(dest_y),    // output Y
			reinterpret_cast<__m128i *>(dest_u),  // output U
			reinterpret_cast<__m128i *>(dest_v)
		);
	}
	else {
		_convert_YUV444toY444(
			width,      // X-dimension (#pixels)
			height,     // Y-dimension (#pixels)
			src_stride >> 2,// distance: #pixels from scanline(x) to scanline(x+1) (units of uint32_t)
			reinterpret_cast<uint32_t const *>(src_444),// pointer to input (YUV444 packed) surface
			dst_stride, // distance: #pixels from scanline(x) to scanline(x+1)
			dest_y,     // pointer to output Y-plane
			dest_u,     // pointer to output U-plane
			dest_v      // pointer to output V-plane
		);
	}
}

void CRepackyuv::_convert_YUV444toY444( // convert packed-pixel(Y444) into planar(4:4:4)
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1) [units of uint32_t]
	const uint32_t src_444[],  // pointer to input (YUV444 packed) surface
	const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1) [units of uint8_t]
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
	}
}

void CRepackyuv::_convert_YUV444toY444_ssse3( // convert packed-pixel(Y444) into planar(4:4:4)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
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
	uint32_t     dst_offset = dst_stride*(height - 1);

	for (uint32_t y = 0; y < height; ++y)
	{
		src_ptr = src_444 + y * src_stride;
		dst_offset = dst_stride * (height - 1 - y);
		//      for ( uint32_t x = 0, yout=height-1; x < width_div_4; x += 4, --yout)
		for (uint32_t x = 0; x < width_div_4; x += 4, ++dst_offset)
		{
			// process 16 pixels per loop-iteration: (x .. x+15) 
			//in_444 = src_444[src_stride*y + x];
			in_444 = src_ptr[0];// pixels [x .. x+3]
			out_y = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_y[0]);
			out_u = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_u[0]);
			out_v = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_v[0]);

			//			in_444 = src_444[src_stride*y + x + 1];
			in_444 = src_ptr[1];// pixels [x+4 .. x+7]
			temp_y = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_y[1]);
			temp_u = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_u[1]);
			temp_v = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_v[1]);
			out_y = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

			//			in_444 = src_444[src_stride*y + x + 2];
			in_444 = src_ptr[2];// pixels [x+8 .. x+11]
			temp_y = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_y[2]);
			temp_u = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_u[2]);
			temp_v = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_v[2]);
			out_y = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

			//			in_444 = src_444[src_stride*y + x + 3];
			in_444 = src_ptr[3];// pixels [x+12 .. x+15]
			temp_y = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_y[3]);
			temp_u = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_u[3]);
			temp_v = _mm_shuffle_epi8(in_444, m128_shuffle_yuv444_v[3]);
			out_y = _mm_or_si128(out_y, temp_y);  // out_y = out_y | temp_y;
			out_u = _mm_or_si128(out_u, temp_u);  // out_u = out_u | temp_u;
			out_v = _mm_or_si128(out_v, temp_v);  // out_v = out_v | temp_v;

			//dest_y[ dst_stride*yout + (x>>2)] = out_y;
			dest_y[dst_offset] = out_y;
			dest_u[dst_offset] = out_u;
			dest_v[dst_offset] = out_v;
			src_ptr += 4;
		} // for x
		/*
		// Move src_ptr down by 1 scanline 
		src_ptr += adj_src_stride;

		// Move dst_offset up by 1 scanline 
		dst_offset -= adj_dst_stride;
		*/
	} // for y
}
void CRepackyuv::_convert_YUV444toY444_avx2( // convert packed-pixel(Y444) into planar(4:4:4)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
	const __m256i src_444[],  // pointer to input (YUV444 packed) surface
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	__m128i dest_y[],   // pointer to output Y-plane
	__m128i dest_u[],   // pointer to output U-plane
	__m128i dest_v[]    // pointer to output V-plane
	)
{
	__m256i in_444, temp_y, temp_u, temp_v, out_y, out_u, out_v;

	const uint32_t width_div_8 = (width + 7) >> 3;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m256i * src_ptr = src_444;
	uint32_t     dst_offset = dst_stride*(height - 1);
	
	for (uint32_t y = 0; y < height; ++y)
	{
		src_ptr = src_444 + y * src_stride;
		dst_offset = dst_stride * (height - 1 - y);
		//      for ( uint32_t x = 0, yout=height-1; x < width_div_4; x += 4, --yout)
		for (uint32_t x = 0; x < width_div_8; x += 2, ++dst_offset)
		{
			// On input-side: process 16 pixels per loop-iteration: (x .. x+15) 
			// On output-side: 16-pixels per out_y/out_u/out_v
			//                 (only the lower 128-bits of mm256 register are filled)

			//in_444 = src_444[src_stride*y + x];
			in_444 = src_ptr[0];// pixels [x .. x+7]
			out_y = _mm256_shuffle_epi8(in_444, m256_shuffle_yuv444_y[0]);
			out_u = _mm256_shuffle_epi8(in_444, m256_shuffle_yuv444_u[0]);
			out_v = _mm256_shuffle_epi8(in_444, m256_shuffle_yuv444_v[0]);

			//			in_444 = src_444[src_stride*y + x + 1];
			in_444 = src_ptr[1];// pixels [x+8 .. x+15]
			temp_y = _mm256_shuffle_epi8(in_444, m256_shuffle_yuv444_y[1]);
			temp_u = _mm256_shuffle_epi8(in_444, m256_shuffle_yuv444_u[1]);
			temp_v = _mm256_shuffle_epi8(in_444, m256_shuffle_yuv444_v[1]);

			temp_y = _mm256_or_si256(out_y, temp_y);  // out_y = out_y | temp_y;
			temp_u = _mm256_or_si256(out_u, temp_u);  // out_u = out_u | temp_u;
			temp_v = _mm256_or_si256(out_v, temp_v);  // out_v = out_v | temp_v;
			out_y = _mm256_permutevar8x32_epi32(temp_y, m256_permc_yuv444);// fixup the previous shuffle(s)
			out_u = _mm256_permutevar8x32_epi32(temp_u, m256_permc_yuv444);
			out_v = _mm256_permutevar8x32_epi32(temp_v, m256_permc_yuv444);

			//dest_y[ dst_stride*yout + (x>>2)] = out_y;
			dest_y[dst_offset] = _mm256_extractf128_si256(out_y, 0);// write lower 128-bits
			dest_u[dst_offset] = _mm256_extractf128_si256(out_u, 0);
			dest_v[dst_offset] = _mm256_extractf128_si256(out_v, 0);
			src_ptr += 2;
		} // for x
		/*
		// Move src_ptr down by 1 scanline
		src_ptr += adj_src_stride;

		// Move dst_offset up by 1 scanline
		dst_offset -= adj_dst_stride;
		*/
	} // for y
}

void CRepackyuv::convert_YUV422toNV12(  // convert packed-pixel(Y422) into 2-plane(NV12)
	const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance: #pixels from scanline(x) to scanline(x+1) [uints of uint8_t]
	const uint8_t  src_422[],  // pointer to input (YUV422 packed) surface [2 pixels per 32bits]
	const uint32_t dst_stride, // distance: #pixels from scanline(x) to scanline(x+1)
	unsigned char  dest_y[],   // pointer to output Y-plane
	unsigned char  dest_uv[]   // pointer to output UV-plane
	)
{
	bool is_xmm_aligned = m_cpu_has_ssse3;// are addresses 16-byte aligned?

	// Check address-alignment of source_yuv plane(s)
	if (reinterpret_cast<uint64_t>(src_422)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_y)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_uv)& 0xF)
		is_xmm_aligned = false;

	if (src_stride & 0xF)
		is_xmm_aligned = false;
	else if (dst_stride & 0xF)
		is_xmm_aligned = false;

	if ( height & 0x1 )  // must have an even# scanlines
		is_xmm_aligned = false;
	else if (width & 0xF)     // horizontal framesize must be multiple of 16
		is_xmm_aligned = false;

	bool is_avx256_aligned = is_xmm_aligned && m_cpu_has_avx2 && m_allow_avx2;
	if (is_avx256_aligned) {
		if (width & 0x1F)     // horizontal framesize must be multiple of 32
			is_avx256_aligned = false;

		if (reinterpret_cast<uint64_t>(src_422)& 0x1F)
			is_avx256_aligned = false;
		else if (reinterpret_cast<uint64_t>(dest_y)& 0x1F)
			is_avx256_aligned = false;
		else if (reinterpret_cast<uint64_t>(dest_uv)& 0x1F)
			is_avx256_aligned = false;

		if (src_stride & 0x1F)
			is_avx256_aligned = false;
		else if (dst_stride & 0x1F)
			is_avx256_aligned = false;
	}

	if (is_avx256_aligned)
		_convert_YUV422toNV12_avx2(
			mode_uyvy, // chroma-order: true=UYVY, false=YUYV
			width, height,
			src_stride >> 5,
			reinterpret_cast<__m256i const *>(src_422),
			dst_stride >> 5,
			reinterpret_cast<__m256i *>(dest_y),  // output Y
			reinterpret_cast<__m256i *>(dest_uv)  // output UV
		);
	else if ( is_xmm_aligned )
		_convert_YUV422toNV12_ssse3(
			mode_uyvy, // chroma-order: true=UYVY, false=YUYV
			width, height,
			src_stride >> 4,
			reinterpret_cast<__m128i const *>(src_422),
			dst_stride >> 4, 
			reinterpret_cast<__m128i *>(dest_y),  // output Y
			reinterpret_cast<__m128i *>(dest_uv)  // output UV
		);
	else
		_convert_YUV422toNV12(  // non-SSE version (slow)
			mode_uyvy, // chroma-order: true=UYVY, false=YUYV
			width, height,
			src_stride >> 1,
			reinterpret_cast<uint32_t const *>(src_422),
			dst_stride,
			dest_y,    // output Y
			dest_uv  // output UV
		);
}

void CRepackyuv::_convert_YUV422toNV12( // convert packed-pixel(Y422) into 2-plane(NV12)
	const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 2
	const uint32_t height,     // Y-dimension (#pixels): must be multiple of 2
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
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels): must be multiple of 2
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	const __m128i src_422[],  // pointer to input (YUV422 packed) surface
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	__m128i dest_y[],   // pointer to output Y-plane
	__m128i dest_uv[]  // pointer to output U-plane
	)
{
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
	const uint32_t src_ptr_y_inc2 = (src_stride << 1) - width_div_8;
	const uint32_t dst_ptr_y_inc2 = (dst_stride << 1) - (width_div_8 >> 1);
	const uint32_t dst_ptr_uv_inc = dst_stride - (width_div_8 >> 1);

	// setup the pointers to 
	src_ptr_y   = src_422;
	src_ptr_yp1 = src_422 + src_stride;

	dst_ptr_y   = dest_y;
	dst_ptr_yp1 = dest_y + dst_stride;
	dst_ptr_uv  = dest_uv;

	// Choose the src_422's color-format: UYVY or YUYV (this affects byte-ordering)
	const __m128i * const mask_y  = mode_uyvy ? m128_shuffle_uyvy422_y : m128_shuffle_yuyv422_y;
	const __m128i * const mask_uv = mode_uyvy ? m128_shuffle_uyvy422_uv : m128_shuffle_yuyv422_uv;

	for (uint32_t y = 0; y < height; y += 2) {
		src_ptr_y   = src_422 + y*src_stride;// set to start of scanline #y
		src_ptr_yp1 = src_ptr_y + src_stride;// set to start of scanline #(y+1)

		dst_ptr_y   = dest_y + y*dst_stride; // set to start of scanline #y
		dst_ptr_yp1 = dst_ptr_y + dst_stride; // set to start of scanline #(y+1)

		dst_ptr_uv  = dest_uv + y * (dst_stride >> 1);

		for (uint32_t x = 0; x < width_div_8; x += 2) {
			// Each x-iteration, process two bundles of 16 pixels (32 pixels total):
			//    @ [scanline y  ]:  pixels {x .. x+15}
			//    @ [scanline y+1]:  pixels {x .. x+15}

			// scanline y: read src-pixels
			//src[0] = src_422[ src_stride*y + x    ];// scanline y, [pixel x ... x+7]
			//src[1] = src_422[ src_stride*y + x + 1];// scanline y, [pixel x+8 ... x+15]
			src[0] = *(src_ptr_y);
			src[1] = *(src_ptr_y + 1);
			src_ptr_y += 2; // advance +16 horizontal YUV422 pixels (32 bytes total)

			// scanline y: Copy the luma(Y) component to output
			tmp_y = _mm_shuffle_epi8(src[0], mask_y[0]);
			out_y = _mm_shuffle_epi8(src[1], mask_y[1]);

			out_y = _mm_or_si128(out_y, tmp_y);  // out_y = out_y | temp_y;
			//dest_y[dst_stride*y + (x>>1)] = out_y;
			(*dst_ptr_y) = out_y;
			++dst_ptr_y;// (destination Y-plane): advance +16 horizontal luma-samples (16 bytes)

			// scanline y: Copy the chroma(UV) components to temp
			uv[0] = _mm_shuffle_epi8(src[0], mask_uv[0]);
			uv[1] = _mm_shuffle_epi8(src[1], mask_uv[1]);
			tmp_uv = _mm_or_si128(uv[0], uv[1]); // tmp = uv[0] | uv[1]

			// scanline y+1: read src-pixels
			//src1[0] = src_422[ src_stride*(y+1) + x    ];// scanline y+1, [pixel x ... x+7]
			//src1[1] = src_422[ src_stride*(y+1) + x + 1];// scanline y+1, [pixel x+8 ... x+15]
			src1[0] = *(src_ptr_yp1);
			src1[1] = *(src_ptr_yp1 + 1);
			src_ptr_yp1 += 2; // advance +16 horizontal YUV422 pixels (32 bytes total)

			tmp_y = _mm_shuffle_epi8(src1[0], mask_y[0]);
			out_y = _mm_shuffle_epi8(src1[1], mask_y[1]);

			// scanline y+1: Copy the chroma(Y) components to temp
			out_y = _mm_or_si128(out_y, tmp_y);  // out_y = out_y | temp_y;
			//dest_y[dst_stride*(y+1) + (x>>1)] = out_y;
			(*dst_ptr_yp1) = out_y;
			++dst_ptr_yp1;// (destination Y-plane): advance +16 horizontal luma-samples (16 bytes)

			// scanline y+1: Copy the chroma(UV) components to temp
			uv[0] = _mm_shuffle_epi8(src1[0], mask_uv[0]);
			uv[1] = _mm_shuffle_epi8(src1[1], mask_uv[1]);
			tmp_uv_yp1 = _mm_or_si128(uv[0], uv[1]); // tmp = uv[0] | uv[1]

			// combine the UV samples from scanlines (y) and (y+1) --
			//    average them together.
			out_uv = _mm_avg_epu8(tmp_uv, tmp_uv_yp1);

			//dest_uv[dst_stride*(y>>1) + (x>>1)] = out_uv;
			(*dst_ptr_uv) = out_uv;
			++dst_ptr_uv;// (destination UV-plane): advance +8 UV-samples (16 bytes)
		} // for x
		/*
		src_ptr_y += src_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)
		src_ptr_yp1 += src_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)

		dst_ptr_y += dst_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)
		dst_ptr_yp1 += dst_ptr_y_inc2;// advance +2 scanlines (2 row of Y-samples)

		dst_ptr_uv += dst_ptr_uv_inc;// advance +2 scanlines (1 row of UV-samples)
		*/
	} // for y
}

void CRepackyuv::_convert_YUV422toNV12_avx2( // convert packed-pixel(Y422) into 2-plane(NV12)
	const bool     mode_uyvy,  // chroma-order: true=UYVY, false=YUYV
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 32
	const uint32_t height,     // Y-dimension (#pixels): must be even#
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
	const __m256i  src_422[],  // pointer to input (YUV422 packed) surface
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
	__m256i dest_y[],   // pointer to output Y-plane
	__m256i dest_uv[]  // pointer to output U-plane
	)
{
	// temp vars
	__m256i src[2];   // scanline #y
	__m256i src1[2];  // scanline #y+1
	__m256i uv[2];
	__m256i tmp_y;
	__m256i out_y, out_uv, tmp_uv_yp1, tmp_uv;

	// input format: UYVY
	//  Byte# -->
	//     0     1   2    3    4    5    6    7
	//    ____ ____ ____ ____ ____ ____ ____ ____ ____ ____
	//   | U01| Y0 | V01| Y1 | U23| Y2 | V23| Y3 | U45| Y4 | ...
	//   :                   :                   :
	//   |<----- 32 bits --->| <--- 32 bits ---->|
	//
	//
	const uint32_t width_div_32 = width >> 5;

	// data-pointers
	const __m256i *src_ptr_y;  // source-pointer to source-pixel {x,y}
	const __m256i *src_ptr_yp1;// source-pointer to source-pixel {x,y+1} (next scanline)
	__m256i *dst_ptr_y;  // pointer to destination (Y)luma-plane: pixel{x,y}
	__m256i *dst_ptr_yp1;// pointer to destination (Y)luma-plane: pixel{x,y+1} (next scanline)
	__m256i *dst_ptr_uv; // pointer to destination (UV)chroma-plane: 
	//    corresponding to the 4 luma-pixels {x..x+1,y..y+1}

	// setup the pointers to 
	src_ptr_y = src_422;
	src_ptr_yp1 = src_422 + src_stride;

	dst_ptr_y = dest_y;
	dst_ptr_yp1 = dest_y + dst_stride;
	dst_ptr_uv = dest_uv;

	// Choose the src_422's color-format: UYVY or YUYV (this affects byte-ordering)
	const __m256i const * mask_y  = mode_uyvy ? m256_shuffle_uyvy422_y : m256_shuffle_yuyv422_y;
	const __m256i const * mask_uv = mode_uyvy ? m256_shuffle_uyvy422_uv : m256_shuffle_yuyv422_uv;

	for (uint32_t y = 0; y < height; y += 2) {
		src_ptr_y = src_422 + y*src_stride;// set to start of scanline #y
		src_ptr_yp1 = src_ptr_y + src_stride;// set to start of scanline #(y+1)

		dst_ptr_y = dest_y + y*dst_stride; // set to start of scanline #y
		dst_ptr_yp1 = dst_ptr_y + dst_stride; // set to start of scanline #(y+1)

		dst_ptr_uv = dest_uv + y * (dst_stride >> 1);

		for (uint32_t x = 0; x < width_div_32; ++x) {
			// Each x-iteration, process two bundles of 32 pixels (64 pixels total):
			//    @ [scanline y  ]:  pixels {x .. x+31}
			//    @ [scanline y+1]:  pixels {x .. x+31}

			// scanline y: read src-pixels
			//src[0] = src_422[ src_stride*y + x    ];// scanline y, [pixel x ... x+15]
			//src[1] = src_422[ src_stride*y + x + 1];// scanline y, [pixel x+16 ... x+31]
			src[0] = *(src_ptr_y);
			src[1] = *(src_ptr_y+1);
			src_ptr_y += 2; // advance +32 horizontal YUV422 pixels (64 bytes total)

			// scanline y: Copy the luma(Y) component to output
			tmp_y = _mm256_shuffle_epi8(src[0], mask_y[0]);// process pixels {x..x+15}
			out_y = _mm256_shuffle_epi8(src[1], mask_y[1]);// process pixels {x+16..x+31}
			tmp_y = _mm256_or_si256(tmp_y, out_y);// merge dest-pixels {0..15} and {16..31}

			// Swap two 64-bit chunks: bits[127:64] <-> bits[191:128]
			//   (Due to side-effect of AVX2 instruction _mm256_shuffle_epi8)
			out_y = _mm256_permute4x64_epi64(tmp_y, _MM_SHUFFLE(3, 1, 2, 0));

			//dest_y[dst_stride*y + (x>>1)] = out_y;
			(*dst_ptr_y) = out_y;
			++dst_ptr_y;// (destination Y-plane): advance +32 horizontal luma-samples (32 bytes)

			// scanline y: Copy the chroma(UV) components to temp
			uv[0] = _mm256_shuffle_epi8(src[0], mask_uv[0]);// process src-pixels {x..x+15}
			uv[1] = _mm256_shuffle_epi8(src[1], mask_uv[1]);// process src-pixels {x+16..x+31}
			tmp_uv = _mm256_or_si256(uv[0], uv[1]); // merge dest-pixels {0..15} and {16..31}

			// scanline y+1: read src-pixels
			//src1[0] = src_422[ src_stride*(y+1) + x    ];// scanline y+1, [pixel x ... x+15]
			//src1[1] = src_422[ src_stride*(y+1) + x + 1];// scanline y+1, [pixel x+16 ... x+31]
			src1[0] = *(src_ptr_yp1);
			src1[1] = *(src_ptr_yp1+1);
			src_ptr_yp1 += 2; // advance +32 horizontal YUV422 pixels (64 bytes total)

			tmp_y = _mm256_shuffle_epi8(src1[0], mask_y[0]);// process src-pixels {x..x+15}
			out_y = _mm256_shuffle_epi8(src1[1], mask_y[1]);// process src-pixels {x+16..x+31}
			tmp_y = _mm256_or_si256(tmp_y, out_y);// merge dest-pixels {0..15} and {16..31}

			out_y = _mm256_permute4x64_epi64(tmp_y, _MM_SHUFFLE(3, 1, 2, 0));

			// scanline y+1: Copy the chroma(Y) components to temp
			//dest_y[dst_stride*(y+1) + (x>>1)] = out_y;
			(*dst_ptr_yp1) = out_y;
			++dst_ptr_yp1;// (destination Y-plane): advance +32 horizontal luma-samples (32 bytes)

			// scanline y+1: Copy the chroma(UV) components to temp
			uv[0] = _mm256_shuffle_epi8(src1[0], mask_uv[0]);// process src-pixels {x..x+15}
			uv[1] = _mm256_shuffle_epi8(src1[1], mask_uv[1]);// process src-pixels {x+16..x+31}
			tmp_uv_yp1 = _mm256_or_si256(uv[0], uv[1]); // merge dest-pixels {0..15} and {16..31}

			// combine the UV samples from scanlines (y) and (y+1) --
			//    average them together.
			out_uv = _mm256_avg_epu8(tmp_uv, tmp_uv_yp1);

			//dest_uv[dst_stride*(y>>1) + (x>>1)] = out_uv;
			(*dst_ptr_uv) = _mm256_permute4x64_epi64(out_uv, _MM_SHUFFLE(3, 1, 2, 0));
			++dst_ptr_uv;// (destination UV-plane): advance +16 UV-samples (32 bytes)
		} // for x
	} // for y
}

void CRepackyuv::convert_RGBFtoY444( // convert packed(RGB f32) into packed(YUV 8bpp)
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
)
{
	bool is_xmm_aligned = m_cpu_has_ssse3;// are addresses 16-byte aligned?

	// Check address-alignment of source_yuv plane(s)
	if (reinterpret_cast<uint64_t>(src_rgb)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_y)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_u)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_v)& 0xF)
		is_xmm_aligned = false;

	if (src_stride & 0xF)
		is_xmm_aligned = false;
	else if (dst_stride & 0xF)
		is_xmm_aligned = false;

	if (width & 0xF)     // horizontal framesize must be multiple of 16
		is_xmm_aligned = false;

	bool is_avx256_aligned = is_xmm_aligned && // are addresses 32-byte aligned
		                     m_cpu_has_avx;

	if (is_avx256_aligned) {
		if (reinterpret_cast<uint64_t>(src_rgb)& 0x1F)
			is_avx256_aligned = false;

		if (src_stride & 0x1F)
			is_avx256_aligned = false;
	}

	if (is_avx256_aligned) {
		if ( m_cpu_has_avx2 && m_allow_avx2 )
			_convert_RGBFtoY444_avx2( // AVX version of converter
				use_bt709, // bt709?
				use_fullscale, // true=PC/full scale, false=video scale (0-235)
				width, height, src_stride >> 5, // src stride (units of _m256)
				reinterpret_cast<__m256 const *>(src_rgb),
				dst_stride >> 4,
				reinterpret_cast<__m128i *>(dest_y),  // output Y
				reinterpret_cast<__m128i *>(dest_u),  // output U
				reinterpret_cast<__m128i *>(dest_v)   // output V
			);
		else if ( m_allow_avx )
			_convert_RGBFtoY444_avx( // AVX version of converter
				use_bt709, // bt709?
				use_fullscale, // true=PC/full scale, false=video scale (0-235)
				width, height, src_stride >> 5, // src stride (units of _m256)
				reinterpret_cast<__m256 const *>(src_rgb),
				dst_stride >> 4,
				reinterpret_cast<__m128i *>(dest_y),  // output Y
				reinterpret_cast<__m128i *>(dest_u),  // output U
				reinterpret_cast<__m128i *>(dest_v)   // output V
			);
	} else { // if ( is_xmm_aligned ) {
		_convert_RGBFtoY444_ssse3( // SSSE3 version of converter
			use_bt709, // bt709?
			use_fullscale, // true=PC/full scale, false=video scale (0-235)
			width, height, src_stride >> 4, // src stride (units of _m128)
			reinterpret_cast<__m128 const *>(src_rgb),
			dst_stride >> 4,
			reinterpret_cast<__m128i *>(dest_y),  // output Y
			reinterpret_cast<__m128i *>(dest_u),  // output U
			reinterpret_cast<__m128i *>(dest_v)   // output V
		);
	}
}

void CRepackyuv::_convert_RGBFtoY444_ssse3( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select: false=bt601, true=bt709
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m128, 16-bytes)
	const __m128 src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	__m128i dest_y[],   // pointer to output Y-plane
	__m128i dest_u[],   // pointer to output U-plane
	__m128i dest_v[]    // pointer to output V-plane
	)
{
	const uint32_t width_div_4 = (width + 3) >> 2;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m128 zero = _mm_setzero_ps();
	const __m128 roffset = _mm_set_ps(0.5, 0.5, 0.5, 0.5);// FP32 rounding-offset: +0.5
	const __m128 cmatrix_y = get_rgb2yuv_coeff_matrix128(use_bt709, use_fullscale, SELECT_COLOR_Y);
	const __m128 cmatrix_u = get_rgb2yuv_coeff_matrix128(use_bt709, use_fullscale, SELECT_COLOR_U);
	const __m128 cmatrix_v = get_rgb2yuv_coeff_matrix128(use_bt709, use_fullscale, SELECT_COLOR_V);
	const __m128i coffset = use_fullscale ? m128_rgb32fyuv_offset0255 : m128_rgb32fyuv_offset16240;// color-offset (int16)

	__m128 temp_y, temp_u, temp_v, temp_yu, temp_yuv;
	__m128i mm32i[4];
	__m128i mm16i[2], pixels;

	__m128i temp_444[4], temp_yout, temp_uout, temp_vout, out_y, out_u, out_v;
	const __m128 *src_ptr = src_rgb;
	uint32_t dst_offset = (height - 1) * dst_stride;// invert the image vertically

	//const uint32_t dst_adjustment = dst_stride + width_div_16;
	//const uint32_t src_adjustment = src_stride - width;

	for (uint32_t y = 0; y < height; ++y) {
		src_ptr = &src_rgb[y * src_stride];
		dst_offset = (height - 1 - y) * dst_stride;

		for (uint32_t x = 0; x < width; x += 16, ++dst_offset) {
			// In each iteration, process 16 source YUV-pixels (in 4 groups of 4):
			//	Pixel# [x,y], [x+1,y], ... [x+15,y]
			//
			// This produces exactly 16 destination YUV-pixels

			for (uint32_t j = 0; j < 4; ++j) {
				for (uint32_t i = 0; i < 4; ++i, ++src_ptr) {
					// (1) process 2 RGBf32 pixels
					temp_y = _mm_mul_ps(src_ptr[0], cmatrix_y);
					temp_u = _mm_mul_ps(src_ptr[0], cmatrix_u);
					temp_v = _mm_mul_ps(src_ptr[0], cmatrix_v);

					// temp_y   A3 A2 A1 A0
					//          Y0 Y0 Y0 Y0

					// temp_u   B3 B2 B1 B0
					//          U0 U0 U0 U0

					// temp_v   C3 C2 C1 C0
					//          V0 V0 V0 V0

					temp_yu = _mm_hadd_ps(temp_y, temp_u);
					//  Y3 Y2 Y1 Y0 <HADD> U3 U2 U1 U0
					//  UUUUUUUUUUUUUUU YYYYYYYYYYYYYYY
					//  (U2+U3) (U0+U1) (Y2+Y3) (Y0+Y1)

					//   Float#3    #2    #1    #0
					//       B3_B2 B1_B0 A2_A3 A0_A1
					//       22222 22222 00000 00000
					//         U0   U0     Y0     Y0

					temp_v = _mm_hadd_ps(temp_v, zero);
					//   Float#3    #2    #1    #0
					//         0    0    A2_A3  A0_A1
					//                   0000   00000
					//                     V0      V0

					temp_yuv = _mm_hadd_ps(temp_yu, temp_v);
					// Float#3  2  1  0
					//       0  V0 U0 Y0

					//temp_yuv = _mm_round_ps(temp_yuv, _MM_FROUND_NINT);// SSE4 instruction
					temp_yuv = _mm_add_ps(temp_yuv, roffset); // add rounding-offset: +0.5
					mm32i[i] = _mm_cvttps_epi32(temp_yuv);// truncate fp32 -> int32_t
				} // for i

				mm16i[0] = _mm_packs_epi32(mm32i[0], mm32i[1]);// pack 32bit->16bit
				mm16i[1] = _mm_packs_epi32(mm32i[2], mm32i[3]);// pack 32bit->16bit

				// add offset:  Y -> +16,  U -> +128, V -> +128
				mm16i[0] = _mm_adds_epi16(mm16i[0], coffset);
				mm16i[1] = _mm_adds_epi16(mm16i[1], coffset);

				pixels = _mm_packus_epi16(mm16i[0], mm16i[1]);// pack 16bit->8bit

				// Byte #15 14 13 12 10 9 8     7  6  5  4  3  2  1  0
				//      0  V3 U3 Y3 0  V2 U2 Y2 0  V1 U1 Y1 0  V0 U0 Y0

				temp_444[j] = _mm_shuffle_epi8(pixels, m128_rgb32fyuv_reorder);
				// Byte #15 14 13 12 11 10  9  8    7  6  5  4  3  2  1  0
				//       0  Y3 U3 V3 0  Y2 U2 V2    0  Y1 U1 V1 0  Y0 U0 V0
			} // for j

			// now convert temp_444[0..3] (sixteen 32bpp packed-pixels)
			//   into planar YUV 4:4:4  (3 separate Y, U, V surfaces)

			//pixels = src_444[src_stride*y + x];
			pixels = temp_444[0];// extract pixels #[x, x+3]
			out_y = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[0]);
			out_u = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[0]);
			out_v = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[0]);

			//			in_444 = src_444[src_stride*y + x + 1];
			pixels = temp_444[1];// extract pixels #[x+4, x+7]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[1]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[1]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[1]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//			in_444 = src_444[src_stride*y + x + 2];
			pixels = temp_444[2];// extract pixels #[x+8, x+11]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[2]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[2]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[2]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//			in_444 = src_444[src_stride*y + x + 3];
			pixels = temp_444[3];// extract pixels #[x+12, x+15]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[3]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[3]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[3]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//dest_y[ dst_stride*yout + (x>>2)] = out_y;
			dest_y[dst_offset] = out_y;
			dest_u[dst_offset] = out_u;
			dest_v[dst_offset] = out_v;
		} // for x

		//src_ptr += src_adjustment;  // move to next scanline
		//dst_offset -= dst_adjustment;// move to next scanline (reverse direction)
	} // for y

	return;
}

void CRepackyuv::_convert_RGBFtoY444_avx( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select: false=bt601, true=bt709
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
	const __m256 src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	__m128i dest_y[],   // pointer to output Y-plane
	__m128i dest_u[],   // pointer to output U-plane
	__m128i dest_v[]    // pointer to output V-plane
)
{
	const uint32_t width_div_2 = (width + 1) >> 1;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m256 zero = _mm256_setzero_ps();
	const __m256 cmatrix_y = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_Y);
	const __m256 cmatrix_u = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_U);
	const __m256 cmatrix_v = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_V);

	const __m128i coffset = use_fullscale ? m128_rgb32fyuv_offset0255 : m128_rgb32fyuv_offset16240;
	const uint32_t dst_adjustment = dst_stride + width_div_16;
	const uint32_t src_adjustment = src_stride - width_div_2;

	__m256 temp_y, temp_u, temp_v, temp_yu, temp_yuv;
	__m256i temp_yuvi;
	__m128i mm32i[2];
	__m128i mm16i[2], pixels;

	__m128i temp_444[4], temp_yout, temp_uout, temp_vout, out_y, out_u, out_v;
	const __m256 *src_ptr;
	uint32_t dst_offset;


	for (uint32_t y = 0; y < height; ++y) {
		src_ptr = &src_rgb[y * src_stride];
		dst_offset = (height - y - 1) * dst_stride;// invert the image vertically

//		for (uint32_t x = 0; x < width; x += 4, ++src_ptr, ++dst_ptr) {
		for (uint32_t x = 0; x < width; x += 16, ++dst_offset) {
			// In each iteration, process 16 source YUV-pixels (in 4 groups of 4):
			//	Pixel# [x,y], [x+1,y], ... [x+15,y]
			//
			// This produces exactly 16 destination YUV-pixels

			for (uint32_t i = 0; i < 4; ++i, src_ptr += 2) {
				// process four RGBf32 pixels per iteration (each pixel is 4x32 bits)
				// Input-format:
				//     One RGBf pixel: Bits# 127:96 95:64 63:32 31:0
				//                            Alpha Blue  Green Red
				//
				//     Each AVX256 register stores 2 RGBf pixels.

				// Output-format:
				//   packed-pixel YUV (8bits per channel, 32bpp total):
				// Byte #15 14 13 12 11 10  9  8    7  6  5  4  3  2  1  0
				//       0  Y3 U3 V3 0  Y2 U2 V2    0  Y1 U1 V1 0  Y0 U0 V0
				//
				//   Output fits in one SSE2 register (128bit) 

				// (1) process 2 RGBf32 pixels
				temp_y = _mm256_mul_ps(src_ptr[0], cmatrix_y);
				temp_u = _mm256_mul_ps(src_ptr[0], cmatrix_u);
				temp_v = _mm256_mul_ps(src_ptr[0], cmatrix_v);

				// temp_y   A7 A6 A5 A4 | A3 A2 A1 A0
				//          Y1 Y1 Y1 Y1   Y0 Y0 Y0 Y0

				// temp_u   B7 B6 B5 B4 | B3 B2 B1 B0
				//          U1 U1 U1 U1   U0 U0 U0 U0

				// temp_v   C7 C6 C5 C4 | C3 C2 C1 C0
				//          V1 V1 V1 V1   V0 V0 V0 V0

				temp_yu = _mm256_hadd_ps(temp_y, temp_u);
				//  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 <HADD> U7 U6 U5 U4 U3 U2 U1 U0
				//  UUUUUUUUUUUUUUU YYYYYYYYYYYYYY  UUUUUUUUUUUUUUU YYYYYYYYYYYYYYY
				//  (U7+U6) (U4+U5) (Y6+Y7) (Y4+Y5) (U2+U3) (U0+U1) (Y2+Y3) (Y0+Y1)

				//   Float#7    #6   #5     #4     #3    #2    #1    #0
				//      B6_B7 B5_B4 A6_A7 A4_A5 B3_B2 B1_B0 A2_A3 A0_A1
				//      33333 33333 11111 11111 22222 22222 00000 00000
				//       U1     U1   Y1     Y1    U0   U0     Y0     Y0

				temp_v = _mm256_hadd_ps(temp_v, zero);
				//   Float#7    #6   #5     #4     #3    #2    #1    #0
				//        0     0    A6_A7  A4_A5   0    0    A2_A3  A0_A1
				//                   11111  11111             0000   00000
				//                    V1      V1                V0      V0

				temp_yuv = _mm256_hadd_ps(temp_yu, temp_v);
				temp_yuv = _mm256_round_ps(temp_yuv, _MM_FROUND_NINT);

				// Float#7  6  5  4  3  2  1  0
				//       0  V1 U1 Y1 0  V0 U0 Y0
				temp_yuvi = _mm256_cvttps_epi32(temp_yuv);// truncate float -> int32_t

				mm32i[0] = _mm256_extractf128_si256(temp_yuvi, 0);
				mm32i[1] = _mm256_extractf128_si256(temp_yuvi, 1);
				mm16i[0] = _mm_packs_epi32(mm32i[0], mm32i[1]);// pack 32bit->16bit

				// add offset:  Y -> +16,  U -> +128, V -> +128
				mm16i[0] = _mm_adds_epi16(mm16i[0], coffset);// save this result for later

				// (2) process 2 more RGBf32 pixels
				temp_y = _mm256_mul_ps(src_ptr[1], cmatrix_y);
				temp_u = _mm256_mul_ps(src_ptr[1], cmatrix_u);
				temp_v = _mm256_mul_ps(src_ptr[1], cmatrix_v);
				temp_yu = _mm256_hadd_ps(temp_y, temp_u);
				temp_v = _mm256_hadd_ps(temp_v, zero);
				temp_yuv = _mm256_hadd_ps(temp_yu, temp_v);
				temp_yuv = _mm256_round_ps(temp_yuv, _MM_FROUND_NINT);
				temp_yuvi = _mm256_cvttps_epi32(temp_yuv);// truncate float -> int32_t
				mm32i[0] = _mm256_extractf128_si256(temp_yuvi, 0);
				mm32i[1] = _mm256_extractf128_si256(temp_yuvi, 1);
				mm16i[1] = _mm_packs_epi32(mm32i[0], mm32i[1]);// pack 32bit->16bit
				mm16i[1] = _mm_adds_epi16(mm16i[1], coffset);

				pixels = _mm_packus_epi16(mm16i[0], mm16i[1]);// pack 16bit->8bit

				// Byte #15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
				//       0  V3 U3 Y3 0  V2 U2 Y2  0  V1 U1 Y1 0  V0 U0 Y0

				temp_444[i] = _mm_shuffle_epi8(pixels, m128_rgb32fyuv_reorder);
				// Byte #15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
				//       0  Y3 U3 V3 0  Y2 U2 V2  0  Y1 U1 V1 0  Y0 U0 V0
			} // for i
			
			// now convert temp_444[0..3] (sixteen 32bpp packed-pixels)
			//   into planar YUV 4:4:4  (3 separate Y, U, V surfaces)

			//pixels = src_444[src_stride*y + x];
			pixels = temp_444[0];// extract pixels #[x, x+3]
			out_y = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[0]);
			out_u = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[0]);
			out_v = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[0]);

			//			in_444 = src_444[src_stride*y + x + 1];
			pixels = temp_444[1];// extract pixels #[x+4, x+7]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[1]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[1]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[1]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//			in_444 = src_444[src_stride*y + x + 2];
			pixels = temp_444[2];// extract pixels #[x+8, x+11]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[2]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[2]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[2]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//			in_444 = src_444[src_stride*y + x + 3];
			pixels = temp_444[3];// extract pixels #[x+12, x+15]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[3]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[3]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[3]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//dest_y[ dst_stride*yout + (x>>2)] = out_y;
			dest_y[dst_offset] = out_y;
			dest_u[dst_offset] = out_u;
			dest_v[dst_offset] = out_v;
		} // for x

		//src_ptr += src_adjustment;  // move to next scanline
		//dst_offset -= dst_adjustment;// move to next scanline (reverse direction)
	} // for y

	return;
}


void CRepackyuv::_convert_RGBFtoY444_avx2( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select: false=bt601, true=bt709
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
	const __m256 src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	__m128i dest_y[],   // pointer to output Y-plane
	__m128i dest_u[],   // pointer to output U-plane
	__m128i dest_v[]    // pointer to output V-plane
	)
{
	const uint32_t width_div_2 = (width + 1) >> 1;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m256 zero = _mm256_setzero_ps();

	const __m256 cmatrix_y = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_Y);
	const __m256 cmatrix_u = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_U);
	const __m256 cmatrix_v = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_V);

	const __m256i coffset = use_fullscale ? m256_rgb32fyuv_offset0255 : m256_rgb32fyuv_offset16240;
	const uint32_t dst_adjustment = dst_stride + width_div_16;
	const uint32_t src_adjustment = src_stride - width_div_2;

	__m256 temp_y, temp_u, temp_v, temp_yu, temp_yuv;
	__m256i pixels256, mm32i[2], mm16i[4], temp_444[2];
	__m128i pixels;

	__m128i temp_yout, temp_uout, temp_vout, out_y, out_u, out_v;
	uint32_t dst_offset;

	for (uint32_t y = 0; y < height; ++y) {
		const __m256 *src_ptr = &src_rgb[y * src_stride];
		dst_offset = (height - y - 1) * dst_stride;// invert the image vertically

		//		for (uint32_t x = 0; x < width; x += 4, ++src_ptr, ++dst_ptr) {
		for (uint32_t x = 0; x < width; x += 16, ++dst_offset) {
			// In each iteration, process 16 source YUV-pixels (in 4 groups of 4):
			//	Pixel# [x,y], [x+1,y], ... [x+15,y]
			//
			// This produces exactly 16 destination YUV-pixels

			for (uint32_t i = 0; i < 4; ++i, src_ptr+=2) {
				// process four RGBf32 pixels per iteration (each pixel is 4x32 bits)
				// Input-format:
				//     One RGBf pixel: Bits# 127:96 95:64 63:32 31:0
				//                            Alpha Blue  Green Red
				//
				//     Each AVX256 register stores 2 RGBf pixels.

				// Output-format:
				//   packed-pixel YUV (8bits per channel, 32bpp total):
				// Byte #15 14 13 12 11 10  9  8    7  6  5  4  3  2  1  0
				//       0  Y3 U3 V3 0  Y2 U2 V2    0  Y1 U1 V1 0  Y0 U0 V0
				//
				//   Output fits in one SSE2 register (128bit) 

				// (1) process 2 RGBf32 pixels
				temp_y = _mm256_mul_ps(src_ptr[0], cmatrix_y);
				temp_u = _mm256_mul_ps(src_ptr[0], cmatrix_u);
				temp_v = _mm256_mul_ps(src_ptr[0], cmatrix_v);

				// temp_y   A7 A6 A5 A4 | A3 A2 A1 A0
				//          Y1 Y1 Y1 Y1   Y0 Y0 Y0 Y0

				// temp_u   B7 B6 B5 B4 | B3 B2 B1 B0
				//          U1 U1 U1 U1   U0 U0 U0 U0

				// temp_v   C7 C6 C5 C4 | C3 C2 C1 C0
				//          V1 V1 V1 V1   V0 V0 V0 V0

				temp_yu = _mm256_hadd_ps(temp_y, temp_u);
				//  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 <HADD> U7 U6 U5 U4 U3 U2 U1 U0
				//  UUUUUUUUUUUUUUU YYYYYYYYYYYYYY  UUUUUUUUUUUUUUU YYYYYYYYYYYYYYY
				//  (U7+U6) (U4+U5) (Y6+Y7) (Y4+Y5) (U2+U3) (U0+U1) (Y2+Y3) (Y0+Y1)

				//   Float#7    #6   #5     #4     #3    #2    #1    #0
				//      B6_B7 B5_B4 A6_A7 A4_A5 B3_B2 B1_B0 A2_A3 A0_A1
				//      33333 33333 11111 11111 22222 22222 00000 00000
				//       U1     U1   Y1     Y1    U0   U0     Y0     Y0

				temp_v = _mm256_hadd_ps(temp_v, zero);
				//   Float#7    #6   #5     #4     #3    #2    #1    #0
				//        0     0    A6_A7  A4_A5   0    0    A2_A3  A0_A1
				//                   11111  11111             0000   00000
				//                    V1      V1                V0      V0

				temp_yuv = _mm256_hadd_ps(temp_yu, temp_v);
				temp_yuv = _mm256_round_ps(temp_yuv, _MM_FROUND_NINT);

				// Float#7  6  5  4  3  2  1  0
				//       0  V1 U1 Y1 0  V0 U0 Y0
				mm32i[0] = _mm256_cvttps_epi32(temp_yuv);// truncate float -> int32_t
				temp_y = _mm256_mul_ps(src_ptr[1], cmatrix_y);
				temp_u = _mm256_mul_ps(src_ptr[1], cmatrix_u);
				temp_v = _mm256_mul_ps(src_ptr[1], cmatrix_v);
				temp_yu = _mm256_hadd_ps(temp_y, temp_u);
				temp_v = _mm256_hadd_ps(temp_v, zero);
				temp_yuv = _mm256_hadd_ps(temp_yu, temp_v);
				temp_yuv = _mm256_round_ps(temp_yuv, _MM_FROUND_NINT);
				mm32i[1] = _mm256_cvttps_epi32(temp_yuv);// truncate float -> int32_t
				pixels256 = _mm256_packs_epi32(mm32i[0], mm32i[1]);

				// Undo the integrated-permute from the previous AVX2 pack instr:
				//    swap bits[192:128] <-> bits[127:64]
				pixels256 = _mm256_permute4x64_epi64(pixels256, _MM_SHUFFLE(3, 1, 2, 0));

				// Change Y/U/V origin (Y+16, U+128, V+128...)
				mm16i[i] = _mm256_adds_epi16(pixels256, coffset);
			}
			
			pixels256   = _mm256_packus_epi16(mm16i[0], mm16i[1]);// pack 16bit->8bit {pixels x .. x+7}
			pixels256 = _mm256_permute4x64_epi64(pixels256, _MM_SHUFFLE(3, 1, 2, 0));

			// Byte #31..16 15 14 13 12 11 10  9  8 7  6  5  4  3  2  1  0
			//        ...Y4 0  V3 U3 Y3 0  V2 U2 Y2 0  V1 U1 Y1 0  V0 U0 Y0
			temp_444[0] = _mm256_shuffle_epi8(pixels256, m256_rgb32fyuv_reorder);
			// Byte #31..16 15 14 13 12 11 10  9  8 7  6  5  4  3  2  1  0
			//        ...V4 0  Y3 U3 V3 0  Y2 U2 V2 0  Y1 U1 V1 0  Y0 U0 V0

			pixels256   = _mm256_packus_epi16(mm16i[2], mm16i[3]);// pack 16bit->8bit {pixels x+8 .. x+15}
			pixels256 = _mm256_permute4x64_epi64(pixels256, _MM_SHUFFLE(3, 1, 2, 0));
			temp_444[1] = _mm256_shuffle_epi8(pixels256, m256_rgb32fyuv_reorder);

			// now convert temp_444[0..1] (sixteen 32bpp YUV packed-pixels)
			//   into planar YUV 4:4:4  (3 separate Y, U, V surfaces)

			//pixels = src_444[src_stride*y + x];
			pixels = _mm256_extractf128_si256(temp_444[0],0);// extract pixels #[x, x+3]
			out_y = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[0]);
			out_u = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[0]);
			out_v = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[0]);

			//			in_444 = src_444[src_stride*y + x + 1];
			pixels = _mm256_extractf128_si256(temp_444[0], 1);// extract pixels #[x+4, x+7]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[1]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[1]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[1]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//			in_444 = src_444[src_stride*y + x + 2];
			pixels = _mm256_extractf128_si256(temp_444[1], 0);// extract pixels #[x+8, x+11]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[2]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[2]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[2]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//			in_444 = src_444[src_stride*y + x + 3];
			pixels = _mm256_extractf128_si256(temp_444[1], 1);// extract pixels #[x+12, x+15]
			temp_yout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_y[3]);
			temp_uout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_u[3]);
			temp_vout = _mm_shuffle_epi8(pixels, m128_shuffle_yuv444_v[3]);
			out_y = _mm_or_si128(out_y, temp_yout);  // out_y = out_y | temp_yout;
			out_u = _mm_or_si128(out_u, temp_uout);  // out_u = out_u | temp_uout;
			out_v = _mm_or_si128(out_v, temp_vout);  // out_v = out_v | temp_vout;

			//dest_y[ dst_stride*yout + (x>>2)] = out_y;
			dest_y[dst_offset] = out_y;
			dest_u[dst_offset] = out_u;
			dest_v[dst_offset] = out_v;
		} // for x

		//src_ptr += src_adjustment;  // move to next scanline
		//dst_offset -= dst_adjustment;// move to next scanline (reverse direction)
	} // for y

	return;
}


void CRepackyuv::convert_RGBFtoNV12( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select: false=bt601, true=bt709
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels)
	const uint32_t height,     // Y-dimension (#pixels)
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) [units of uint8_t]
	const uint8_t  src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of uint8_t]
	uint8_t dest_y[],   // pointer to output Y-plane
	uint8_t dest_uv[]   // pointer to output UV-plane
	)
{
	bool is_xmm_aligned = m_cpu_has_ssse3;// are addresses 16-byte aligned?

	// Check address-alignment of source_yuv plane(s)
	if (reinterpret_cast<uint64_t>(src_rgb)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_y)& 0xF)
		is_xmm_aligned = false;
	else if (reinterpret_cast<uint64_t>(dest_uv)& 0xF)
		is_xmm_aligned = false;

	if (src_stride & 0xF)
		is_xmm_aligned = false;
	else if (dst_stride & 0xF)
		is_xmm_aligned = false;

	if (height & 0x1)  // must have an even# scanlines
		is_xmm_aligned = false;
	else if (width & 0xF)     // horizontal framesize must be multiple of 16
		is_xmm_aligned = false;

	bool is_avx256_aligned = is_xmm_aligned && // are addresses 32-byte aligned
		m_cpu_has_avx && m_allow_avx;

	// Check if AVX1 can be used
	if (is_avx256_aligned) {
		if (reinterpret_cast<uint64_t>(src_rgb)& 0x1F)
			is_avx256_aligned = false;

		if (src_stride & 0x1F)
			is_avx256_aligned = false;
	}

	// Check if AVX2 can be used
	bool is_avx2 = is_avx256_aligned && m_cpu_has_avx2 && m_allow_avx2;
	if (is_avx2){
		if (reinterpret_cast<uint64_t>(dest_y)& 0x1F)
			is_avx2 = false;
		else if (reinterpret_cast<uint64_t>(dest_uv)& 0x1F)
			is_avx2 = false;

		if (dst_stride & 0x1F)
			is_avx2 = false;
	}

	if (is_avx2) {
		_convert_RGBFtoNV12_avx2( //AVX2 version of converter
			use_bt709, // bt709?
			use_fullscale, // true=PC/full scale, false=video scale (0-235)
			width, height,
			src_stride >> 5, // src stride (units of _m256)
			reinterpret_cast<__m256 const *>(src_rgb),
			dst_stride >> 5,  // dst stride (units of _m256i)
			reinterpret_cast<__m256i *>(dest_y),    // output Y
			reinterpret_cast<__m256i *>(dest_uv)   // output UV
		);
	} 
	else if ( is_avx256_aligned ){
		_convert_RGBFtoNV12_avx( //AVX version of converter
			use_bt709, // bt709?
			use_fullscale, // true=PC/full scale, false=video scale (0-235)
			width, height,
			src_stride >> 5, // src stride (units of _m256)
			reinterpret_cast<__m256 const *>(src_rgb),
			dst_stride >> 4,  // dst stride (units of _m128i)
			reinterpret_cast<__m128i *>(dest_y),    // output Y
			reinterpret_cast<__m128i *>(dest_uv)   // output UV
		);
	}
	else {
		_convert_RGBFtoNV12_ssse3( // SSSE3 version of converter
			use_bt709, // bt709?
			use_fullscale, // true=PC/full scale, false=video scale (0-235)
			width, height,
			src_stride >> 4, // src stride (units of _m128)
			reinterpret_cast<__m128 const *>(src_rgb),
			dst_stride >> 4,  // dst stride (units of _m128i)
			reinterpret_cast<__m128i *>(dest_y),    // output Y
			reinterpret_cast<__m128i *>(dest_uv)   // output UV
		);
	}
}

void CRepackyuv::_convert_RGBFtoNV12_avx( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select: false=bt601, true=bt709
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels): must be multiple of 2
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
	const __m256 src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	__m128i dest_y[],   // pointer to output Y-plane
	__m128i dest_uv[]   // pointer to output UV-plane
	)
{
	const uint32_t width_div_2 = (width + 1) >> 1;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m256 zero = _mm256_setzero_ps();
	const __m128i zero128 = _mm_setzero_si128();
	const __m128i round_offset = { { 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0 } };// rounding offset for div/4 operation
	const __m256 cmatrix_y = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_Y);
	const __m256 cmatrix_u = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_U);
	const __m256 cmatrix_v = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_V);

	const __m128i coffset = use_fullscale ? m128_rgb32fyuv_offset0255 : m128_rgb32fyuv_offset16240;
	//const uint32_t dst_adjustment = dst_stride + width_div_16;
	//const uint32_t src_adjustment = src_stride - width_div_2;

	__m256 temp_y[2], temp_u[2], temp_v[2], temp_yu[2], temp_yuv[2];
	__m256i temp_yuvi[2];// temp var for 32bit int SIMD-data
	__m128i mm32i[2][2];// temp var for 32bit int SIMD-data
	__m128i mm16i[2][8];// temp var for 16bit int SIMD-data
	__m128i pixels_y[2], pixels_y_yp1[2], pixels_uv, pixels_uv2[4], pixels;

	const __m256 *src_ptr;    // source-pointer: scanline#y
	const __m256 *src_ptr_yp1;// source-pointer: scanline#(y+1)
	__m128i *dst_ptr_y;    // destination-pointer: y_plane (scanline #y)
	__m128i *dst_ptr_y_yp1;// destination-pointer: y_plane (scanline #y+1)
	__m128i *dst_ptr_uv;   // destination-pointer: uv_plane

	for (uint32_t y = 0; y < height; y += 2) {
		// Source-pointers: RGB32f-surface
		src_ptr     = src_rgb + (y * src_stride); // scanline (scanline #y)
		src_ptr_yp1 = src_ptr + src_stride;       // scanline (scanline #y+1)

		// Destination pointer: Y-surface
		dst_ptr_y = dest_y + (height - 1 - y) * dst_stride;// scanline (#y)
		dst_ptr_y_yp1 = dst_ptr_y - dst_stride;            // scanline (#y+1) 

		// Destination pointer: UV-surface
		dst_ptr_uv    = dest_uv + ((height - 1 - y)>>1) * dst_stride;

		//		for (uint32_t x = 0; x < width; x += 4, ++src_ptr, ++dst_ptr) {
		for (uint32_t x = 0; x < width; x += 16, ++dst_ptr_uv) {
			// In each iteration, process 16 source YUV-pixels (in 4 groups of 4):
			//	Pixel# [x,y], [x+1,y], ... [x+15,y]
			//
			// This produces exactly 16 destination YUV-pixels

			for (uint32_t i = 0; i < 4; ++i) {
				uint32_t k;
				for (uint32_t j = 0; j < 2; ++j, ++src_ptr, ++src_ptr_yp1) {
					k = (i << 1) + j;
					// process two RGBf32 pixels per iteration (each pixel is 4x32 bits)
					// Input-format:
					//     One RGBf pixel: Bits# 127:96 95:64 63:32 31:0
					//                            Alpha Blue  Green Red
					//
					//     Each AVX256 register stores 2 RGBf pixels.

					// Output-format:
					//   packed-pixel YUV (8bits per channel, 32bpp total):
					// Byte #15 14 13 12 11 10  9  8    7  6  5  4  3  2  1  0
					//       0  Y3 U3 V3 0  Y2 U2 V2    0  Y1 U1 V1 0  Y0 U0 V0
					//
					//   Output fits in one SSE2 register (128bit) 

					// (1) process 2 RGBf32 pixels
					temp_y[0] = _mm256_mul_ps(src_ptr[0], cmatrix_y); //pixel(x,y)
					temp_u[0] = _mm256_mul_ps(src_ptr[0], cmatrix_u);
					temp_v[0] = _mm256_mul_ps(src_ptr[0], cmatrix_v);
					temp_y[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_y); //pixel(x,y+1)
					temp_u[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_u);
					temp_v[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_v);

					// temp_y   A7 A6 A5 A4 | A3 A2 A1 A0
					//          Y1 Y1 Y1 Y1   Y0 Y0 Y0 Y0

					// temp_u   B7 B6 B5 B4 | B3 B2 B1 B0
					//          U1 U1 U1 U1   U0 U0 U0 U0

					// temp_v   C7 C6 C5 C4 | C3 C2 C1 C0
					//          V1 V1 V1 V1   V0 V0 V0 V0

					temp_yu[0] = _mm256_hadd_ps(temp_y[0], temp_u[0]);
					temp_yu[1] = _mm256_hadd_ps(temp_y[1], temp_u[1]);
					//  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 <HADD> U7 U6 U5 U4 U3 U2 U1 U0
					//  UUUUUUUUUUUUUUU YYYYYYYYYYYYYY  UUUUUUUUUUUUUUU YYYYYYYYYYYYYYY
					//  (U7+U6) (U4+U5) (Y6+Y7) (Y4+Y5) (U2+U3) (U0+U1) (Y2+Y3) (Y0+Y1)

					//   Float#7    #6   #5     #4     #3    #2    #1    #0
					//      B6_B7 B5_B4 A6_A7 A4_A5 B3_B2 B1_B0 A2_A3 A0_A1
					//      33333 33333 11111 11111 22222 22222 00000 00000
					//       U1     U1   Y1     Y1    U0   U0     Y0     Y0

					temp_v[0] = _mm256_hadd_ps(temp_v[0], zero);
					temp_v[1] = _mm256_hadd_ps(temp_v[1], zero);
					//   Float#7    #6   #5     #4     #3    #2    #1    #0
					//        0     0    A6_A7  A4_A5   0    0    A2_A3  A0_A1
					//                   11111  11111             0000   00000
					//                    V1      V1                V0      V0

					temp_yuv[0] = _mm256_hadd_ps(temp_yu[0], temp_v[0]);
					temp_yuv[0] = _mm256_round_ps(temp_yuv[0], _MM_FROUND_NINT);
					temp_yuv[1] = _mm256_hadd_ps(temp_yu[1], temp_v[1]);
					temp_yuv[1] = _mm256_round_ps(temp_yuv[1], _MM_FROUND_NINT);

					// Float#7  6  5  4  3  2  1  0
					//       0  V1 U1 Y1 0  V0 U0 Y0
					temp_yuvi[0] = _mm256_cvttps_epi32(temp_yuv[0]);// truncate float -> int32_t
					temp_yuvi[1] = _mm256_cvttps_epi32(temp_yuv[1]);// truncate float -> int32_t

					mm32i[0][0] = _mm256_extractf128_si256(temp_yuvi[0], 0);// pixel(x,y)
					mm32i[0][1] = _mm256_extractf128_si256(temp_yuvi[0], 1);
					mm16i[0][k] = _mm_packs_epi32(mm32i[0][0], mm32i[0][1]);// pack 32bit->16bit

					mm32i[1][0] = _mm256_extractf128_si256(temp_yuvi[1], 0);// pixel(x,y+1)
					mm32i[1][1] = _mm256_extractf128_si256(temp_yuvi[1], 1);
					mm16i[1][k] = _mm_packs_epi32(mm32i[1][0], mm32i[1][1]);// pack 32bit->16bit

					// add offset:  Y -> +16,  U -> +128, V -> +128
					mm16i[0][k] = _mm_adds_epi16(mm16i[0][k], coffset);// save this result for later
					mm16i[1][k] = _mm_adds_epi16(mm16i[1][k], coffset);// save this result for later
				} // for j
			} // for i

			// At this point, we have 16 YUV444 pixels, with 16bits per channel (48bpp)
			// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
			//         0   --V1- --U1-  -Y1-      0    -V0-  -U0-  -Y0-

			// mm16i[0][0] = pixels (x   .. x+1)  scanline#y
			// mm16i[0][1] = pixels (x+3 .. x+2) scanline#y
			// mm16i[0][2] = pixels (x+5 .. x+4) scanline#y
			// ...
			// mm16i[0][7] = pixels (x+15.. x+14) scanline#y
			//
			// mm16i[1][0] = pixels (x   .. x+1) scanline#y+1
			// ...
			// mm16i[1][7] = pixels (x+15.. x+14) scanline#y+1

			// extract y-plane
			for (uint32_t i = 0; i < 2; ++i) {
				uint32_t k;
				pixels_y[i] = zero128;
				pixels_y_yp1[i] = zero128;

				// Handle Luma(Y) plane
				for (uint32_t j = 0; j < 4; ++j) {
					// on first iteration i=0, then  k = 0..3
					// on second iteration i=1, then k = 4..7
					k = (i << 2) + j;
					pixels      = _mm_shuffle_epi8(mm16i[0][k], m128_shuffle_y16to8[j]);
					pixels_y[i] = _mm_or_si128(pixels, pixels_y[i]);

					pixels      = _mm_shuffle_epi8(mm16i[1][k], m128_shuffle_y16to8[j]);
					pixels_y_yp1[i] = _mm_or_si128(pixels, pixels_y_yp1[i]);
				}
			} // for i

			// Write the Y-pixels [x .. x+15], 
			//    then advance -> +16 pixels to the right
			*dst_ptr_y++     = _mm_packus_epi16(pixels_y[0], pixels_y[1]);// pack 16bit -> 8bit
			*dst_ptr_y_yp1++ = _mm_packus_epi16(pixels_y_yp1[0], pixels_y_yp1[1]);

			// Handle (chroma) UV-plane:
			//    (a) cut resolution in half (horizontally and vertically)
			//    (b) do this by taking the average of values from a 2x2 box:
			//        (X,Y), (X+1,Y), (X,Y+1), (X+1,Y+1)
			//        Then dividing the sum by 4.
			for (uint32_t i = 0; i < 4; ++i) {
				uint32_t k;
				pixels_uv = zero128;
				for (uint32_t j = 0; j < 2; ++j) {
					k = (i << 1) + j;

					// Cut the vertical chroma-resolution in half by 
					// summing scanline#(y) and (y+11)
					pixels = _mm_adds_epi16(mm16i[0][k], mm16i[1][k]);
					// U/V pixels now have 9-bit magnitude (from 8-bit)

					// Repack the UV-pixels from
					// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
					//         0   --V1- --U1-  -Y1-      0    -V0-  -U0-  -Y0-

					pixels = _mm_shuffle_epi8(pixels, m128_shuffle_uv16to8[j]);
					// into
					// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
					//  j=0                              -V1-  -V0-  -U1-  -U0-
					//  j=1  -V3-- --V2- --U3-  -U2-
					pixels_uv = _mm_or_si128(pixels, pixels_uv);
				}
				pixels_uv2[i] = pixels_uv;

			} // for i

			// average the box of 4 UV-pixels into a single pixel
			// Output U(X,Y) = 
			//                 U(X,Y)   U(X+1,Y)    \___  (sum of 4)
			//                 U(X,Y+1) U(X+1,Y+1)  /
			//
			// The {right-shift >> 2} re-normalizes the 10-bit U/V values to 8-bit magnitude
			pixels_uv = _mm_hadds_epi16(pixels_uv2[0], pixels_uv2[1]);
			// U/V pixels now have 10-bit magnitude (from 9-bit)
			pixels_uv = _mm_adds_epi16(pixels_uv, round_offset);
			pixels_uv = _mm_srai_epi16(pixels_uv, 2); // arithmetic right-shift >> 2

			pixels = _mm_hadds_epi16(pixels_uv2[2], pixels_uv2[3]);
			pixels = _mm_adds_epi16(pixels, round_offset);
			pixels = _mm_srai_epi16(pixels, 2); // arithmetic right-shift >> 2

			*dst_ptr_uv = _mm_packus_epi16(pixels_uv, pixels);// 16bit -> 8bit pack
		} // for x
	} // for y
}

/*
void CRepackyuv::_convert_RGBFtoNV12_avx2( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select: false=bt601, true=bt709
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels): must be multiple of 2
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
	const __m256 src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
	__m256i dest_y[],   // pointer to output Y-plane
	__m256i dest_uv[]   // pointer to output UV-plane
	)
{
	const uint32_t width_div_2 = (width + 1) >> 1;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m256 zero = _mm256_setzero_ps();
	const __m256i zero256 = _mm256_setzero_si256();
	const __m256i round_offset = _mm256_set_epi16(  // rounding offset for div/4 operation
		2, 2, 2, 2, 2, 2, 2, 2,
		2, 2, 2, 2, 2, 2, 2, 2
	);
	const __m256 cmatrix_y = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_Y);
	const __m256 cmatrix_u = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_U);
	const __m256 cmatrix_v = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_V);

	const __m256i coffset256 = use_fullscale ? m256_rgb32fyuv_offset0255 : m256_rgb32fyuv_offset16240;
	//const uint32_t dst_adjustment = dst_stride + width_div_16;
	//const uint32_t src_adjustment = src_stride - width_div_2;

	__m256 temp_y[2], temp_u[2], temp_v[2], temp_yu[2], temp_yuv[2];
	__m256i temp_yuvi[2];// temp var for 32bit int SIMD-data
	__m256i mm32i[2][2];// temp var for 32bit int SIMD-data
	__m256i mm16i[2][8];// temp var for 16bit int SIMD-data
	__m256i pixels_y[2], pixels_y_yp1[2], pixels_uv, pixels_uv2[4], pixels;

	const __m256 *src_ptr;    // source-pointer: scanline#y
	const __m256 *src_ptr_yp1;// source-pointer: scanline#(y+1)
	__m256i *dst_ptr_y;    // destination-pointer: y_plane (scanline #y)
	__m256i *dst_ptr_y_yp1;// destination-pointer: y_plane (scanline #y+1)
	__m256i *dst_ptr_uv;   // destination-pointer: uv_plane

	for (uint32_t y = 0; y < height; y += 2) {
		// Source-pointers: RGB32f-surface
		src_ptr = src_rgb + (y * src_stride); // scanline (scanline #y)
		src_ptr_yp1 = src_ptr + src_stride;       // scanline (scanline #y+1)

		// Destination pointer: Y-surface
		dst_ptr_y = dest_y + (height - 1 - y) * dst_stride;// scanline (#y)
		dst_ptr_y_yp1 = dst_ptr_y - dst_stride;            // scanline (#y+1) 

		// Destination pointer: UV-surface
		dst_ptr_uv = dest_uv + ((height - 1 - y) >> 1) * dst_stride;

		//		for (uint32_t x = 0; x < width; x += 4, ++src_ptr, ++dst_ptr) {
		for (uint32_t x = 0; x < width; x += 32, ++dst_ptr_uv) {
			// In each iteration, process 32 source YUV-pixels (in 8 groups of 4):
			//	Pixel# [x,y], [x+1,y], ... [x+15,y]
			//
			// This produces exactly 16 destination YUV-pixels

			for (uint32_t i = 0; i < 8; ++i) {
				for (uint32_t j = 0; j < 2; ++j, ++src_ptr, ++src_ptr_yp1) {
					// process two RGBf32 pixels per iteration (each pixel is 4x32 bits)
					// Input-format:
					//     One RGBf pixel: Bits# 127:96 95:64 63:32 31:0
					//                            Alpha Blue  Green Red
					//
					//     Each AVX256 register stores 2 RGBf pixels.

					// Output-format:
					//   packed-pixel YUV (8bits per channel, 32bpp total):
					// Byte #15 14 13 12 11 10  9  8    7  6  5  4  3  2  1  0
					//       0  Y3 U3 V3 0  Y2 U2 V2    0  Y1 U1 V1 0  Y0 U0 V0
					//
					//   Output fits in one SSE2 register (128bit) 

					// (1) process 2 RGBf32 pixels
					temp_y[0] = _mm256_mul_ps(src_ptr[0], cmatrix_y); //pixel(x,y)
					temp_u[0] = _mm256_mul_ps(src_ptr[0], cmatrix_u);
					temp_v[0] = _mm256_mul_ps(src_ptr[0], cmatrix_v);
					temp_y[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_y); //pixel(x,y+1)
					temp_u[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_u);
					temp_v[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_v);

					// temp_y   A7 A6 A5 A4 | A3 A2 A1 A0
					//          Y1 Y1 Y1 Y1   Y0 Y0 Y0 Y0

					// temp_u   B7 B6 B5 B4 | B3 B2 B1 B0
					//          U1 U1 U1 U1   U0 U0 U0 U0

					// temp_v   C7 C6 C5 C4 | C3 C2 C1 C0
					//          V1 V1 V1 V1   V0 V0 V0 V0

					temp_yu[0] = _mm256_hadd_ps(temp_y[0], temp_u[0]);
					temp_yu[1] = _mm256_hadd_ps(temp_y[1], temp_u[1]);
					//  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 <HADD> U7 U6 U5 U4 U3 U2 U1 U0
					//  UUUUUUUUUUUUUUU YYYYYYYYYYYYYY  UUUUUUUUUUUUUUU YYYYYYYYYYYYYYY
					//  (U7+U6) (U4+U5) (Y6+Y7) (Y4+Y5) (U2+U3) (U0+U1) (Y2+Y3) (Y0+Y1)

					//   Float#7    #6   #5     #4     #3    #2    #1    #0
					//      B6_B7 B5_B4 A6_A7 A4_A5 B3_B2 B1_B0 A2_A3 A0_A1
					//      33333 33333 11111 11111 22222 22222 00000 00000
					//       U1     U1   Y1     Y1    U0   U0     Y0     Y0

					temp_v[0] = _mm256_hadd_ps(temp_v[0], zero);
					temp_v[1] = _mm256_hadd_ps(temp_v[1], zero);
					//   Float#7    #6   #5     #4     #3    #2    #1    #0
					//        0     0    A6_A7  A4_A5   0    0    A2_A3  A0_A1
					//                   11111  11111             0000   00000
					//                    V1      V1                V0      V0

					temp_yuv[0] = _mm256_hadd_ps(temp_yu[0], temp_v[0]);
					temp_yuv[0] = _mm256_round_ps(temp_yuv[0], _MM_FROUND_NINT);
					temp_yuv[1] = _mm256_hadd_ps(temp_yu[1], temp_v[1]);
					temp_yuv[1] = _mm256_round_ps(temp_yuv[1], _MM_FROUND_NINT);

					// Float#7  6  5  4  3  2  1  0
					//       0  V1 U1 Y1 0  V0 U0 Y0
					mm32i[0][j] = _mm256_cvttps_epi32(temp_yuv[0]);// truncate float -> int32_t
					mm32i[1][j] = _mm256_cvttps_epi32(temp_yuv[1]);// truncate float -> int32_t
				} // for j

				temp_yuvi[0] = _mm256_packs_epi32(mm32i[0][0], mm32i[0][1]);// pack 32bit->16bit
				temp_yuvi[1] = _mm256_packs_epi32(mm32i[1][0], mm32i[1][1]);// pack 32bit->16bit

				// swap bits[191:128] <-> bits[127:64]
				temp_yuvi[0] = _mm256_permute4x64_epi64(temp_yuvi[0], _MM_SHUFFLE(3, 1, 2, 0));
				temp_yuvi[1] = _mm256_permute4x64_epi64(temp_yuvi[1], _MM_SHUFFLE(3, 1, 2, 0));

				// add offset:  Y -> +16,  U -> +128, V -> +128
				mm16i[0][i] = _mm256_adds_epi16(temp_yuvi[0], coffset256);// save this result for later
				mm16i[1][i] = _mm256_adds_epi16(temp_yuvi[1], coffset256);// save this result for later
			} // for i

			// At this point, we have 32 YUV444 pixels, with 16bits per channel
			//   ...this is 64 bits per pixel (16 bits are unused, alpha-channel)
			// In each AVX2-256bit register, 4 pixels fit
			// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
			//         0   --V1- --U1-  -Y1-      0    -V0-  -U0-  -Y0-

			// mm16i[0][0] = pixels (x   .. x+3) scanline#y
			// mm16i[0][1] = pixels (x+4 .. x+7) scanline#y
			// mm16i[0][2] = pixels (x+8 .. x+11) scanline#y
			// mm16i[0][3] = pixels (x+12.. x+15) scanline#y
			// ... 
			// mm16i[0][7] = pixels (x+28.. x+31) scanline#y

			//
			// mm16i[1][0] = pixels (x   .. x+3) scanline#y+1
			// ...
			// mm16i[1][7] = pixels (x+28.. x+31) scanline#y+1

			// extract y-plane
			for (uint32_t i = 0; i < 2; ++i) {
				uint32_t k;
				pixels_y[i] = zero256;
				pixels_y_yp1[i] = zero256;

				// Handle Luma(Y) plane
				for (uint32_t j = 0; j < 4; ++j) {
					// on first iteration i=0, then  k = 0..3
					// on second iteration i=1, then k = 4..7
					k = (i << 2) + j;
					pixels = _mm256_shuffle_epi8(mm16i[0][k], m256_shuffle_y16to8[j]);
					pixels = _mm256_permutevar8x32_epi32(pixels, m256_permc_y16to8[j]);
					pixels_y[i] = _mm256_or_si256(pixels, pixels_y[i]);

					pixels = _mm256_shuffle_epi8(mm16i[1][k], m256_shuffle_y16to8[j]);
					pixels = _mm256_permutevar8x32_epi32(pixels, m256_permc_y16to8[j]);
					pixels_y_yp1[i] = _mm256_or_si256(pixels, pixels_y_yp1[i]);
				}
			} // for i

			// Write the Y-pixels [x .. x+31], 
			//    then advance -> +31 pixels to the right
			temp_yuvi[0] = _mm256_packus_epi16(pixels_y[0], pixels_y[1]);// pack 16bit -> 8bit
			temp_yuvi[1] = _mm256_packus_epi16(pixels_y_yp1[0], pixels_y_yp1[1]);
			*dst_ptr_y++ = _mm256_permute4x64_epi64(temp_yuvi[0], _MM_SHUFFLE(3, 1, 2, 0));
			*dst_ptr_y_yp1++ = _mm256_permute4x64_epi64(temp_yuvi[1], _MM_SHUFFLE(3, 1, 2, 0));
			//*dst_ptr_y++ = _mm256_packus_epi16(pixels_y[0], pixels_y[1]);// pack 16bit -> 8bit
			//*dst_ptr_y_yp1++ = _mm256_packus_epi16(pixels_y_yp1[0], pixels_y_yp1[1]);

			// Handle (chroma) UV-plane:
			//    (a) cut resolution in half (horizontally and vertically)
			//    (b) do this by taking the average of values from a 2x2 box:
			//        (X,Y), (X+1,Y), (X,Y+1), (X+1,Y+1)
			//        Then dividing the sum by 4.
			for (uint32_t i = 0; i < 4; ++i) {
				uint32_t k;
				pixels_uv = zero256;
				for (uint32_t j = 0; j < 2; ++j) {
					k = (i << 1) + j;

					// Cut the vertical chroma-resolution in half by 
					// summing scanline#(y) and (y+11)
					pixels = _mm256_adds_epi16(mm16i[0][k], mm16i[1][k]);
					// U/V pixels now have 9-bit magnitude (from 8-bit)

					// Repack the UV-pixels from
					// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
					//         0   --V1- --U1-  -Y1-      0    -V0-  -U0-  -Y0-

					pixels = _mm256_shuffle_epi8(pixels, m256_shuffle_uv16to8[j]);
					// into
					// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
					//  j=0                              -V1-  -V0-  -U1-  -U0-
					//  j=1  -V3-- --V2- --U3-  -U2-
					pixels_uv = _mm256_or_si256(pixels, pixels_uv);
				}
				// swap bits[192:128]<->bits[127:64]
				pixels_uv2[i] = _mm256_permute4x64_epi64( pixels_uv, _MM_SHUFFLE(3,1,2,0));

			} // for i

			// average the box of 4 UV-pixels into a single pixel
			// Output U(X,Y) = 
			//                 U(X,Y)   U(X+1,Y)    \___  (sum of 4)
			//                 U(X,Y+1) U(X+1,Y+1)  /
			//
			// The {right-shift >> 2} re-normalizes the 10-bit U/V values to 8-bit magnitude
			pixels_uv = _mm256_hadds_epi16(pixels_uv2[0], pixels_uv2[1]);
			// U/V pixels now have 10-bit magnitude (from 9-bit)
			pixels_uv = _mm256_permute4x64_epi64(pixels_uv, _MM_SHUFFLE(3, 1, 2, 0));
			pixels_uv = _mm256_adds_epi16(pixels_uv, round_offset);
			pixels_uv = _mm256_srai_epi16(pixels_uv, 2); // arithmetic right-shift >> 2

			pixels = _mm256_hadds_epi16(pixels_uv2[2], pixels_uv2[3]);
			pixels = _mm256_permute4x64_epi64(pixels, _MM_SHUFFLE(3, 1, 2, 0));// swap bits
			pixels = _mm256_adds_epi16(pixels, round_offset);
			pixels = _mm256_srai_epi16(pixels, 2); // arithmetic right-shift >> 2
			
			pixels = _mm256_packus_epi16(pixels_uv, pixels);// 16bit -> 8bit pack
			*dst_ptr_uv = _mm256_permute4x64_epi64(pixels, _MM_SHUFFLE(3, 1, 2, 0));// swap bits
		} // for x
	} // for y
}
*/

void CRepackyuv::_convert_RGBFtoNV12_ssse3( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels): must be multiple of 2
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
	const __m128 src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m128i]
	__m128i dest_y[],   // pointer to output Y-plane
	__m128i dest_uv[]   // pointer to output UV-plane
	)
{
	const uint32_t width_div_2 = (width + 1) >> 1;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m128 zero = _mm_setzero_ps();
	const __m128i zero128 = _mm_setzero_si128();
	const __m128i round_offset =
		_mm_set_epi16(2, 2, 2, 2, 2, 2, 2, 2);// rounding offset for div/4 operation
	const __m128 cmatrix_y = get_rgb2yuv_coeff_matrix128(use_bt709, use_fullscale, SELECT_COLOR_Y);
	const __m128 cmatrix_u = get_rgb2yuv_coeff_matrix128(use_bt709, use_fullscale, SELECT_COLOR_U);
	const __m128 cmatrix_v = get_rgb2yuv_coeff_matrix128(use_bt709, use_fullscale, SELECT_COLOR_V);

	const __m128i coffset = use_fullscale ? m128_rgb32fyuv_offset0255 : m128_rgb32fyuv_offset16240;
	const __m128  roffset = { 0.5, 0.5, 0.5, 0.5 }; // floating-point (FP32) rounding-offset +0.5

	//const uint32_t dst_adjustment = dst_stride + width_div_16;
	//const uint32_t src_adjustment = src_stride - width_div_2;

	__m128 temp_y[2], temp_u[2], temp_v[2], temp_yu[2], temp_yuv[2];
	__m128i mm32i[2][2];// temp var for 32bit int SIMD-data
	__m128i mm16i[2][8];// temp var for 16bit int SIMD-data
	__m128i pixels_y[2], pixels_y_yp1[2], pixels_uv, pixels_uv2[4], pixels;

	const __m128 *src_ptr;    // source-pointer: scanline#y
	const __m128 *src_ptr_yp1;// source-pointer: scanline#(y+1)
	__m128i *dst_ptr_y;    // destination-pointer: y_plane (scanline #y)
	__m128i *dst_ptr_y_yp1;// destination-pointer: y_plane (scanline #y+1)
	__m128i *dst_ptr_uv;   // destination-pointer: uv_plane

	for (uint32_t y = 0; y < height; y += 2) {
		// Source-pointers: RGB32f-surface
		src_ptr = src_rgb + (y * src_stride); // scanline (scanline #y)
		src_ptr_yp1 = src_ptr + src_stride;       // scanline (scanline #y+1)

		// Destination pointer: Y-surface
		dst_ptr_y = dest_y + (height - 1 - y) * dst_stride;// scanline (#y)
		dst_ptr_y_yp1 = dst_ptr_y - dst_stride;            // scanline (#y+1) 

		// Destination pointer: UV-surface
		dst_ptr_uv = dest_uv + ((height - 1 - y) >> 1) * dst_stride;

		//		for (uint32_t x = 0; x < width; x += 4, ++src_ptr, ++dst_ptr) {
		for (uint32_t x = 0; x < width; x += 16, ++dst_ptr_uv) {
			// In each iteration, process 16 source YUV-pixels (in 4 groups of 4):
			//	Pixel# [x,y], [x+1,y], ... [x+15,y]
			//
			// This produces exactly 16 destination YUV-pixels

			for (uint32_t i = 0; i < 4; ++i) {
				uint32_t k;
				for (uint32_t j = 0; j < 2; ++j) {
					k = (i << 1) + j;
					for (uint32_t j2 = 0; j2 < 2; ++j2, ++src_ptr, ++src_ptr_yp1) {
						// process one RGBf32 pixel per iteration (each pixel is 4x32 bits)
						// Input-format:
						//     One RGBf pixel: Bits# 127:96 95:64 63:32 31:0
						//                            Alpha Blue  Green Red
						//
						//     Each SSE2 register stores 1 RGBf pixels.

						// Output-format:
						//   packed-pixel YUV (8bits per channel, 32bpp total):
						// Byte #15 14 13 12 11 10  9  8    7  6  5  4  3  2  1  0
						//       0  Y3 U3 V3 0  Y2 U2 V2    0  Y1 U1 V1 0  Y0 U0 V0
						//
						//   Output fits in one SSE2 register (128bit) 

						// (1) process 1 RGBf32 pixel
						temp_y[0] = _mm_mul_ps(src_ptr[0], cmatrix_y); //pixel(x,y)
						temp_u[0] = _mm_mul_ps(src_ptr[0], cmatrix_u);
						temp_v[0] = _mm_mul_ps(src_ptr[0], cmatrix_v);
						temp_y[1] = _mm_mul_ps(src_ptr_yp1[0], cmatrix_y); //pixel(x,y+1)
						temp_u[1] = _mm_mul_ps(src_ptr_yp1[0], cmatrix_u);
						temp_v[1] = _mm_mul_ps(src_ptr_yp1[0], cmatrix_v);

						// temp_y    A3 A2 A1 A0
						//           Y0 Y0 Y0 Y0

						// temp_u    B3 B2 B1 B0
						//           U0 U0 U0 U0

						// temp_v    C3 C2 C1 C0
						//           V0 V0 V0 V0

						temp_yu[0] = _mm_hadd_ps(temp_y[0], temp_u[0]);
						temp_yu[1] = _mm_hadd_ps(temp_y[1], temp_u[1]);
						//  Y3 Y2 Y1 Y0 <HADD> U3 U2 U1 U0
						//  UUUUUUUUUUUUUUU YYYYYYYYYYYYYYY
						//  (U2+U3) (U0+U1) (Y2+Y3) (Y0+Y1)

						//   Float#3    #2    #1    #0
						//       B3_B2 B1_B0 A2_A3 A0_A1
						//       22222 22222 00000 00000
						//         U0   U0     Y0     Y0

						temp_v[0] = _mm_hadd_ps(temp_v[0], zero);
						temp_v[1] = _mm_hadd_ps(temp_v[1], zero);
						//   Float#3    #2    #1    #0
						//         0    0    A2_A3  A0_A1
						//                   0000   00000
						//                     V0      V0

						temp_yuv[0] = _mm_hadd_ps(temp_yu[0], temp_v[0]);
						//temp_yuv[0] = _mm_round_ps(temp_yuv[0], _MM_FROUND_NINT);
						temp_yuv[0] = _mm_add_ps(temp_yuv[0], roffset); // add rounding-offset: +0.5

						temp_yuv[1] = _mm_hadd_ps(temp_yu[1], temp_v[1]);
						//temp_yuv[1] = _mm_round_ps(temp_yuv[1], _MM_FROUND_NINT);
						temp_yuv[1] = _mm_add_ps(temp_yuv[1], roffset); // add rounding-offset: +0.5

						// Float#3  2  1  0
						//       0  V0 U0 Y0
						mm32i[0][j2] = _mm_cvttps_epi32(temp_yuv[0]);// truncate float -> int32_t

						mm32i[1][j2] = _mm_cvttps_epi32(temp_yuv[1]);// truncate float -> int32_t
						//////
					} // for j2

					mm16i[0][k] = _mm_packs_epi32(mm32i[0][0], mm32i[0][1]);// pack 32bit->16bit
					mm16i[1][k] = _mm_packs_epi32(mm32i[1][0], mm32i[1][1]);// pack 32bit->16bit

					// add offset:  Y -> +16,  U -> +128, V -> +128
					mm16i[0][k] = _mm_adds_epi16(mm16i[0][k], coffset);// save this result for later
					mm16i[1][k] = _mm_adds_epi16(mm16i[1][k], coffset);// save this result for later
				} // for j
			} // for i

			// At this point, we have 16 YUV444 pixels, with 16bits per channel (48bpp)
			// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
			//         0   --V1- --U1-  -Y1-      0    -V0-  -U0-  -Y0-

			// mm16i[0][0] = pixels (x   .. x+1)  scanline#y
			// mm16i[0][1] = pixels (x+3 .. x+2) scanline#y
			// mm16i[0][2] = pixels (x+5 .. x+4) scanline#y
			// ...
			// mm16i[0][7] = pixels (x+15.. x+14) scanline#y
			//
			// mm16i[1][0] = pixels (x   .. x+1) scanline#y+1
			// ...
			// mm16i[1][7] = pixels (x+15.. x+14) scanline#y+1

			// extract y-plane
			for (uint32_t i = 0; i < 2; ++i) {
				uint32_t k;
				pixels_y[i] = zero128;
				pixels_y_yp1[i] = zero128;

				// Handle Luma(Y) plane
				for (uint32_t j = 0; j < 4; ++j) {
					// on first iteration i=0, then  k = 0..3
					// on second iteration i=1, then k = 4..7
					k = (i << 2) + j;
					pixels = _mm_shuffle_epi8(mm16i[0][k], m128_shuffle_y16to8[j]);
					pixels_y[i] = _mm_or_si128(pixels, pixels_y[i]);

					pixels = _mm_shuffle_epi8(mm16i[1][k], m128_shuffle_y16to8[j]);
					pixels_y_yp1[i] = _mm_or_si128(pixels, pixels_y_yp1[i]);
				}
			} // for i

			// Write the Y-pixels [x .. x+15], 
			//    then advance -> +16 pixels to the right
			*dst_ptr_y++ = _mm_packus_epi16(pixels_y[0], pixels_y[1]);// pack 16bit -> 8bit
			*dst_ptr_y_yp1++ = _mm_packus_epi16(pixels_y_yp1[0], pixels_y_yp1[1]);

			// Handle (chroma) UV-plane:
			//    (a) cut resolution in half (horizontally and vertically)
			//    (b) do this by taking the average of values from a 2x2 box:
			//        (X,Y), (X+1,Y), (X,Y+1), (X+1,Y+1)
			//        Then dividing the sum by 4.
			for (uint32_t i = 0; i < 4; ++i) {
				uint32_t k;
				pixels_uv = zero128;
				for (uint32_t j = 0; j < 2; ++j) {
					k = (i << 1) + j;

					// Cut the vertical chroma-resolution in half by 
					// summing scanline#(y) and (y+11)
					pixels = _mm_adds_epi16(mm16i[0][k], mm16i[1][k]);
					// U/V pixels now have 9-bit magnitude (from 8-bit)

					// Repack the UV-pixels from
					// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
					//         0   --V1- --U1-  -Y1-      0    -V0-  -U0-  -Y0-

					pixels = _mm_shuffle_epi8(pixels, m128_shuffle_uv16to8[j]);
					// into
					// Byte #15 14 13 12 11 10  9  8     7  6  5  4  3  2  1  0
					//  j=0                              -V1-  -V0-  -U1-  -U0-
					//  j=1  -V3-- --V2- --U3-  -U2-
					pixels_uv = _mm_or_si128(pixels, pixels_uv);
				}
				pixels_uv2[i] = pixels_uv;

			} // for i

			// average the box of 4 UV-pixels into a single pixel
			// Output U(X,Y) = 
			//                 U(X,Y)   U(X+1,Y)    \___  (sum of 4)
			//                 U(X,Y+1) U(X+1,Y+1)  /
			//
			// The {right-shift >> 2} re-normalizes the 10-bit U/V values to 8-bit magnitude
			pixels_uv = _mm_hadds_epi16(pixels_uv2[0], pixels_uv2[1]);
			// U/V pixels now have 10-bit magnitude (from 9-bit)
			pixels_uv = _mm_adds_epi16(pixels_uv, round_offset);
			pixels_uv = _mm_srai_epi16(pixels_uv, 2); // arithmetic right-shift >> 2

			pixels = _mm_hadds_epi16(pixels_uv2[2], pixels_uv2[3]);
			pixels = _mm_adds_epi16(pixels, round_offset);
			pixels = _mm_srai_epi16(pixels, 2); // arithmetic right-shift >> 2

			*dst_ptr_uv = _mm_packus_epi16(pixels_uv, pixels);// 16bit -> 8bit pack
		} // for x
	} // for y
}

bool CRepackyuv::set_cpu_allow_avx(bool flag) { // sets control-flag, allow_avx
	if (m_cpu_has_avx) {
		m_allow_avx = flag;
		return flag;
	}
	else {
		return false;
	}
}
bool CRepackyuv::set_cpu_allow_avx2(bool flag) {// sets control-flag, allow_avx2

	if (m_cpu_has_avx2) {
		m_allow_avx2 = flag;
		return flag;
	}
	else {
		return false;
	}
}

void CRepackyuv::_convert_RGBFtoNV12_avx2( // convert packed(RGB f32) into packed(YUV f32)
	const bool     use_bt709,     // color-space select: false=bt601, true=bt709
	const bool     use_fullscale, // true=PC/full scale, false=video(16-235)
	const uint32_t width,      // X-dimension (#pixels): must be multiple of 16
	const uint32_t height,     // Y-dimension (#pixels): must be multiple of 2
	const uint32_t src_stride, // distance from scanline(x) to scanline(x+1) (in units of __m256, 32-bytes)
	const __m256 src_rgb[],   // source RGB 32f plane (128 bits per pixel)
	const uint32_t dst_stride, // distance from scanline(x) to scanline(x+1) [units of __m256i]
	__m256i dest_y[],   // pointer to output Y-plane
	__m256i dest_uv[]   // pointer to output UV-plane
	)
{
	const uint32_t width_div_2 = (width + 1) >> 1;
	const uint32_t width_div_16 = (width + 15) >> 4;
	const __m256 zero = _mm256_setzero_ps();
	const __m256 cmatrix_y = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_Y);
	const __m256 cmatrix_u = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_U);
	const __m256 cmatrix_v = get_rgb2yuv_coeff_matrix256(use_bt709, use_fullscale, SELECT_COLOR_V);
	
	const __m256i offset_y = use_fullscale ?
		_mm256_setzero_si256() :  // for full-scale video range ( 0..255 )
		_mm256_set_epi16(16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16);

	// The U/V-offset is +128.  Here the regvalue is scaled up by x4 to
	// compensate for a divide-by-4 operation.
	const __m256i offset_uv = _mm256_set_epi16(
		512 + 2, 512 + 2, 512 + 2, 512 + 2,
		512 + 2, 512 + 2, 512 + 2, 512 + 2,
		512 + 2, 512 + 2, 512 + 2, 512 + 2,
		512 + 2, 512 + 2, 512 + 2, 512 + 2
	);

	//m256_rgb32fyuv_offset0255 : m256_rgb32fyuv_offset16240;

	const __m256i coffset256 = use_fullscale ? m256_rgb32fyuv_offset0255 : m256_rgb32fyuv_offset16240;
	//const uint32_t dst_adjustment = dst_stride + width_div_16;
	//const uint32_t src_adjustment = src_stride - width_div_2;

	__m256 temp_y[2], temp_u[2], temp_v[2], temp_yu[2], temp_yuv[2];
	__m256i temp_yuvi[2];// temp var for 32bit int SIMD-data
	__m256  mm32[2][4]; // temp var for 32bit  float SIMD-data
	__m256  mm32_y[2][4]; // temp var for 32bit  float SIMD-data
	__m256  mm32_uv[2][8]; // temp var for 32bit  float SIMD-data

	//__m256i temp_y2[2];

	//__m256i mm16i[2][8];// temp var for 326bit int SIMD-data
	__m256i pixels_y[2], pixels_y_yp1[2], pixels_uv, pixels, pixels_uv2[4];

	const __m256 *src_ptr;    // source-pointer: scanline#y
	const __m256 *src_ptr_yp1;// source-pointer: scanline#(y+1)
	__m256i *dst_ptr_y;    // destination-pointer: y_plane (scanline #y)
	__m256i *dst_ptr_y_yp1;// destination-pointer: y_plane (scanline #y+1)
	__m256i *dst_ptr_uv;   // destination-pointer: uv_plane

	// combine the Y-plane extraction with a permute4x64
	//  (The value '7' selects a unused (zero'd) pixel)
	const __m256i _permc_y[4] = {
		_mm256_set_epi32(7, 7, 7, 7, 7, 7, 4, 0), // Write pixels 0,1 
		_mm256_set_epi32(7, 7, 7, 7, 4, 0, 7, 7), // Write pixels 2,3
		_mm256_set_epi32(7, 7, 4, 0, 7, 7, 7, 7), // Write pixels 4,5
		_mm256_set_epi32(4, 0, 7, 7, 7, 7, 7, 7)  // Write pixels 6,7
	};

	// combine the UV-plane extraction with a permute4x64
	//  (The value '7' selects a unused (zero'd) pixel)
	const __m256i _permc_uv[2] = {
		_mm256_set_epi32(7, 7, 7, 7, 6, 2, 5, 1), // [0-3] re-arrange to V1 V0 U1 U0
		_mm256_set_epi32(6, 2, 5, 1, 7, 7, 7, 7)  // [4-7] re-arrange to V3 V2 U3 U2
	};

	for (uint32_t y = 0; y < height; y += 2) {
		// Source-pointers: RGB32f-surface
		src_ptr = src_rgb + (y * src_stride); // scanline (scanline #y)
		src_ptr_yp1 = src_ptr + src_stride;       // scanline (scanline #y+1)

		// Destination pointer: Y-surface
		dst_ptr_y = dest_y + (height - 1 - y) * dst_stride;// scanline (#y)
		dst_ptr_y_yp1 = dst_ptr_y - dst_stride;            // scanline (#y+1) 

		// Destination pointer: UV-surface
		dst_ptr_uv = dest_uv + ((height - 1 - y) >> 1) * dst_stride;

		//		for (uint32_t x = 0; x < width; x += 4, ++src_ptr, ++dst_ptr) {
		for (uint32_t x = 0; x < width; x += 32, ++dst_ptr_uv) {
			// In each iteration, process 32 source YUV-pixels (in 8 groups of 4):
			//	Pixel# [x,y], [x+1,y], ... [x+15,y]
			//
			// This produces exactly 16 destination YUV-pixels

			for (uint32_t i = 0; i < 4; ++i) {
				
				mm32_y[0][i] = _mm256_setzero_ps();// zero
				mm32_y[1][i] = _mm256_setzero_ps();// zero

				for (uint32_t j = 0; j < 4; ++j, ++src_ptr, ++src_ptr_yp1) {
					// process two RGBf32 pixels per iteration (each pixel is 4x32 bits)
					// Input-format:
					//     One RGBf pixel: Bits# 127:96 95:64 63:32 31:0
					//                            Alpha Blue  Green Red
					//
					//     Each AVX256 register stores 2 RGBf pixels.

					// Output-format:
					//   packed-pixel YUV (32bits per channel, 128bpp total):
					// Byte #7  6  5  4  3  2  1  0
					//       0  V1 U1 Y1 0  V0 U0 Y0

					// (1) process 2 RGBf32 pixels
					temp_y[0] = _mm256_mul_ps(src_ptr[0], cmatrix_y); //pixel(x,y)
					temp_u[0] = _mm256_mul_ps(src_ptr[0], cmatrix_u);
					temp_v[0] = _mm256_mul_ps(src_ptr[0], cmatrix_v);
					temp_y[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_y); //pixel(x,y+1)
					temp_u[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_u);
					temp_v[1] = _mm256_mul_ps(src_ptr_yp1[0], cmatrix_v);

					// temp_y   A7 A6 A5 A4 | A3 A2 A1 A0
					//          Y1 Y1 Y1 Y1   Y0 Y0 Y0 Y0

					// temp_u   B7 B6 B5 B4 | B3 B2 B1 B0
					//          U1 U1 U1 U1   U0 U0 U0 U0

					// temp_v   C7 C6 C5 C4 | C3 C2 C1 C0
					//          V1 V1 V1 V1   V0 V0 V0 V0

					temp_yu[0] = _mm256_hadd_ps(temp_y[0], temp_u[0]);
					temp_yu[1] = _mm256_hadd_ps(temp_y[1], temp_u[1]);
					//  Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0 <HADD> U7 U6 U5 U4 U3 U2 U1 U0
					//  UUUUUUUUUUUUUUU YYYYYYYYYYYYYY  UUUUUUUUUUUUUUU YYYYYYYYYYYYYYY
					//  (U7+U6) (U4+U5) (Y6+Y7) (Y4+Y5) (U2+U3) (U0+U1) (Y2+Y3) (Y0+Y1)

					//   Float#7    #6   #5     #4     #3    #2    #1    #0
					//      B6_B7 B5_B4 A6_A7 A4_A5 B3_B2 B1_B0 A2_A3 A0_A1
					//      33333 33333 11111 11111 22222 22222 00000 00000
					//       U1     U1   Y1     Y1    U0   U0     Y0     Y0

					temp_v[0] = _mm256_hadd_ps(temp_v[0], zero);
					temp_v[1] = _mm256_hadd_ps(temp_v[1], zero);
					//   Float#7    #6   #5     #4     #3    #2    #1    #0
					//        0     0    A6_A7  A4_A5   0    0    A2_A3  A0_A1
					//                   11111  11111             0000   00000
					//                    V1      V1                V0      V0

					// mm32[][]
					// Float#7  6  5  4  3  2  1  0
					//       0  V1 U1 Y1 0  V0 U0 Y0
					mm32[0][j] = _mm256_hadd_ps(temp_yu[0], temp_v[0]);
					mm32[1][j] = _mm256_hadd_ps(temp_yu[1], temp_v[1]);

					// Extract the y-bytes:  each iteration(j) gets 2 Y-samples
					// Float#7  6  5  4  3  2  1  0
					//       Y7 V6 Y5 Y4 Y3 Y2 Y1 Y0
					temp_y[0] = _mm256_permutevar8x32_ps(mm32[0][j], _permc_y[j]);
					temp_y[1] = _mm256_permutevar8x32_ps(mm32[1][j], _permc_y[j]);
					mm32_y[0][i] = _mm256_or_ps(mm32_y[0][i], temp_y[0]);
					mm32_y[1][i] = _mm256_or_ps(mm32_y[1][i], temp_y[1]);
				} // for j

				// Extract U/V-plane:
				//    Process pixels {x+0..x+15} (there are half as many UV pixels as Y pixels)
				for (uint32_t k = 0; k < 2; ++k) {
					// k == 0: scanline y
					// k == 1: scanline y+1
					for (uint32_t j = 0; j < 2; ++j) {
						const uint32_t src_j = (j << 1) + 0;
						const uint32_t dst_j = (i << 1) + j;
						// pixels {x+0..x+3}: extract the U/V pixel-pair from mm32
						mm32_uv[k][dst_j] = _mm256_permutevar8x32_ps(mm32[k][src_j + 0], _permc_uv[0]);

						// pixels {x+4..x+7}: extract the U/V pixel-pair from mm32
						temp_u[k] = _mm256_permutevar8x32_ps(mm32[k][src_j + 1], _permc_uv[1]);
						mm32_uv[k][dst_j] = _mm256_or_ps(temp_u[k], mm32_uv[k][dst_j]);
					} // for j
				} // for k

				// at this point:
				// mm32_y[0][0..3] = Y-pixels (32bpp float), scanline#y     (total of 32 pixels)
				//                   (each Y-pixel is 1 32bit float value)
				// mm32_y[1][0..3] = Y-pixels (32bpp float), scanline#y+1   (total of 32 pixels)
				// mm32_uv[0][0..7] = U/V-pixels (32bpp float), scanline#y  (total of 16 pixels)
				//                   (each U/V-pixel is 2 32bit float values)
				// mm32_uv[1][0..7] = U/V-pixels (32bpp float), scanline#y+1(total of 16 pixels)
			} // for i

			// extract y-plane
			//    Process pixels {x+0..x+31}:
			//       Convert from 32bit float into 8bpp integer (unsigned)
			for (uint32_t i = 0; i < 2; ++i) {
				// Each loop iteration, process 16 pixels from scanline#(y)
				//                      and another 16 pixels from scanline#(y+1)
				//                      (Total of 32 pixels)

				// grab pixels x+0...x+15 (scanline y)
				temp_y[0] = _mm256_round_ps(mm32_y[0][(i<<1)+0], _MM_FROUND_NINT);// pixels x..x+7
				temp_y[1] = _mm256_round_ps(mm32_y[0][(i<<1)+1], _MM_FROUND_NINT);// pixels x+8..x+15

				pixels    = _mm256_cvttps_epi32(temp_y[0]);
				pixels_uv = _mm256_cvttps_epi32(temp_y[1]);

				pixels_y[i] = _mm256_packs_epi32(pixels, pixels_uv);// pixels x+0..x+15 (16bpp), scanline#y
				// swap bits[191:128] <-> bits[127:64]
				pixels_y[i] = _mm256_adds_epi16(pixels_y[i], offset_y);

				// grab pixels x+0..x+15 (scanline y+1)
				temp_y[0] = _mm256_round_ps(mm32_y[1][(i<<1)+0], _MM_FROUND_NINT);
				temp_y[1] = _mm256_round_ps(mm32_y[1][(i<<1)+1], _MM_FROUND_NINT);

				pixels    = _mm256_cvttps_epi32(temp_y[0]);
				pixels_uv = _mm256_cvttps_epi32(temp_y[1]);

				pixels_y_yp1[i] = _mm256_packs_epi32(pixels, pixels_uv);// pixels x+0..x+15 (16bpp), scanline#y+1
				
				// Add offset (to normalize video-scale to 16-235)
				pixels_y_yp1[i] = _mm256_adds_epi16(pixels_y_yp1[i], offset_y);
			} // for i

			// Write the Y-pixels [x .. x+31], 
			//    then advance -> +31 pixels to the right
			temp_yuvi[0] = _mm256_packus_epi16(pixels_y[0], pixels_y[1]);// pack 16bit -> 8bit
			temp_yuvi[1] = _mm256_packus_epi16(pixels_y_yp1[0], pixels_y_yp1[1]);

			// undo 2 back-to-back permute ops: packs_epi32 and packus_epi16
			*dst_ptr_y++     = _mm256_permutevar8x32_epi32(temp_yuvi[0], m256_permc_pack2);
			*dst_ptr_y_yp1++ = _mm256_permutevar8x32_epi32(temp_yuvi[1], m256_permc_pack2);

			// Handle (chroma) UV-plane:
			//    (a) cut resolution in half (horizontally and vertically)
			//    (b) do this by taking the average of values from a 2x2 box:
			//        (X,Y), (X+1,Y), (X,Y+1), (X+1,Y+1)
			//        Then dividing the sum by 4.
			//    (c) convert from 32bit-float to 8-bit int

			for (uint32_t i = 0; i < 8; i+=2) {
				// Cut the vertical chroma-resolution in half by 
				// summing scanline#(y) and (y+1)
				temp_u[0] = _mm256_add_ps(mm32_uv[0][i], mm32_uv[1][i]);
				temp_u[1] = _mm256_add_ps(mm32_uv[0][i + 1], mm32_uv[1][i + 1]);

				// cut horziontal chroma-resolution in half by hadd
				temp_yu[0] = _mm256_hadd_ps(temp_u[0], temp_u[1]);
				temp_yu[0] = _mm256_round_ps(temp_yu[0], _MM_FROUND_NINT);
				pixels     = _mm256_cvttps_epi32(temp_yu[0]);
				pixels_uv2[i >> 1] = _mm256_permute4x64_epi64(pixels, _MM_SHUFFLE(3, 1, 2, 0));
			} // for i

			for (uint32_t i = 0; i < 4; i+=2) {
				pixels = _mm256_packs_epi32(pixels_uv2[i], pixels_uv2[i+1]);
				pixels = _mm256_adds_epi16(pixels, offset_uv); // +128 and rounding offset
				pixels_uv2[i >> 1] = _mm256_srai_epi16(pixels, 2); // re-normalize to 8-bit magnitude
			}
			
			pixels = _mm256_packus_epi16(pixels_uv2[0], pixels_uv2[1]);
			// undo 2 back-to-back permute-operations: packs_eip32, and packus_epi16
			*dst_ptr_uv = _mm256_permutevar8x32_epi32(pixels, m256_permc_pack2);// undo 2 permutes
		} // for x
	} // for y
}
