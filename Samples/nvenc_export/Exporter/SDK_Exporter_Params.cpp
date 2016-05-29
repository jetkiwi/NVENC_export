/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 1992-2008 Adobe Systems Incorporated                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Systems Incorporated and its suppliers, if    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Systems Incorporated and its    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Systems         */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

#include "SDK_Exporter.h" // fwrite_callback()
#include "SDK_Exporter_Params.h"
#include <cstdio>
#include <sstream>
#include <vector>

#include <nvapi.h> // NVidia NVAPI - functions to query system-info (eg. version of Geforce driver)
#include <cuda.h>
#include "helper_cuda.h"

#include "CNvEncoder.h"
#include "CNvEncoderH264.h"
#include "CNvEncoderH265.h"
#include "guidutil2.h"

#include "videoformats.h" // IsNV12Format, isYV12Format
#include "cpuid_ssse3.h"  // CPUID operation (do we support SSSE3?)

#define MAX_NEGATIVE 0x80000000 // maximum positive int32_t value
#define MAX_POSITIVE 0x7FFFFFFF // maximum positive int32_t value

// Kludge for snprintf(): 
//	MS Visual Studio 2010 (and earlier) don't support C99 sprintf() function
//   simulate it below using the macro, which terminates the
//   a max-length string with the null character (to prevent overrun.)
#ifdef _MSC_VER
	#define snprintf(dst, sizeof_dst, ... ) \
		_snprintf(dst, sizeof_dst, __VA_ARGS__ ), \
		dst[(sizeof_dst)-1] = 0
	#define snwprintf(dst, sizeof_dst, ... ) \
		_snwprintf(dst, sizeof_dst, __VA_ARGS__ ), \
		dst[(sizeof_dst)-1] = 0
#endif

prSuiteError
NVENC_SetParamName( ExportSettings *lRec, const csSDK_uint32 exID, const char GroupID[], const wchar_t ParamUIName[], const wchar_t ParamDescription[]) {
	//prUTF16Char				tempString[1024];
	prUTF16Char				*tempString = new prUTF16Char[16384]; // large enough to hold all tooltip help text!
	prSuiteError			error;
	copyConvertStringLiteralIntoUTF16(ParamUIName, tempString); // convert the string that will be shown in the dialog-box
	error = lRec->exportParamSuite->SetParamName(exID, 0, GroupID, tempString);
	
	// if != NULL, set the pop-up menu-tip:
	// Note: Adobe Premiere Elements 11 (PRE11) doesn't support this
	//
	// Check the suite-version to verify we aren't running PRE11.
	if ( ParamDescription && (lRec->exportParamSuite_version >= kPrSDKExportParamSuiteVersion4) ) {
		copyConvertStringLiteralIntoUTF16(ParamDescription, tempString);
		error = lRec->exportParamSuite->SetParamDescription(exID, 0, GroupID, tempString);
	}

	delete [] tempString;
	return error;
}

void
NVENC_errormessage_bad_key( ExportSettings *lRec )
{
		wostringstream oss; // text scratchpad for messagebox and errormsg 
		prUTF16Char title[256];
		prUTF16Char desc[256];
		HWND mainWnd = lRec->windowSuite->GetMainWindow();

		copyConvertStringLiteralIntoUTF16( L"NVENC-export error", title );
		oss << "!!! NVENC_EXPORT error, cannot open NVENC session !!!" << endl << endl;
		oss << "  Reason: the client license-key is expired or incompatible with this GPU device driver!" << endl << endl;
		oss << "Solution: Contact NVidia for a new NVENC license-key," << endl;
		oss << "          or install Quadro/Tesla professional graphics card!" << endl;

		copyConvertStringLiteralIntoUTF16( L"Cannot export because the client license-key is expired or incompatible with this GPU device driver", desc );
		lRec->errorSuite->SetEventStringUnicode( PrSDKErrorSuite::kEventTypeError, title, desc );

		MessageBoxW( GetLastActivePopup(mainWnd),
								oss.str().c_str(),
								EXPORTER_NAME_W,
								MB_OK | MB_ICONERROR );
}

unsigned
update_exportParamSuite_GPUSelectGroup_GPUIndex(
	const csSDK_uint32 exID,
	const ExportSettings *lRec,
	std::vector<NvEncoderGPUInfo_s> &gpulist
);

	// the following description-objects are declared in CNVEncoder.h
	// The 'desc_*' objects are tables of strings and their associated index-value
	//
	// For example:
	//	cls_convert_guid desc_nv_enc_profile_names 
	//		"NV_ENC_H264_PROFILE_BASE_GUID"		=	0,	{GUID}
	//		"NV_ENC_H264_PROFILE_MAIN_GUID"		=	1,	{GUID}
	//		"NV_ENC_H264_PROFILE_HIGH_GUID"		=	2,	{GUID}
	//		"NV_ENC_H264_PROFILE_STEREO_GUID"	=	3,	{GUID}
	//
	// The string-value is given to the Adobe UserInterface manager (so that
	// the strings appear in the plugin's dialog-box.)

	extern const cls_convert_guid desc_nv_enc_params_frame_mode_names;
	extern const cls_convert_guid desc_nv_enc_ratecontrol_names ;
	extern const cls_convert_guid desc_nv_enc_mv_precision_names ;
	extern const cls_convert_guid desc_nv_enc_codec_names ;
	extern const cls_convert_guid desc_nv_enc_profile_names ;
	extern const cls_convert_guid desc_nv_enc_preset_names ;
	extern const cls_convert_guid desc_nv_enc_buffer_format_names ;
	extern const cls_convert_guid desc_nv_enc_level_h264_names ;
	extern const cls_convert_guid desc_nv_enc_level_hevc_names ;
	extern const cls_convert_guid desc_nv_enc_tier_hevc_names ;
	extern const cls_convert_guid desc_nv_enc_h264_fmo_names ;
	extern const cls_convert_guid desc_nv_enc_h264_entropy_coding_mode_names ;
	extern const cls_convert_guid desc_nv_enc_adaptive_transform_names ;
	extern const cls_convert_guid desc_nv_enc_stereo_packing_mode_names ;

	// create an enumerated list of Adobe PrPixelFormats; to
	// assign each PrPixelFormat an index 
	const st_guid_entry table_PrPixelFormat[] = {
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709),// 4:2:0 planar
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUYV_422_8u_709),  // 16bpp packed-pixel (4:2:2)
		GUID_ENTRY(NO_GUID, PrPixelFormat_UYVY_422_8u_709),
		GUID_ENTRY(NO_GUID, PrPixelFormat_YUYV_422_8u_601),
		GUID_ENTRY(NO_GUID, PrPixelFormat_UYVY_422_8u_601),
		GUID_ENTRY(NO_GUID, PrPixelFormat_VUYX_4444_8u_709), // 32bpp packed-pixel (4:4:4)
		GUID_ENTRY(NO_GUID, PrPixelFormat_VUYA_4444_8u_709),
		GUID_ENTRY(NO_GUID, PrPixelFormat_VUYX_4444_8u),
		GUID_ENTRY(NO_GUID, PrPixelFormat_VUYA_4444_8u)
//		GUID_ENTRY(NO_GUID, PrPixelFormat_BGRX_4444_8u), // fallback, if YUV-output isn't supported
//		GUID_ENTRY(NO_GUID, PrPixelFormat_BGRA_4444_8u)
	};

	const cls_convert_guid desc_PrPixelFormat = cls_convert_guid(
		table_PrPixelFormat, array_length(table_PrPixelFormat)
	);
		
bool
PrPixelFormat_is_YUV420( const PrPixelFormat p )
{
	// returns: true if 'p' is a Yuv420 planar format
	switch( p ) {
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709:
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601:
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange:
			return true;
			break;
	}

	return false;
}

bool
PrPixelFormat_is_YUV422( const PrPixelFormat p )
{
	// returns: true if 'p' is a 16bpp packed-pixel format (4:2:2)
	switch( p ) {
		case PrPixelFormat_YUYV_422_8u_709:
		case PrPixelFormat_UYVY_422_8u_709:
		case PrPixelFormat_YUYV_422_8u_601:
		case PrPixelFormat_UYVY_422_8u_601:
			return true;
			break;
	}

	return false;
}

bool
PrPixelFormat_is_YUV444( const PrPixelFormat p )
{
	// returns: true if 'p' is a Yuv444 32bpp packed-pixel format
	switch( p ) {
		case PrPixelFormat_VUYX_4444_8u_709:
		case PrPixelFormat_VUYA_4444_8u_709:
		case PrPixelFormat_VUYX_4444_8u:
		case PrPixelFormat_VUYA_4444_8u:
			return true;
			break;
	}

	return false;
}

void
NVENC_GetEncoderCaps(const NvEncodeCompressionStd codec, const nv_enc_caps_s &caps, string &s)
{
	string        temps;
	ostringstream os;

	#define QUERY_PRINT_CAP(x) os << #x << " = " << std::dec << caps.value_ ## x << std::endl

	os << "<< This exporter-plugin is BETA-quality software >>" << std::endl;
	os << "<< written by an independent third-party. >>" << std::endl;
	os << "<< It is not supported by Adobe or NVidia in any way! >>" << std::endl;
	os << std::endl;
	os << "NVENC_export Build date: " <<  __DATE__ << " " << __TIME__ << std::endl;
	os << SDK_NAME << ", NVENC API version " << std::dec << NVENCAPI_MAJOR_VERSION 
		<< "." << std::dec << NVENCAPI_MINOR_VERSION << std::endl << std::endl;

	// Gthe currently selected codec (H264, HEVC, etc.)
	desc_nv_enc_codec_names.value2string(codec, temps);

	os << "codec=" << std::dec << codec << " (" << temps << "_GUID)" << std::endl << std::endl;

	QUERY_PRINT_CAP(NV_ENC_CAPS_NUM_MAX_BFRAMES);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_FIELD_ENCODING);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_MONOCHROME);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_FMO);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_QPELMV);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_BDIRECT_MODE);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_CABAC);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM);
	QUERY_PRINT_CAP(NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES);
	QUERY_PRINT_CAP(NV_ENC_CAPS_LEVEL_MAX);
	QUERY_PRINT_CAP(NV_ENC_CAPS_LEVEL_MIN);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SEPARATE_COLOUR_PLANE);
	QUERY_PRINT_CAP(NV_ENC_CAPS_WIDTH_MAX);
	QUERY_PRINT_CAP(NV_ENC_CAPS_HEIGHT_MAX);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_INTRA_REFRESH);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE);
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION);
	QUERY_PRINT_CAP(NV_ENC_CAPS_PREPROC_SUPPORT);
	QUERY_PRINT_CAP(NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT);
	QUERY_PRINT_CAP(NV_ENC_CAPS_MB_NUM_MAX); // broken in 320.79 driver, works in 334.89 WHQL driver (fixed in later drivers)
	QUERY_PRINT_CAP(NV_ENC_CAPS_MB_PER_SEC_MAX); // broken in 320.79 driver, works in 334.89 WHQL driver (...)
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_YUV444_ENCODE); 
	QUERY_PRINT_CAP(NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE);
/*
	for(unsigned i = 0; i < m_dwInputFmtCount; ++i ) {
		string s;
		printf("\t[%0u]\t", i);
		desc_nv_enc_buffer_format_names.value2string( m_pAvailableSurfaceFmts[i], s);
		//PrintGUID( m_stCodecProfileGUIDArray[i] );
		printf("%0s\n", s.c_str() );
	}

	// printf("CNvEncoder::PrintEncodeFormats() show supported codecs (MPEG-2, VC1, H264, etc.) \n");
	for(unsigned i = 0; i < m_dwEncodeGUIDCount; ++i ) {
		string s;
		printf("\t[%0u]\t", i);
		desc_nv_enc_codec_names.guid2string( m_stEncodeGUIDArray[i], s);
		//PrintGUID( m_stEncodeGUIDArray[i] );
		printf("%0s\n", s.c_str() );
	}

	// printf("CNvEncoder::PrintEncodeProfiles() show supported codecs (MPEG-2, VC1, H264, etc.) \n");
	for(unsigned i = 0; i < m_dwCodecProfileGUIDCount; ++i ) {
		string s;
		printf("\t[%0u]\t", i);
		desc_nv_enc_profile_names.guid2string( m_stCodecProfileGUIDArray[i], s);
		//PrintGUID( m_stCodecProfileGUIDArray[i] );
		printf("%0s\n", s.c_str() );
	}

	//printf("CNvEncoder::PrintEncodePresets() show supported encoding presets \n");
	for(unsigned i = 0; i < m_dwCodecPresetGUIDCount; ++i ) {
		string s;
		printf("\t[%0u]\t", i);
		desc_nv_enc_preset_names.guid2string( m_stCodecPresetGUIDArray[i], s);
		//PrintGUID( m_stCodecPreseteGUIDArray[i] );
		printf("%0s\n", s.c_str() );
	}
*/
	s = os.str();
}

uint32_t NVENC_Calculate_H264_MaxKBitRate( 
	const NV_ENC_LEVEL level, 
	const enum_NV_ENC_H264_PROFILE profile
)
{
	if ( level == NV_ENC_LEVEL_AUTOSELECT )
		return 0; // unconstrained

	// workaround -
	// -----------
	// NVENC SDK 2.0 Beta, Geforce 314.14 driver
	//    For some reason, the NVENC's high-profile bitrate-constraints appear 
	//    to follow the values for baseline/main profile.
	// Note, this is still true as of NVENC SDK 3 (Geforce 334.89 WHQL driver)
	//
	// Updated for NVENC SDK 5.0 (Dec 2014)


	if ( (profile >= NV_ENC_H264_PROFILE_BASELINE) &&
	    (profile < NV_ENC_H264_PROFILE_HIGH) ) 
		switch( level ) {
			case NV_ENC_LEVEL_H264_1  : return 64;
			case NV_ENC_LEVEL_H264_1b : return 128;
			case NV_ENC_LEVEL_H264_11 : return 192;
			case NV_ENC_LEVEL_H264_12 : return 384;
			case NV_ENC_LEVEL_H264_13 : return 768;
			case NV_ENC_LEVEL_H264_2  : return 2000;
			case NV_ENC_LEVEL_H264_21 : return 4000;
			case NV_ENC_LEVEL_H264_22 : return 4000;
			case NV_ENC_LEVEL_H264_3  : return 10000;
			case NV_ENC_LEVEL_H264_31 : return 14000;
			case NV_ENC_LEVEL_H264_32 : return 20000;
			case NV_ENC_LEVEL_H264_4  : return 20000;
			case NV_ENC_LEVEL_H264_41 : return 50000;
			case NV_ENC_LEVEL_H264_42 : return 50000;
			case NV_ENC_LEVEL_H264_5  : return 135000;
			case NV_ENC_LEVEL_H264_51 : return 240000;
		    default              : return 0; // unconstrained
		}

	if ( (profile >= NV_ENC_H264_PROFILE_HIGH) &&
         (profile <= NV_ENC_H264_PROFILE_CONSTRAINED_HIGH) ) 
		switch( level ) {
			case NV_ENC_LEVEL_H264_1  : return 64;
			case NV_ENC_LEVEL_H264_1b : return 128;
			case NV_ENC_LEVEL_H264_11 : return 192;
			case NV_ENC_LEVEL_H264_12 : return 384;
			case NV_ENC_LEVEL_H264_13 : return 768;  // should be 960
			case NV_ENC_LEVEL_H264_2  : return 2000; // should be 2500
			case NV_ENC_LEVEL_H264_21 : return 4000; // should be 5000
			case NV_ENC_LEVEL_H264_22 : return 4000; // should be 5000
			case NV_ENC_LEVEL_H264_3  : return 10000;// should be 12500
			case NV_ENC_LEVEL_H264_31 : return 14000;// should be 17500
			case NV_ENC_LEVEL_H264_32 : return 20000;// should be 25000
			case NV_ENC_LEVEL_H264_4  : return 24000;// should be 25000
			case NV_ENC_LEVEL_H264_41 : return 60000;// should be 62500
			case NV_ENC_LEVEL_H264_42 : return 60000;// should be 62500
			case NV_ENC_LEVEL_H264_5  : return 160000;// should be 168750
			case NV_ENC_LEVEL_H264_51 : return 280000;// should be 300000
		    default              : return 0; // unconstrained
		}

	return 0; // unconstrained
}

uint32_t NVENC_Calculate_HEVC_MaxKBitRate(
	const NV_ENC_LEVEL level,
	const NV_ENC_LEVEL tier
	)
{
	const bool main_tier = (tier == NV_ENC_TIER_HEVC_MAIN);

	switch (level) {
		case NV_ENC_LEVEL_HEVC_1:  return 128;
		case NV_ENC_LEVEL_HEVC_2:  return 1500;
		case NV_ENC_LEVEL_HEVC_21: return 3000;
		case NV_ENC_LEVEL_HEVC_3:  return 6000;
		case NV_ENC_LEVEL_HEVC_31: return 10000;
		case NV_ENC_LEVEL_HEVC_4:  return main_tier ? 
			                              12000  :  30000;
		case NV_ENC_LEVEL_HEVC_41: return main_tier ?
			                              20000  :  50000;
		case NV_ENC_LEVEL_HEVC_5:  return main_tier ?
			                              25000  : 100000;
		case NV_ENC_LEVEL_HEVC_51: return main_tier ?
			                              40000  : 160000;
		case NV_ENC_LEVEL_HEVC_52: return main_tier ?
			                              60000  : 240000;
		case NV_ENC_LEVEL_HEVC_6:  return main_tier ?
			                              60000  : 240000;
		case NV_ENC_LEVEL_HEVC_61: return main_tier ?
			                              120000 : 480000;
		case NV_ENC_LEVEL_HEVC_62: return main_tier ?
			                              240000 : 800000;
	} // switch (level)
	
	return 0; // default : unconstrained
}

NV_ENC_LEVEL
NVENC_Convert_HEVC_NV_ENC_CAPS_LEVEL(const uint32_t hevc_nv_enc_caps_level)
{
	if ( hevc_nv_enc_caps_level <= 10 )
		return NV_ENC_LEVEL_HEVC_1;
	else if (hevc_nv_enc_caps_level <= 20)
		return NV_ENC_LEVEL_HEVC_2;
	else if (hevc_nv_enc_caps_level <= 21)
		return NV_ENC_LEVEL_HEVC_21;
	else if (hevc_nv_enc_caps_level <= 30)
		return NV_ENC_LEVEL_HEVC_3;
	else if (hevc_nv_enc_caps_level <= 31)
		return NV_ENC_LEVEL_HEVC_31;
	else if (hevc_nv_enc_caps_level <= 40)
		return NV_ENC_LEVEL_HEVC_4;
	else if (hevc_nv_enc_caps_level <= 41)
		return NV_ENC_LEVEL_HEVC_41;
	else if (hevc_nv_enc_caps_level <= 50)
		return NV_ENC_LEVEL_HEVC_5;
	else if (hevc_nv_enc_caps_level <= 51)
		return NV_ENC_LEVEL_HEVC_51;
	else if (hevc_nv_enc_caps_level <= 52)
		return NV_ENC_LEVEL_HEVC_52;
	else if (hevc_nv_enc_caps_level <= 60)
		return NV_ENC_LEVEL_HEVC_6;
	else if (hevc_nv_enc_caps_level <= 61)
		return NV_ENC_LEVEL_HEVC_61;
	else 
		return NV_ENC_LEVEL_HEVC_62;
}

uint32_t
NVENC_Calculate_H264_MaxRefFrames(
//	NV_ENC_LEVEL level, 
	int level,  // NV_ENC_LEVEL (H264)
	const uint32_t pixels // total #pixels (width * height)
)
{
	uint32_t r = 1;

#define SET_R_FLOOR(x) r = (r > (x)) ? (r) : x

	// autoselect: change it to the higehst support level (5.1)
	if ( level == NV_ENC_LEVEL_AUTOSELECT )
		level = NV_ENC_LEVEL_H264_51;  // maximum
	
	if ( level >= NV_ENC_LEVEL_H264_1) {
		if ( pixels <= (128*96) ) 
			SET_R_FLOOR(8);
		else if ( pixels <= (176 * 144) ) 
			SET_R_FLOOR(4);
	}

	if ( level >= NV_ENC_LEVEL_H264_11 ) {
		if ( pixels <= (176*144) ) 
			SET_R_FLOOR(9);
		else if ( pixels <= (320 * 240) ) 
			SET_R_FLOOR(3);
		else if ( pixels <= (320 * 288) ) 
			SET_R_FLOOR(2);
	}

	if ( level >= NV_ENC_LEVEL_H264_12 ) {
		if ( pixels <= (320 * 240) ) 
			SET_R_FLOOR(7);
		else if ( pixels <= (352 * 288) ) 
			SET_R_FLOOR(6);
	}

	if ( level >= NV_ENC_LEVEL_H264_21 ) {
		if ( pixels <= (320 * 480) ) 
			SET_R_FLOOR(7);
		else if ( pixels <= (352 * 576) ) 
			SET_R_FLOOR(6);
	}

	if ( level >= NV_ENC_LEVEL_H264_22 ) {
		if ( pixels <= (320 * 480) ) 
			SET_R_FLOOR(10);
		else if ( pixels <= (352 * 576) ) 
			SET_R_FLOOR(7);
		else if ( pixels <= (720 * 480) ) 
			SET_R_FLOOR(6);
		else if ( pixels <= (720 * 576) ) 
			SET_R_FLOOR(5);
	}

	if ( level >= NV_ENC_LEVEL_H264_3 ) {
		if ( pixels <= (320 * 480) ) 
			SET_R_FLOOR(12);
		else if ( pixels <= (352 * 576) ) 
			SET_R_FLOOR(10);
	}

	if ( level >= NV_ENC_LEVEL_H264_31 ) {
		if ( pixels <= (720 * 480) ) 
			SET_R_FLOOR(13);
		else if ( pixels <= (720 * 576) ) 
			SET_R_FLOOR(11);
		else if ( pixels <= (1280 * 720) ) 
			SET_R_FLOOR(5);
	}

	if ( level >= NV_ENC_LEVEL_H264_32 ) {
		if ( pixels <= (1280 * 1024) ) 
			SET_R_FLOOR(4);
	}

	if ( level >= NV_ENC_LEVEL_H264_4 ) {
		if ( pixels <= (1280 * 720) ) 
			SET_R_FLOOR(9);
		else if ( pixels <= (1920 * 1080) ) 
			SET_R_FLOOR(4);
		else if ( pixels <= (2048 * 1024) ) 
			SET_R_FLOOR(4);
	}

	if ( level >= NV_ENC_LEVEL_H264_42 ) {
		if ( pixels <= (2048 * 1080) ) 
			SET_R_FLOOR(4);
	}

	if ( level >= NV_ENC_LEVEL_H264_5 ) {
		if ( pixels <= (1920 * 1080) ) 
			SET_R_FLOOR(13);
		else if ( pixels <= (2048 * 1080) ) 
			SET_R_FLOOR(12);
		else if ( pixels <= (2560 * 1920) ) 
			SET_R_FLOOR(5);
		else if ( pixels <= (3672 * 1536) ) 
			SET_R_FLOOR(5);
	}

	if ( level >= NV_ENC_LEVEL_H264_51 ) {
		if ( pixels <= (1920 * 1080) ) 
			SET_R_FLOOR(16);
		else if ( pixels <= (2560 * 1920) ) 
			SET_R_FLOOR(9);
		else if ( pixels <= (3840 * 2160) ) 
			SET_R_FLOOR(5);
		else if ( pixels <= (4096 * 2304) ) 
			SET_R_FLOOR(5);
	}

	// Workaround:
	// -----------
	// NVENC SDK 2.0 Beta and Geforce 314.14 drivers (02/2013)
	//   at 4k resolution, NVENC crashes if ref_frames > 2
	// Update: NVENC SDK 3.0, with Geforce 334.89 WHQL (02/2014),
	//   at 4k resolution, this restriction is gone.
//	if ( (pixels > (1920 * 1080)) && (r > 2) )
//		r = 2;

	return r;
}

bool
NVENC_legalize_codec_setting( const int GPUIndex, const csSDK_uint32 exID, ExportSettings *lRec )
{
	CNvEncoder *const enc = lRec->p_NvEncoder;
	exParamValues exParamValue_temp;
	bool codec_check_supported = false;
	NvEncodeCompressionStd codec, codec0;
	nv_enc_caps_s nv_enc_caps;

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_NV_ENC_CODEC, &exParamValue_temp);
	codec = static_cast<NvEncodeCompressionStd>(exParamValue_temp.value.intValue);

	if (GPUIndex < 0)
		return false;
		
	enc->QueryEncodeSession(GPUIndex, nv_enc_caps);

//	This function validates the user's codec-selection against GPU's reported
// capabilities.  If GPU doesn't support the user-selected codec, then we must
// manually switch the codec-choice something the GPU does support
// (assumption: NV_ENC_H264 is always supported)
//
//  This switch is necessary to preven the plugin from crashing when Adobe
// re-opens the plugin, and the preset references an codec unsupported by the
// current setup.  Or, when the user switches between multiple NVidia GPUs,
// and the target-GPU has fewer NVENC capabilities.

	for (unsigned i = 0; i < enc->m_dwEncodeGUIDCount; ++i) {
		string s;
		int    val;

		desc_nv_enc_codec_names.guid2string(enc->m_stEncodeGUIDArray[i], s);
		desc_nv_enc_codec_names.guid2value(enc->m_stEncodeGUIDArray[i], val);

		if (i == 0)  // capture the first item (i.e. the 1st supported codec)
			codec0 = static_cast<NvEncodeCompressionStd>(val); 

		if (codec == val)
			codec_check_supported = true; // found it, this GPU supports the user's codec selection
	} // for(i

	if (!codec_check_supported) {
		// the user's current choice isn't supported on this hardware.
		
		codec = codec0;// Fallback to a default safe codec 
		exParamValue_temp.value.intValue = codec0;

		lRec->exportParamSuite->ChangeParam(exID, 0, ParamID_NV_ENC_CODEC, &exParamValue_temp);
	}

	// Kludge: re-run the query to refresh enc's profile-table (m_stCodecProfileGUIDArray)
	//         This fixes a bug when user restarts a nvenc_export dialog that defaults to
	//         a Adobe-preset which chooses NV_ENC_H265.
	enc->QueryEncodeSessionCodec(GPUIndex, codec, nv_enc_caps);

	// Always update lRec (for lRec->NvEncodeConfig.codec)
	NVENC_ExportSettings_to_EncodeConfig(exID, lRec);// capture the new codec
	lRec->NvGPUInfo.nv_enc_caps = nv_enc_caps;

	return codec_check_supported;
}

prMALError exSDKGenerateDefaultParams(
	exportStdParms				*stdParms, 
	exGenerateDefaultParamRec	*generateDefaultParamRec)
{
	prMALError				result				= malNoError;
	ExportSettings			*lRec				= reinterpret_cast<ExportSettings*>(generateDefaultParamRec->privateData);
	PrSDKExportParamSuite	*exportParamSuite	= lRec->exportParamSuite;
	PrSDKExportInfoSuite	*exportInfoSuite	= lRec->exportInfoSuite;
	PrSDKTimeSuite			*timeSuite			= lRec->timeSuite;
	const csSDK_int32		exporterPluginID	= generateDefaultParamRec->exporterPluginID;
	csSDK_int32				mgroupIndex			= 0;
	PrParam					hasVideo,
							hasAudio,
							seqWidth,
							seqHeight,
							seqPARNum,
							seqPARDen,
							seqFrameRate,
							seqFieldOrder,
							seqChannelType,
							seqSampleRate;
	prUTF16Char				tempString[256];
	exNewParamInfo			Param;
	s_cpuid_info			cpuid_info;
	
	get_cpuinfo_has_ssse3( &cpuid_info );// get CPU information (do we support SSSE3?)

	if( exportInfoSuite == NULL) {
		return malUnknownError;
	}

	exportInfoSuite->GetExportSourceInfo(	exporterPluginID,
											kExportInfo_SourceHasVideo,
											&hasVideo);
	exportInfoSuite->GetExportSourceInfo(	exporterPluginID,
											kExportInfo_SourceHasAudio,
											&hasAudio);
	exportInfoSuite->GetExportSourceInfo(	exporterPluginID,
											kExportInfo_VideoWidth,
											&seqWidth);
	exportInfoSuite->GetExportSourceInfo(	exporterPluginID,
											kExportInfo_VideoHeight,
											&seqHeight);
	// copy these settings since they'll be needed later
	lRec->SDKFileRec.hasAudio = hasAudio.mBool ? kPrTrue : kPrFalse;
	lRec->SDKFileRec.hasVideo = hasVideo.mBool ? kPrTrue : kPrFalse;
	// When AME first initializes the exporter, it does so with a dummy sequence of width and height 0
	// In that case, set width and height to reasonable defaults
	// Otherwise, these 0 values will be returned to AME when exSelQueryOutputSettings is called,
	// and the preview will be turned off (1882928)
	if (seqWidth.mInt32 == 0)
	{
		seqWidth.mInt32 = 720;
	}
	if (seqHeight.mInt32 == 0)
	{
		seqHeight.mInt32 = 480;
	}
	exportInfoSuite->GetExportSourceInfo(	exporterPluginID,
											kExportInfo_VideoFrameRate,
											&seqFrameRate);
	exportInfoSuite->GetExportSourceInfo( exporterPluginID,
											kExportInfo_PixelAspectNumerator,
											&seqPARNum);
	exportInfoSuite->GetExportSourceInfo( exporterPluginID,
											kExportInfo_PixelAspectDenominator,
											&seqPARDen);
	exportInfoSuite->GetExportSourceInfo( exporterPluginID,
											kExportInfo_VideoFieldType,
											&seqFieldOrder);
	exportInfoSuite->GetExportSourceInfo( exporterPluginID,
											kExportInfo_AudioChannelsType,
											&seqChannelType);
	exportInfoSuite->GetExportSourceInfo( exporterPluginID,
											kExportInfo_AudioSampleRate,
											&seqSampleRate);

	if( exportParamSuite == NULL) {
		return malUnknownError;
	}

#define Add_NVENC_Param_Group( group_id, group_name, parent ) \
	copyConvertStringLiteralIntoUTF16( group_name, tempString), \
	exportParamSuite->AddParamGroup(	exporterPluginID, \
										mgroupIndex, \
										parent, \
										group_id, \
										tempString, \
										kPrFalse, \
										kPrFalse, \
										kPrFalse)

#define Add_NVENC_Param( _paramType, valueType, _flags, group, name, min, max, dflt, _disabled, _hidden ) \
	safeStrCpy(Param.identifier, 256, name); \
	Param.paramType = _paramType; \
	Param.flags = _flags; \
	Param.paramValues.rangeMin. ##valueType = min; \
	Param.paramValues.rangeMax. ##valueType = max; \
	Param.paramValues.value. ##valueType = dflt; \
	Param.paramValues.disabled = _disabled; \
	Param.paramValues.hidden = _hidden; \
	exportParamSuite->AddParam(	exporterPluginID, \
									mgroupIndex, \
									group, \
									&Param);

#define Add_NVENC_Param_Optional( _paramType, valueType, _flags, group, name, min, max, dflt, dflt_opt_ena, _disabled, _hidden ) \
	safeStrCpy(Param.identifier, 256, name); \
	Param.paramType = _paramType; \
	Param.flags = static_cast<exParamFlags>(exParamFlag_optional | _flags); \
	Param.paramValues.rangeMin. ##valueType = min; \
	Param.paramValues.rangeMax. ##valueType = max; \
	Param.paramValues.value. ##valueType = dflt; \
	Param.paramValues.optionalParamEnabled = dflt_opt_ena; \
	Param.paramValues.disabled = _disabled; \
	Param.paramValues.hidden = _hidden; \
	exportParamSuite->AddParam(	exporterPluginID, \
									mgroupIndex, \
									group, \
									&Param);

#define Add_NVENC_Param_int_dh_optional( group, name, min, max, dflt, dflt_opt_ena, _disabled, _hidden ) \
	Add_NVENC_Param_Optional( exParamType_int, intValue, exParamFlag_none, group, name, min, max, dflt, dflt_opt_ena, _disabled, _hidden )

#define Add_NVENC_Param_int_dh( group, name, min, max, dflt, _disabled, _hidden ) \
	Add_NVENC_Param( exParamType_int, intValue, exParamFlag_none, group, name, min, max, dflt, _disabled, _hidden )

#define Add_NVENC_Param_int( group, name, min, max, dflt ) \
	Add_NVENC_Param_int_dh( group, name, min, max, dflt, kPrFalse, kPrFalse )

#define Add_NVENC_Param_float_dh( group, name, min, max, dflt, _disabled, _hidden ) \
	Add_NVENC_Param( exParamType_float, floatValue, exParamFlag_none, group, name, min, max, dflt, _disabled, _hidden )

#define Add_NVENC_Param_float( group, name, min, max, dflt ) \
	Add_NVENC_Param_float_dh( group, name, min, max, dflt, kPrFalse, kPrFalse )

#define Add_NVENC_Param_string_dh( group, name, dflt, _flags, _disabled, _hidden ) \
	safeStrCpy(Param.identifier, 256, name); \
	Param.paramType = exParamType_string; \
	Param.flags = _flags; \
	copyConvertStringLiteralIntoUTF16( dflt, Param.paramValues.paramString ); \
	Param.paramValues.disabled = _disabled; \
	Param.paramValues.hidden = _hidden; \
	exportParamSuite->AddParam(	exporterPluginID, \
									mgroupIndex, \
									group, \
									&Param);

#define Add_NVENC_Param_bool( group, name, dflt ) \
	Add_NVENC_Param( exParamType_bool, intValue, exParamFlag_none, group, name, 0, 1, dflt, kPrFalse, kPrFalse )

#define Add_NVENC_Param_int_slider_dh( group, name, min, max, dflt, _disabled, _hidden ) \
	Add_NVENC_Param( exParamType_int, intValue, exParamFlag_slider, group, name, min, max, dflt, _disabled, _hidden )

#define Add_NVENC_Param_int_slider( group, name, min, max, dflt ) \
	Add_NVENC_Param_int_slider_dh( group, name, min, max, dflt, kPrFalse, kPrFalse )

#define Add_NVENC_Param_int_slider_optional( group, name, min, max, dflt, dflt_opt_ena ) \
	Add_NVENC_Param_Optional( exParamType_int, intValue, exParamFlag_slider, group, name, min, max, dflt, dflt_opt_ena, kPrFalse, kPrFalse )

#define Add_NVENC_Param_float_slider( group, name, min, max, dflt ) \
	Add_NVENC_Param( exParamType_float, floatValue, exParamFlag_slider, group, name, min, max, dflt, kPrFalse, kPrFalse )

#define Add_NVENC_Param_button_dh( group, name, _flags, _disabled, _hidden ) \
	safeStrCpy(Param.identifier, 256, name); \
	Param.paramType = exParamType_button; \
	Param.flags = _flags; \
	Param.paramValues.disabled = _disabled; \
	Param.paramValues.hidden = _hidden; \
	Param.paramValues.arbData = 0; \
	Param.paramValues.arbDataSize = 0; \
	exportParamSuite->AddParam(	exporterPluginID, \
									mgroupIndex, \
									group, \
									&Param)

#define Add_NVENC_Param_button( group, name, _flags ) \
	Add_NVENC_Param_button_dh( group, name, _flags, kPrFalse, kPrFalse )

	// Pre-calculate some video-sequence parameters
	PrTime ticksPerSecond;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);
	double default_video_fps = ((double)ticksPerSecond) / ((double)seqFrameRate.mInt64);

	// Restore all default-settings to NvEncodeConfig
	lRec->p_NvEncoder->initEncoderConfig( &lRec->NvEncodeConfig );

	exportParamSuite->AddMultiGroup(exporterPluginID, &mgroupIndex);

///////////////////////////////////////////////////////////////////////////////
//
// Video Tab
//
	// Even though the real parameter strings are provided during exSelPostProcessParams,
	// we still need to provide them here too, otherwise AddParamGroup won't work
	Add_NVENC_Param_Group( ADBEVideoTabGroup, GroupName_TopVideo, ADBETopParamGroup );

	///////////////////////////////////////////////////////////////////////////////
	// GPU selection group
	///////////////////////////////////////////////////////////////////////////////
	Add_NVENC_Param_Group( GroupID_GPUSelect, GroupName_GPUSelect, ADBEVideoTabGroup );

	// Parameter: GPUIndex
	//  This is the index (ID#) of the NVidia GPU.  (Default=0)
	//  If more than 1 Nvidia GPU is installed in the system,
	//  then the index can greater than 0.

	//    Scan for available NVidia GPUs,
	//    then, pick the lowest# GPU that supports NVENC capability
	std::vector<NvEncoderGPUInfo_s> gpulist;
	unsigned int numGPUs = update_exportParamSuite_GPUSelectGroup_GPUIndex(exporterPluginID, lRec, gpulist);
	unsigned int default_GPUIndex = 0;

	for(unsigned int i = 0; i < numGPUs; ++i)
		if ( gpulist[i].nvenc_supported ) {
			default_GPUIndex = i; // found an NVENC capable GPU, use this one
			break;
		}

	Add_NVENC_Param_int(GroupID_GPUSelect, ParamID_GPUSelect_GPUIndex, -1, 255, default_GPUIndex)

	// The next members of GroupID_GPUSelect are Read-Only (disabled), and
	// are intended to report hardware-information back to the user.

	// (Read-only) Report Video RAM (MB)
	Add_NVENC_Param_int( GroupID_GPUSelect, ParamID_GPUSelect_Report_VRAM, 0, MAX_POSITIVE, 0)

	// (Read-only) Report CUDA ComputeCapability (3.0, 3.5, ...)
	Add_NVENC_Param_int( GroupID_GPUSelect, ParamID_GPUSelect_Report_CCAP, 0, MAX_POSITIVE, 0)

	// (Read-only) Report NvIdia Driver and CUDA-library Version
	Add_NVENC_Param_int( GroupID_GPUSelect, ParamID_GPUSelect_Report_DV, 0, MAX_POSITIVE, 0)
	Add_NVENC_Param_int( GroupID_GPUSelect, ParamID_GPUSelect_Report_CV, 0, MAX_POSITIVE, 0)

	Add_NVENC_Param_button( GroupID_GPUSelect, ParamID_NVENC_Info_Button, exParamFlag_none );

///////////////////////////////////////////////////////////////////////////////
// NVENC Cfg Group
///////////////////////////////////////////////////////////////////////////////
	Add_NVENC_Param_Group( GroupID_NVENCCfg, GroupName_NVENCCfg, ADBEVideoTabGroup );

//    NvEncodeCompressionStd    codec;  // H.264, VC-1, MPEG-2, etc.
//    unsigned int              profile;// Base Profile, Main Profile, High Profile, etc.
//    unsigned int              level;     // encode level setting (41 = bluray)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_CODEC, 0, MAX_POSITIVE, NV_ENC_CODEC_H264)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_H264_PROFILE, 0, MAX_POSITIVE, NV_ENC_H264_PROFILE_HIGH)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_HEVC_PROFILE, 0, MAX_POSITIVE, NV_ENC_HEVC_PROFILE_MAIN)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_PRESET, 0, MAX_POSITIVE, NV_ENC_PRESET_BD)

	const bool res1080p_or_less = (seqWidth.mInt32 * seqHeight.mInt32) <= (1920*1080);
	const bool res1080p_or_less2 = (seqWidth.mInt32 * seqHeight.mInt32) <= (1920 * 1080);
	const bool res2160p_or_less2 = (seqWidth.mInt32 * seqHeight.mInt32) <= (4096 * 2160);

	unsigned dflt_nv_enc_level_h264, dflt_nv_enc_level_hevc;
	if ( (seqWidth.mInt32 * seqHeight.mInt32) <= (720*576) )
		dflt_nv_enc_level_h264 = NV_ENC_LEVEL_H264_31;
	else if ((seqWidth.mInt32 * seqHeight.mInt32) <= (1280*720) )
		dflt_nv_enc_level_h264 = NV_ENC_LEVEL_H264_4;
	else if (res1080p_or_less && (default_video_fps <= 30))
		dflt_nv_enc_level_h264 = NV_ENC_LEVEL_H264_41;
	else if (res1080p_or_less )
		dflt_nv_enc_level_h264 = NV_ENC_LEVEL_H264_42;
	else 
		dflt_nv_enc_level_h264 = NV_ENC_LEVEL_H264_51;

	if ((seqWidth.mInt32 * seqHeight.mInt32) <= (960 * 540))
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_3;
	else if ((seqWidth.mInt32 * seqHeight.mInt32) <= (1280 * 720))
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_31;
	else if (res1080p_or_less2 && (default_video_fps <= 30))
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_4;
	else if (res1080p_or_less2)
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_41;
	else if (res2160p_or_less2 && (default_video_fps <= 30))
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_5;
	else if (res2160p_or_less2 && (default_video_fps <= 60))
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_51;
	else if (res2160p_or_less2)
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_52;
	else
		dflt_nv_enc_level_hevc = NV_ENC_LEVEL_HEVC_6;

	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_LEVEL_H264, 0, MAX_POSITIVE, dflt_nv_enc_level_h264)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_LEVEL_HEVC, 0, MAX_POSITIVE, dflt_nv_enc_level_hevc)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_TIER_HEVC, 0, MAX_POSITIVE, NV_ENC_TIER_HEVC_HIGH)

//    unsigned int              chromaFormatIDC;// chroma format
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_chromaFormatIDC, 0, MAX_POSITIVE, cudaVideoChromaFormat_420)

	//    int                       preset;
//    unsigned int              application; // 0=default, 1= HP, 2= HQ, 3=VC, 4=WIDI, 5=Wigig, 6=FlipCamera, 7=BD, 8=IPOD
		
	//Add_NVENC_Param_int( GroupID_NVENCCfg, "application", 0, MAX_POSITIVE, 0)

//    int                       stereo3dMode;
//    int                       stereo3dEnable;
	//Add_NVENC_Param_bool( GroupID_NVENCCfg, "stereo3dMode", false )
	//Add_NVENC_Param_bool( GroupID_NVENCCfg, "stereo3dEnable", false )

//    unsigned int              width;  // video-resolution: X
//    unsigned int              height; // video-resolution: Y
//    unsigned int              frameRateNum;// fps (Numerator  )
//    unsigned int              frameRateDen;// fps (Denominator)
//    unsigned int              darRatioX;   // Display Aspect Ratio: X
//    unsigned int              darRatioY;   // Display Aspect Ratio: Y
//    unsigned int              avgBitRate;  // Average Bitrate (for CBR, VBR rateControls)
//    unsigned int              peakBitRate; // Peak BitRate (for VBR rateControl)

//    unsigned int              gopLength;   // (#frames) per Group-of-Pictures
//    unsigned int              idr_period;// I-frame don't re-use period (should be same as gopLength?)
	// set the Default Group-Of-Picture length to the video-FPS,
	//    this guarantees a minimum of 1 reference-frame per second (and is Bluray compliant?)
	unsigned dflt_gopLength = static_cast<unsigned>(default_video_fps); 
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_gopLength, 1, MAX_POSITIVE, dflt_gopLength)

	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_monoChromeEncoding, 0)

// hmm, Adobe Bug?  Can't initialize the OptionalparamEnabled field to a non-false value!
	//Add_NVENC_Param_int_dh_optional( GroupID_NVENCCfg, ParamID_gopLength, 1, MAX_POSITIVE, dflt_gopLength, kPrTrue, kPrFalse, kPrFalse )

	//Add_NVENC_Param_int( GroupID_NVENCCfg, "idr_period", 1, MAX_POSITIVE, 23) // TODO: CNvEncoder ignores this value

//    unsigned int              max_ref_frames; // maximum #reference frames (2 required for B-frames)
//    unsigned int              numBFrames;  // Max# B-frames
//    unsigned int              FieldEncoding;// Field-encoding mode (field/Progressive/MBAFF)

	// workaround: at higher than >1080p resolution, limit #refframes to 2.
	//             otherwise, use allow up to 4 (as permitted by the H264 specification)
	unsigned dflt_max_ref_frames = (!res1080p_or_less) ? 2 :
		NVENC_Calculate_H264_MaxRefFrames( NV_ENC_LEVEL_H264_41, seqWidth.mInt32 * seqHeight.mInt32 );
	if ( dflt_max_ref_frames > 2 )
		dflt_max_ref_frames = 2; // (Mar 2013) NVENC 2.0 motion-estmation never uses more than 2 ref-frames,
	                             // so there's no advantange in selecting >2 ref_frames.
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_max_ref_frames, 1, MAX_POSITIVE, dflt_max_ref_frames)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_numBFrames, 0, MAX_POSITIVE, 1)
	unsigned dflt_FieldEncoding = ( seqFieldOrder.mInt32 == prFieldsNone ) ?
		NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME :
		NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD;
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_FieldEncoding, 0, MAX_POSITIVE, dflt_FieldEncoding)

//    unsigned int              rateControl; // 0= QP, 1= CBR. 2= VBR
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_rateControl, 0, MAX_POSITIVE, NV_ENC_PARAMS_RC_CONSTQP )

//    unsigned int              qpPrimeYZeroTransformBypassFlag;// (for lossless encoding: set to 1, else set 0)
	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_qpPrimeYZeroTransformBypassFlag, false)

//    unsigned int              qpI; // Quality:  I-frame (for QP rateControl)
//    unsigned int              qpP; // Quality: P-frame (...)
//    unsigned int              qpB; // Quality: B-frame
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_qpI, 0, CNvEncoder::MAX_QP, 20)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_qpP, 0, CNvEncoder::MAX_QP, 20)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_qpB, 0, CNvEncoder::MAX_QP, 20)

//    unsigned int              min_qpI; // min_QP Quality: I-frame
//    unsigned int              min_qpP; // min_QP Quality: P-frame (...)
//    unsigned int              min_qpB; // min_QP Quality: B-frame
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_min_qpI, 0, CNvEncoder::MAX_QP, 0)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_min_qpP, 0, CNvEncoder::MAX_QP, 0)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_min_qpB, 0, CNvEncoder::MAX_QP, 0)

//    unsigned int              max_qpI; // max_QP Quality: I-frame
//    unsigned int              max_qpP; // max_QP Quality: P-frame (...)
//    unsigned int              max_qpB; // max_QP Quality: B-frame
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_max_qpI, 0, CNvEncoder::MAX_QP, CNvEncoder::MAX_QP)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_max_qpP, 0, CNvEncoder::MAX_QP, CNvEncoder::MAX_QP)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_max_qpB, 0, CNvEncoder::MAX_QP, CNvEncoder::MAX_QP)

//    unsigned int              initial_qpI; // initial_QP Quality: I-frame
//    unsigned int              initial_qpP; // initial_QP Quality: P-frame (...)
//    unsigned int              initial_qpB; // initial_QP Quality: B-frame
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_initial_qpI, 0, CNvEncoder::MAX_QP, 20)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_initial_qpP, 0, CNvEncoder::MAX_QP, 20)
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_initial_qpB, 0, CNvEncoder::MAX_QP, 20)
	
//    unsigned int              vbvBufferSize;
//    unsigned int              vbvInitialDelay;
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_vbvBufferSize, 0, MAX_POSITIVE, 0)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_vbvInitialDelay, 0, MAX_POSITIVE, 0)

//    NV_ENC_H264_FMO_MODE      enableFMO;   // flexible macroblock ordering (Baseline profile)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_H264_FMO, 0, MAX_POSITIVE, NV_ENC_H264_FMO_AUTOSELECT)

//    FILE                     *fOutput; // file output pointer
//    int                       hierarchicalP;// enable hierarchial P
//    int                       hierarchicalB;// enable hierarchial B

	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_hierarchicalP, false)
	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_hierarchicalB, false)

//    int                       svcTemporal; //
//    unsigned int              numlayers;
//    int                       outBandSPSPPS;
//    unsigned int              viewId;  // (stereo/MVC only) view ID
//    unsigned int              numEncFrames;// #frames to encode (not used)
//    unsigned int              sliceMode; // always use 3 (divide a picture into <sliceModeData> slices)
//    int                       sliceModeData;
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_sliceMode, 0, 3, 3)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_sliceModeData, 0, MAX_POSITIVE, 1)

//    unsigned int              vle_entropy_mode;// (high-profile only) enable CABAC
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_vle_entropy_mode, 0, MAX_POSITIVE, NV_ENC_H264_ENTROPY_CODING_MODE_CABAC)

//    unsigned int              separate_color_plane;// encode pictures as 3 independent color planes?
	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_separateColourPlaneFlag, false)

//    unsigned int              output_sei_BufferPeriod;
//    NV_ENC_MV_PRECISION       mvPrecision; // 1=FULL_PEL, 2=HALF_PEL, 3= QUARTER_PEL
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_MV_PRECISION, 0, MAX_POSITIVE, NV_ENC_MV_PRECISION_QUARTER_PEL)

//    int                       output_sei_PictureTime;
//
//    unsigned int              aud_enable;
//    unsigned int              report_slice_offsets;
//    unsigned int              enableSubFrameWrite;
//    unsigned int              disableDeblock;
	Add_NVENC_Param_int_slider( GroupID_NVENCCfg, ParamID_disableDeblock, 0, 2, 0)

//    unsigned int              disable_ptd;

//    NV_ENC_H264_ADAPTIVE_TRANSFORM_MODE adaptive_transform_mode;
//	NV_ENC_H264_BDIRECT_MODE  bdirectMode;// 0=auto, 1=disable, 2 = temporal, 3=spatial
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM, 0, MAX_POSITIVE, NV_ENC_H264_ADAPTIVE_TRANSFORM_AUTOSELECT)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_H264_BDIRECT_MODE, 0, MAX_POSITIVE, NV_ENC_H264_BDIRECT_MODE_AUTOSELECT)

//	NV_ENC_HEVC_CUSIZE        minCUsize; // (HEVC only) minimum coding unit size
//	NV_ENC_HEVC_CUSIZE        maxCUsize; // (HEVC only) maximum coding unit size
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_HEVC_MINCUSIZE, 0, MAX_POSITIVE, NV_ENC_HEVC_CUSIZE_AUTOSELECT)
	Add_NVENC_Param_int( GroupID_NVENCCfg, ParamID_NV_ENC_HEVC_MAXCUSIZE, 0, MAX_POSITIVE, NV_ENC_HEVC_CUSIZE_AUTOSELECT)

//    NV_ENC_PREPROC_FLAGS      preprocess;
//    unsigned int              maxWidth;
//    unsigned int              maxHeight;
//    unsigned int              curWidth;
//    unsigned int              curHeight;
//    int                       syncMode; // 1==async mode, 0==sync mode
	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_syncMode, true)
//    NvEncodeInterfaceType     interfaceType;// select iftype: DirectX9, DirectX10, DirectX11, or CUDA
//    int                       disableCodecCfg;
//    unsigned int              useMappedResources;// enable 

	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_enableVFR, false)

	Add_NVENC_Param_bool( GroupID_NVENCCfg, ParamID_enableAQ, false)

///////////////////////////////////////////////////////////////////////////////
// Codec Param Group
///////////////////////////////////////////////////////////////////////////////
	Add_NVENC_Param_Group( ADBEVideoCodecGroup, GroupName_VideoCodec, ADBEVideoTabGroup );

	// Codec
	// this param was leftover from the SDK sample code, don't need it anymore
	//Add_NVENC_Param_int( ADBEVideoCodecGroup, ADBEVideoCodec, 0, 0, SDK_8_BIT_RGB)

	// Button: 'codec info' 
	Add_NVENC_Param_button( ADBEVideoCodecGroup, ADBEVideoCodecPrefsButton, exParamFlag_none );

	// Basic Video Param Group
	Add_NVENC_Param_Group( ADBEBasicVideoGroup, GroupName_BasicVideo, ADBEVideoTabGroup );

	// Broadcast standard (NTSC, PAL, SECAM) 
	//   (default: PAL"1" if fps==25 or 50, NTSC"0" otherwise)
	bool guess_pal = (default_video_fps > 24.9) && (default_video_fps < 25.1) ||
		(default_video_fps > 49.9) && (default_video_fps < 50.1);
	Add_NVENC_Param_int( ADBEBasicVideoGroup, ADBEMPEGCodecBroadcastStandard, 0, 3, guess_pal ? 1 : 0)

	// Width
	Add_NVENC_Param_int( ADBEBasicVideoGroup, ADBEVideoWidth, 16, 4096, seqWidth.mInt32)

	// Height
	Add_NVENC_Param_int( ADBEBasicVideoGroup, ADBEVideoHeight, 16, 4096, seqHeight.mInt32)

	// Pixel aspect ratio
	exNewParamInfo PARParam;
	safeStrCpy(PARParam.identifier, 256, ADBEVideoAspect);
	PARParam.paramType = exParamType_ratio;
	PARParam.flags = exParamFlag_none;
	PARParam.paramValues.rangeMin.ratioValue.numerator = 10;
	PARParam.paramValues.rangeMin.ratioValue.denominator = 11;
	PARParam.paramValues.rangeMax.ratioValue.numerator = 2;
	PARParam.paramValues.rangeMax.ratioValue.denominator = 1;
	PARParam.paramValues.value.ratioValue.numerator = seqPARNum.mInt32;
	PARParam.paramValues.value.ratioValue.denominator = seqPARDen.mInt32;
	PARParam.paramValues.disabled = kPrFalse;
	PARParam.paramValues.hidden = kPrFalse;
	exportParamSuite->AddParam(	exporterPluginID,
									mgroupIndex,
									ADBEBasicVideoGroup,
									&PARParam);
	// Frame rate
	exNewParamInfo frameRateParam;
	safeStrCpy(frameRateParam.identifier, 256, ADBEVideoFPS);
	frameRateParam.paramType = exParamType_ticksFrameRate;
	frameRateParam.flags = exParamFlag_none;
	frameRateParam.paramValues.rangeMin.timeValue = 1;
	timeSuite->GetTicksPerSecond (&frameRateParam.paramValues.rangeMax.timeValue);
	frameRateParam.paramValues.value.timeValue = seqFrameRate.mInt64;
	frameRateParam.paramValues.disabled = kPrFalse;
	frameRateParam.paramValues.hidden = kPrFalse;
	exportParamSuite->AddParam(	exporterPluginID,
									mgroupIndex,
									ADBEBasicVideoGroup,
									&frameRateParam);
	// Field order
	Add_NVENC_Param_int( ADBEBasicVideoGroup, ADBEVideoFieldType, 0, MAX_POSITIVE, seqFieldOrder.mInt32)

	// Video Bit Rate Encoding : max bitrate
	Add_NVENC_Param_float_slider( ADBEBasicVideoGroup, ADBEVideoMaxBitrate, 1, 999, 40.0)

	// Video Bit Rate Encoding : target/average bitrate
	Add_NVENC_Param_float_slider( ADBEBasicVideoGroup, ADBEVideoTargetBitrate, 1, 999, 25.0)

	// NVENC pixelformat advertising
	lRec->forced_PixelFormat0 = false; // false == let Adobe autoselect pixel-format
	Add_NVENC_Param_int_dh_optional( ADBEBasicVideoGroup, ParamID_forced_PrPixelFormat, 0, MAX_POSITIVE,
		0, // default: index#0 which is Bt709
		kPrFalse, kPrFalse, kPrFalse )

///////////////////////////////////////////////////////////////////////////////////
//
// Audio Tab
//
	Add_NVENC_Param_Group( ADBEAudioTabGroup, GroupName_TopAudio, ADBETopParamGroup );
	Add_NVENC_Param_Group( GroupID_AudioFormat, GroupName_AudioFormat, ADBEAudioTabGroup );
	Add_NVENC_Param_Group( ADBEBasicAudioGroup, GroupName_BasicAudio, ADBEAudioTabGroup );

	////////////////
	// GroupID_AudioFormat -
	// 
	Add_NVENC_Param_int( GroupID_AudioFormat, ADBEAudioCodec, MAX_NEGATIVE, MAX_POSITIVE, ADBEAudioCodec_PCM)

	// for AAC-format
	Add_NVENC_Param_string_dh(GroupID_AudioFormat, ParamID_AudioFormat_NEROAAC_Path, Default_AudioFormat_NEROAAC_Path, exParamFlag_none, kPrTrue, kPrTrue );
//	Add_NVENC_Param_string_dh(GroupID_AudioFormat, ParamID_AudioFormat_NEROAAC_Path, Default_AudioFormat_NEROAAC_Path, exParamFlag_filePath, kPrTrue, kPrTrue );
	Add_NVENC_Param_button_dh(GroupID_AudioFormat, ParamID_AudioFormat_NEROAAC_Button, exParamFlag_none, kPrFalse, kPrTrue);

	////////////////
	// GroupID_BasicAudio -
	// 

	// Audio Sample rate
	Add_NVENC_Param_float( ADBEBasicAudioGroup, ADBEAudioRatePerSecond, 0, 999999, seqSampleRate.mFloat64)

	// Channel type

	// kludge - TODO we don't support 16-channel audio,
	//          so switch 16-channel to 5.1 audio
	if ( seqChannelType.mInt32 >= kPrAudioChannelType_16Channel )
		seqChannelType.mInt32 = kPrAudioChannelType_51;
	// kludge2 - if we don't support SSSE3, then disable kPrAudioChannelType_51
	if ( seqChannelType.mInt32 == kPrAudioChannelType_51 && !cpuid_info.bSupplementalSSE3 )
		seqChannelType.mInt32 = kPrAudioChannelType_Stereo;
	Add_NVENC_Param_int( ADBEBasicAudioGroup, ADBEAudioNumChannels, 0, MAX_POSITIVE, seqChannelType.mInt32)

	// for AAC-audio
	csSDK_int32 dflt_abitrate;
	switch( seqChannelType.mInt32 ) {
		case kPrAudioChannelType_Mono	: dflt_abitrate = 96; break;
		case kPrAudioChannelType_Stereo	: dflt_abitrate = 192; break;
		case kPrAudioChannelType_51		: dflt_abitrate = 384; break;
		case kPrAudioChannelType_16Channel : dflt_abitrate = 640; break;
		default : dflt_abitrate = 160; break;
	}
	Add_NVENC_Param_int_slider_dh( ADBEBasicAudioGroup, ADBEAudioBitrate, 0, MAX_POSITIVE, dflt_abitrate, kPrFalse, kPrTrue)

///////////////////////////////////////////////////////////////////////////////////
//
// Multiplexer Tab
//
	Add_NVENC_Param_Group( ADBEMultiplexerTabGroup, GroupName_TopMux, ADBETopParamGroup );
	Add_NVENC_Param_Group( GroupID_NVENCMultiplexer, GroupName_BasicMux, ADBEMultiplexerTabGroup );

	// Default 'MUX_MODE_NONE' = no muxing 
	Add_NVENC_Param_int( GroupID_NVENCMultiplexer, ADBEVMCMux_Type, 0, MAX_POSITIVE, MUX_MODE_NONE)

	//////////
	// TSMUXER-path (string parameter)
	
	// Hide tsmuxer and mp4box by default because default mux_type == MUX_MODE_NONE
	Add_NVENC_Param_string_dh(GroupID_NVENCMultiplexer, ParamID_BasicMux_TSMUXER_Path, Default_BasicMux_TSMUXER_Path, exParamFlag_none, kPrTrue, kPrTrue );

	// <Button> TSMUXER path selection
	// This is a workaround because exParamFlag_filePath doesn't seem to work in (CS6)
	Add_NVENC_Param_button_dh(GroupID_NVENCMultiplexer, ParamID_BasicMux_TSMUXER_Button, exParamFlag_none, kPrFalse, kPrTrue);

	//////////
	// MP4BOX-path (string parameter)
	Add_NVENC_Param_string_dh(GroupID_NVENCMultiplexer, ParamID_BasicMux_MP4BOX_Path, Default_BasicMux_MP4BOX_Path, exParamFlag_none, kPrTrue, kPrTrue );
	Add_NVENC_Param_button_dh(GroupID_NVENCMultiplexer, ParamID_BasicMux_MP4BOX_Button, exParamFlag_none, kPrFalse, kPrTrue);

	//////////
	// MKVMERGE-path (string parameter)
	Add_NVENC_Param_string_dh(GroupID_NVENCMultiplexer, ParamID_BasicMux_MKVMERGE_Path, Default_BasicMux_MKVMERGE_Path, exParamFlag_none, kPrTrue, kPrTrue);
	Add_NVENC_Param_button_dh(GroupID_NVENCMultiplexer, ParamID_BasicMux_MKVMERGE_Button, exParamFlag_none, kPrFalse, kPrTrue);

	// [TODO - ZL] Add more params: 8-bit vs 32-bit processing

	////////////
	//
	// If you make any changes to the ParamID_* definitions, then you must either
	// (1) clear Adobe's preset cache
	// (2) increment SDK_FILE_CURRENT_VERSION,
	// 
	// which forces Adobe to rebuild this plugin's preset (with the latest version
	// of the plugin.)  Failure to do so will result in missing parameters from the plugin's
	// user-interface!

	//PostProcessParams(exporterPluginID, exportParamSuite);
	exportParamSuite->SetParamsVersion(exporterPluginID, SDK_FILE_CURRENT_VERSION);

	return result;
}

#include<deque>
#include<string>
#include<vector>
#include <cuda.h>
#include <nvEncodeAPI.h>                // the NVENC common API header
#include <include/helper_cuda_drvapi.h> // helper functions for CUDA driver API
#include <include/helper_string.h>      // helper functions for string parsing
#include <include/helper_timer.h>       // helper functions for timing
#define log_printf(...)

using namespace std;

//
// NE_GetGPUList() : scan system for installed NVidia GPUs, return them in a list
//     Argument: vector<> gpulist       List of detected GPUs (with their properties)
//
//     Return value: # detected NVidia GPUs
//
unsigned
NE_GetGPUList( std::vector<NvEncoderGPUInfo_s> &gpulist )
{
	// Initialization code that checks the GPU encoders available and fills a table
    CUresult cuResult = CUDA_SUCCESS;
    CUdevice cuDevice = 0;
	CUcontext cuContext = 0;

    char gpu_name[100];
	int  major, minor;     // CUDA capability (major), CUDA capability (minor)
    int  deviceCount = 0;  // Number of detected NVidia GPUs
    int  NVENC_devices = 0;// Number of NVENC-capable NVidia GPUs

	gpulist.clear();
    log_printf("\n");

    // CUDA interfaces
    cuResult = cuInit(0);
    if (cuResult != CUDA_SUCCESS) {
        log_printf(">> GetNumberEncoders() - cuInit() failed error:0x%x\n", cuResult);
//      exit(EXIT_FAILURE);
    }
	else {
	    checkCudaErrors(cuDeviceGetCount(&deviceCount));
	}

    if (deviceCount == 0) {
		// Special-case: NO NVidia devices were detected

		//   Since we MUST return something, create a list of *1* phony-entry,
		//   then we'll mark the entry as invalid.
		gpulist.resize(1);

		memset( &gpulist[0], 0, sizeof(gpulist[0]));  // clear gpulist[0]
		snprintf( gpulist[0].gpu_name , sizeof(gpulist[0].gpu_name), "<No NVIDIA GPUs detected>" );
		gpulist[0].device = -1; // mark this entry as invalid
    } else {
		// *1* or more NVidia GPU(s) is present.  Scan and return all of them
		gpulist.resize(deviceCount);

        log_printf(">> GetNumberEncoders() has detected %d CUDA capable GPU device(s) <<\n", deviceCount);
        for (int i=0; i < deviceCount; i++) {
			int temp_int;
			size_t vram_total_bytes, vram_free_bytes; // GPU memory (bytes)
			memset( &gpulist[i], 0, sizeof(gpulist[i])); // clear gpulist[i]
            checkCudaErrors(cuDeviceGet(&cuDevice, i));
            checkCudaErrors(cuDeviceGetName(gpu_name, sizeof(gpu_name), cuDevice));
			checkCudaErrors(cuCtxCreate(&cuContext, 0, cuDevice));

			checkCudaErrors(cuDeviceTotalMem( &vram_total_bytes, cuDevice));
			checkCudaErrors(cuMemGetInfo(&vram_free_bytes, &vram_total_bytes));
			checkCudaErrors(cuCtxDestroy(cuContext));

			gpulist[i].vram_total = vram_total_bytes >> 20; // Convert bytes -> MegaBytes
			gpulist[i].vram_free  = vram_free_bytes >> 20;  // Convert bytes -> MegaBytes

		    checkCudaErrors(cuDeviceGetAttribute( &temp_int, CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE, cuDevice));
			gpulist[i].vram_clock = temp_int;

		    checkCudaErrors(cuDeviceGetAttribute( &temp_int, CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH, cuDevice));
			gpulist[i].vram_width = temp_int;

			strcpy( gpulist[i].gpu_name, gpu_name );

            checkCudaErrors(cuDeviceComputeCapability(&major, &minor, i));
			checkCudaErrors(cuDriverGetVersion( &gpulist[i].driver_version ) );

			// nvenc_supported: 
			//   (1) GPU must possess Compute Capability 3.0 or higher
			//   (2) CUDA API version must be 5.5 (5050) or higher
			gpulist[i].nvenc_supported = ((((major << 4) + minor) >= 0x30) && (gpulist[i].driver_version >= 5050)) ?
				true : false;
			log_printf("  [ GPU #%d - < %s > has Compute SM %d.%d, NVENC %s ]\n", 
                            i, gpu_name, major, minor, 
                            gpulist[i].nvenc_supported ? "Available" : "Not Available");

			gpulist[i].device     = i;   // NVidia device# (index)
			gpulist[i].cuda_major = major;
			gpulist[i].cuda_minor = minor;

//            if (((major << 4) + minor) >= 0x30)
//            {
//                encoderInfo[NVENC_devices].device = i;
//                strcpy(encoderInfo[NVENC_devices].gpu_name, gpu_name);
                NVENC_devices++;
//            }
        }
    }
    return deviceCount;
}

unsigned
update_exportParamSuite_GPUSelectGroup_GPUIndex(
	const csSDK_uint32 exID,
	const ExportSettings *lRec,
	std::vector<NvEncoderGPUInfo_s> &gpulist
)
{
	exOneParamValueRec		tempGPUIndex;// GPU index
	prUTF16Char				tempString[256];

	//
	//  GPUIndex : NVidia GPU selector (user-adjustable)
	//
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ParamID_GPUSelect_GPUIndex);
	csSDK_int32 numGPUs = NE_GetGPUList( gpulist );
	for (csSDK_int32 i = 0; i < gpulist.size(); i++)
	{
		tempGPUIndex.intValue = gpulist[i].device;

		// convert <char gpu_name[]> to <wchar_t[], and then to tempString[]>
		std::wostringstream oss; // output stream for wide-text conversion
		if ( numGPUs )
			oss << "(#" << std::dec << i << ") "; // Print Index#: "(x)"
		oss << gpulist[i].gpu_name;
		copyConvertStringLiteralIntoUTF16(oss.str().c_str(), tempString );

		lRec->exportParamSuite->AddConstrainedValuePair(
			exID,
			0,
			ParamID_GPUSelect_GPUIndex,
			&tempGPUIndex,
			tempString);
	}

	return numGPUs;
}

void update_exportParamSuite_GPUSelectGroup(
	const csSDK_uint32 exID,
	const ExportSettings *lRec,
	const int GPUIndex,   // GPU-ID#, '-1' means NO NVidia GPUs found
	const NvEncoderGPUInfo_s &gpuinfo )
{
	prMALError		result	= malNoError;

	exOneParamValueRec		vr; // temporary var 
	prUTF16Char				tempString[256];

	exParamValues intValue; // exParamValues template: generic HIDDEN integer

	intValue.rangeMin.intValue = 0;
	intValue.rangeMax.intValue = 1 << 31;
	intValue.value.intValue = 0;
	intValue.disabled = kPrFalse;

	//
	// Note, all of the '_Report_*' parameters are READ-ONLY, and simply provide
	// information for the user.  
	//
	
	// Special note: If no NVidia GPUs are detected (GPUIndex<0), then 
	//               *hide* the parameters.
	//               Otherwise, unhide them.

	intValue.hidden = (GPUIndex<0) ? kPrTrue : kPrFalse;
	

	//
	//  GPU VRAM: report the selected GPU's video RAM size
	//
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ParamID_GPUSelect_Report_VRAM);
	
	{
		// convert 'int' into wchar_t[] text-string, then to tempString[]>
		std::wostringstream oss; // output stream for wide-text conversion
		oss << std::dec << gpuinfo.vram_free;
		oss << " free (" << std::dec << gpuinfo.vram_total;
		oss << " total), " << std::dec << gpuinfo.vram_width;
		// convert vram_clock (KHz) into DDR-MHz ( * 2 / 1000)
		oss << "bits @ " << std::dec << (gpuinfo.vram_clock * 2 / 1000) << "MHz";
		copyConvertStringLiteralIntoUTF16(oss.str().c_str(), tempString );

		vr.intValue = 0;
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ParamID_GPUSelect_Report_VRAM,
															&vr,
															tempString);
		// update this parameter's flag 'Hidden'
		lRec->exportParamSuite->ChangeParam( 
			exID,
			0,
			ParamID_GPUSelect_Report_VRAM,
			&intValue
		);
	}

	//
	// GPU Compute Capability level
	//
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_GPUSelect_Report_CCAP);
	{
		// convert <major>.<minor> into a wchar_t[] text-string, then to tempString[]>
		std::wostringstream oss; // output stream for wide-text conversion
		oss << std::dec << gpuinfo.cuda_major << "." << std::dec << gpuinfo.cuda_minor;
		if ( ((gpuinfo.cuda_major <<4) + gpuinfo.cuda_minor) >= 0x30) 
			oss << " (NVENC supported)";
		else
			oss << " (NVENC not supported, Compute 3.0 or higher required)";

		copyConvertStringLiteralIntoUTF16(oss.str().c_str(), tempString );

		vr.intValue = 0;
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
			0,
			ParamID_GPUSelect_Report_CCAP,
			&vr,
			tempString);
		// update this parameter's flag 'Hidden'
		lRec->exportParamSuite->ChangeParam( 
			exID,
			0,
			ParamID_GPUSelect_Report_CCAP,
			&intValue
		);
	}

	//
	// GPU driver "CUDA version"
	//
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_GPUSelect_Report_CV);
	{
		// convert <major>.<minor> into a wchar_t[] text-string, then to tempString[]>
		std::wostringstream oss; // output stream for wide-text conversion
		oss << std::dec << gpuinfo.driver_version << " ";
		if ( gpuinfo.driver_version >= 5000 )
			oss << "(NVENC supported)";
		else oss << "(NVENC not supported)";

		copyConvertStringLiteralIntoUTF16(oss.str().c_str(), tempString );

		vr.intValue = 0;
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
			0,
			ParamID_GPUSelect_Report_CV,
			&vr,
			tempString);
		// update this parameter's flag 'Hidden'
		lRec->exportParamSuite->ChangeParam( 
			exID,
			0,
			ParamID_GPUSelect_Report_CV,
			&intValue
		);
	}

	//
	// GPU driver version
	//
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_GPUSelect_Report_DV);
	{
		// Get the Geforce driver-version using NVAPI -
		//   NVENC functionality is a hardware+firmware implementation, so it is important
		//   to report both the GPU-hardware and the Geforce driver revision.
		NvU32             NVidia_DriverVersion;
		NvAPI_ShortString szBuildBranchString;
		NvAPI_Status      nvs = NvAPI_SYS_GetDriverAndBranchVersion( &NVidia_DriverVersion, szBuildBranchString);


		// convert <major>.<minor> into a wchar_t[] text-string, then to tempString[]>
		std::wostringstream oss; // output stream for wide-text conversion
		if ( nvs == NVAPI_OK )
			oss << szBuildBranchString << "  (" << std::dec 
				<< static_cast<unsigned>(NVidia_DriverVersion / 100)  << "."
				<< std::dec << static_cast<unsigned>(NVidia_DriverVersion % 100) << ")";
		else
			oss << "??UNKNOWN??";

		copyConvertStringLiteralIntoUTF16(oss.str().c_str(), tempString );

		vr.intValue = 0;
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
			0,
			ParamID_GPUSelect_Report_DV,
			&vr,
			tempString);

		// update this parameter's flag 'Hidden'
		lRec->exportParamSuite->ChangeParam( 
			exID,
			0,
			ParamID_GPUSelect_Report_DV,
			&intValue
		);
	}
}


bool
update_exportParamSuite_NVENCCfgGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec)
{
	CNvEncoder *const enc = lRec->p_NvEncoder;
	PrSDKExportParamSuite	*paramSuite	= lRec->exportParamSuite;
	prUTF16Char				tempString[256];
	exParamValues			exParamValue_GPUIndex, exParamValue_temp,
							videoWidth, videoHeight;
	unsigned int			GPUIndex;
	nv_enc_caps_s           nv_enc_caps = {0};
	HRESULT                 hr = S_FALSE; // fail
	
	bool    need_to_update_videogroup = false;
	bool	nvenc_supported;
	int						val;// temporary var (for macro-use)
	exOneParamValueRec		vr; // temporary var (for macro-use)
	string					s; // temporary var  (for macro-use)

	
//	PrSDKTimeSuite			*timeSuite	= lRec->timeSuite;
//	PrTime			ticksPerSecond;
	csSDK_int32		mgroupIndex = 0;
	
	if ( paramSuite == NULL ) // TODO trap error
		return need_to_update_videogroup;

	// Get the current selection for GPUIndex
	paramSuite->GetParamValue(exID, mgroupIndex, ParamID_GPUSelect_GPUIndex, &exParamValue_GPUIndex);
	GPUIndex = exParamValue_GPUIndex.value.intValue;
	nvenc_supported = lRec->NvGPUInfo.nvenc_supported;
	if ( nvenc_supported ) {
		// Kludge: the user's codec selection may not be supported on the chosen GPU hardware.
		//         This can happen if the user saved a profile, and reloads it on a different
		//         PC (or uninstalls/disables the NVidia GPU.)

		// Validate the codec setting (and if necessary, change it to something we DO support)
		NVENC_legalize_codec_setting(GPUIndex, exID, lRec);

		NVENCSTATUS nvencstatus = enc->QueryEncodeSessionCodec( GPUIndex, lRec->NvEncodeConfig.codec, nv_enc_caps );

		// TODO: need better error trapping & reporting why QueryEncodeSession failed
		//  Currently, the return code doesn't tell why the Query attempt failed.
		//  There is a chance that the Query failed due to *TRIAL* key expiration.
		//  (As of Geforce driver 314.07, the TRIAL Key still works.)  
		if ( nvencstatus == NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY )
			NVENC_errormessage_bad_key( lRec );

		// If Query failed, then disable NVENC capability in this plugin,
		//    regardless of the actual reason (such as a programming-error on
		//    the part of this plugin.)
		if ( nvencstatus != NV_ENC_SUCCESS ) {
			lRec->NvGPUInfo.nvenc_supported = false;
			nvenc_supported = false; // don't care why it failed, can't use NVENC!
		}
	}

	// Keep a copy of the NVENC's reported EncoderCapabilities
	lRec->NvGPUInfo.nv_enc_caps = nv_enc_caps;

#define _UpdateParam_dh( name, d, h ) \
{ \
	lRec->exportParamSuite->GetParamValue(exID, 0, name, &exParamValue_temp); \
	exParamValue_temp.disabled				= d; \
	exParamValue_temp.hidden				= h; \
	lRec->exportParamSuite->ChangeParam(exID, \
										0, \
										name, \
										&exParamValue_temp); \
}

#define _UpdateParam( type, name, min, max, d, h ) \
{ \
	lRec->exportParamSuite->GetParamValue(exID, 0, name, &exParamValue_temp); \
	exParamValue_temp.rangeMin. ##type		= min; \
	exParamValue_temp.rangeMax. ##type		= max; \
	exParamValue_temp.disabled				= d; \
	exParamValue_temp.hidden				= h; \
	if ( exParamValue_temp.value. ##type > (max) ) \
		exParamValue_temp.value. ##type = (max); \
	if ( exParamValue_temp.value. ##type < (min) ) \
		exParamValue_temp.value. ##type = (min); \
	lRec->exportParamSuite->ChangeParam(exID, \
										0, \
										name, \
										&exParamValue_temp); \
}

#define _UpdateIntParam( name, min, max, d, h ) _UpdateParam( intValue, name, min, max, d, h )
#define _UpdateIntSliderParam( name, min, max, d, h ) _UpdateParam( intValue, name, min, max, d, h )
#define _UpdateFloatSliderParam( name, min, max, d, h ) _UpdateParam( floatValue, name, min, max, d, h )

#define _AddConstrainedIntValuePair(x) \
	std::wostringstream oss; \
	vr.intValue = val; \
	oss << s.c_str(); \
	copyConvertStringLiteralIntoUTF16(oss.str().c_str(), tempString ); \
	\
	lRec->exportParamSuite->AddConstrainedValuePair( exID, \
		0, \
		x, \
		&vr, \
		tempString);

	////////////////////////////

#define _ClearAndDisableParam( name ) \
	lRec->exportParamSuite->ClearConstrainedValues(	exID, \
		0, \
		name \
	), \
	lRec->exportParamSuite->GetParamValue(exID, 0, name, &exParamValue_temp), \
	exParamValue_temp.value.floatValue = 0, \
	exParamValue_temp.value.intValue = 0, \
	exParamValue_temp.value.timeValue = 0, \
	exParamValue_temp.disabled = kPrTrue, \
	lRec->exportParamSuite->ChangeParam(exID, 0, name, &exParamValue_temp)

	//
	// Here, must verify that we're currently running on NVENC-capable hardware.
	// if we aren't, then we need to disable all the control-options and QUIT.
	// (The remainder of function will crash becase *pEnc is NULL.)
	//
	
	if ( !nvenc_supported ) {
		_ClearAndDisableParam( ParamID_NV_ENC_CODEC );
		_ClearAndDisableParam( ParamID_NV_ENC_H264_PROFILE );
		_ClearAndDisableParam( ParamID_NV_ENC_HEVC_PROFILE );
		_ClearAndDisableParam( ParamID_NV_ENC_PRESET );
		_ClearAndDisableParam( ParamID_NV_ENC_LEVEL_H264 );
		_ClearAndDisableParam( ParamID_NV_ENC_LEVEL_HEVC );
		_ClearAndDisableParam( ParamID_NV_ENC_TIER_HEVC);
		_ClearAndDisableParam( ParamID_chromaFormatIDC);
		_ClearAndDisableParam( ParamID_gopLength );
		_ClearAndDisableParam( ParamID_monoChromeEncoding );
		//_ClearAndDisableParam( "idr_period" );// TODO: CNvEncoder ignores this value
		_ClearAndDisableParam( ParamID_max_ref_frames );
		_ClearAndDisableParam( ParamID_numBFrames );
		_ClearAndDisableParam( ParamID_FieldEncoding );
		_ClearAndDisableParam( ParamID_rateControl );
		_ClearAndDisableParam( ParamID_qpPrimeYZeroTransformBypassFlag );
		_ClearAndDisableParam( ParamID_vbvBufferSize );
		_ClearAndDisableParam( ParamID_vbvInitialDelay );
		_ClearAndDisableParam( ParamID_qpI );
		_ClearAndDisableParam( ParamID_qpP );
		_ClearAndDisableParam( ParamID_qpB );
		_ClearAndDisableParam( ParamID_min_qpI );
		_ClearAndDisableParam( ParamID_min_qpP );
		_ClearAndDisableParam( ParamID_min_qpB );
		_ClearAndDisableParam( ParamID_max_qpI );
		_ClearAndDisableParam( ParamID_max_qpP );
		_ClearAndDisableParam( ParamID_max_qpB );
		_ClearAndDisableParam( ParamID_initial_qpI );
		_ClearAndDisableParam( ParamID_initial_qpP );
		_ClearAndDisableParam( ParamID_initial_qpB );
		_ClearAndDisableParam( ParamID_NV_ENC_H264_FMO );
		_ClearAndDisableParam( ParamID_hierarchicalP );
		_ClearAndDisableParam( ParamID_hierarchicalB );
		_ClearAndDisableParam( ParamID_sliceMode );
		_ClearAndDisableParam( ParamID_sliceModeData );
		_ClearAndDisableParam( ParamID_vle_entropy_mode );
		_ClearAndDisableParam( ParamID_separateColourPlaneFlag );
		_ClearAndDisableParam( ParamID_NV_ENC_MV_PRECISION );
		_ClearAndDisableParam( ParamID_disableDeblock );
		_ClearAndDisableParam( ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM );
		_ClearAndDisableParam( ParamID_NV_ENC_H264_BDIRECT_MODE );
		_ClearAndDisableParam( ParamID_NV_ENC_HEVC_MINCUSIZE );
		_ClearAndDisableParam( ParamID_NV_ENC_HEVC_MAXCUSIZE );
		_ClearAndDisableParam( ParamID_syncMode );
		_ClearAndDisableParam( ParamID_enableVFR );
		_ClearAndDisableParam( ParamID_enableAQ );

		return need_to_update_videogroup;
	}
	
	//////////////////////////

	//
	// ------------- After this point, NVENC-capable hardware must be present!
	//               (Otherwise the calls to pEnc will crash on NULL-pointer.)
	//

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_CODEC);

	for(unsigned i = 0; i < enc->m_dwEncodeGUIDCount; ++i ) {
		desc_nv_enc_codec_names.guid2string( enc->m_stEncodeGUIDArray[i], s);
		desc_nv_enc_codec_names.guid2value( enc->m_stEncodeGUIDArray[i], val);

		_AddConstrainedIntValuePair(ParamID_NV_ENC_CODEC)
	}

	if ( enc->m_dwEncodeGUIDCount == 0 ) {
		s = "ERROR, NO SUPPORT";
		val = 0;
		_AddConstrainedIntValuePair(ParamID_NV_ENC_CODEC)
	}

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_NV_ENC_CODEC, &exParamValue_temp);
	const NvEncodeCompressionStd codec = static_cast<NvEncodeCompressionStd>(exParamValue_temp.value.intValue);
	const bool codec_is_h264 = (codec == NV_ENC_H264);
	const bool codec_is_hevc = (codec == NV_ENC_H265);

	////////////////////////////
	// constraint due to Profile:
	enum_NV_ENC_H264_PROFILE profileValue;

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_NV_ENC_H264_PROFILE, &exParamValue_temp);
	profileValue = static_cast<enum_NV_ENC_H264_PROFILE>(exParamValue_temp.value.intValue);

	// kludge: if 'autoselect' is picked, then assume we are operating at HIGH profile.
	if ( profileValue == NV_ENC_CODEC_PROFILE_AUTOSELECT )
		profileValue = NV_ENC_H264_PROFILE_HIGH;

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_H264_PROFILE);

	if (codec_is_h264) {
		for (unsigned i = 0; i < enc->m_dwCodecProfileGUIDCount; ++i) {
			desc_nv_enc_profile_names.guid2string(enc->m_stCodecProfileGUIDArray[i], s);
			desc_nv_enc_profile_names.guid2value(enc->m_stCodecProfileGUIDArray[i], val);

			// If HIGH_444 is not supported, then manually disable the HIGH_444 profile 
			if (s == "NV_ENC_H264_PROFILE_HIGH_444_GUID" && !nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_YUV444_ENCODE)
				continue;

			// kludge: if nv_enc_caps reports MVC not supported, then manually disable the STEREO profile 
			if (s == "NV_ENC_H264_PROFILE_STEREO_GUID") // && !nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_STEREO_MVC )
				continue; // always disable STEREO_GUID (nvenc_export doesn't support MVC)

			_AddConstrainedIntValuePair(ParamID_NV_ENC_H264_PROFILE)
		}
	} // codec_is_h264

	lRec->exportParamSuite->ClearConstrainedValues(exID,
		0,
		ParamID_NV_ENC_HEVC_PROFILE);

	if (codec_is_hevc) {
		for (unsigned i = 0; i < enc->m_dwCodecProfileGUIDCount; ++i) {
			desc_nv_enc_profile_names.guid2string(enc->m_stCodecProfileGUIDArray[i], s);
			desc_nv_enc_profile_names.guid2value(enc->m_stCodecProfileGUIDArray[i], val);

			_AddConstrainedIntValuePair(ParamID_NV_ENC_HEVC_PROFILE)
		}
	} // codec_is_hevc

	if ( enc->m_dwCodecProfileGUIDCount == 0 ) {
		s = "ERROR, NO SUPPORT";
		val = 0;
		{ _AddConstrainedIntValuePair(ParamID_NV_ENC_H264_PROFILE) }
		{ _AddConstrainedIntValuePair(ParamID_NV_ENC_HEVC_PROFILE) }
	}

	_UpdateIntParam(ParamID_NV_ENC_H264_PROFILE, 0, MAX_POSITIVE,
		kPrFalse, codec_is_h264 ? kPrFalse : kPrTrue
		);
	_UpdateIntParam(ParamID_NV_ENC_HEVC_PROFILE, 0, MAX_POSITIVE,
		kPrFalse, codec_is_hevc ? kPrFalse : kPrTrue
		);

	////////////////////////////
	// constraint due to Preset:
	exParamValues presetValue;

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_NV_ENC_PRESET, &presetValue);
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_PRESET);

	//HRESULT hr_getpresetconfig = enc->GetPresetConfig(0);
	for(unsigned i = 0; i < enc->m_dwCodecPresetGUIDCount; ++i ) {
		desc_nv_enc_preset_names.guid2string( enc->m_stCodecPresetGUIDArray[i], s);
		desc_nv_enc_preset_names.guid2value( enc->m_stCodecPresetGUIDArray[i], val);

		if ( val == NV_ENC_PRESET_LOSSLESS_DEFAULT ||
			 val == NV_ENC_PRESET_LOSSLESS_HP )
		{
			// NVENC API 4 : lossless presets are only support in HIGH_444 profile
			if (profileValue < NV_ENC_H264_PROFILE_HIGH_444)
				continue; // don't include lossless profiles

			// NVENC API 4 : lossless presets requires hardware capability
			if ( nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE == 0 )
				continue; // don't include lossless profiles
		}

		_AddConstrainedIntValuePair(ParamID_NV_ENC_PRESET)
	}

	if ( enc->m_dwCodecPresetGUIDCount == 0 ) {
		s = "ERROR, NO SUPPORT";
		val = 0;
		_AddConstrainedIntValuePair(ParamID_NV_ENC_PRESET)
	}
	else if ( codec_is_h264 ) {
		// NVENC 4 API:
		// Lossless PRESET is only supported in the HIGH444 profile.
		// If HIGH profile isn't selected, downgrade preset
		val = presetValue.value.intValue;
		if ( (profileValue < NV_ENC_H264_PROFILE_HIGH_444) &&
			(val == NV_ENC_PRESET_LOSSLESS_DEFAULT ||
			 val == NV_ENC_PRESET_LOSSLESS_HP) )
		{
			presetValue.value.intValue = NV_ENC_PRESET_BD;
			lRec->exportParamSuite->ChangeParam(exID, 0, ParamID_NV_ENC_PRESET, &presetValue);
		}
	} // codec_is_h264

	const bool presetValue_is_lossless =
		(presetValue.value.intValue == NV_ENC_PRESET_LOSSLESS_DEFAULT) ||
		(presetValue.value.intValue == NV_ENC_PRESET_LOSSLESS_HP);

	const bool presetValue_is_lowlatency =
		(presetValue.value.intValue == NV_ENC_PRESET_LOW_LATENCY_DEFAULT ) ||
		(presetValue.value.intValue == NV_ENC_PRESET_LOW_LATENCY_HP) ||
		(presetValue.value.intValue == NV_ENC_PRESET_LOW_LATENCY_HQ);

	////////////////////////////
	
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_LEVEL_H264);

	if (codec_is_h264) {
		for (unsigned i = 0; i < desc_nv_enc_level_h264_names.Size(); ++i) {
			desc_nv_enc_level_h264_names.index2string(i, s);
			desc_nv_enc_level_h264_names.index2value(i, val);

			// If this particular level isn't supported by NVENC hardware, then skip it
			if (val > nv_enc_caps.value_NV_ENC_CAPS_LEVEL_MAX)
				continue; // not supported, skip it

			if ((val < nv_enc_caps.value_NV_ENC_CAPS_LEVEL_MIN) &&
				val != NV_ENC_LEVEL_AUTOSELECT)
				continue; // not supported, skip it

			_AddConstrainedIntValuePair(ParamID_NV_ENC_LEVEL_H264)
		}
	} // codec_is_h264

	////////////////////////////
	lRec->exportParamSuite->ClearConstrainedValues(exID,
		0,
		ParamID_NV_ENC_LEVEL_HEVC);

	lRec->exportParamSuite->ClearConstrainedValues(exID,
		0,
		ParamID_NV_ENC_TIER_HEVC);

	if (codec_is_hevc) {

		for (unsigned i = 0; i < desc_nv_enc_level_hevc_names.Size(); ++i) {
			desc_nv_enc_level_hevc_names.index2string(i, s);
			desc_nv_enc_level_hevc_names.index2value(i, val);

			// If this particular level isn't supported by NVENC hardware, then skip it
			if (val > NVENC_Convert_HEVC_NV_ENC_CAPS_LEVEL(nv_enc_caps.value_NV_ENC_CAPS_LEVEL_MAX))
				continue; // not supported, skip it

			if ((val < NVENC_Convert_HEVC_NV_ENC_CAPS_LEVEL(nv_enc_caps.value_NV_ENC_CAPS_LEVEL_MIN)) &&
				val != NV_ENC_LEVEL_AUTOSELECT)
				continue; // not supported, skip it

			_AddConstrainedIntValuePair(ParamID_NV_ENC_LEVEL_HEVC)
		}

	////////////////////////////

		for (unsigned i = 0; i < desc_nv_enc_tier_hevc_names.Size(); ++i) {
			desc_nv_enc_tier_hevc_names.index2string(i, s);
			desc_nv_enc_tier_hevc_names.index2value(i, val);

			_AddConstrainedIntValuePair(ParamID_NV_ENC_TIER_HEVC)
		}
	} // if (codec_is_hevc)

	//
	// disable LEVEL/TIER as necessary
	//
	_UpdateIntParam(ParamID_NV_ENC_LEVEL_H264, 0, MAX_POSITIVE,
		kPrFalse, codec_is_h264 ? kPrFalse : kPrTrue
		);
	_UpdateIntParam(ParamID_NV_ENC_LEVEL_HEVC, 0, MAX_POSITIVE,
		kPrFalse, codec_is_hevc ? kPrFalse : kPrTrue
		);
	_UpdateIntParam(ParamID_NV_ENC_TIER_HEVC, 0, MAX_POSITIVE,
		kPrFalse, codec_is_hevc ? kPrFalse : kPrTrue
		);

	////////////////////////////

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_chromaFormatIDC);

	lRec->exportParamSuite->GetParamValue(	exID,
		0,
		ParamID_chromaFormatIDC,
		&exParamValue_temp);
	if ( codec_is_h264 ) {
		// validate the chromaformat: if necessary, force it back to NV12

		cudaVideoChromaFormat chromafmt = static_cast<cudaVideoChromaFormat>(exParamValue_temp.value.intValue);
		if ((chromafmt != cudaVideoChromaFormat_420) && ((profileValue < NV_ENC_H264_PROFILE_HIGH_444) ||
			!nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_YUV444_ENCODE) )
		{
			exParamValue_temp.value.intValue = enc->m_pAvailableSurfaceFmts[0];
			lRec->exportParamSuite->ChangeParam(exID,
				0,
				ParamID_chromaFormatIDC,
				&exParamValue_temp);
		}
	} // codec_is_h264

	// If hardware or selected-profile doesn't support separate_color_planes,
	//   disable the button
	const bool disable_scp = (!codec_is_h264) || 
		!nv_enc_caps.value_NV_ENC_CAPS_SEPARATE_COLOUR_PLANE ||
		(profileValue < NV_ENC_H264_PROFILE_HIGH_444);

	_UpdateIntParam( ParamID_separateColourPlaneFlag, 0, disable_scp ? 0 : 1, 
		disable_scp ? kPrTrue : kPrFalse,  kPrFalse
	);

	////////////////////////////


	// Set the slider ranges, while preserving the current value
	_UpdateIntSliderParam( ParamID_gopLength, 1, 999, kPrFalse, kPrFalse );
	//_UpdateIntSliderParam( "idr_period", 1, 999, kPrFalse, kPrFalse ); // TODO: CNvEncoder ignores this value

	// If NVENC hardware doesn't support it, disable the monoChromeEncoding
	_UpdateIntParam( ParamID_monoChromeEncoding, 0, 1, 
		nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_MONOCHROME ? kPrFalse :kPrTrue,
		kPrFalse
	);

	// max_ref_frames:
	// --------------
	// Set the upper-limit based on the current frame resolution:
	//   (1) up to 1080p resolution, 4 refframes (to stay inside Level 4.1)
	//   (2) larger than 1080p resolution, 2 refframes (plugin crashes otherwise) TODO investigate!
	lRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &videoWidth);
	lRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &videoHeight);
	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_NV_ENC_LEVEL_H264, &exParamValue_temp);
	int limit_max_ref_frames;
	if (codec_is_h264)
		limit_max_ref_frames = NVENC_Calculate_H264_MaxRefFrames(
			exParamValue_temp.value.intValue,
			videoWidth.value.intValue * videoHeight.value.intValue);
	else
		limit_max_ref_frames = 6; // for HEVC, just use the baseline value of 6

	_UpdateIntSliderParam( ParamID_max_ref_frames, 1, limit_max_ref_frames , kPrFalse, kPrFalse );

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_max_ref_frames, &exParamValue_temp);
	int max_ref_frames = exParamValue_temp.value.intValue;
	
	bool bframes_disabled = (nv_enc_caps.value_NV_ENC_CAPS_NUM_MAX_BFRAMES == 0) ||
		(profileValue == NV_ENC_H264_PROFILE_BASELINE) ||
		presetValue_is_lossless ||
		( max_ref_frames < 2);

	_UpdateIntSliderParam( ParamID_numBFrames, 0, 
		bframes_disabled ? 0 : nv_enc_caps.value_NV_ENC_CAPS_NUM_MAX_BFRAMES,
		bframes_disabled ? kPrTrue : kPrFalse, // disable: if no BFrames allowed
		kPrFalse
	);

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_numBFrames, &exParamValue_temp);
	int numBFrames = bframes_disabled ? 0 : exParamValue_temp.value.intValue;

		////////////////////////////

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_FieldEncoding);

	for(unsigned i = 0; i < desc_nv_enc_params_frame_mode_names.Size(); ++i ) {
		desc_nv_enc_params_frame_mode_names.index2string(i, s);
		desc_nv_enc_params_frame_mode_names.index2value(i, val);
		if ( val != NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME )
		{
			if ( !nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_FIELD_ENCODING )
				continue; // hardware doesn't support it, so don't add this option
			if ( profileValue == NV_ENC_H264_PROFILE_BASELINE )
				continue; // interlaced/MBAFF not available on base-profile
		}

		// MBAFF is only supported if support-field encoding == 2
		if ( val == NV_ENC_PARAMS_FRAME_FIELD_MODE_MBAFF &&
			nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_FIELD_ENCODING < 2)
			continue;
		_AddConstrainedIntValuePair(ParamID_FieldEncoding)
	}

	// In BASELINE profile, disable the control (because only FRAME-mode is allowed)
	_UpdateIntParam( ParamID_FieldEncoding, 0, MAX_POSITIVE, 
		(profileValue == NV_ENC_H264_PROFILE_BASELINE) ? kPrTrue : kPrFalse,
		kPrFalse
	);
		////////////////////////////
	
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_rateControl);
	for(unsigned i = 0; i < desc_nv_enc_ratecontrol_names.Size(); ++i ) {
		desc_nv_enc_ratecontrol_names.index2string(i, s);
		desc_nv_enc_ratecontrol_names.index2value(i, val);

		// NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES is a bitmask of supported
		// rate-control modes.  Check if hardware supports this mode
		if ( (val != NV_ENC_PARAMS_RC_CONSTQP) && // constqp is *ALWAYS* supported
			!(nv_enc_caps.value_NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES & val) )
			continue; // this rc-mode isn't supported, skip it

		// NVENC 3.0 API kludge -- skip the low-latency '2_PASS' (CBR) modes
		if ( val == NV_ENC_PARAMS_RC_2_PASS_QUALITY || val == NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP)
			if ( !presetValue_is_lowlatency )
				continue; // skip it

		_AddConstrainedIntValuePair(ParamID_rateControl)
	}
	
	// Fixup: If user selected a LOSSLESS preset, 
	//        then force rateControl to CONSTQP.
	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_rateControl, &exParamValue_temp);
	if ( presetValue_is_lossless ) {
		exParamValue_temp.value.intValue = NV_ENC_PARAMS_RC_CONSTQP;
		lRec->exportParamSuite->ChangeParam(
			exID, 0, ParamID_rateControl, &exParamValue_temp
		);
	}

	_UpdateParam_dh( ParamID_rateControl,
		presetValue_is_lossless ? kPrTrue : kPrFalse, // Lossless: don't let user change setting
		kPrFalse
	);
	
	NV_ENC_PARAMS_RC_MODE rateControl = static_cast<NV_ENC_PARAMS_RC_MODE>(exParamValue_temp.value.intValue);

	////////////////////////////

	// qpPrimeYZeroTransformBypassFlag - lossless encoding:
	// -----------------------------
	// For HIGH_PROFILE and lower (not HIGH_444), clear the box and disable this setting
	// For HIGH_444_PROFILE:
	//     if presetValue is LOSSLESS, then set the box and disable this setting
	//      otherwise, enable the box (i.e. let use select the value)
	if ( (codec_is_h264 && (profileValue < NV_ENC_H264_PROFILE_HIGH_444)) ||
		codec_is_hevc )
	{
		// selected profile is not HIGH_444_PREDICTIVE,
		//    lossless mode is not supported here.  So disable the control
		//    and force it to 0 (false)
		// hide this control in hevc mode
		_UpdateIntParam( ParamID_qpPrimeYZeroTransformBypassFlag, 0, 0, 
			kPrTrue, 
			codec_is_h264 ? kPrFalse : kPrTrue
		); 
	}
	else {
		// selected profile is HIGH_444_PREDICTIVE
		// lossless mode is supported.
		// Control's e
		if ( presetValue_is_lossless ) {
			// Disable the control, and force it to 1 (true)
			_UpdateIntParam( ParamID_qpPrimeYZeroTransformBypassFlag, 1, 1, kPrTrue, kPrFalse ); 
		}
		else {
			// Enable the control: let user turn on/off this option
			_UpdateIntParam( ParamID_qpPrimeYZeroTransformBypassFlag, 0, 1, kPrFalse, kPrFalse ); 
		}
	}

	////////////////////////////

	// VBV bufferSize - DEFAULT == 0 (auto)
	//    hidden if nv_enc_caps doesn't support adjustable VBV
	prBool hide_vbv = nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE ? kPrFalse : kPrTrue;
	_UpdateIntSliderParam( ParamID_vbvBufferSize, 0, 999, hide_vbv, kPrFalse );

	// VBV initialDelay - DEFAULT == 0 (auto)
	//    hidden if nv_enc_caps doesn't support adjustable VBV
	_UpdateIntSliderParam( ParamID_vbvInitialDelay, 0, 999, hide_vbv, kPrFalse );

	////////////////////////////

	// The ConstQP parameters qpI/qpP/qpB are *only* used in ConstQP rate-control mode:
	//    hide them if plugin is not set to ConstQP.
	exParamValues exParamValue_temp1, exParamValue_temp2;

	#define _ceiling_exParamValue_int( ParamID_target, ParamID_ref ) \
	{ \
		lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_target, &exParamValue_temp1); \
		lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_ref, &exParamValue_temp2); \
		if ( exParamValue_temp1.value.intValue > exParamValue_temp2.value.intValue ) { \
			exParamValue_temp1.value.intValue = exParamValue_temp2.value.intValue; \
			lRec->exportParamSuite->ChangeParam(exID, mgroupIndex, ParamID_target, &exParamValue_temp1); \
		} \
	}

	#define _floor_exParamValue_int( ParamID_target, ParamID_ref ) \
	{ \
		lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_target, &exParamValue_temp1); \
		lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_ref, &exParamValue_temp2); \
		if ( exParamValue_temp1.value.intValue < exParamValue_temp2.value.intValue ) { \
			exParamValue_temp1.value.intValue = exParamValue_temp2.value.intValue; \
			lRec->exportParamSuite->ChangeParam(exID, mgroupIndex, ParamID_target, &exParamValue_temp1); \
		} \
	}

	prBool qp_hidden = (rateControl == NV_ENC_PARAMS_RC_CONSTQP) ?
		kPrFalse : kPrTrue;

	prBool qpb_hidden = ((qp_hidden == kPrTrue) || bframes_disabled || (numBFrames==0)) ?
		kPrTrue : kPrFalse;

	// If user selected 'LOSSLESS' preset, then restrict qp-Sliders to 0.
	prBool qp_disabled = presetValue_is_lossless ? kPrTrue : kPrFalse;
	int    _qp_mm      = presetValue_is_lossless ? 0 : CNvEncoder::MAX_QP;

	_UpdateIntSliderParam( ParamID_qpI, 0, _qp_mm, qp_disabled, qp_hidden  );
	_UpdateIntSliderParam( ParamID_qpP, 0, _qp_mm, qp_disabled, qp_hidden );
	_UpdateIntSliderParam( ParamID_qpB, 0, _qp_mm, qp_disabled, qpb_hidden );

	// The MinQP & MaxQP parameters are *always* enabled.  At 4k-resolution encoding, both
	// seem to affect all of NVENC's different rate-controls: if they are disabled,
	// then NVENC ignores the user-programmed rate-control parameters.
	prBool min_qp_hidden = kPrFalse;

	prBool min_qpb_hidden = ((min_qp_hidden == kPrTrue) || bframes_disabled || (numBFrames==0)) ?
		kPrTrue : kPrFalse;

	_qp_mm = min_qp_hidden ? 0 : CNvEncoder::MAX_QP;

	// clamp the minQP parameters so that they can never be set GREATER than QP or maxQP.
	if ( rateControl == NV_ENC_PARAMS_RC_CONSTQP ) {
		_ceiling_exParamValue_int( ParamID_min_qpI, ParamID_qpI )
		_ceiling_exParamValue_int( ParamID_min_qpP, ParamID_qpP )
		_ceiling_exParamValue_int( ParamID_min_qpB, ParamID_qpB )
	}
	else {
		_ceiling_exParamValue_int(ParamID_min_qpI, ParamID_max_qpI)
		_ceiling_exParamValue_int(ParamID_min_qpP, ParamID_max_qpP)
		_ceiling_exParamValue_int(ParamID_min_qpB, ParamID_max_qpB)
	}

	_UpdateIntSliderParam( ParamID_min_qpI, 0, _qp_mm, qp_disabled, min_qp_hidden  );
	_UpdateIntSliderParam( ParamID_min_qpP, 0, _qp_mm, qp_disabled, min_qp_hidden  );
	_UpdateIntSliderParam( ParamID_min_qpB, 0, _qp_mm, qp_disabled, min_qpb_hidden );

	// Hide the MaxQP parameters whenever the MinQP parameters are hidden.
	prBool max_qp_hidden = min_qp_hidden;
	prBool max_qpb_hidden = min_qpb_hidden;
	_qp_mm = max_qp_hidden ? CNvEncoder::MAX_QP : 0;

	// clamp the MaxQP parameters so that they can never be set to LESS than minQP or QP.
	if ( rateControl == NV_ENC_PARAMS_RC_CONSTQP ) {
		_floor_exParamValue_int( ParamID_max_qpI, ParamID_qpI )
		_floor_exParamValue_int( ParamID_max_qpP, ParamID_qpP )
		_floor_exParamValue_int( ParamID_max_qpB, ParamID_qpB )
	}
	else {
		_floor_exParamValue_int(ParamID_max_qpI, ParamID_min_qpI)
		_floor_exParamValue_int(ParamID_max_qpP, ParamID_min_qpP)
		_floor_exParamValue_int(ParamID_max_qpB, ParamID_min_qpB)
	}

	_UpdateIntSliderParam( ParamID_max_qpI, _qp_mm, CNvEncoder::MAX_QP, kPrFalse, max_qp_hidden );
	_UpdateIntSliderParam( ParamID_max_qpP, _qp_mm, CNvEncoder::MAX_QP, kPrFalse, max_qp_hidden );
	_UpdateIntSliderParam( ParamID_max_qpB, _qp_mm, CNvEncoder::MAX_QP, kPrFalse, max_qpb_hidden );

	// The InitialQP parameters are *only* used when?!?
	//    hide them if plugin is not set to VBR_MinQP.
	// TODO: when are the initial-QP params used?
	_UpdateIntSliderParam( ParamID_initial_qpI, 0, CNvEncoder::MAX_QP, kPrFalse, kPrTrue ); // TODO
	_UpdateIntSliderParam( ParamID_initial_qpP, 0, CNvEncoder::MAX_QP, kPrFalse, kPrTrue );
	_UpdateIntSliderParam( ParamID_initial_qpB, 0, CNvEncoder::MAX_QP, kPrFalse, kPrTrue );

	////////////////////////////

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_H264_FMO);

	if (codec_is_h264) {
		for (unsigned i = 0; i < desc_nv_enc_h264_fmo_names.Size(); ++i) {
			desc_nv_enc_h264_fmo_names.index2string(i, s);
			desc_nv_enc_h264_fmo_names.index2value(i, val);
			if (val != NV_ENC_H264_FMO_DISABLE)
			{
				if (!nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_FMO)
					continue; // hardware doesn't support it, so don't add this option
			}
			_AddConstrainedIntValuePair(ParamID_NV_ENC_H264_FMO)
		}
	} // if (codec_is_h264)

	// (H264 only) FMO is only available on BASELINE profile, so disable it for all other PROFILEs
	_UpdateIntParam( ParamID_NV_ENC_H264_FMO, 0, MAX_POSITIVE, 
		codec_is_h264 && (profileValue != NV_ENC_H264_PROFILE_BASELINE) ? kPrTrue : kPrFalse,
		codec_is_h264 ? kPrFalse : kPrTrue
	);

	////////////////////////////

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_hierarchicalP, &exParamValue_temp),
	exParamValue_temp.disabled	= kPrFalse;
	if (nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES == 0) {
		// unsupported on this NVENC harware, so disable this setting and clear the checkbox
		exParamValue_temp.disabled	= kPrTrue;
		exParamValue_temp.value.intValue = 0; // uncheck the box
	}
	exParamValue_temp.hidden	= kPrFalse;
	lRec->exportParamSuite->ChangeParam(exID, \
		0, \
		ParamID_hierarchicalP, \
		&exParamValue_temp);

	////////////////////////////

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_hierarchicalB, &exParamValue_temp),
	exParamValue_temp.disabled	= kPrFalse;
	if (nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES == 0) {
		// unsupported on this NVENC harware, so disable this setting and clear the checkbox
		exParamValue_temp.disabled	= kPrTrue;
		exParamValue_temp.value.intValue = 0; // uncheck the box
	}
	exParamValue_temp.hidden	= false;
	lRec->exportParamSuite->ChangeParam(exID, \
		0, \
		ParamID_hierarchicalB, \
		&exParamValue_temp);

	////////////////////////////

	_UpdateIntSliderParam( ParamID_sliceMode, 0, 3, kPrFalse, kPrFalse );

	_UpdateIntSliderParam( ParamID_sliceModeData, 0, MAX_POSITIVE, kPrFalse, kPrFalse );

	////////////////////////////

	lRec->exportParamSuite->ClearConstrainedValues(exID,
		0, 
		ParamID_vle_entropy_mode);

	if (codec_is_h264) {
		for (unsigned i = 0; i < desc_nv_enc_h264_entropy_coding_mode_names.Size(); ++i) {
			desc_nv_enc_h264_entropy_coding_mode_names.index2string(i, s);
			desc_nv_enc_h264_entropy_coding_mode_names.index2value(i, val);
			if (val != NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC)
			{
				if (!nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_CABAC)
					continue; // hardware doesn't support it, so don't add this option
				if (profileValue <= NV_ENC_H264_PROFILE_BASELINE)
					continue; // base-profile doesn't support CABAC
			}

			_AddConstrainedIntValuePair(ParamID_vle_entropy_mode)
		}
	} // codec_is_h264

	_UpdateIntParam(ParamID_vle_entropy_mode, 0, MAX_POSITIVE,
		codec_is_h264 ? kPrFalse : kPrTrue,
		codec_is_h264 ? kPrFalse : kPrTrue
	);

	////////////////////////////

	bool already_added_420 = false;
	bool already_added_444 = false;
	for (unsigned i = 0; i < enc->m_dwInputFmtCount; ++i) {
		bool exists = desc_nv_enc_buffer_format_names.value2string(enc->m_pAvailableSurfaceFmts[i], s);
		if ( exists ) {
			val = enc->m_pAvailableSurfaceFmts[i];
			NV_ENC_BUFFER_FORMAT bufferfmt = static_cast<NV_ENC_BUFFER_FORMAT>(val);

			if ( IsYV12Format(bufferfmt) || IsNV12Format(bufferfmt) ) {
				if (already_added_420)
					continue; // kludge for HEVC: don't add multiple 420 entries
				// type-convert from <NV_ENC_BUFFER_FORMAT> into <cudaVideoChromaFormat>
				val = cudaVideoChromaFormat_420;
				already_added_420 = true;
			}

			if ( IsYUV444Format(bufferfmt) )
			{
				if ( !nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_YUV444_ENCODE )
					continue; // HiP444 requires the separate_colour_plane capability

				if ( profileValue < NV_ENC_H264_PROFILE_HIGH_444 )
					continue; // ... and the Hi444 profile must be selected

				if (already_added_444)
					continue; // kludge for HEVC: don't add multiple 420 entries

				// type-convert from <NV_ENC_BUFFER_FORMAT> into <cudaVideoChromaFormat>
				val = cudaVideoChromaFormat_444;
				already_added_444 = true;
			}

			// remap item's name from the NV_ENC_BUFFER_FORMAT -> cudaVideoChromaFormat
			desc_cudaVideoChromaFormat_names.value2string(val, s);
			_AddConstrainedIntValuePair(ParamID_chromaFormatIDC)
		}
	}

	////////////////////////////
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_MV_PRECISION);

	for(unsigned i = 0; i < desc_nv_enc_mv_precision_names.Size(); ++i ) {
		desc_nv_enc_mv_precision_names.index2string(i, s);
		desc_nv_enc_mv_precision_names.index2value(i, val);
		if ( (val == NV_ENC_MV_PRECISION_QUARTER_PEL) && !nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_QPELMV )
		{
			continue; // hardware doesn't support it, so don't add this option
		}
		_AddConstrainedIntValuePair(ParamID_NV_ENC_MV_PRECISION)
	}

	_UpdateIntParam(ParamID_NV_ENC_MV_PRECISION, 0, MAX_POSITIVE,
		codec_is_h264 ? kPrFalse : kPrTrue,
		codec_is_h264 ? kPrFalse : kPrTrue
	);

	////////////////////////////

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM);

	if (codec_is_h264) {
		for (unsigned i = 0; i < desc_nv_enc_adaptive_transform_names.Size(); ++i) {
			desc_nv_enc_adaptive_transform_names.index2string(i, s);
			desc_nv_enc_adaptive_transform_names.index2value(i, val);
			if (val != NV_ENC_H264_ADAPTIVE_TRANSFORM_DISABLE)
			{
				if (!nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM)
					continue; // hardware doesn't support it, so don't add this option
				if (profileValue <= NV_ENC_H264_PROFILE_MAIN)
					continue; // adaptive transform (4x4/8x8) not available @ base/main profile
			}

			_AddConstrainedIntValuePair(ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM)
		}
	} // codec_is_h264

	_UpdateIntParam(ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM, 0, MAX_POSITIVE,
		codec_is_h264 ? kPrFalse : kPrTrue,
		codec_is_h264 ? kPrFalse : kPrTrue
	);

	////////////////////////////
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_NV_ENC_H264_BDIRECT_MODE);

	if (codec_is_h264) {
		for (unsigned i = 0; i < desc_nv_enc_h264_bdirect_mode_names.Size(); ++i) {
			desc_nv_enc_h264_bdirect_mode_names.index2string(i, s);
			desc_nv_enc_h264_bdirect_mode_names.index2value(i, val);

			if ((val != NV_ENC_H264_BDIRECT_MODE_DISABLE) && (!nv_enc_caps.value_NV_ENC_CAPS_SUPPORT_BDIRECT_MODE))
				continue; // hardware doesn't support it, so don't add this option

			_AddConstrainedIntValuePair(ParamID_NV_ENC_H264_BDIRECT_MODE)
		}
	} // codec_is_h264

	// BDirect-mode is only active when B-frames are enabled
	_UpdateIntParam(ParamID_NV_ENC_H264_BDIRECT_MODE, 0, MAX_POSITIVE,
		codec_is_h264 ? kPrFalse : kPrTrue,
		(qpb_hidden || !codec_is_h264) ? kPrTrue : kPrFalse
	);

	////////////////////////////
	lRec->exportParamSuite->ClearConstrainedValues(exID,
		0,
		ParamID_NV_ENC_HEVC_MINCUSIZE);
	lRec->exportParamSuite->ClearConstrainedValues(exID,
		0,
		ParamID_NV_ENC_HEVC_MAXCUSIZE);

	if (codec_is_hevc) {
		for (unsigned i = 0; i < desc_nv_enc_hevc_cusize_names.Size(); ++i) {
			desc_nv_enc_hevc_cusize_names.index2string(i, s);
			desc_nv_enc_hevc_cusize_names.index2value(i, val);

			{_AddConstrainedIntValuePair(ParamID_NV_ENC_HEVC_MINCUSIZE)}
			{_AddConstrainedIntValuePair(ParamID_NV_ENC_HEVC_MAXCUSIZE)}
		}
	}

	// BDirect-mode is only active when B-frames are enabled
	_UpdateIntParam(ParamID_NV_ENC_HEVC_MINCUSIZE, 0, MAX_POSITIVE,
		kPrFalse,
		codec_is_hevc ? kPrFalse : kPrTrue
	);
	_UpdateIntParam(ParamID_NV_ENC_HEVC_MAXCUSIZE, 0, MAX_POSITIVE,
		kPrFalse,
		codec_is_hevc ? kPrFalse : kPrTrue
	);

	////////////////////////////

	// don't allow syncMode to be adjusted
	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_syncMode, &exParamValue_temp),
	//exParamValue_temp.disabled	= (nv_enc_caps.value_NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT == 0);
	exParamValue_temp.disabled	= kPrTrue;
	lRec->exportParamSuite->ChangeParam(exID,
										0,
										ParamID_syncMode,
										&exParamValue_temp);
	/*
	//NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_CODEC, LParamID_NV_ENC_CODEC, L"Compression standard (H.264, VC-1, MPEG-2, etc.)"  );
	CREATE_NV_ENC_PARAM_DESCRIPTOR( nv_enc_stereo_packing_mode_names )
	*/
	_UpdateIntParam( ParamID_enableVFR, 0, 1, kPrFalse, kPrFalse );

	prBool aq_disable = (rateControl == NV_ENC_PARAMS_RC_CONSTQP) ?
		kPrTrue : kPrFalse;

	_UpdateIntParam(ParamID_enableAQ, 0, aq_disable ? 0 : 1, aq_disable, kPrFalse);

	_UpdateIntParam( ADBEVideoWidth, 1, nv_enc_caps.value_NV_ENC_CAPS_WIDTH_MAX, kPrFalse, kPrFalse );
	_UpdateIntParam( ADBEVideoHeight, 1, nv_enc_caps.value_NV_ENC_CAPS_HEIGHT_MAX, kPrFalse, kPrFalse );

	return need_to_update_videogroup;
}

void
update_exportParamSuite_VideoGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec)
{
	csSDK_int32				mgroupIndex		= 0;
	int						val;// temporary var (for macro-use)
	exOneParamValueRec		vr; // temporary var (for macro-use)
	string					s; // temporary var  (for macro-use)


	const csSDK_int32		fieldOrders[]		= {
		prFieldsNone,
		prFieldsUpperFirst,
		prFieldsLowerFirst
	};
	const wchar_t* const	fieldOrderStrings[]	= {
		L"None (Progressive)",
		L"Upper First",
		L"Lower First"
	};
	const nv_enc_caps_s * const nv_enc_caps = &(lRec->NvGPUInfo.nv_enc_caps);
	const bool codec_is_h264 = (lRec->NvEncodeConfig.codec == NV_ENC_H264);
	const bool codec_is_hevc = (lRec->NvEncodeConfig.codec == NV_ENC_H265);

	exParamValues exParamValue_temp;
	exOneParamValueRec fieldValue; 
	prUTF16Char tempString[256];


	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													mgroupIndex,
													ADBEVideoFieldType);
	for (csSDK_int32 i = 0; i < 3; i++)
	{
		fieldValue.intValue = fieldOrders[i];
		copyConvertStringLiteralIntoUTF16(fieldOrderStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															mgroupIndex,
															ADBEVideoFieldType,
															&fieldValue,
															tempString);
	}


	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_rateControl, &exParamValue_temp);
	NV_ENC_PARAMS_RC_MODE rateControl = static_cast<NV_ENC_PARAMS_RC_MODE>(exParamValue_temp.value.intValue);

	// Target/Average bitrate is used for all rate-control modes except ConstantQP
	prBool tgtbitrate_hidden = (rateControl == NV_ENC_PARAMS_RC_CONSTQP) ?
		kPrTrue : kPrFalse;

	// Max bitrate is used for all rate-control modes except Constant bitrate and Constant-QP
	prBool maxbitrate_hidden = (rateControl == NV_ENC_PARAMS_RC_CONSTQP || 
		rateControl == NV_ENC_PARAMS_RC_CBR || 
		rateControl == NV_ENC_PARAMS_RC_2_PASS_QUALITY ||         // low-latency CBR
		rateControl == NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP ) ?  // low-latency CBR
		kPrTrue : kPrFalse;
	//
	// Set the slider ranges, while preserving the current value
	//
	exParamValues levelValue, profileValue, tierValue;
	exParamValues maxValue, targetValue;

	if (codec_is_h264)  {
		lRec->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_NV_ENC_LEVEL_H264, &levelValue);
		lRec->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_NV_ENC_H264_PROFILE, &profileValue);
	}
	else { // HEVC
		lRec->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_NV_ENC_LEVEL_HEVC, &levelValue);
		lRec->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_NV_ENC_TIER_HEVC, &tierValue);
	}

	double max_bitrate;
	if ( codec_is_h264 ) 
		max_bitrate = NVENC_Calculate_H264_MaxKBitRate(
			static_cast<NV_ENC_LEVEL>(levelValue.value.intValue),
			static_cast<enum_NV_ENC_H264_PROFILE>(profileValue.value.intValue)
		);
	else
		max_bitrate = NVENC_Calculate_HEVC_MaxKBitRate(
			static_cast<NV_ENC_LEVEL>(levelValue.value.intValue),
			static_cast<NV_ENC_LEVEL>(tierValue.value.intValue)
		);

	max_bitrate /= 1000.0; // convert Kilobps into Megabps

	// check for unconstrained bitrate
	if ( codec_is_h264 && (max_bitrate == 0))
		max_bitrate = 240.0; // unconstrained bitrate: cap it @ 240 Mbps

	_UpdateFloatSliderParam( ADBEVideoMaxBitrate, 1, max_bitrate, kPrFalse, maxbitrate_hidden );

	// target/average bitrate:
	// if user decreased max-bitrate lower than the target-bitrate, then decrease the target-bitrate.
	lRec->exportParamSuite->GetParamValue(exID, mgroupIndex, ADBEVideoMaxBitrate, &maxValue);
	lRec->exportParamSuite->GetParamValue(exID, mgroupIndex, ADBEVideoTargetBitrate, &targetValue);
	if ( (maxbitrate_hidden == kPrFalse) && (targetValue.value.floatValue > maxValue.value.floatValue) ) {
		targetValue.value.floatValue = maxValue.value.floatValue;
		lRec->exportParamSuite->ChangeParam(exID, mgroupIndex, ADBEVideoTargetBitrate, &targetValue);
	}

	_UpdateFloatSliderParam( ADBEVideoTargetBitrate, 1, max_bitrate, kPrFalse, tgtbitrate_hidden );

	//_UpdateIntParam( "autoselect_PrPixelFormat", 0, MAX_POSITIVE, kPrFalse, kPrFalse )

	lRec->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_chromaFormatIDC, &exParamValue_temp);
	const cudaVideoChromaFormat chromaFormatIDC = static_cast<cudaVideoChromaFormat>(
		exParamValue_temp.value.intValue
	); // get the user-setting for framebuffer color-format

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													mgroupIndex,
													ParamID_forced_PrPixelFormat);
	for (unsigned i = 0; i < desc_PrPixelFormat.Size(); i++)
	{
		const bool user_420 = (chromaFormatIDC == cudaVideoChromaFormat_420);
		const bool user_444 = (chromaFormatIDC == cudaVideoChromaFormat_444);

		int pp; // tempvar
		val = i;
		desc_PrPixelFormat.index2string(i, s);
		desc_PrPixelFormat.index2value(i, pp);

		// If user has chosen YUV444, then only list YUV444 pixelFormats
		if ( user_444 && !PrPixelFormat_is_YUV444( static_cast<PrPixelFormat>(pp) ) )
			continue;

		// If user has chosen YUV420, then only list YUV420 and YUV422 pixelFormats
		if ( user_420 && !(PrPixelFormat_is_YUV420( static_cast<PrPixelFormat>(pp)) ||
			PrPixelFormat_is_YUV422( static_cast<PrPixelFormat>(pp))) )
			continue;
		_AddConstrainedIntValuePair(ParamID_forced_PrPixelFormat)
	}
//		exParamValue_temp.value.intValue ? kPrTrue : kPrFalse, // if "Autoselect" is ON, then disable this control
	_UpdateIntSliderParam( ParamID_forced_PrPixelFormat, 0, MAX_POSITIVE, kPrFalse, kPrFalse );
}

void
update_exportParamSuite_NVENCMultiplexerGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec)
{
	CNvEncoder *const enc = lRec->p_NvEncoder;
	PrSDKExportParamSuite	*paramSuite	= lRec->exportParamSuite;
	exParamValues			exParamValue_muxType, exParamValue_temp;

	csSDK_int32		mgroupIndex = 0;
	
	if ( paramSuite == NULL ) // TODO trap error
		return;

	// Get the current MuxType & Button
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVMCMux_Type, &exParamValue_muxType);


	// TSMUXER MuxPath & button parameters:
	// ------------------------------------
	//  Dynamically hide/unhide (they are only visible when muxtype selection == TS)
	prBool hidden = (exParamValue_muxType.value.intValue == MUX_MODE_M2T) ?
		kPrFalse : kPrTrue;

	_UpdateParam_dh(ParamID_BasicMux_TSMUXER_Path, kPrTrue, hidden );
	_UpdateParam_dh(ParamID_BasicMux_TSMUXER_Button, kPrFalse, hidden );

	// MP4BOX MuxPath & button parameters:
	// ------------------------------------
	//  Dynamically hide/unhide (they are only visible when muxtype selection == TS)
	hidden = (exParamValue_muxType.value.intValue == MUX_MODE_MP4) ?
		kPrFalse : kPrTrue;

	_UpdateParam_dh(ParamID_BasicMux_MP4BOX_Path, kPrTrue, hidden );
	_UpdateParam_dh(ParamID_BasicMux_MP4BOX_Button, kPrFalse, hidden );

	// MKVMERGE MuxPath & button parameters:
	// ------------------------------------
	//  Dynamically hide/unhide (they are only visible when muxtype selection == TS)
	hidden = (exParamValue_muxType.value.intValue == MUX_MODE_MKV) ?
	kPrFalse : kPrTrue;

	_UpdateParam_dh(ParamID_BasicMux_MKVMERGE_Path, kPrTrue, hidden);
	_UpdateParam_dh(ParamID_BasicMux_MKVMERGE_Button, kPrFalse, hidden);
}

void
update_exportParamSuite_AudioFormatGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec)
{
	PrSDKExportParamSuite	*paramSuite	= lRec->exportParamSuite;
	exParamValues			exParamValue_temp;
	int32_t					audioCodec;
	
	csSDK_int32		mgroupIndex = 0;
	
	if ( paramSuite == NULL ) // TODO trap error
		return;

	paramSuite->GetParamValue(exID, mgroupIndex, ADBEVMCMux_Type, &exParamValue_temp);
	bool is_mp4 = (exParamValue_temp.value.intValue == MUX_MODE_MP4);

	// AudioCodec setting - 
	// (1) if (Multiplexer-type == MP4), then force the value to 'AAC' ,
	//     (MP4BOX doesn't support muxing WAV files into *.MP4)
	// (2)  ... and disable the control (so user can't change it.)

	if ( is_mp4 ) {
		// disable the parameter and change-value to AAC
		_UpdateIntParam( ADBEAudioCodec, ADBEAudioCodec_AAC, ADBEAudioCodec_AAC, kPrTrue, kPrFalse );
	}
	else {
		// re-enable the parameter
		_UpdateParam_dh( ADBEAudioCodec, kPrFalse, kPrFalse );
	}

	paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioCodec, &exParamValue_temp);
	audioCodec = exParamValue_temp.value.intValue;
	
	// neroAacEnc MuxPath & button parameters:
	// ------------------------------------
	//  Dynamically hide/unhide (they are only visible when audioCodec selection == AAC)
	prBool aac_hidden = (audioCodec == ADBEAudioCodec_AAC) ? kPrFalse : kPrTrue;

	_UpdateParam_dh(ParamID_AudioFormat_NEROAAC_Path, kPrTrue, aac_hidden );
	_UpdateParam_dh(ParamID_AudioFormat_NEROAAC_Button, kPrFalse, aac_hidden );
}

void
update_exportParamSuite_BasicAudioGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec)
{
	PrSDKExportParamSuite	*paramSuite	= lRec->exportParamSuite;
	exParamValues			exParamValue_temp;
	int32_t					audioCodec;
	int32_t					audioChannels;
	csSDK_int32				new_min, new_max;
	csSDK_int32		mgroupIndex = 0;
	
	if ( paramSuite == NULL ) // TODO trap error
		return;

	// Get the current AudioCodec & ChannelCount setting
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioCodec, &exParamValue_temp);
	audioCodec = exParamValue_temp.value.intValue;
	paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioNumChannels, &exParamValue_temp);
	audioChannels = exParamValue_temp.value.intValue;

	// AudioBitrate:
	// ------------------------------------
	//	(1) Dynamically hide/unhide (only visible when audioCodec selection == AAC)
	//	(2) Adjust bitrate min/max based on audio-channel type
	prBool aac_hidden = (audioCodec == ADBEAudioCodec_AAC) ? 
		kPrFalse :  // AAC : audio bitrate selection enabled
		kPrTrue;    // PCM : audio bitrate selection disabled

	if (audioChannels == kPrAudioChannelType_Mono) {
		new_min = 16;
		new_max = 160;
	}
	else if (audioChannels == kPrAudioChannelType_Stereo) {
		new_min = 16;
		new_max = 320;
	}
	else { // 5.1-surround and up
		new_min = 192;
		new_max = 640;
	}

	_UpdateIntSliderParam(ADBEAudioBitrate, new_min, new_max, kPrFalse, aac_hidden );
}

/*
void
update_exportParamSuite_NVENCCfgGroup_ratecontrol_names(
	const nv_enc_caps_s nv_enc_caps,
	const csSDK_uint32 exID,
	ExportSettings *lRec
)
{
	int						val;// temporary var (for macro-use)
	exOneParamValueRec		vr; // temporary var (for macro-use)
	string					s; // temporary var  (for macro-use)

	exParamValues           paramValue;

	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_NV_ENC_PRESET, &paramValue);

	bool preset_is_lossless =   // did user select a lossless preset?
		(paramValue.value.intValue == NV_ENC_PRESET_LOSSLESS_DEFAULT) ||
		(paramValue.value.intValue == NV_ENC_PRESET_LOSSLESS_HP);

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
		0,
		ParamID_rateControl);

	for(unsigned i = 0; i < desc_nv_enc_ratecontrol_names.Size(); ++i ) {
		desc_nv_enc_ratecontrol_names.index2string(i, s);
		desc_nv_enc_ratecontrol_names.index2value(i, val);

		// NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES is a bitmask of supported
		// rate-control modes.  Check if hardware supports this mode
		if ( (val != NV_ENC_PARAMS_RC_CONSTQP) && // constqp is *ALWAYS* supported
			!(nv_enc_caps.value_NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES & val) )
			continue; // this rc-mode isn't supported, skip it

		// NVENC 3.0 API kludge -- skip the low-latency '2_PASS' (CBR) modes
		if ( val == NV_ENC_PARAMS_RC_2_PASS_QUALITY || val == NV_ENC_PARAMS_RC_2_PASS_FRAMESIZE_CAP)
			continue; // skip it

		// NVENC 4.0 API kludge -- the 'LOSSLESS' presets only support ConstQP (TODO)
		if ( preset_is_lossless && (val != NV_ENC_PARAMS_RC_CONSTQP))
			//continue; // skip it

		_AddConstrainedIntValuePair(ParamID_rateControl)
	}
}*/

const wchar_t * const	frameRateStrings[]	= {
	L"10",
	L"15",
	L"23.976 (NTSC)",
	L"24",
	L"25 (PAL)",
	L"29.97 (NTSC)",
	L"30",
	L"47.952 (NTSC)",
	L"48",
	L"50 (PAL)",
	L"59.94 (NTSC)",
	L"60",
	L"100",
	L"119.88",
	L"120",
};

PrTime			frameRates[ sizeof(frameRateStrings) / sizeof(frameRateStrings[0]) ];

const PrTime	frameRateNumDens[][2]={
	{10, 1},
	{15, 1},
	{24000, 1001},
	{24, 1},
	{25, 1},
	{30000, 1001},
	{30, 1},
	{48000, 1001},
	{48, 1},
	{50, 1},
	{60000, 1001},
	{60, 1},
	{100, 1},
	{120000, 1001},
	{120, 1}
};

const wchar_t * const ADBEVMCMux_TypeStrings[] = {
	L"MPEG-1",	// (0) not supported by NVENC
	L"VCD",		// (1) not supported by NVENC
	L"MPEG-2",	// (2) not supported by NVENC
	L"SVCD",	// (3) not supported by NVENC
	L"DVD",		// (4) not supported by NVENC
	L"TS",		// (5) Transport stream (requires external application TSMuxer)
	L"None",	// (6) None      (separate audio + video output files)
	L"MP4",		// (7) MP4 system (requires external application MP4BOX)
	L"MKV"		// (8) MKV/Matroska (requires external application MKVMERGE)
};

// Need to give parameters, parameter groups, and constrained values their names here
prMALError exSDKPostProcessParams (
	exportStdParms			*stdParmsP, 
	exPostProcessParamsRec	*postProcessParamsRecP)
{
	prMALError		result	= malNoError;
	csSDK_uint32	exID	= postProcessParamsRecP->exporterPluginID;
	ExportSettings	*lRec	= reinterpret_cast<ExportSettings*>(postProcessParamsRecP->privateData);

	exOneParamValueRec		tempCodec;
//	const csSDK_int32		codecs[]			= {SDK_8_BIT_RGB, SDK_10_BIT_YUV};
//	const wchar_t* const	codecStrings[]		= {	SDK_8_BIT_RGB_NAME, SDK_10_BIT_YUV_NAME};

	exOneParamValueRec		tempPAR;
	const csSDK_int32		PARs[][2]			= {
		{1, 1},			// Square pixels (1.0)
		{10, 11},		// D1/DV NTSC (0.9091)
		{40, 33},		// D1/DV NTSC Widescreen 16:9 (1.2121)
		{768, 702},		// D1/DV PAL (1.0940)
		{1024, 702},	// D1/DV PAL Widescreen 16:9 (1.4587)
		{2, 1},			// Anamorphic 2:1 (2.0)
		{4, 3},			// HD Anamorphic 1080 (1.3333)
		{3, 2}			// DVCPRO HD (1.5)
	};
	const wchar_t* const	PARStrings[]		= {
		L"Square pixels (1.0)",
		L"D1/DV NTSC (0.9091)",
		L"D1/DV NTSC Widescreen 16:9 (1.2121)",
		L"D1/DV PAL (1.0940)",
		L"D1/DV PAL Widescreen 16:9 (1.4587)",
		L"Anamorphic 2:1 (2.0)",
		L"HD Anamorphic 1080 (1.3333)",
		L"DVCPRO HD (1.5)"
	};

	PrTime					ticksPerSecond		= 0;
	exOneParamValueRec		tempFrameRate;

	// Initialize arrays of various settings
/*
	exOneParamValueRec		tempFieldOrder;
	const csSDK_int32		fieldOrders[]		= {
		prFieldsNone,
		prFieldsUpperFirst,
		prFieldsLowerFirst
	};
	const wchar_t* const	fieldOrderStrings[]	= {
		L"None (Progressive)",
		L"Upper First",
		L"Lower First"
	};
*/
	const csSDK_int32		AudioFormats[]		= {ADBEAudioCodec_PCM,
													ADBEAudioCodec_AAC};
	const wchar_t* const	AudioFormatStrings[]= {	L"PCM", L"AAC" };

	const float				sampleRates[]		= {	8000.0f, 11025.0f,
													16000.0f, 22050.0f, 
													32000.0f, 44100.0f, 
													48000.0f, 96000.0f};
	const wchar_t* const	sampleRateStrings[]	= {	L"8000 Hz", L"11025 Hz",
													L"16000 Hz", L"22050 Hz", 
													L"32000 Hz", L"44100 Hz", 
													L"48000 Hz", L"96000 Hz"};
	exOneParamValueRec		tempChannelType;


	const csSDK_int32		channelTypes[]		= {kPrAudioChannelType_Mono, kPrAudioChannelType_Stereo,
													kPrAudioChannelType_51, kPrAudioChannelType_16Channel};
	const wchar_t* const	channelTypeStrings[]= {	STR_CHANNEL_TYPE_MONO, STR_CHANNEL_TYPE_STEREO,
													STR_CHANNEL_TYPE_51, STR_CHANNEL_TYPE_16CHANNEL};
	CodecSettings			codecSettings;
	csSDK_int32				codecSettingsSize	= static_cast<csSDK_int32>(sizeof(CodecSettings));

	const wchar_t* const	tvformatStrings[]	= {
		L"NTSC",
		L"PAL"
//		L"SECAM"
	};

	prUTF16Char				tempString[256];
	exOneParamValueRec		tempSampleRate;
	s_cpuid_info			cpuid_info;

	// get CPUID information (to determine if CPU supports SSSE3 for audio-5.1 processing)
	get_cpuinfo_has_ssse3(&cpuid_info);

	lRec->timeSuite->GetTicksPerSecond (&ticksPerSecond);
	for (csSDK_int32 i = 0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		frameRates[i] = ticksPerSecond / frameRateNumDens[i][0] * frameRateNumDens[i][1];
	}

	// set the groups
	NVENC_SetParamName(lRec, exID, ADBEVideoCodecGroup, GroupName_VideoCodec, NULL );
	NVENC_SetParamName(lRec, exID, GroupID_GPUSelect, GroupName_GPUSelect, NULL );
	NVENC_SetParamName(lRec, exID, GroupID_NVENCCfg, GroupName_NVENCCfg, NULL);

	// now set individual parameters
	NVENC_SetParamName(lRec, exID, ParamID_GPUSelect_GPUIndex,
		L"Nvidia GPU#",		L"If multiple NVidia GPUs are installed,\n\
select which GPU to use");
	NVENC_SetParamName(lRec, exID, ParamID_GPUSelect_Report_VRAM,
		L"VRAM (MB)",		L"GPU onboard video-RAM (2048 or more recommended)");
	NVENC_SetParamName(lRec, exID, ParamID_GPUSelect_Report_CCAP,
		L"Compute Level",	L"CUDA device Compute Capability (NVENC requires 3.0 or higher)\n\
In general, the Compute Level indicates what codecs are supported by\n\
the GPU's NVENC block:\n\
\n\
3.0 or higher:  H264 (AVC)  supported (High-Profile, 4:2:0 color, 8bpp)\n\
5.0 or higher:  H264 (AVC)  supported (High-Profile444, 4:4:4 color, lossless)\n\
5.2 or higher:  H265 (HEVC) supported (Main-Porfile, 4:2:0 color, 8bpp)\
" );
	NVENC_SetParamName(lRec, exID, ParamID_GPUSelect_Report_CV,
		L"CUDA API Ver",	L"NVidia CUDA API revision level reported by the installed GPU-driver\n\
nvenc_export requires the GPU-driver to support NVENC API 5.0 (Dec 2014),\n\
which likely means that compatible GPU-drivers will all be at\n\
CUDA '6050' (6.5) or later.\
" );
	NVENC_SetParamName(lRec, exID, ParamID_GPUSelect_Report_DV,
		L"GPU Driver Ver",	L"NVidia driver Version (requires 347.09 or higher)\n\
nvenc_export requires the GPU driver to support NVENC API 5.0.\n\
\n\
Since NVENC SDK 5.0 was publically released in Dec 2014, compatible\n\
the oldest GPU drivers compatible with nvenc_export appeared no\n\
earlier than Dec 2014.\
" );
	NVENC_SetParamName(lRec, exID, ParamID_NVENC_Info_Button,
		L"NVENC Info",		L"Click <NVENC Info> to print reported capabilities" );

//	NVENC_SetParamName(lRec, exID, ADBEVideoCodec,
//		L"Codec",			L"The video codec to be used for encode (MPEG2/VC1/H264)");
	NVENC_SetParamName(lRec, exID, ADBEVideoCodecPrefsButton, 
		L"Codec Settings",	NULL);
	NVENC_SetParamName(lRec, exID, ADBEBasicVideoGroup, 
		GroupName_BasicVideo,	NULL);
	NVENC_SetParamName(lRec, exID, ADBEMPEGCodecBroadcastStandard, 
		L"TV Standard", L"The video format:\n\
NTSC = North America, Japan, and others\n\
PAL = Europe, most of Asia\n\
This information is only used to generate VUI metadata -\n\
video usability information.  It has no effect on the\n\
video-encoding operation.\
" );
	NVENC_SetParamName(lRec, exID, ADBEVideoWidth, 
		L"Width (in pixels)",	NULL);
	NVENC_SetParamName(lRec, exID, ADBEVideoHeight, 
		L"Height (in pixels)",	NULL);
	NVENC_SetParamName(lRec, exID, ADBEVideoAspect, 
		L"Pixel Aspect Ratio",	L"Pixel Aspect Ratio\n\
Legacy SDTV resolutions (NTSC 720x480, PAL 720x576) used non-square pixels.\n\
Modern HD resolutions such as 1280x720 (and higher) use square pixels (1:1)\n\
\n\
...Not to be confused with the \"frame aspect-ratio\", which is the refers to\n\
the ratio of the entire frame-area, and not the individual pixels that form it.\
" );
	NVENC_SetParamName(lRec, exID, ADBEVideoFPS, 
		L"Frame Rate (fps)", L"Video frames per second\n\
Controls the framerate of the (Adobe) video renderer's output to NVENC, as well\n\
as the value written to the H264/HEVC sequence-header.\
" );
	NVENC_SetParamName(lRec, exID, ADBEVideoFieldType, 
		L"Field Type",	L"field-order (top first or bottom first, for interlaced-video only)\n\
This controls the Adobe video-renderer's output interlacer, and whether\n\
field-based (or frame-based) sequences are sent to the exporter\n\
(nvenc_export).  It does not affect the encoder's interlacing control;\n\
that must be set independently\n\
(...see the other setting 'FieldEncoding' setting.)\
" );
	NVENC_SetParamName(lRec, exID, ADBEVideoMaxBitrate, 
		L"Maximum Bitrate [Mbps]", L"When rateControl is set to Variable bitrate (VBR),\n\
this is the maximum allowed bitrate of the encoded video\n\
(lower# = smaller file, higher# = better quality)");
	NVENC_SetParamName(lRec, exID, ADBEVideoTargetBitrate, 
		L"Target Bitrate [Mbps]", L"When rateControl is set to Constant bitrate (CBR) or Variable bitrate (VBR),\n\
this is the target[/average] bitrate of the encoded video\n\
(lower# = smaller file, higher# = better quality)");
	NVENC_SetParamName(lRec, exID, ParamID_forced_PrPixelFormat, 
		LParamID_forced_PrPixelFormat, L"If checkbox is set, then NVENC\n\
will force the Adobe app to output the rendered-video (to NVENC) in\n\
the user-selected pixelformat, which can be any of the following:\n\
\n\
YUV420_709  (4:2:0 Bt709)\n\
YUV420_601  (4:2:0 Bt601)\n\
YUYV422_601 (4:2:2 Bt601)\n\
YUYV422_709 (4:2:2 Bt709)\n\
VUYA_709    (4:4:4 Bt709)\n\
\n\
Regarding pixelformats, there are 2 separate things to consider:\n\
First is color-timing: example Bt601 vs Bt709.  If any segment of\n\
the rendered video does not match the chosen color-timing\n\
(eg. Bt601 vs Bt709), then the Adobe rendere must apply the\n\
corresponding color-conversion formula on every pixel of the\n\
rendered segemnt. This is a computationally expensive task, and\n\
may degrade speed/throughput of the video renderer output.\n\
\n\
Second is the framebuffer format (eg. 4:2:0 vs 4:2:2 vs 4:4:4),\n\
which has to do with the layout of pixels in video-memory.\n\
nvenc_export requires video-input in either NV12 (4:2:0 2-plane)\n\
or YUV444 (4:4:4 3-plane) surfaceformat.  Adobe doesn't natively\n\
support either one, therefore nvenc_export must ALWAYS perform\n\
a repacking operation (and possibly a chroma-downsampling from\n\
4:2:2 to 4:2:0.)\n\
\n\
Given a choice, nvenc_export prefers 4:2:0 over 4:2:2, beacuse it\n\
requires less work to convert.  At this time, nvenc_export's\n\
repacker runs entirely in software on the host-CPU; it does not\n\
take advantage of GPU-acceleration.)  However, when MPE\n\
harwdare-acceleration is enabled, Adobe's video-renderer\n\
refuses to output Planar_420.\n\
\n\
Recommendation: If you encounter errors during export, force the\n\
pixelformat to 422 accelerated flow will not output Planar_420\n\
if a CUDA-accelerated effect is used anywhere throughout the video.\n\
" );

	////////////
	//
	// GroupID_AudioFormat
	//
	NVENC_SetParamName(lRec, exID, GroupID_AudioFormat, 
		GroupName_AudioFormat,	NULL );

	NVENC_SetParamName(lRec, exID, ADBEAudioCodec, 
		L"Audio Format",	L"The audio compression type: <uncmpressed PCM> or <MPEG-4 Advanced Audio Codec>");
	NVENC_SetParamName(lRec, exID, ParamID_AudioFormat_NEROAAC_Path, 
		L"neroAacEnc.exe path", L"Location of external program 'neroAacEnc.EXE'\n\
(This AAC-encoder can be downloaded for free from http://www.nero.com\
" );
	NVENC_SetParamName(lRec, exID, ParamID_AudioFormat_NEROAAC_Button, 
		L"neroAac_Button",	L"Click <Button> to specify path to neroAacEnc.EXE" );

	////////////
	//
	// GroupID_BasicAudio
	//
	NVENC_SetParamName(lRec, exID, ADBEBasicAudioGroup, 
		GroupName_BasicAudio,	NULL);
	NVENC_SetParamName(lRec, exID, ADBEAudioRatePerSecond, 
		STR_SAMPLE_RATE,	L"Sampling frequency (#samples per second)\n\
(Most modern audio devices use 48000.  Audio-CD uses 44100.)");
	NVENC_SetParamName(lRec, exID, ADBEAudioNumChannels, 
		STR_CHANNEL_TYPE,	L"Audio channel configuration\n\
(5.1 = surround sound.  FrontLeft, Front Center, Front Right, Rear Left, Read Right, LFE");
	NVENC_SetParamName(lRec, exID, ADBEAudioBitrate, 
		L"Bitrate [kbps]",	L"Encoded AAC audio bitrate (in kilo bits per second)\nuse 192 for stereo, and 384 for 5.1-surround)");

	//
	//  GPUIndex : NVidia GPU selector (user-adjustable)
	//
	std::vector<NvEncoderGPUInfo_s> gpulist;
	csSDK_int32 numGPUs = update_exportParamSuite_GPUSelectGroup_GPUIndex( exID, lRec, gpulist);

	// get the current user-selection for GPUIndex, and perform a range-check on it
	exParamValues GPUIndexValue;
	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_GPUSelect_GPUIndex, &GPUIndexValue);
	int           GPUIndex = GPUIndexValue.value.intValue;
	if ( GPUIndex >= numGPUs) 
		GPUIndex = numGPUs - 1;// out of range

	//
	// Create the read-only section of the GPU-group.
	//
	// The 'read-only' entries simply report information back to the user, and cannot be changed.
	lRec->NvGPUInfo = gpulist[GPUIndex];
	update_exportParamSuite_GPUSelectGroup(
		exID,		// exporterID
		lRec,		// ExportSettings
		GPUIndex,	// select which NVidia GPU to use
		lRec->NvGPUInfo
	);

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_CODEC, 
		LParamID_NV_ENC_CODEC, L"Selects the video compression standard\n\
H264 = AVC ... this codec is used for a variety of applications:\n\
	   satellite-TV, Bluray-Disc, internet streaming video, etc.\n\
	   It is a de-facto video distribution format, with widespread\n\
	   support among mobile, consumer, and PC devices. Nearly\n\
	   everything nowadays can play some form of H264 video.\n\
\n\
*H265 = HEVC ... this new codec offers better compression\n\
	   efficiency than H264 (with the biggest gains at\n\
	   4k-resolution),  but has steep hardware requirements.\n\
	   HEVC is supported by fewer playback devices, though\n\
	   more and more support it is being added every day.\n\
\n\
*This option will only be shown if your GPU supports it\n\
 (HEVC-encode debuted on Maxwell Gen2: GM200/GM204/GM206)\
" );
	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_PRESET, 
		LParamID_NV_ENC_PRESET, L"Selects a group of internal default settings based on desired application:\n\
(2) HP = high-performance (faster encode speed, lower quality)\n\
(3) HQ = high-quality (slower encode speed, higher quality)\n\
(4) BD = Bluray-disc compliant parameters\n\
	(you must also use the following settings:\n\
	 sliceMode=1, sliceModeData=4,\n\
	 bframes less than or equal 3,\n\
	 level_41, high_profile)\n\
LOW_LATENCY_* = low-latency realtime streaming (no b-frames)\n\
	 nvenc_export does not support low_latency, because it\n\
	 ignores several NVENC API-control which are integral\n\
	 to proper low_latency encoding.)\n\
(5)    ... LOW_LATENCY_HQ = higher-quality output, slower\n\
(6)    ... LOW_LATENCY_HP = faster encode time\n\
(8) LOSSLESS = mathematically lossless encoding\n\
(9) LOSSLESS_HP = faster lossless encoding (bigger files?)\n\
    (LOSSLESS_*: Requires PROFILE_HIGH_444 or above)");

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_H264_PROFILE, 
		LParamID_NV_ENC_H264_PROFILE, L"Compression Profile:\n\
(66)  BASE = simplest (CAVLC only, no B-frames)\n\
(67)  MAIN = more complex (interlacing, CABAC, B-frames)\n\
(100) HIGH = more complex (adds adaptive-transform)\n\
    ... most applications use HIGH profile (Bluray, web, TV, etc.)\n\
(128) STEREO = adds stereoscopic (MVC) multi view coding to HIGH\n\
    ... stereo is no longer supported by NVENC API\n\
(244) HIGH_444 = most complex (lossless, 4:4:4, separateColourPlaneFlag)\n\
" );

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_HEVC_PROFILE, 
		LParamID_NV_ENC_HEVC_PROFILE, L"Compression Profile:\n\
(300) MAIN = (8bpp, 4:2:0)\n\
" );

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_LEVEL_H264,
		LParamID_NV_ENC_LEVEL_H264, L"Compression Level:\n\
Each level defines upper-limits for video framesize,\n\
framerate, and encoded bitrate.  The values below are\n\
only approximate, because NVENC API doesn't seem to\n\
follow the ISO specification precisely:\n\
\n\
H264 3   = ~720x576 up to 25fps (or 720x480 @ 30fps)\n\
H264 3.1 = 1280x720, up to 30fps\n\
H264 4   = 2048x1080, up to 30fps (up to 25Mbps)\n\
H264 4.1 = 2048x1080, up to 30fps (up to 50Mbps)\n\
H264 4.2 = 2048x1080, up to 60fps\n\
H264 5   = 2560x1920, up to 30fps (up to 135Mbps)\n\
H264 5.1 = 4096x2160, up to 30fps\n\
*H264 5.2 = 4096x2160, up to 60fps (to use this profile, choose 'autoselect')\n\
");

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_LEVEL_HEVC,
		LParamID_NV_ENC_LEVEL_HEVC, L"Compression Level:\n\
Each level defines upper-limits for video framesize,\n\
framerate, and encoded bitrate.  The values below are\n\
only approximate, because NVENC API doesn't seem to\n\
follow the ISO specification precisely:\n\
\n\
HEVC 3   = 960x540  , up to 30fps (main_tier 6Mbps)\n\
HEVC 3.1 = 1280x720 , up to 33fps (main_tier 10Mbps)\n\
HEVC 4   = 2048x1080, up to 30fps (main_tier 12Mbps, high_tier 30Mbps)\n\
HEVC 4.1 = 2048x1080, up to 60fps (main_tier 20Mbps, high_tier 50Mbps)\n\
HEVC 5   = 4096x2160, up to 30fps (main_tier 25Mbps, high_tier 100Mbps)\n\
HEVC 5.1 = 4096x2160, up to 60fps (main_tier 40Mbps, high_tier 160Mbps)\n\
HEVC 5.2 = 4096x2160, up to 120fps(main_tier 60Mbps, high_tier 240Mbps)\
");

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_TIER_HEVC,
		LParamID_NV_ENC_TIER_HEVC, L"Tier:\n\
For each defined level, tier affects the max allowed bitrate\n\
Main tier is intended to cover general purpose applications,\n\
while high tier is intended for demanding applications..\n\
");

	NVENC_SetParamName(lRec, exID, ParamID_chromaFormatIDC, 
		LParamID_chromaFormatIDC, L"Encoder pixel Color format\n\
Specifies the chromaformat for the encoded video:\n\
(1) format_420 = 4:2:0, chroma-resolution is half-horizontal,\n\
	and half-vertical\n\
\n\
(3) format_444 = 4:4:4, no subsampling (full chroma resolution)\n\
\n\
4:2:0 is the standard format for nearly all consumer/web apps\n\
4:4:4 is used in professional environments\n\
(4:4:4 requires PROFILE_HIGH_444 or above.  Note that\n\
       very few consumer-devices can play 4:4:4 video.");

	//Add_NVENC_Param_bool( GroupID_NVENCCfg, "stereo3dMode", false )
	//Add_NVENC_Param_bool( GroupID_NVENCCfg, "stereo3dEnable", false )

	NVENC_SetParamName(lRec, exID, ParamID_gopLength, 
		LParamID_gopLength, L"Group of Picture length (#frames)\n\
#maximum distance between key-frames\n\
for 'infinite GOP, set to '999',\n\
for Bluray video this value must be less than the #frames per second (23)" );
	//NVENC_SetParamName(lRec, exID, "idr_period", L"idr_period", L"I-frame don't reuse period (#frames)" );// TODO: CNvEncoder ignores this value

	NVENC_SetParamName(lRec, exID, ParamID_monoChromeEncoding, 
		LParamID_monoChromeEncoding, L"monoChromeEncoding\n\
force black&white encoding (encode luma plane only,\n\
do not encode chroma planes)" );

	NVENC_SetParamName(lRec, exID, ParamID_max_ref_frames, 
		LParamID_max_ref_frames, L"Number of reference frames\n\
(for H264 main-profile and above, default=2,\n\
 NVENC motion-estimation never uses more than 2,\n\
 so there is no efficiency advantage in using more\n\
 than 2.)");
	NVENC_SetParamName(lRec, exID, ParamID_numBFrames, 
		LParamID_numBFrames, L"maximum # of consecutive bi-directionally predicted frames\n\
(B-frames require at least 2 ref_frames, and are slower\n\
 to encode, but may improve compression efficiency under\n\
 some circumstances. Not recommended for noisy/grainy video)\
" );

	NVENC_SetParamName(lRec, exID, ParamID_FieldEncoding, 
		LParamID_FieldEncoding, L"Progressive or interlaced frame-mode\n\
MODE_FIELD = encode video as a sequence of interlaced images.\n\
	(A pair of interlaced images form a complete\n\
	frame, each interlaced image contains alternating\n\
	even or odd scanlines).  Note, you should also\n\
	set the 'Field Type' setting to match.)\n\
\n\
MODE_FRAME = encode video as a sequence of complete (non-interlaced)\n\
	frames.\n\
MODE_MBAFF = not currently supported\
" );

	NVENC_SetParamName(lRec, exID, ParamID_rateControl, 
		LParamID_rateControl, L"Bitrate Control mode\n\
Determines the method/metric used to control the size\n\
of the encoded video:\n\
\n\
(0)  CONSTQP = constant Quantization-P\n\
     Encoder allows bitrate to fluctuate in an attempt to maintain\n\
     a constant quality-level (as measured via QP)\n\
(1)  VBR     = variable bitrate\n\
     User sets an average target bitrate (along with a max limit).\n\
	 Encoder allows attempts to match the user's target bitrate,\n\
	 allowing momentary spikes in bitrate (to deal with scene cuts\n\
	 and fast motion.)\n\
(2)  CBR     = constant bitrate\n\
     User sets a hard target bitrate.  Encoder attempts to match\n\
	 the bitrate exactly.\n\
(4)  VBR_MINQP = variable bitrate with Minimum Quantization-P\n\
(8)  *2_PASS_QUALITY = low-latency CBR with localized 2-passes\n\
     (higher-quality than CBR, slow)\n\
(16) *2_PASS_FRAMESIZE_CAP = low-latency CBR with localized 2-passes\n\
(32) 2_PASS_VBR = variable bitrate with localized 2-passes\n\
     (higher-quality than VBR, slow)\n\
	 * these modes are only available when one of the LOW_LATENCY\n\
	   presets is selected." );

	NVENC_SetParamName(lRec, exID, ParamID_qpPrimeYZeroTransformBypassFlag, 
		LParamID_qpPrimeYZeroTransformBypassFlag, L"enable mathematically lossless encoding\n\
\n\
(Requires rate-control ConstQP, and qpI/P/B set to 0/0/0)\n\
(Requires PROFILE_HIGH_444 or above)\
(Requires vle_entropy_mode to be set to 'CAVLC' or auto.)\
");

	NVENC_SetParamName(lRec, exID, ParamID_vbvBufferSize, 
		LParamID_vbvBufferSize, L"Video Buffer Size VBV(HRD) in bits (0=auto)\n\
Affects the Video Buffer Verifier in the Hypothetical Reference\n\
Decoder model.\n\
\n\
(Do not change this unless you know what you are doing.)");

	NVENC_SetParamName(lRec, exID, ParamID_vbvInitialDelay, 
		LParamID_vbvInitialDelay, L"VBV(HRD) initial delay in bits (0=auto)\n\
Affects the Video Buffer Verifier in the Hypothetical Reference\n\
Decoder model.\n\
\n\
(Do not change this unless you know what you are doing.)");

	NVENC_SetParamName(lRec, exID, ParamID_qpI, 
		LParamID_qpI, L"Constant-QP: I-frame target quality level (for key-frames)\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.");
	NVENC_SetParamName(lRec, exID, ParamID_qpP, 
		LParamID_qpP, L"Constant-QP: P-frame target quality level (for forward predicted frames)\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.");
	NVENC_SetParamName(lRec, exID, ParamID_qpB, 
		LParamID_qpB, L"Constant-QP: B-frame target quality level (for bidirectionally predicted frames)\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.");

	NVENC_SetParamName(lRec, exID, ParamID_min_qpI, 
		LParamID_min_qpI, L"MinVBR-QP (highest allowed quality): I-frame max quality level\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.\n\
\n\
Do not change this from it's default value (0) unless you know what you are doing.");
	NVENC_SetParamName(lRec, exID, ParamID_min_qpP, 
		LParamID_min_qpP, L"MinVBR-QP (highest allowed quality): P-frame max quality level\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.\n\
\n\
Do not change this from it's default value (0) unless you know what you are doing.");
	NVENC_SetParamName(lRec, exID, ParamID_min_qpB, 
		LParamID_min_qpB, L"MinVBR-QP (highest allowed quality): B-frame max quality level\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.\n\
\n\
Do not change this from it's default value (0) unless you know what you are doing.");

	// not sure what maxqp is really used for?!?
	NVENC_SetParamName(lRec, exID, ParamID_max_qpI, 
		LParamID_max_qpI, L"MinVBR-QP (lowest allowed quality): I-frame min quality level\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.\n\
\n\
Do not change this from it's default value (51) unless you know what you are doing.");
	NVENC_SetParamName(lRec, exID, ParamID_max_qpP, 
		LParamID_max_qpP, L"MinVBR-QP (lowest allowed quality): P-frame min quality level\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.\n\
\n\
Do not change this from it's default value (51) unless you know what you are doing.");
	NVENC_SetParamName(lRec, exID, ParamID_max_qpB, 
		LParamID_max_qpB, L"MinVBR-QP (lowest allowed quality): B-frame min quality level\n\
A lower value corresponds to better quality (larger file).\n\
0 is effectively lossless, 51 is the coarsest value.\n\
\n\
Do not change this from it's default value (51) unless you know what you are doing.");

	// not sure what initialqp is really used for?!?
	NVENC_SetParamName(lRec, exID, ParamID_initial_qpI, 
		LParamID_initial_qpI, L"initial-QP: reference (I) frame: Quality level (lower# = better)");
	NVENC_SetParamName(lRec, exID, ParamID_initial_qpP, 
		LParamID_initial_qpP, L"initial-QP: reference (P) frame: Quality level (lower# = better)");
	NVENC_SetParamName(lRec, exID, ParamID_initial_qpB, 
		LParamID_initial_qpB, L"initial-QP: bidi (B) frame: quality setting (lower# = better)");

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_H264_FMO, 
		LParamID_NV_ENC_H264_FMO, L"Flexible Macroblock ordering mode (baseline profile only)" );

	NVENC_SetParamName(lRec, exID, ParamID_hierarchicalP, 
		LParamID_hierarchicalP, L"Enable (P) frame hierarchical encoding mode" );
	NVENC_SetParamName(lRec, exID, ParamID_hierarchicalB, 
		LParamID_hierarchicalB, L"Enable (B) frame hierarchical encoding mode" );

	NVENC_SetParamName(lRec, exID, ParamID_sliceMode,
		LParamID_sliceMode, L"How picture is segmented into smaller units:\n\
0 = Macroblock based slices\n\
1 = Byte based slices\n\
2 = Macroblock-row based slices\n\
3 = numSlices in Picture (default)\n\
\n\
(Do not change this setting unless you know what you are doing.)\
");

	NVENC_SetParamName(lRec, exID, ParamID_sliceModeData,
		LParamID_sliceModeData, L"Interpretation of sliceModeData depends on 'sliceMode':\n\
0 = this value sets the #macroblocks in a slice\n\
1 = this value sets the #bytes in a slice\n\
2 = this value sets the #macroblock-rows in a slices\n\
3 = this value sets the #slices in a Picture (default)\
" );

	NVENC_SetParamName(lRec, exID, ParamID_vle_entropy_mode, 
		LParamID_vle_entropy_mode, L"Entropy mode\n\
CAVLC=less complex\n\
CABAC=more compression efficiency (usually)\n\
(CABAC: Requires PROFILE_MAIN or above.\n\
     Not supported if preset is set to LOSSLESS mode.)");

	NVENC_SetParamName(lRec, exID, ParamID_separateColourPlaneFlag, 
		LParamID_separateColourPlaneFlag, L"Frames are encoded as 3 separate \n\
planes (Y,U,V), instead of a single one\n\
(Requires PROFILE_HIGH_444 or above)" );

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_MV_PRECISION, 
		LParamID_NV_ENC_MV_PRECISION, L"Motion vector precision\n\
full pixel = least complex, least accurate\n\
half pixel = more complex, more accurate\n\
quarter pixel = most accurate, best compression efficiency (default)" );
	NVENC_SetParamName(lRec, exID, ParamID_disableDeblock, 
		LParamID_disableDeblock, L"Controls deblock filter usage:\n\
0 = use deblocking filter (do not disble) *default\n\
1 = disable deblock filter\n\
2 = constrain deblock filter to current slice.\n\
\n\
(Do not change this setting unless you know what you are doing.)\
" );

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM, 
		LParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM, L"adaptive transform control\n\
enables dynamic macroblock sizing (4x4 or 8x8), which improves\n\
compression efficiency.\n\
\n\
(Requires PROFILE_HIGH or above)");
	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_H264_BDIRECT_MODE, 
		LParamID_NV_ENC_H264_BDIRECT_MODE, L"B-frame encoding optimization" );

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_HEVC_MINCUSIZE,
		LParamID_NV_ENC_HEVC_MINCUSIZE, L"The minimum size coding-unit to be considered for use\n\
The coding-unit size has a direct effect on compression efficiency.\n\
A smaller coding unit size (eg 8x8) enables complex scene changes\n\
and/or movement to be accurately predicted from one frame to the next.\n\
\n\
Leave this setting on 'auto', unless you know what you're doing.\
" );

	NVENC_SetParamName(lRec, exID, ParamID_NV_ENC_HEVC_MAXCUSIZE,
		LParamID_NV_ENC_HEVC_MAXCUSIZE, L"The maximum size coding-unit to be considered for use\n\
The coding-unit size has a direct effect on compression efficiency.\n\
A larger coding unit size (eg 64x64) improves the encoder's ability\n\
to more efficiently compress large regions of spatial redundancy.\n\
The improvement is significant at higher-resolutions (eg. @4k)\n\
\n\
Leave this setting on 'auto', unless you know what you're doing.\
" );

	NVENC_SetParamName(lRec, exID, ParamID_syncMode, 
		LParamID_syncMode, L"NVENC front-end interface\n\
checkbox enabled = asynchronous mode (execute in\n\
	background thread)\n\
checkbox disabled= synchronous mode (execute in caller\n\
	thread)\n\
\n\
(nvenc_export always runs in asynchronous mode.  In this mode,\n\
 the GPU handles all aspects of encoding autonomously.  The\n\
 plugin simply feeds rendered-video frames into the NVENC API,\n\
then waits for the API to send back the compressed bitstream.)\
" );

	NVENC_SetParamName(lRec, exID, ParamID_enableVFR, 
		LParamID_enableVFR, L"enable variable frame rate\n\
(allows NVENC to dynamically alter encoded frame-rate)\
" );

	NVENC_SetParamName(lRec, exID, ParamID_enableAQ, 
		LParamID_enableAQ, L"enable adaptive quantization\n\
(When rate_control is ConstQP, adaptive quantization is not\n\
 allowed because constant-QP is, by definition, not adjustable.)\
" );

	//
	// Multiplexer group
	//
	NVENC_SetParamName(lRec, exID, ADBEMultiplexerTabGroup,
		L"Multiplexer", NULL );
	NVENC_SetParamName(lRec, exID, GroupID_NVENCMultiplexer,
		L"Basic Settings", NULL );
	NVENC_SetParamName(lRec, exID, ADBEVMCMux_Type, 
		L"Multiplexing", L"Post-encode multiplexing action:\n\
\n\
none = no multiplexing, elementary bitstream only\n\
	 (if both audio + video export are enabled, nvenc_export will\n\
	  generate 2 files.)\n\
\n\
TS = MPEG-2 transport stream (must specify location of\n\
	 third-party program 'TSMuxer')\n\
\n\
MP4= MPEG-4 system stream (must specify location of\n\
	 third-party program 'MP4Box')\n\
\n\
MKV= Matroska fie (must specify location of\n\
	 third-party program 'MKVMERGE')\n\
\n\
If encoding HEVC, it is recommended to use MKV, as the other two choices\n\
appear to have problems (dropped frames, poor random-seeking, etc.)\n\
If encoding H264, all 3 types of muxers should function properly.\
" );

	NVENC_SetParamName(lRec, exID, ParamID_BasicMux_TSMUXER_Path, 
		L"TSMuxer.exe path", L"Location of external program 'TSMuxer.EXE'");
	NVENC_SetParamName(lRec, exID, ParamID_BasicMux_TSMUXER_Button,
		L"TSMUXER_Button", L"Click <Button> to specify path to TSmuxer.EXE" );
	NVENC_SetParamName(lRec, exID, ParamID_BasicMux_MP4BOX_Path, 
		L"MP4Box.exe path", L"Location of external program 'MP4Box.EXE'");
	NVENC_SetParamName(lRec, exID, ParamID_BasicMux_MP4BOX_Button,
		L"MP4BOX_Button", L"Click <Button> to specify path to MP4Box.EXE" );
	NVENC_SetParamName(lRec, exID, ParamID_BasicMux_MKVMERGE_Path,
		L"MKVMERGE.exe path", L"Location of external program 'MKVMERGE.EXE'");
	NVENC_SetParamName(lRec, exID, ParamID_BasicMux_MKVMERGE_Button,
		L"MKVMERGE_Button", L"Click <Button> to specify path to MKVMERGE.EXE");

	//
	// Update the GroupID_NVENCCfg
	//
	update_exportParamSuite_NVENCCfgGroup( exID, lRec);

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEMPEGCodecBroadcastStandard);
	for (csSDK_int32 i = 0; i < sizeof (tvformatStrings) / sizeof (tvformatStrings[0]); i++)
	{
		tempCodec.intValue = i;
		copyConvertStringLiteralIntoUTF16(tvformatStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEMPEGCodecBroadcastStandard,
															&tempCodec,
															tempString);
	}
/*
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEVideoCodec);
	for (csSDK_int32 i = 0; i < sizeof (codecs) / sizeof (csSDK_int32); i++)
	{
		tempCodec.intValue = codecs[i];
		copyConvertStringLiteralIntoUTF16(codecStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEVideoCodec,
															&tempCodec,
															tempString);
	}
*/
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEVideoAspect);
	for (csSDK_int32 i = 0; i < sizeof (PARs) / sizeof(PARs[0]); i++)
	{
		tempPAR.ratioValue.numerator = PARs[i][0];
		tempPAR.ratioValue.denominator = PARs[i][1];
		copyConvertStringLiteralIntoUTF16(PARStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEVideoAspect,
															&tempPAR,
															tempString);
	}
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEVideoFPS);
	for (csSDK_int32 i = 0; i < sizeof(frameRates) / sizeof (PrTime); i++)
	{
		tempFrameRate.timeValue = frameRates[i];
		copyConvertStringLiteralIntoUTF16(frameRateStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEVideoFPS,
															&tempFrameRate,
															tempString);
	}

	/* // ADBEVideoFieldType is updated in the update_*() function
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEVideoFieldType);
	for (csSDK_int32 i = 0; i < 3; i++)
	{
		tempFieldOrder.intValue = fieldOrders[i];
		copyConvertStringLiteralIntoUTF16(fieldOrderStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEVideoFieldType,
															&tempFieldOrder,
															tempString);
	}*/

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEAudioCodec);
	for (csSDK_int32 i = 0; i < sizeof (AudioFormatStrings) / sizeof (AudioFormatStrings[0]); i++)
	{
		tempCodec.intValue = AudioFormats[i];
		copyConvertStringLiteralIntoUTF16(AudioFormatStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEAudioCodec,
															&tempCodec,
															tempString);
	}

	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEAudioRatePerSecond);
	for (csSDK_int32 i = 0; i < sizeof(sampleRates) / sizeof (float); i++)
	{
		tempSampleRate.floatValue = sampleRates[i];
		copyConvertStringLiteralIntoUTF16(sampleRateStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEAudioRatePerSecond,
															&tempSampleRate,
															tempString);
	}
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEAudioNumChannels);
	for (csSDK_int32 i = 0; i < sizeof(channelTypes) / sizeof (csSDK_int32); i++)
	{
		// if CPU doesn't support SSSE3, then don't allow 5.1-audio because the
		// channel-reordering function uses SSSE3 instructinos
		if ( channelTypes[i] == kPrAudioChannelType_51 && !cpuid_info.bSupplementalSSE3 ) 
			continue;// cpu doesn't support SSSE3, so disable NVENC's support for 5.1-audio
		else if ( channelTypes[i] > kPrAudioChannelType_51 ) 
			continue;// NVENC plugin doesn't support more than 6-channel surround

		tempChannelType.intValue = channelTypes[i];
		copyConvertStringLiteralIntoUTF16(channelTypeStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEAudioNumChannels,
															&tempChannelType,
															tempString);
	}

	// if the audio-format is changed, need to (hide/unhide) the Path Parameter(s)
	update_exportParamSuite_AudioFormatGroup(exID, lRec);

	// change in audio-format possibly requires change in AAC AudioBitRate range
	update_exportParamSuite_BasicAudioGroup(exID, lRec);

	//
	// Set the slider Video bitrate ranges, while preserving the current value
	//
	update_exportParamSuite_VideoGroup( exID, lRec );

	lRec->exportParamSuite->GetArbData(	exID,
										0,
										ADBEVideoCodecPrefsButton,
										&codecSettingsSize,
										NULL);

	if (codecSettingsSize)
	{
		// Settings valid.  Let's get them.
		lRec->exportParamSuite->GetArbData(exID,
											0,
											ADBEVideoCodecPrefsButton,
											&codecSettingsSize,
											reinterpret_cast<void*>(&codecSettings));
	}
	else
	{
		// Settings invalid.  Let's set default ones.
		codecSettings.sampleSetting = kPrFalse;
		codecSettingsSize = static_cast<csSDK_int32>(sizeof(CodecSettings));
		lRec->exportParamSuite->SetArbData(	exID,
											0,
											ADBEVideoCodecPrefsButton,
											codecSettingsSize,
											reinterpret_cast<void*>(&codecSettings));
	}
	
	//
	// Multiplexer group
	//
	lRec->exportParamSuite->ClearConstrainedValues(	exID,
													0,
													ADBEVMCMux_Type);
	for (csSDK_int32 i = 0; i < sizeof (ADBEVMCMux_TypeStrings) / sizeof (ADBEVMCMux_TypeStrings[0]); i++)
	{
		if ( i < 5 ) // don't support items 0-4
			continue;
		tempCodec.intValue = i;
		copyConvertStringLiteralIntoUTF16(ADBEVMCMux_TypeStrings[i], tempString);
		lRec->exportParamSuite->AddConstrainedValuePair(	exID,
															0,
															ADBEVMCMux_Type,
															&tempCodec,
															tempString);
	}

	return result;
}


prMALError
exSDKGetParamSummary (
	exportStdParms			*stdParmsP, 
	exParamSummaryRec		*summaryRecP)
{
	std::wostringstream oss; // stream to convert text into wchar_t text
	prMALError		result	= malNoError;
	wchar_t			videoSummary[256],
					audioSummary[256],
					bitrateSummary[256];
	exParamValues	width,
					height,
					frameRate,
					pixelAspectRatio,
					sampleRate,
					channelType,
					audioCodec, audioBitrate,
					video_rateControl,
					videoBitrate,
					GPUIndex;
	ExportSettings	*lRec	= reinterpret_cast<ExportSettings*>(summaryRecP->privateData);
	PrSDKExportParamSuite	*paramSuite	= lRec->exportParamSuite;
	PrSDKTimeSuite			*timeSuite	= lRec->timeSuite;
	PrTime			ticksPerSecond;
	csSDK_int32		mgroupIndex = 0,
					exporterPluginID	= summaryRecP->exporterPluginID;

	if (!paramSuite)
		return malUnknownError;
	
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEVideoWidth, &width);
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEVideoHeight, &height);
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEVideoFPS, &frameRate);
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEVideoAspect, &pixelAspectRatio);
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEAudioCodec, &audioCodec);
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEAudioRatePerSecond, &sampleRate);
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEAudioNumChannels, &channelType);
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ParamID_GPUSelect_GPUIndex, &GPUIndex );
	paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ParamID_rateControl, &video_rateControl );
	timeSuite->GetTicksPerSecond(&ticksPerSecond);
	
	swprintf(videoSummary, sizeof(videoSummary), L"%ix%i, %.3f fps, %.4f PAR, ",
				width.value.intValue, height.value.intValue,
				static_cast<double>(ticksPerSecond) / static_cast<double>(frameRate.value.timeValue),
				static_cast<double>(pixelAspectRatio.value.ratioValue.numerator) /
				static_cast<double>(pixelAspectRatio.value.ratioValue.denominator)
	);
	oss << videoSummary;
	if ( lRec->NvGPUInfo.nvenc_supported && (GPUIndex.value.intValue >= 0) ) {
		oss << "(#" << std::dec << GPUIndex.value.intValue << ") ";
		oss << lRec->NvGPUInfo.gpu_name << " (";
		oss << std::dec << lRec->NvGPUInfo.vram_free << " MB)";
	}
	else {
		oss << "<ERROR, GPU does not support NVENC encoding!>";
	}
	copyConvertStringLiteralIntoUTF16(oss.str().c_str(), summaryRecP->videoSummary);

	///////
	//
	// Audio summary
	//

	swprintf(audioSummary, 20, L"%.0f Hz, ",
				sampleRate.value.floatValue);
	switch(channelType.value.intValue)
	{
		case kPrAudioChannelType_Mono:
			safeWcscat(audioSummary, 256, STR_CHANNEL_TYPE_MONO);
			break;
		case kPrAudioChannelType_Stereo:
			safeWcscat(audioSummary, 256, STR_CHANNEL_TYPE_STEREO);
			break;
		case kPrAudioChannelType_51:
			safeWcscat(audioSummary, 256, STR_CHANNEL_TYPE_51);
			break;
		case kPrAudioChannelType_16Channel:
			safeWcscat(audioSummary, 256, STR_CHANNEL_TYPE_16CHANNEL);
			break;
		default:
			safeWcscat(audioSummary, 256, L"Unknown channel type");
	} // switch( channelType.value.intValue)

	// audioFormat
	if ( audioCodec.value.intValue == ADBEAudioCodec_AAC ) {
		safeWcscat(audioSummary, 256, L" (AAC)");
	}
	else {
		safeWcscat(audioSummary, 256, L" (PCM)");
	}

	copyConvertStringLiteralIntoUTF16(audioSummary, summaryRecP->audioSummary);

	/////////////
	//
	// bitrate summary
	//
	std::wostringstream oss2; // stream to convert text into wchar_t text
	oss2 << "Encoded bitrate: ";
	bool has_vbr_video = video_rateControl.value.intValue == NV_ENC_PARAMS_RC_CBR ||
		video_rateControl.value.intValue == NV_ENC_PARAMS_RC_VBR ||
		video_rateControl.value.intValue == NV_ENC_PARAMS_RC_VBR_MINQP || 
		video_rateControl.value.intValue == NV_ENC_PARAMS_RC_2_PASS_VBR;
	if ( has_vbr_video ) {
		paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEVideoTargetBitrate, &videoBitrate);
		oss2 << "H264=" << std::dec << (videoBitrate.value.floatValue * 1024 )<< " Kbps";
	}

	if ( audioCodec.value.intValue == ADBEAudioCodec_AAC ) {
		paramSuite->GetParamValue(exporterPluginID, mgroupIndex, ADBEAudioBitrate, &audioBitrate);// for AAC-only
		if ( has_vbr_video )
			oss2 << ", ";
		oss2 << "AAC=" << std::dec << audioBitrate.value.intValue << " Kbps";
	}

	bitrateSummary[0] = 0;
	safeWcscat(bitrateSummary, 256, oss2.str().c_str() );
	copyConvertStringLiteralIntoUTF16(bitrateSummary, summaryRecP->bitrateSummary);
	
	return result;
}


// Pop any custom settings UI, since the user has just clicked on the Configure button
// in the Sequence Settings dialog, or the Codec Settings button in the Export Movie dialog.
// Save the user's choices for use during export
prMALError exSDKParamButton(
		exportStdParms		*stdParmsP, 
		exParamButtonRec	*getFilePrefsRecP)
{
	ostringstream messagebox_text;
	string text;

	prMALError		result				= malNoError;
	ExportSettings	*lRec				= reinterpret_cast<ExportSettings*>(getFilePrefsRecP->privateData);
	CodecSettings	codecSettings;
	csSDK_int32		codecSettingsSize	= static_cast<csSDK_int32>(sizeof(CodecSettings));
	#ifdef PRWIN_ENV
	csSDK_int32		returnValue			= 0;
	#endif
	const csSDK_int32		exID		= getFilePrefsRecP->exporterPluginID;
	PrSDKExportParamSuite	*paramSuite	= lRec->exportParamSuite;
	HWND mainWnd                        = lRec->windowSuite->GetMainWindow();
	csSDK_int32		mgroupIndex         = 0;

	enum {
		button_nvenc_info = 0,
		button_tsmuxer,
		button_mp4box,
		button_mkvmerge,
		button_neroaac,
		button_codecprefs,
		button_other
	} select_button;
	const char *pParamID_filepath = NULL; // pointer to paramID


	/////////////////////////////////////////////
	//
	// Identify *which* button got pressed:
	//
	if ( strcmp(getFilePrefsRecP->buttonParamIdentifier, ParamID_NVENC_Info_Button ) == 0 ) {
		select_button = button_nvenc_info;
		pParamID_filepath = NULL;
	} else if ( strcmp(getFilePrefsRecP->buttonParamIdentifier, ParamID_BasicMux_TSMUXER_Button ) == 0 ) {
		select_button = button_tsmuxer;
		pParamID_filepath = ParamID_BasicMux_TSMUXER_Path;
	} else if ( strcmp(getFilePrefsRecP->buttonParamIdentifier, ParamID_BasicMux_MP4BOX_Button ) == 0 ) {
		select_button = button_mp4box;
		pParamID_filepath = ParamID_BasicMux_MP4BOX_Path;
	} else if (strcmp(getFilePrefsRecP->buttonParamIdentifier, ParamID_BasicMux_MKVMERGE_Button) == 0) {
		select_button = button_mkvmerge;
		pParamID_filepath = ParamID_BasicMux_MKVMERGE_Path;
	} else if ( strcmp(getFilePrefsRecP->buttonParamIdentifier, ParamID_AudioFormat_NEROAAC_Button ) == 0 ) {
		select_button = button_neroaac;
		pParamID_filepath = ParamID_AudioFormat_NEROAAC_Path;
	} else if ( strcmp(getFilePrefsRecP->buttonParamIdentifier, ADBEVideoCodecPrefsButton ) == 0 ) {
		select_button = button_codecprefs;
		pParamID_filepath = ADBEVideoCodecPrefsButton;
	} else {
		assert(0);// FALLTHROUGH: this should never happen!
		select_button = button_other;
		pParamID_filepath = NULL; // not used
	}

	//////////////////////////////////////
	// user pressed the NVENC_Info_Button
	//
	if ( select_button == button_nvenc_info ) {
		UINT mb_flags = MB_OK;
		if ( lRec->NvGPUInfo.nvenc_supported ) {
			NVENC_GetEncoderCaps(lRec->NvEncodeConfig.codec, lRec->NvGPUInfo.nv_enc_caps, text);
		}
		else {
			mb_flags |= MB_ICONERROR;
			text = "!!! Selected GPU does not have NVENC H264 hardware-capability !!!\n";
		}


		returnValue = MessageBox(	GetLastActivePopup(mainWnd),
								text.c_str(),
								EXPORTER_NAME,
								mb_flags  );
								//| MB_RIGHT );
		return returnValue;
	} 
	else if ( select_button == button_tsmuxer || select_button == button_mp4box ||
		select_button == button_mkvmerge || select_button == button_neroaac )
	{
		//
		// user pressed the 'TSUMXER Path' button
		//
		DWORD err = 0;
		const wchar_t strFilter[] = {
			L"executable programs (*.EXE)\0*.EXE\0\0" 
		};
		OPENFILENAMEW ofn = {0};
		exParamValues	exParamValue_path;
		wchar_t filepath_string[sizeof(exParamValue_path.paramString)/sizeof(exParamValue_path.paramString[0])];

		paramSuite->GetParamValue(exID, mgroupIndex, pParamID_filepath,
			&exParamValue_path
		);
		copyConvertStringLiteralIntoUTF16(exParamValue_path.paramString, filepath_string);
		
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = mainWnd;
		ofn.hInstance = NULL; // ?!?
		ofn.lpstrFilter = strFilter;
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter = 0;
		ofn.nFilterIndex = 1; // point to *.EXE
		ofn.lpstrFile = filepath_string;
		ofn.nMaxFile = sizeof(filepath_string)/sizeof(filepath_string[0]);
		ofn.lpstrFileTitle = L"lpstrFileTitle";
		ofn.lpstrInitialDir = NULL;
		switch( select_button ) {
			case button_mkvmerge:	ofn.lpstrTitle = L"Specify Path to MKVMERGE.EXE application"; break;
			case button_mp4box:		ofn.lpstrTitle = L"Specify Path to Mp4Box.EXE application"; break;
			case button_tsmuxer:	ofn.lpstrTitle = L"Specify Path to TSMuxer.EXE application"; break;
			case button_neroaac:	ofn.lpstrTitle = L"Specify Path to neroAacEnc.EXE application"; break;
			default:				ofn.lpstrTitle = L"?!? INTERNAL ERROR (UNKNOWN) ?!?"; break;
		}

		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.nFileOffset = 0; // ???
		ofn.nFileExtension = 0;// ???
		ofn.lpstrDefExt = NULL; // no appended extenstion

		BOOL b = GetOpenFileNameW( &ofn );

		// if successful, copy the user-selected filepath to corresponding UI-parameter
		if ( b ) {
			copyConvertStringLiteralIntoUTF16(filepath_string, exParamValue_path.paramString);
			lRec->exportParamSuite->ChangeParam( 
				exID,
				mgroupIndex,
				pParamID_filepath,
				&exParamValue_path
			);
		}
		else {
			err = CommDlgExtendedError();
		}

		return returnValue;
	} 
	//else if ( select_button == select_other ) {

	/////////////////////////////////////
	//
	// If we made it this far, then it has to be the 'other button'
	//
	lRec->exportParamSuite->GetArbData(getFilePrefsRecP->exporterPluginID,
		getFilePrefsRecP->multiGroupIndex,
		getFilePrefsRecP->buttonParamIdentifier,
		&codecSettingsSize,
		NULL);
	if (codecSettingsSize)
	{
		// Settings valid.  Let's get them.
		lRec->exportParamSuite->GetArbData(getFilePrefsRecP->exporterPluginID,
			getFilePrefsRecP->multiGroupIndex,
			getFilePrefsRecP->buttonParamIdentifier,
			&codecSettingsSize,
			reinterpret_cast<void*>(&codecSettings));
	}
	else
	{
		codecSettings.sampleSetting = kPrFalse;
		codecSettingsSize = static_cast<csSDK_int32>(sizeof(CodecSettings));
	}

	#ifdef PRMAC_ENV
	/*
	CFStringRef				exporterName		= CFSTR (EXPORTER_NAME),
							settingString		= CFSTR (SETTING_STRING),
							yesString			= CFSTR ("Yes"),
							noString			= CFSTR ("No");
	DialogRef				alert;
	*/
	DialogItemIndex			outItemHit;
	/*
	AlertStdCFStringAlertParamRec inAlertParam = {	kStdCFStringAlertVersionOne,
													kPrTrue,
													kPrFalse,
													yesString,
													noString,
													NULL,
													kAlertStdAlertOKButton,
													kAlertStdAlertCancelButton,
													kWindowDefaultPosition,
													NULL};
	 */
	#endif

	// Show sample dialog and modify export settings
	#ifdef PRWIN_ENV
	UINT mb_flags = MB_OK;

	if ( lRec->NvGPUInfo.nvenc_supported ) {
		// Refresh the struct lRec->NvEncodeConfig
		NVENC_ExportSettings_to_EncodeConfig( exID, lRec );

		NVENC_GetEncoderCaps(lRec->NvEncodeConfig.codec, lRec->NvGPUInfo.nv_enc_caps, text);
		messagebox_text << text;
		messagebox_text << std::endl;

		lRec->NvEncodeConfig.print(text);
		messagebox_text << text;
	}
	else {
		// NVENC not supported
		mb_flags |= MB_ICONERROR;
		text = "!!! Selected GPU does not have NVENC H264 hardware-capability !!!\n";
		messagebox_text << text;
	}

	returnValue = MessageBox(	GetLastActivePopup(mainWnd),
//		SETTING_STRING,
		//messagebox_text.str().c_str(),
		text.c_str(),
		EXPORTER_NAME,
		mb_flags
	);
	#else
	
/*	[TODO] Will need to use Cocoa with NSAlertPanel
	returnValue = CreateStandardAlert (	kAlertNoteAlert,
										exporterName,
										settingString,
										&inAlertParam,
										&alert);
	returnValue = RunStandardAlert (alert, NULL, &outItemHit);
	CFRelease (exporterName);
	CFRelease (settingString);
*/	#endif
	
	#ifdef PRWIN_ENV
	if (returnValue == IDYES)
	#else
	if (outItemHit == kAlertStdAlertOKButton)
	#endif
	{
		codecSettings.sampleSetting = kPrTrue;
	}
	#ifdef PRWIN_ENV
	else if (returnValue == IDNO)
	#else
	else if (outItemHit == kAlertStdAlertOtherButton)
	#endif
	{
		codecSettings.sampleSetting = kPrFalse;
	}
	else
	{
		// If user cancelled, return this value so that host knows nothing changed
		// If the user has a preset selected, hits Codec Settings, and cancels,
		// this ensures that the preset remains selected rather than changing to Custom.
		result = exportReturn_ParamButtonCancel;
	}

	lRec->exportParamSuite->SetArbData(getFilePrefsRecP->exporterPluginID,
		getFilePrefsRecP->multiGroupIndex,
		getFilePrefsRecP->buttonParamIdentifier,
		codecSettingsSize,
		reinterpret_cast<void*>(&codecSettings)
	);
	return result;
}

// An exporter can monitor parameter changes, and modify other parameters based on the changes
prMALError
exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP)
{
	prMALError				result			= malNoError;
	csSDK_uint32			exID			= validateParamChangedRecP->exporterPluginID;
	ExportSettings			*lRec			= reinterpret_cast<ExportSettings*>(validateParamChangedRecP->privateData);
	exParamValues			changedValue, exParamValue_temp;

	if (!lRec->exportParamSuite)
		return exportReturn_ErrMemory;


	bool UI_GPUIndex_changed = strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_GPUSelect_GPUIndex) == 0;

	// kludge: always rescan the system for NVidia GPUs
	std::vector<NvEncoderGPUInfo_s> gpulist;
	unsigned int numGPUs = update_exportParamSuite_GPUSelectGroup_GPUIndex(exID, lRec, gpulist);

	lRec->exportParamSuite->GetParamValue(exID,
		validateParamChangedRecP->multiGroupIndex,
		ParamID_GPUSelect_GPUIndex,
		&changedValue);
	int GPUIndex_unv = changedValue.value.intValue; // unvalidated GPUIndex
	int GPUIndex     = GPUIndex_unv;

	// GET the changed parameter (changedValue)
	lRec->exportParamSuite->GetParamValue(exID,
		validateParamChangedRecP->multiGroupIndex,
		validateParamChangedRecP->changedParamIdentifier,
		&changedValue
	);

	// Update the exportAudio/exportVideo settings (in case they changed)
	lRec->SDKFileRec.hasAudio = validateParamChangedRecP->exportAudio ? kPrTrue : kPrFalse;
	lRec->SDKFileRec.hasVideo = validateParamChangedRecP->exportVideo ? kPrTrue : kPrFalse;

	// The #Nvidia GPUs may have changed; 
	//    Validate GPUIndex; if it's outside legal-range then force it to 0
	if ( numGPUs == 0 )
		GPUIndex = -1; // No NVidia GPUs detected, set to special-value
	else if ( GPUIndex >= numGPUs )
		GPUIndex = numGPUs - 1;  // ceiling (GPUIndex selected a GPU# that no longer exists)

	//
	// If the GPUIndex changed (either due to user or system-update), then
	// update the GPUSelect group (which shows the readout of name/vram/cc)
	//
	if ( UI_GPUIndex_changed || (GPUIndex != GPUIndex_unv) )
	{
		//lRec->SDKFileRec.width = changedValue.value.intValue;

		//
		// Update the read-only section of the GPU-group.
		//
		// (These parameters are non-adjustable and not used for input; they are only used 
		//  to report information back to the user.)
		lRec->NvGPUInfo = gpulist[ (numGPUs>0) ? GPUIndex : 0];
		update_exportParamSuite_GPUSelectGroup(
			exID,		// exporterID
			lRec,		// ExportSettings
			GPUIndex,	// select which NVidia GPU to use
			lRec->NvGPUInfo
		);

		// In case the NVENC capabilities changed, update everything
		update_exportParamSuite_NVENCCfgGroup( exID, lRec );
		update_exportParamSuite_VideoGroup( exID, lRec );
	}

	// Sometimes the if/else tree doesn't work as expected - 
	//   It seems that the first (if) executes most of the time, and the remaining else/if's
	//   don't always get evaluated when a parameter-change notification comes in.
	if ((strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_CODEC) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_PRESET) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_H264_PROFILE) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_HEVC_PROFILE) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_LEVEL_H264) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_LEVEL_HEVC) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_TIER_HEVC) == 0) )
	{
		// Codec change -
		if (strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_CODEC) == 0) {
			// Codec-change: this requires that we update lRec now (for lRec->NvEncodeConfig.codec)
			NVENC_ExportSettings_to_EncodeConfig(exID, lRec);// capture the new codec
		}

		// Parameter value fixup: 
		//   If PROFILE was downgraded, then also downgrade the preset if it is lossless.
		if ( strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_NV_ENC_H264_PROFILE) == 0){
			lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_NV_ENC_PRESET, &exParamValue_temp);
			int preset = exParamValue_temp.value.intValue; // preset
			if ( (changedValue.value.intValue < NV_ENC_H264_PROFILE_HIGH_444) &&
				(preset == NV_ENC_PRESET_LOSSLESS_DEFAULT ||
				preset == NV_ENC_PRESET_LOSSLESS_HP) )
			{
				// downgrade the lossless-reset to BluRay preset.
				exParamValue_temp.value.intValue = NV_ENC_PRESET_BD;
				lRec->exportParamSuite->ChangeParam(exID, 0, ParamID_NV_ENC_PRESET, &exParamValue_temp);
			}
		}

		update_exportParamSuite_NVENCCfgGroup( exID, lRec );
		update_exportParamSuite_VideoGroup( exID, lRec );
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_chromaFormatIDC) == 0) {
		// update the useChroma444hack button
		update_exportParamSuite_NVENCCfgGroup( exID, lRec );
		// update the allowed formats in forced_PrPixelFormat box (VideoGroup)
		update_exportParamSuite_VideoGroup(exID, lRec);
	}
	else if ((strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_rateControl) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_numBFrames) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_max_ref_frames) == 0)
		)
	{
		// rate-control mode change affects the qp/qpI/qpP/qpB Params in GroupID_NVENCCfg,
		//	and the ADBEVideoMaxBitrate, ADBEVideoTargetBitrate Param

		// max_ref_frames affects numBFrames
		// numBFrames affects qpB
		update_exportParamSuite_NVENCCfgGroup( exID, lRec );
		update_exportParamSuite_VideoGroup( exID, lRec );
	}
	else if (
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_qpI) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_qpP) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_qpB) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_min_qpI) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_min_qpP) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_min_qpB) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_max_qpI) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_max_qpP) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_max_qpB) == 0)
		)
	{
		// min_qp* can never exceed max_qp*.  Update GroupID_NVENCCfg
		update_exportParamSuite_NVENCCfgGroup( exID, lRec );
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEMPEGCodecBroadcastStandard) == 0)
	{
		lRec->SDKFileRec.tvformat = changedValue.value.intValue;
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVideoWidth) == 0)
	{
		lRec->SDKFileRec.width = changedValue.value.intValue;
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVideoHeight) == 0)
	{
		lRec->SDKFileRec.height = changedValue.value.intValue;
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVideoAspect) == 0)
	{
		lRec->SDKFileRec.pixelAspectNum = changedValue.value.ratioValue.numerator;
		lRec->SDKFileRec.pixelAspectDen = changedValue.value.ratioValue.denominator;
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVideoFPS) == 0)
	{
		/* [TODO - ZL] Need to convert ticks to frame rate
		lRec->SDKFileRec.value = changedValue.value.timeValue;
		lRec->SDKFileRec.sampleSize = changedValue.value.timeValue;
		*/
	}
	else if ( (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVideoMaxBitrate) == 0) ||
		(strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVideoTargetBitrate) == 0))
	{
		update_exportParamSuite_VideoGroup(exID, lRec);
	}
	else if ( (strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_forced_PrPixelFormat) == 0))
	{
		lRec->forced_PixelFormat0 = changedValue.optionalParamEnabled ? 
			true : // If param is enabled, then user will force the pixelformat
			false;
		update_exportParamSuite_VideoGroup(exID, lRec);
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEAudioRatePerSecond) == 0)
	{
		lRec->SDKFileRec.sampleRate = changedValue.value.floatValue;
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEAudioCodec) == 0 )
	{
		// if the audio-format is changed, need to (hide/unhide) the Path Parameter(s)
		update_exportParamSuite_AudioFormatGroup(exID, lRec);

		// change in audio-format possibly requires change in AAC AudioBitRate range
		update_exportParamSuite_BasicAudioGroup(exID, lRec);
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEAudioNumChannels) == 0 )
	{
		// change in audio-channels possibly requires change in AAC AudioBitRate range
		update_exportParamSuite_BasicAudioGroup(exID, lRec);
	}
	else if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVMCMux_Type) == 0 ||
		strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_BasicMux_TSMUXER_Path) == 0 ||
		strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_BasicMux_MKVMERGE_Path) == 0 ||
		strcmp(validateParamChangedRecP->changedParamIdentifier, ParamID_BasicMux_MP4BOX_Path) == 0)
	{
		// if Multiplexer-type is changed, need to (hide/unhide) the Path Parameter(s)
		update_exportParamSuite_NVENCMultiplexerGroup(exID, lRec);

		// also, if necessary, force AudioFormat to AAC (because MP4-muxing doesn't support PCM)
		if (strcmp(validateParamChangedRecP->changedParamIdentifier, ADBEVMCMux_Type) == 0) {
			update_exportParamSuite_AudioFormatGroup(exID, lRec);
			update_exportParamSuite_BasicAudioGroup(exID, lRec);
		}
	}

	// copy the Ppro plugin settings into lRec.NvEncodeConfig
	NVENC_ExportSettings_to_EncodeConfig( exID, lRec );
	return result;
}

prMALError
exSDKValidateOutputSettings( // used by selector exSelValidateOutputSettings
	exportStdParms		*stdParmsP, 
	exValidateOutputSettingsRec *validateOutputSettingsRec
) {
	prUTF16Char title[256];
	prUTF16Char desc[256];
	ExportSettings *privateData = reinterpret_cast<ExportSettings *>(validateOutputSettingsRec->privateData);
	csSDK_uint32			exID			= validateOutputSettingsRec->exporterPluginID;
	PrSDKExportParamSuite	*paramSuite	= privateData->exportParamSuite;
	csSDK_int32		mgroupIndex = 0;
	HWND mainWnd = privateData->windowSuite->GetMainWindow();
	csSDK_int32		muxType;

	exParamValues exParamValue;
	paramSuite->GetParamValue( exID, mgroupIndex, ADBEVMCMux_Type, &exParamValue);
	muxType = exParamValue.value.intValue;

	// Premiere Pro CC - exportStdParamSuite doesn't work
	//exQueryOutputSettingsRec outputSettingsRec;
	//privateData->exportStdParamSuite->QueryOutputSettings(exID, &outputSettingsRec);

	/*
	// If the selected NVidia GPU doesn't support NVENC, or if no NVidia GPU is present.
	// then return an error, otherwise we are good to go!
	if ( privateData->SDKFileRec.hasAudio && privateData->SDKFileRec.hasVideo ) {
		copyConvertStringLiteralIntoUTF16( L"NVENC-export error", title );
		copyConvertStringLiteralIntoUTF16( L"Cannot export both <audio + video> simultaneously!", desc );
		privateData->errorSuite->SetEventStringUnicode( PrSDKErrorSuite::kEventTypeError, title, desc );

		MessageBox(	GetLastActivePopup(mainWnd),
								msgtext.c_str(),
								EXPORTER_NAME,
								MB_OK  );
								//| MB_RIGHT );

		return exportReturn_ErrLastErrorSet; // NVENC not supported on this hardware
	}
	else*/
	if ( (validateOutputSettingsRec->fileType == SDK_FILE_TYPE_M4V) && !privateData->NvGPUInfo.nvenc_supported ) {
		wostringstream oss; // text scratchpad for messagebox and errormsg 
		copyConvertStringLiteralIntoUTF16( L"NVENC-export error", title );
		oss << "!!! NVENC_EXPORT error, can not confirm user settings !!!" << endl << endl;
		oss << "  Reason: selected GPU does not have NVENC hardware capability" << endl << endl;
		oss << "Solution: If more than 1 NVidia GPU is installed, verify the selected GPU has NVENC capability!" << endl;
		oss << "          Otherwise, you will need to install a supported NVidia graphics adapter!" << endl;

		copyConvertStringLiteralIntoUTF16( L"Cannot export because the selected GPU does not have NVENC capability!", desc );
		privateData->errorSuite->SetEventStringUnicode( PrSDKErrorSuite::kEventTypeError, title, desc );

		MessageBoxW( GetLastActivePopup(mainWnd),
								oss.str().c_str(),
								EXPORTER_NAME_W,
								MB_OK | MB_ICONERROR );
		return exportReturn_ErrLastErrorSet; // NVENC not supported on this hardware
	}
	
	// if Multiplxer is set to *.TS output (mpeg-2 transport stream), then 
	//    verify TSMUXER.EXE path is valid

	if ( muxType == MUX_MODE_M2T ) {
		paramSuite->GetParamValue( exID, mgroupIndex, ParamID_BasicMux_TSMUXER_Path, &exParamValue);
		wchar_t parent_executable[ MAX_PATH ];
		HINSTANCE h;
		
		h = FindExecutableW( exParamValue.paramString, NULL, parent_executable );
		if ( reinterpret_cast<int>(h) <= 32) {
			wostringstream oss; // text scratchpad for messagebox and errormsg 

			// ERROR
			oss << "!!! NVENC_EXPORT error, can not confirm user settings !!!" << endl << endl;
			oss << "  Reason: TSMuxer.EXE path is invalid: " << exParamValue.paramString << endl << endl;
			oss << "Solution: Check the <Multiplexer> tab and confirm path (or change MuxType to NONE)" << endl;

			copyConvertStringLiteralIntoUTF16( L"NVENC-export error", title );
			copyConvertStringLiteralIntoUTF16( oss.str().c_str(), desc );
			privateData->errorSuite->SetEventStringUnicode( PrSDKErrorSuite::kEventTypeError, title, desc );

			MessageBoxW( GetLastActivePopup(mainWnd),
								oss.str().c_str(),
								EXPORTER_NAME_W,
								MB_OK | MB_ICONERROR );

			return exportReturn_ErrLastErrorSet; // NVENC is supported
		}
	}

	// if Multiplexer is set to *.MP4 output (mpeg-4 system stream), then 
	//    verify MP4BOX.EXE path is valid
	if ( muxType == MUX_MODE_MP4 ) {
		paramSuite->GetParamValue( exID, mgroupIndex, ParamID_BasicMux_MP4BOX_Path, &exParamValue);
		wchar_t parent_executable[ MAX_PATH ];
		HINSTANCE h;
		
		h = FindExecutableW( exParamValue.paramString, NULL, parent_executable );
		if ( reinterpret_cast<int>(h) <= 32) {
			wostringstream oss; // text scratchpad for messagebox and errormsg 

			// ERROR
			oss << "!!! NVENC_EXPORT error, can not confirm user settings !!!" << endl << endl;
			oss << "  Reason: MP4BOX.EXE path is invalid: " << exParamValue.paramString << endl << endl;
			oss << "Solution: Check the <Multiplexer> tab and confirm path (or change MuxType to NONE)" << endl;

			copyConvertStringLiteralIntoUTF16( L"NVENC-export error", title );
			copyConvertStringLiteralIntoUTF16( oss.str().c_str(), desc );
			privateData->errorSuite->SetEventStringUnicode( PrSDKErrorSuite::kEventTypeError, title, desc );

			MessageBoxW( GetLastActivePopup(mainWnd),
								oss.str().c_str(),
								EXPORTER_NAME_W,
								MB_OK | MB_ICONERROR );

			return exportReturn_ErrLastErrorSet; // NVENC is supported
		}
	}

	// if Multiplexer is set to *.MKV output (Matroska file), then 
	//    verify MKVMERGE.EXE path is valid
	if (muxType == MUX_MODE_MKV) {
		paramSuite->GetParamValue(exID, mgroupIndex, ParamID_BasicMux_MKVMERGE_Path, &exParamValue);
		wchar_t parent_executable[MAX_PATH];
		HINSTANCE h;

		h = FindExecutableW(exParamValue.paramString, NULL, parent_executable);
		if (reinterpret_cast<int>(h) <= 32) {
			wostringstream oss; // text scratchpad for messagebox and errormsg 

			// ERROR
			oss << "!!! NVENC_EXPORT error, can not confirm user settings !!!" << endl << endl;
			oss << "  Reason: MKVMERGE.EXE path is invalid: " << exParamValue.paramString << endl << endl;
			oss << "Solution: Check the <Multiplexer> tab and confirm path (or change MuxType to NONE)" << endl;

			copyConvertStringLiteralIntoUTF16(L"NVENC-export error", title);
			copyConvertStringLiteralIntoUTF16(oss.str().c_str(), desc);
			privateData->errorSuite->SetEventStringUnicode(PrSDKErrorSuite::kEventTypeError, title, desc);

			MessageBoxW(GetLastActivePopup(mainWnd),
				oss.str().c_str(),
				EXPORTER_NAME_W,
				MB_OK | MB_ICONERROR);

			return exportReturn_ErrLastErrorSet; // NVENC is supported
		}
	}

	// if AudioCodec is set to *.AAC output, then 
	//    verify NEROAACENC.EXE path is valid

	paramSuite->GetParamValue( exID, mgroupIndex, ADBEAudioCodec, &exParamValue);
	//if ( privateData->SDKFileRec.hasAudio && exParamValue.value.intValue == ADBEAudioCodec_AAC ) {
	if ( exParamValue.value.intValue == ADBEAudioCodec_AAC ) {
	//if ( outputSettingsRec.inExportAudio && exParamValue.value.intValue == ADBEAudioCodec_AAC ) {
		paramSuite->GetParamValue( exID, mgroupIndex, ParamID_AudioFormat_NEROAAC_Path, &exParamValue);
		wchar_t parent_executable[ MAX_PATH ];
		HINSTANCE h;
		
		h = FindExecutableW( exParamValue.paramString, NULL, parent_executable );
		if ( reinterpret_cast<int>(h) <= 32) {
			wostringstream oss; // text scratchpad for messagebox and errormsg 
			
			// ERROR
			oss << "!!! NVENC_EXPORT error, can not confirm user settings !!!" << endl << endl;
			oss << "  Reason: NEROAACENC.EXE path is invalid: " << exParamValue.paramString << endl << endl;
			oss << "Solution: Check the <Audio> tab and confirm path (or change AudioCodec to WAV)" << endl;

			copyConvertStringLiteralIntoUTF16( L"NVENC-export error", title );
			copyConvertStringLiteralIntoUTF16( oss.str().c_str(), desc );
			privateData->errorSuite->SetEventStringUnicode( PrSDKErrorSuite::kEventTypeError, title, desc );

			MessageBoxW( GetLastActivePopup(mainWnd),
								oss.str().c_str(),
								EXPORTER_NAME_W,
								MB_OK | MB_ICONERROR );

			return exportReturn_ErrLastErrorSet; // NVENC is supported
		}
	}

	return malNoError; // NVENC is supported
}

// NVENC_ExportSettings_to_EncodeConfig():
// ---------------------------------------
// When starts the 'encode' operation from the Adobe user-interface,
// Adobe spawns a new expoerter-instance, and none of the exporter's 
// user-defined vars are set.  This function copies the GUI selections
// from Adobe's param-interface, into nvenc_exporter's Encoder config-struct.
//
prSuiteError
NVENC_ExportSettings_to_EncodeConfig(
	const csSDK_uint32 exID,
	ExportSettings * const lRec
)
{
	exParamValues paramValue;// temporary var
	if ( lRec == NULL )
		return exportReturn_ErrMemory;

	EncodeConfig *config = &(lRec->NvEncodeConfig);
	HWND mainWnd         = lRec->windowSuite->GetMainWindow();

#define _AdobeParamToEncodeConfig(adobe_param,adobe_type,nvenc_param,nvenc_type) \
	lRec->exportParamSuite->GetParamValue(exID, 0, adobe_param, &paramValue), \
	config->##nvenc_param = static_cast<nvenc_type>( paramValue.value.adobe_type )

#define _AdobeBitRateToEncodeConfig(adobe_param,nvenc_param,nvenc_type) \
	lRec->exportParamSuite->GetParamValue(exID, 0, adobe_param, &paramValue), \
	config->##nvenc_param = static_cast<nvenc_type>( paramValue.value.floatValue*1000000.0 )

	// Set the var 'lRec->NvGPUInfo.device', which controls which NVidia GPU will be
	// used to perform the video-encode.
	std::vector<NvEncoderGPUInfo_s> gpulist;
	const csSDK_int32 numGPUs = NE_GetGPUList( gpulist );// scan system for NVidia GPUs
	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_GPUSelect_GPUIndex, &paramValue);
	const int GPUIndex       = (numGPUs > paramValue.value.intValue) ? paramValue.value.intValue : 0;
	lRec->NvGPUInfo.device   = gpulist[ GPUIndex ].device;

	_AdobeParamToEncodeConfig( ParamID_NV_ENC_CODEC, intValue, codec, NvEncodeCompressionStd );
	_AdobeParamToEncodeConfig( ParamID_NV_ENC_PRESET, intValue, preset, int );

	if ( lRec->NvEncodeConfig.codec == NV_ENC_H265) {
		_AdobeParamToEncodeConfig(ParamID_NV_ENC_HEVC_PROFILE, intValue, profile, unsigned int);
		_AdobeParamToEncodeConfig(ParamID_NV_ENC_HEVC_PROFILE, intValue, application, unsigned int); // ?
		_AdobeParamToEncodeConfig(ParamID_NV_ENC_LEVEL_HEVC, intValue, level, unsigned int);
	}
	else { // (codec == NV_ENC_H264)
		_AdobeParamToEncodeConfig(ParamID_NV_ENC_H264_PROFILE, intValue, profile, unsigned int);
		_AdobeParamToEncodeConfig(ParamID_NV_ENC_H264_PROFILE, intValue, application, unsigned int); // ?
		_AdobeParamToEncodeConfig(ParamID_NV_ENC_LEVEL_H264, intValue, level, unsigned int);
	}
	_AdobeParamToEncodeConfig(ParamID_NV_ENC_TIER_HEVC, intValue, tier, unsigned int);

	_AdobeParamToEncodeConfig( ParamID_chromaFormatIDC, intValue, chromaFormatIDC, cudaVideoChromaFormat );
	_AdobeParamToEncodeConfig( ParamID_gopLength, intValue, gopLength, unsigned int );
	if ( config->gopLength >= 999 )
		config->gopLength = 0xFFFFFFFFUL; // force to infinite

	_AdobeParamToEncodeConfig( ParamID_monoChromeEncoding, intValue, monoChromeEncoding, unsigned int );
	//_AdobeParamToEncodeConfig( "idr_period", intValue, idr_period, unsigned int );// TODO: CNvEncoder ignores this value
	config->idr_period = config->gopLength;
	_AdobeParamToEncodeConfig( ParamID_max_ref_frames, intValue, max_ref_frames, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_numBFrames, intValue, numBFrames, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_FieldEncoding, intValue, FieldEncoding, NV_ENC_PARAMS_FRAME_FIELD_MODE );

	// the ConstQP parameters are only used in ConstQP rate-control mode
	_AdobeParamToEncodeConfig( ParamID_rateControl, intValue, rateControl, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_qpPrimeYZeroTransformBypassFlag, intValue, qpPrimeYZeroTransformBypassFlag, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_vbvBufferSize, intValue, vbvBufferSize, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_vbvInitialDelay, intValue, vbvInitialDelay, unsigned int );

	_AdobeParamToEncodeConfig( ParamID_qpI, intValue, qpI, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_qpP, intValue, qpP, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_qpB, intValue, qpB, unsigned int );

	// Both MinQP & MaxQP parameters are always enabled; because NVENC appears to use these
	//   in all rate-control modes (i.e. they're always active.)
	config->min_qp_ena = true;
	_AdobeParamToEncodeConfig( ParamID_min_qpI, intValue, min_qpI, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_min_qpP, intValue, min_qpP, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_min_qpB, intValue, min_qpB, unsigned int );

	// the MaxQP parameters are only used in MinQP rate-control mode
	config->max_qp_ena = config->min_qp_ena;
	_AdobeParamToEncodeConfig( ParamID_max_qpI, intValue, max_qpI, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_max_qpP, intValue, max_qpP, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_max_qpB, intValue, max_qpB, unsigned int );

	// the initialQP parameters are only used in MinQP rate-control mode
	config->initial_qp_ena = false; // TODO (not supported yet)
	_AdobeParamToEncodeConfig( ParamID_initial_qpI, intValue, initial_qpI, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_initial_qpP, intValue, initial_qpP, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_initial_qpB, intValue, initial_qpB, unsigned int );

	_AdobeParamToEncodeConfig( ParamID_NV_ENC_H264_FMO, intValue, enableFMO, NV_ENC_H264_FMO_MODE );

	_AdobeParamToEncodeConfig( ParamID_hierarchicalP, intValue, hierarchicalP, int );
	_AdobeParamToEncodeConfig( ParamID_hierarchicalB, intValue, hierarchicalB, int );
	_AdobeParamToEncodeConfig( ParamID_sliceMode, intValue, sliceMode, int );
	_AdobeParamToEncodeConfig( ParamID_sliceModeData, intValue, sliceModeData, int );
	_AdobeParamToEncodeConfig( ParamID_vle_entropy_mode, intValue, vle_entropy_mode, NV_ENC_H264_ENTROPY_CODING_MODE );

	_AdobeParamToEncodeConfig( ParamID_separateColourPlaneFlag, intValue, separateColourPlaneFlag, unsigned int );

	_AdobeParamToEncodeConfig( ParamID_NV_ENC_MV_PRECISION, intValue, mvPrecision, NV_ENC_MV_PRECISION );
	_AdobeParamToEncodeConfig( ParamID_disableDeblock, intValue, disableDeblock, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM, intValue, adaptive_transform_mode, NV_ENC_H264_ADAPTIVE_TRANSFORM_MODE );
	_AdobeParamToEncodeConfig( ParamID_NV_ENC_H264_BDIRECT_MODE, intValue, bdirectMode, NV_ENC_H264_BDIRECT_MODE );

	_AdobeParamToEncodeConfig( ParamID_NV_ENC_HEVC_MINCUSIZE, intValue, minCUsize, NV_ENC_HEVC_CUSIZE );
	_AdobeParamToEncodeConfig( ParamID_NV_ENC_HEVC_MAXCUSIZE, intValue, maxCUsize, NV_ENC_HEVC_CUSIZE );

	_AdobeParamToEncodeConfig( ParamID_syncMode, intValue, syncMode, int );
	_AdobeParamToEncodeConfig( ParamID_enableVFR, intValue, enableVFR, unsigned int );
	_AdobeParamToEncodeConfig( ParamID_enableAQ, intValue, enableAQ, unsigned int );

	_AdobeParamToEncodeConfig( ADBEVideoWidth, intValue, width, unsigned int );
	_AdobeParamToEncodeConfig( ADBEVideoHeight, intValue, height, unsigned int );
	_AdobeParamToEncodeConfig( ADBEVideoWidth, intValue, maxWidth, unsigned int );
	_AdobeParamToEncodeConfig( ADBEVideoHeight, intValue, maxHeight, unsigned int );
	_AdobeParamToEncodeConfig( ADBEVideoWidth, intValue, curWidth, unsigned int );
	_AdobeParamToEncodeConfig( ADBEVideoHeight, intValue, curHeight, unsigned int );

	_AdobeBitRateToEncodeConfig( ADBEVideoMaxBitrate, peakBitRate, unsigned int );
	_AdobeBitRateToEncodeConfig( ADBEVideoTargetBitrate, avgBitRate, unsigned int );

	//
	// Set config->frameRateNum, config->frameRateDen
	//

	// The Plugin stores the frame-rate in terms of adobe timeunits -- need to search the array frameRates[] 
	// for the matching value; then lookup the proper ratio (N/D)
	lRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoFPS, &paramValue);
	bool fps_found = false;
	unsigned fps_index = 0;
	for( unsigned i = 0; !fps_found && (i < sizeof(frameRates)/sizeof(frameRates[0])); ++i )
		if ( paramValue.value.timeValue == frameRates[i] ) {
			fps_index = i;
			fps_found = true;
		}

	config->frameRateNum = static_cast<unsigned int>(frameRateNumDens[fps_index][0]);
	config->frameRateDen = static_cast<unsigned int>(frameRateNumDens[fps_index][1]);

	//
	// Set AspectRatio
	//

	lRec->exportParamSuite->GetParamValue(exID, 0, ADBEVideoAspect, &paramValue);
	config->darRatioX = config->curWidth * paramValue.value.ratioValue.numerator;
	config->darRatioY = config->curHeight * paramValue.value.ratioValue.denominator;

	//
	// Set TV-format
	//
	lRec->exportParamSuite->GetParamValue(exID, 0, ADBEMPEGCodecBroadcastStandard, &paramValue);
	lRec->SDKFileRec.tvformat = paramValue.value.intValue;

	//
	// NVENC advertised PrPixelFormat:
	//
	// During the first frame of the render, "forced_PrPixelFormat" determines whether
	// NVENC advertises all supported formats, or only the user-selected PrPixelFormat (i.e.
	// to force a colorimetry to Bt601/709.)
	int32_t requested_PixelFormat0;
	lRec->exportParamSuite->GetParamValue(exID, 0, ParamID_forced_PrPixelFormat, &paramValue);
	lRec->forced_PixelFormat0 = paramValue.optionalParamEnabled ? 
		true : // If param is enabled, then autoselect is disabled (false)
		false;
	desc_PrPixelFormat.index2value( paramValue.value.intValue, requested_PixelFormat0 );
	lRec->requested_PixelFormat0 = static_cast<PrPixelFormat>(requested_PixelFormat0);

	return S_OK;
}