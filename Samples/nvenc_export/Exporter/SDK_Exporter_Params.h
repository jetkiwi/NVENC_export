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

#include	<SDK_File.h>
#include <string>
#include<deque>
#include<vector>

#define		EXPORTER_NAME					"NVENC_Export v1"
#define		EXPORTER_NAME_W					L"NVENC_Export v1"
#define		SETTING_STRING					"Turn on marker export and warnings? (Don't turn this on for rendering preview files)"

// Parameter strings
#define		GroupName_GPUSelect				L"Nvidia GPU selection"
#define		GroupID_GPUSelect				"GroupID_GPUSelect"
#define		ParamID_GPUSelect_GPUIndex		"ParamID_GPUSelect_GPUIndex" // GPU index# (int)
#define		ParamID_GPUSelect_Report_VRAM	"ParamID_GPUSelect_Report_VRAM" // (read-only) Video RAM (MBytes)
#define		ParamID_GPUSelect_Report_CCAP	"ParamID_GPUSelect_Report_CCAP"   // (read-only) compute-capability ("3.0, 3.5, ...")
#define		ParamID_GPUSelect_Report_DV		"ParamID_GPUSelect_Report_DV"// NVidia driver Version


#define		GroupName_NVENCInfo				L"NVENC Capabilities"
#define		GroupID_NVENCInfo				"GroupID_NVENCInfo"
#define		GroupName_NVENCCfg				L"NVENC Config"
#define		GroupID_NVENCCfg				"GroupID_NVENCCfg"
#define MAKE_PARAM_STRING(x) const wchar_t ParamString_ ## x[] = L#x;

        MAKE_PARAM_STRING(NV_ENC_CAPS_NUM_MAX_BFRAMES)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_FIELD_ENCODING)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_MONOCHROME)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_FMO)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_QPELMV)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_BDIRECT_MODE)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_CABAC)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_STEREO_MVC)
        MAKE_PARAM_STRING(NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES)
        MAKE_PARAM_STRING(NV_ENC_CAPS_LEVEL_MAX)
        MAKE_PARAM_STRING(NV_ENC_CAPS_LEVEL_MIN)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SEPARATE_COLOUR_PLANE)
        MAKE_PARAM_STRING(NV_ENC_CAPS_WIDTH_MAX)
        MAKE_PARAM_STRING(NV_ENC_CAPS_HEIGHT_MAX)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_INTRA_REFRESH)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE)
        MAKE_PARAM_STRING(NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION)
        MAKE_PARAM_STRING(NV_ENC_CAPS_PREPROC_SUPPORT)
        MAKE_PARAM_STRING(NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT)

#define		GroupName_TopVideo		L"Sample top-level video param group"
#define		GroupName_VideoCodec	L"Video codec group"
#define		GroupName_BasicVideo	L"Sample basic video param group"

#define		GroupName_TopAudio		L"Sample top-level audio param group"
#define		GroupName_AudioFormat	L"Audio Format Settings"
#define		GroupID_AudioFormat		"Audio Format Settings"
#define		GroupName_BasicAudio	L"Basic Audio Settings"

	// ParamID 'ADBEAudioCodec': legal choices supported by NVENC
	#define		ADBEAudioCodec_PCM	'PCMA'
	#define		ADBEAudioCodec_AAC	'aac'

#define		STR_SLIDER						L"Slider"
#define		STR_CHECKBOX					L"Checkbox"
#define		STR_SAMPLE_RATE					L"Sample Rate"
#define		STR_CHANNEL_TYPE				L"Channels"
#define		STR_CHANNEL_TYPE_MONO			L"Mono"
#define		STR_CHANNEL_TYPE_STEREO			L"Stereo"
#define		STR_CHANNEL_TYPE_51				L"5.1"
#define		STR_CHANNEL_TYPE_16CHANNEL		L"16 channel"

	#define ParamID_AudioFormat_NEROAAC_Button "NEROAAC_Button" // NeroAAC path

	#define	ParamID_AudioFormat_NEROAAC_Path	"ParamID_AudioFormat_NEROAAC_Path"
	#define	Default_AudioFormat_NEROAAC_Path	L"C:\\TEMP\\NEROAACENC\\win32\\NEROAACENC.EXE"

	///////////////////////////
	//
	// ParamIDs - Identifier-string for user-configurable parameters that
	//            will be registerd with the Adobe application
	//
	// (1) Most ParamIDs take the following form:
	//     ParamID_<GroupID>_<ParamName>
	//
	//     these can easily be matched to the GroupID which owns them.
	//
	// (2) But the ParamIDs which directly control CNVEncoderH264 config-options
	//     are not shortened to just the h264-config parameter:
	//
	//     ParamID_<EncodeConfigParam>
	//
	//     This is to avoid overly long strings in the source-code.
	//
	// (3) Adobe's predefined GroupIDs and ParamIDs are are used unmodified:
	//     most of them start with the string "ADBE*"
	//
	// (4) LParamID_* is a wchar_t version of ParamID_*.  Most of them are set to
	//     the exact-same string as the matching ParamID_*, so that the
	//     parameter shows up with the same string in the actual plugin user-interface.
	//     (Hence making debug easier.)

		#define ParamID_NV_ENC_CODEC "NV_ENC_CODEC"
		#define LParamID_NV_ENC_CODEC L"NV_ENC_CODEC"
		#define ParamID_NV_ENC_PRESET "NV_ENC_PRESET"
		#define LParamID_NV_ENC_PRESET L"NV_ENC_PRESET"
		#define ParamID_NV_ENC_H264_PROFILE "NV_ENC_H264_PROFILE"
		#define LParamID_NV_ENC_H264_PROFILE L"NV_ENC_H264_PROFILE"
		#define ParamID_NV_ENC_LEVEL_H264 "NV_ENC_LEVEL_H264"
		#define LParamID_NV_ENC_LEVEL_H264 L"NV_ENC_LEVEL_H264"
		#define ParamID_gopLength "gopLength"
		#define LParamID_gopLength L"gopLength"
		//#define "idr_period" // TODO: CNvEncoder ignores this value
		#define ParamID_max_ref_frames "max_ref_frames"
		#define LParamID_max_ref_frames L"max_ref_frames"
		#define ParamID_numBFrames "numBFrames"
		#define LParamID_numBFrames L"numBFrames"
		#define ParamID_FieldEncoding "FieldEncoding"
		#define LParamID_FieldEncoding L"FieldEncoding"
		#define ParamID_rateControl "rateControl"
		#define LParamID_rateControl L"rateControl"
		#define ParamID_qpI "qpI"
		#define LParamID_qpI L"qpI"
		#define ParamID_qpP "qpP"
		#define LParamID_qpP L"qpP"
		#define ParamID_qpB "qpB"
		#define LParamID_qpB L"qpB"
		#define ParamID_min_qpI "min_qpI"
		#define LParamID_min_qpI L"min_qpI"
		#define ParamID_min_qpP "min_qpP"
		#define LParamID_min_qpP L"min_qpP"
		#define ParamID_min_qpB "min_qpB"
		#define LParamID_min_qpB L"min_qpB"
		#define ParamID_max_qpI "max_qpI"
		#define LParamID_max_qpI L"max_qpI"
		#define ParamID_max_qpP "max_qpP"
		#define LParamID_max_qpP L"max_qpP"
		#define ParamID_max_qpB "max_qpB"
		#define LParamID_max_qpB L"max_qpB"
		#define ParamID_initial_qpI "initial_qpI"
		#define LParamID_initial_qpI L"initial_qpI"
		#define ParamID_initial_qpP "initial_qpP"
		#define LParamID_initial_qpP L"initial_qpP"
		#define ParamID_initial_qpB "initial_qpB"
		#define LParamID_initial_qpB L"initial_qpB"
		#define ParamID_bufferSize "bufferSize"
		#define LParamID_bufferSize L"bufferSize"
		#define ParamID_NV_ENC_H264_FMO "NV_ENC_H264_FMO"
		#define LParamID_NV_ENC_H264_FMO L"NV_ENC_H264_FMO"
		#define ParamID_hierarchicalP "hierarchicalP"
		#define LParamID_hierarchicalP L"hierarchicalP"
		#define ParamID_hierarchicalB "hierarchicalB"
		#define LParamID_hierarchicalB L"hierarchicalB"
		#define ParamID_numSlices "numSlices"
		#define LParamID_numSlices L"numSlices"
		#define ParamID_vle_entropy_mode "vle_entropy_mode"
		#define LParamID_vle_entropy_mode L"vle_entropy_mode"
		#define ParamID_chromaFormatIDC "chromaFormatIDC"
		#define LParamID_chromaFormatIDC L"chromaFormatIDC"
		#define ParamID_NV_ENC_MV_PRECISION "NV_ENC_MV_PRECISION"
		#define LParamID_NV_ENC_MV_PRECISION L"NV_ENC_MV_PRECISION"
		#define ParamID_disable_deblocking "disable_deblocking"
		#define LParamID_disable_deblocking L"disable_deblocking"
		#define ParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM "NV_ENC_H264_ADAPTIVE_TRANSFORM"
		#define LParamID_NV_ENC_H264_ADAPTIVE_TRANSFORM L"NV_ENC_H264_ADAPTIVE_TRANSFORM"
		#define ParamID_NV_ENC_H264_BDIRECT_MODE "NV_ENC_H264_BDIRECT_MODE"
		#define LParamID_NV_ENC_H264_BDIRECT_MODE L"NV_ENC_H264_BDIRECT_MODE"
		#define ParamID_syncMode "syncMode"
		#define LParamID_syncMode L"syncMode"
		#define ParamID_forced_PrPixelFormat "forced_PrPixelFormat"
		#define LParamID_forced_PrPixelFormat L"forced_PrPixelFormat"

		//#define L"idr_period" // TODO: CNvEncoder ignores this value

		// workaround for "exParamFlag_filePath" not working correctly,
		//   A manually-implemented button to select Path of TSMUXER
		#define ParamID_BasicMux_TSMUXER_Button "TSMUXER_Button" // dialog-button, activates Path-select dialog-box
		#define ParamID_NVENC_Info_Button "NVENC_Info_Button"

		#define ParamID_BasicMux_MP4BOX_Button "MP4BOX_Button" // dialog-button, activates Path-select dialog-box

#define		ADBEMPEGCodecBroadcastStandard "ADBEMPEGCodecBroadcastStandard" // ParamID TV-standard

#define		ADBEMultiplexerTabGroup		"ADBEAudienceTabGroup" // top-level Mux group
#define		GroupName_TopMux			L"Multiplexer"
#define		GroupID_NVENCMultiplexer	"GroupID_NVENCMultiplexer" // basic mux tab group
#define		GroupName_BasicMux			L"Basic Multiplexer param group"
#define		ADBEVMCMux_Type				"ADBEVMCMux_Type"
#define		MUX_MODE_M2T				5 // value to select "MPEG-2 TS mode" 
#define		MUX_MODE_NONE				6 // value to select "disable mux" 
#define		MUX_MODE_MP4				7 // *NVENC-only* value to select "MPEG-4 mode" 
#define		ParamID_BasicMux_TSMUXER_Path	"ParamID_BasicMux_TSMUXER_Path"
#define		Default_BasicMux_TSMUXER_Path	L"C:\\TEMP\\TSMUXER\\TSMUXER.EXE"

#define		ParamID_BasicMux_MP4BOX_Path	"ParamID_BasicMux_MP4BOX_Path"
#define		Default_BasicMux_MP4BOX_Path	L"C:\\TEMP\\MP4BOX\\MP4BOX.EXE"

prMALError exSDKGenerateDefaultParams(
	exportStdParms				*stdParms, 
	exGenerateDefaultParamRec	*generateDefaultParamRec);

prMALError exSDKPostProcessParams (
	exportStdParms			*stdParmsP, 
	exPostProcessParamsRec	*postProcessParamsRecP);

prMALError exSDKGetParamSummary (
	exportStdParms			*stdParmsP, 
	exParamSummaryRec		*summaryRecP);

prMALError exSDKParamButton(
		exportStdParms		*stdParmsP, 
		exParamButtonRec	*getFilePrefsRecP);

prMALError exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP);

//////////////////
// NVENC specific structures & routines
//

prSuiteError NVENC_SetParamName( 
	ExportSettings *lRec, 
	const csSDK_uint32 exID, 
	const char ParamGroupName[],
	const wchar_t ParamName[],
	const wchar_t ParamDescription[]
);

void
NVENC_errormessage_bad_key( ExportSettings *lRec );

unsigned
update_exportParamSuite_GPUSelectGroup_GPUIndex(
	const csSDK_uint32 exID,
	const ExportSettings *lRec,
	std::vector<NvEncoderGPUInfo_s> &gpulist
);

bool
PrPixelFormat_is_YUV420( const PrPixelFormat p );

void
NVENC_GetEncoderCaps(const nv_enc_caps_s &caps, string &s);

uint32_t
NVENC_Calculate_H264_MaxKBitRate( 
	const NV_ENC_LEVEL level, 
	const enum_NV_ENC_H264_PROFILE profile
);

uint32_t
NVENC_Calculate_H264_MaxRefFrames(
//	NV_ENC_LEVEL level, 
	int level,  // NV_ENC_LEVEL (H264)
	const uint32_t pixels // total #pixels (width * height)
);

prMALError
exSDKGenerateDefaultParams(
	exportStdParms				*stdParms, 
	exGenerateDefaultParamRec	*generateDefaultParamRec
);

unsigned
NE_GetGPUList( std::vector<NvEncoderGPUInfo_s> &gpulist );

unsigned
update_exportParamSuite_GPUSelectGroup_GPUIndex(
	const csSDK_uint32 exID,
	const ExportSettings *lRec,
	std::vector<NvEncoderGPUInfo_s> &gpulist
);

void
update_exportParamSuite_GPUSelectGroup(
	const csSDK_uint32 exID,
	const ExportSettings *lRec,
	const int GPUIndex,   // GPU-ID#, '-1' means NO NVidia GPUs found
	const NvEncoderGPUInfo_s &gpuinfo );

void
update_exportParamSuite_NVENCCfgGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec);

void
update_exportParamSuite_VideoGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec);

void
update_exportParamSuite_NVENCMultiplexerGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec
);

void
update_exportParamSuite_AudioFormatGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec
);

void
update_exportParamSuite_BasicAudioGroup(
	const csSDK_uint32 exID,
	ExportSettings *lRec
);

prMALError
exSDKPostProcessParams (
	exportStdParms			*stdParmsP, 
	exPostProcessParamsRec	*postProcessParamsRecP
);

prMALError
exSDKGetParamSummary (
	exportStdParms			*stdParmsP, 
	exParamSummaryRec		*summaryRecP);

prMALError
exSDKParamButton(
		exportStdParms		*stdParmsP, 
		exParamButtonRec	*getFilePrefsRecP);


prMALError
exSDKValidateParamChanged (
	exportStdParms		*stdParmsP, 
	exParamChangedRec	*validateParamChangedRecP);

prMALError
exSDKValidateOutputSettings( // used by selector exSelValidateOutputSettings
	exportStdParms		*stdParmsP, 
	exValidateOutputSettingsRec *validateOutputSettingsRec
);

prSuiteError
NVENC_ExportSettings_to_EncodeConfig(
	const csSDK_uint32 exID,
	ExportSettings * const lRec
);
