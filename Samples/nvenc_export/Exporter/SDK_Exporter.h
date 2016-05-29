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

#define SDK_FILE_NAME			L"NVENC_export"

#include	<PrSDKMarkerSuite.h>
#include	<PrSDKErrorSuite.h>
#include	<SDK_File.h>

#define WARNING_TITLE	L"This is the warning title"
#define WARNING_DESC	L"This is the warning description"
#define INFO_TITLE		L"This is the info title"
#define INFO_DESC		L"This is the info description"

//----------------------------------------------------------
// Prototypes


//////////////////
// helper functions

void
nvenc_make_output_filename(
	const wstring &src,
	const wstring &ext,
	wstring &dst
);

void
nvenc_make_output_dirname(
	const wstring &src,
	wstring &dst
);

size_t
fwrite_callback(
	_In_count_x_(_Size*_Count) void * _Str,
	size_t _Size, 
	size_t _Count, 
	FILE * _File, 
	void *privateData
);

prSuiteError
nvenc_initialize_h264_session(
	const PrPixelFormat PixelFormat0,
	exDoExportRec * const exportInfoP
);

// NVENC_mux_m2t() - multiplex the Audio/Video file(s) into a MPEG-2 
//                   transport bitstream by calling the third-party
//                   program "TSMUXER.EXE"
BOOL
NVENC_mux_m2t( 
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to TSMUXER.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings,
	const csSDK_int32 audioCodec
);

// NVENC_mux_mp4() - multiplex the Audio/Video file(s) into a MPEG-4 stream by 
//                   calling an the third-party program "MP4BOX.EXE"
BOOL
NVENC_mux_mp4(
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to MP4BOX.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings
);

// Declare plug-in entry point with C linkage
extern "C" {
DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParms, 
	void			*param1, 
	void			*param2);
}

prMALError exSDKStartup(
	exportStdParms					*stdParms, 
	exExporterInfoRec				*infoRec);

prMALError exSDKBeginInstance(
	exportStdParms					*stdParmsP,
	exExporterInstanceRec			*instanceRecP);

prMALError exSDKEndInstance(
	exportStdParms					*stdParmsP, 
	exExporterInstanceRec			*instanceRecP);

prMALError exSDKQueryOutputSettings(
	exportStdParms					*stdParmsP,
	exQueryOutputSettingsRec		*outputSettingsP);

prMALError exSDKFileExtension(
	exportStdParms					*stdParmsP, 
	exQueryExportFileExtensionRec	*exportFileExtensionRecP);

prMALError exSDKFileList(
	exportStdParms					*stdParmsP, 
	exQueryOutputFileListRec		*exQueryOutputFileListRecP);

prMALError exSDKExport(
	exportStdParms					*stdParms,
	exDoExportRec					*exportInfoP);

prMALError exSDKValidateOutputSettings(
	exportStdParms					*stdParms,
	exValidateOutputSettingsRec		*exportInfoP);


prSuiteError
NVENC_export_FrameCompletionFunction(
	const csSDK_uint32 inWhichPass,
	const csSDK_uint32 inFrameNumber,
	const csSDK_uint32 inFrameRepeatCount,
	PPixHand inRenderedFrame,
	void* inCallbackData
);

prMALError
RenderAndWriteAllVideo(
	exDoExportRec	*exportInfoP,
	float			progress,
	float			videoProgress,
	PrTime			*exportDuration
);

