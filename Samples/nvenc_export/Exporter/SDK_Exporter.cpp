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
/*
	Revision History
		
	Version		Change										Engineer	Date
	=======		======										========	======
	1.0			created										eks			10/11/1999
	1.1			Added note on supporting multiple			bbb			6/14/2000
				audioRate in compGetAudioIndFormat
	1.2			Converted to C++, imposed coding guidelines	bbb			10/22/2001
				fixed a supervision logic bug or two...
	1.3			Updated for Adobe Premiere 6.5				bbb			5/21/2002
	1.4			Fixed work area export						zal			1/20/2003
	1.5			Updated for Adobe Premiere Pro 1.0			zal			2/28/2003
	1.6			Fixed row padding problem					zal			8/11/2003
	2.0			Added audio support for Premiere Pro,		zal			1/6/2004
				arbitrary audio sample rates,
				multi-channel audio, pixel aspect ratio,
				and fields; code cleanup
	2.5			Updated for Adobe Premiere Pro 2.0,			zal			3/10/2006
				code cleanup
	3.0			High-bit video support (v410)				zal			6/20/2006
	4.0			Ported to new export API					zal			3/3/2008
*/

#include <PrSDKStringSuite.h>
#include "SDK_Exporter.h"
#include "SDK_Exporter_Params.h"
#include "SDK_File.h"

#include "CNVEncoder.h"
#include "CNVEncoderH264.h"
#include <sstream>
#include <cwchar>
#include <Shellapi.h> // for ShellExecute()
#include <iostream>
#include <fstream>
#include <windows.h> // CreateFile(), CloseHandle()

//
// nvenc_make_output_filename(): transforms 'src' filename into the output filename 'dst' by
//  (1) stripping off the src filename's extension
//
// Example:
//		src = "hello.mpeg"
//		postfix = "_temp_1414"
//		ext = "ts"
//
//		Result:  dst = "hello_temp_1414.ts"

void
nvenc_make_output_filename( const wstring &src, const wstring &postfix, const wstring &ext, wstring &dst )
{
	dst = src;
	size_t dst_newlen = dst.rfind( L"." );
	if ( dst_newlen != string::npos )
		dst.erase( dst_newlen, dst.size() );

	dst += postfix;
	dst += L".";
	dst += ext;
}

void
nvenc_make_output_dirname( const wstring &src, wstring &dst )
{
	dst = src;
	// Hazardous --  assume the src fileaname includes at least one '\' character 
	//               We search for that '\' character, erase it and everything after,
	//               then append ext to it.
	//
	// Example:
	//    src = "C:\TEMP\abcd\hello.mpeg"
	//    Result:  dst = "C:\TEMP\acbd"

	size_t dst_newlen = dst.rfind( L"\\" );
	dst.erase( dst_newlen, dst.size() );
}

//
// fwrite_callback() - CNvEncoder calls this function whenever it wants to write bits to the output file.
//
size_t
fwrite_callback(_In_count_x_(_Size*_Count) void * _Str, size_t _Size, size_t _Count, FILE * _File, void *privateData)
{
	exDoExportRec				*exportInfoP = (exDoExportRec*)privateData;
	prSuiteError 				resultS					= malNoError;
	csSDK_uint32				exID					= exportInfoP->exporterPluginID;
	ExportSettings				*mySettings = reinterpret_cast<ExportSettings *>(exportInfoP->privateData);
	

	//return fwrite(_Str, _Size, _Count, mySettings->SDKFileRec.FileRecord_Video.fp );
/*
	// Old, using Adobe-app's file-API
	resultS = mySettings->exportFileSuite->Write(exportInfoP->fileObject, _Str, _Size * _Count);

	if ( resultS == malNoError )
		return (_Size * _Count); // success
	else
		return 0; // failed, didn't write all bytes!
*/

	DWORD bytes_written = 0;
	BOOL wfrc = WriteFile(
		mySettings->SDKFileRec.FileRecord_Video.hfp,
		_Str,
		(_Size * _Count), // nNumberOfBytesToWrite
		&bytes_written,
		NULL // not overlapped
	);

	return wfrc ?
		bytes_written : // write is successful, return the exact #bytes written
		0; // write-error
}


prSuiteError
nvenc_initialize_h264_session( const PrPixelFormat PixelFormat0, exDoExportRec * const exportInfoP )
{
	ExportSettings	*mySettings = reinterpret_cast<ExportSettings *>(exportInfoP->privateData);
	NV_ENC_CONFIG_H264_VUI_PARAMETERS vui; // Encoder's video-usability struct (for color info)
	NVENCSTATUS nvencstatus = NV_ENC_SUCCESS; // OpenSession() return code
	HRESULT hr;
	
	//
	// The video-usability (VUI) struct describes the color-space/format of the encoded H264 video.
	//
	
	// Note, although Premiere allows on-the-fly changing of the render
	// PrPixelFormat, NVENC only sets the VUI once, at the start of the render.
	// Therefore, the NVENC-plugin MUST ensure the video-render uses the
	// same PrPixelFormat for the entire clip. 

	memset(&vui, 0, sizeof(NV_ENC_CONFIG_H264_VUI_PARAMETERS));

	vui.videoSignalTypePresentFlag = 1; // control: vui.videoFormat is valid

	// videoFormat
	// -----------
	//1 PAL
	// 2 NTSC
	//3 SECAM
	//4 MAC
	//5 Unspecified video format
	switch( mySettings->SDKFileRec.tvformat ) {
		case 0 : // NTSC
			vui.videoFormat = 2;
			break;
		case 1 : // PAL
			vui.videoFormat = 1;
			break;
		case 2 : // SECAM
			vui.videoFormat = 3;
			break;
		default: // unknown
			vui.videoFormat = 5; // unspecified
	}
		
	vui.colourDescriptionPresentFlag = 1; // control: colourMatrix, primaries, etc. are valid

	// Matrix
	//  0 = GBR
	//  1 = BT-709.5
	//  2 = unspecified
	//  3 = Rserved
	//  4 = US FC
	//  5 = ITU-R Rec. BT.470-6 System B, G (historical), BT.601-6
	//  6 = BT 601.6 525
	//  7 = SMPTE 240M
	//  8 = E-19 to E-33
	switch( PixelFormat0 ) {
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709:
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUYV_422_8u_709:
		case PrPixelFormat_UYVY_422_8u_709:
		case PrPixelFormat_VUYX_4444_8u_709:
		case PrPixelFormat_VUYA_4444_8u_709:
		case PrPixelFormat_VUYP_4444_8u_709:
		case PrPixelFormat_VUYA_4444_32f_709:
		case PrPixelFormat_VUYX_4444_32f_709:
		case PrPixelFormat_VUYP_4444_32f_709:
		case PrPixelFormat_V210_422_10u_709:
		case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709:
		case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709:
		case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709_FullRange:
			vui.colourMatrix = 1;
			break;

		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601:
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange:
		case PrPixelFormat_YUYV_422_8u_601:
		case PrPixelFormat_UYVY_422_8u_601:
		case PrPixelFormat_V210_422_10u_601:
		case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601:
		case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601:
		case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601_FullRange:
			vui.colourMatrix = 6;
			break;
		default:
			vui.colourMatrix = 2; // unspecified
	}

	// colourPrimaries
	//  0 =reserved
	//  1 = BT 709.5
	//  2 = unspecified
	//  3 = reserved
	//  ...
	vui.colourPrimaries = vui.colourMatrix;
	vui.transferCharacteristics = vui.colourPrimaries ;

	// Is the video full-range (0..255)?
	switch(PixelFormat0) {
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange:
		case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709_FullRange:
		case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601_FullRange:
			vui.videoFullRangeFlag = 1;
			break;
		default :
			vui.videoFullRangeFlag = 0; // off
	}

	//////////////////////////////
	//
	// Create H.264 based encoder -
	//
	//  ... actually, this is already done in SDK_Exporter (exSDKBeginInstance())

	//mySettings->p_NvEncoder = new CNvEncoderH264();
	//mySettings->p_NvEncoder->Register_fwrite_callback(fwrite_callback);
	if ( mySettings->p_NvEncoder == NULL ) {
		printf("\nnvEncoder Error: NVENC H.264 encoder == NULL!\n");
		assert(0); // NVENC H.264 p_NvEncoder is NULL
		return malUnknownError;
	}

	// Store the encoding job's context-info in the p_NvEncoder object,
	//    so that the fwrite_callback() will write to the correct fileHandle.
	mySettings->p_NvEncoder->m_privateData = (void *)exportInfoP;

	// Section 2.1 (Opening an Encode Session on nDeviceID)
	hr = mySettings->p_NvEncoder->OpenEncodeSession(
		mySettings->NvEncodeConfig,
		mySettings->NvGPUInfo.device,
		nvencstatus );

	// Check for a expired NVENC license-key:
	// --------------------------------------
	//  this check is here because there is no error-handling in the plugin, and
	//  this specific error-message will occur if NVidia retires/revokes the
	//  free 'trial license key' which is used by this plugin.
	if ( nvencstatus == NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY )
		NVENC_errormessage_bad_key( mySettings );

	if ( hr != S_OK ) {

		printf("\nnvEncoder Error: NVENC H.264 encoder OpenEncodeSession failure!\n");
		assert(0); // NVENC H.264 encoder OpenEncodeSession failure
		return malUnknownError;
	}

	hr = mySettings->p_NvEncoder->InitializeEncoderH264( &vui );
	if ( hr != S_OK ) {
		printf("\nnvEncoder Error: NVENC H.264 encoder initialization failure! Check input params!\n");
		assert(0); // NVENC H.264 encoder InitializeEncoderH264 failure
		return malUnknownError;
	}

	return hr;
}



BOOL
nvenc_create_neroaac_pipe(
	ExportSettings *lRec
)
{
	bool success = true;
	SECURITY_ATTRIBUTES saAttr; 
 
	printf("\n->Start of parent execution.\n");

// Set the bInheritHandle flag so pipe handles are inherited. 
 
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 

	// Create a pipe for the child process's STDOUT. 
	lRec->SDKFileRec.H_pipe_aacin = 0;
	lRec->SDKFileRec.H_pipe_wavout = 0;

	success = CreatePipe(
		&(lRec->SDKFileRec.H_pipe_aacin),  // PIPE-output (read by neroAacEnc process)
		&(lRec->SDKFileRec.H_pipe_wavout), // PIPE-input (written by nvenc_export WAVwriter)
		&saAttr,
		0
	 );

	if ( !success ) {
		if ( lRec->SDKFileRec.H_pipe_aacin )
			CloseHandle( lRec->SDKFileRec.H_pipe_aacin );
		if ( lRec->SDKFileRec.H_pipe_wavout )
			CloseHandle( lRec->SDKFileRec.H_pipe_wavout );

		return success;
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if ( ! SetHandleInformation(lRec->SDKFileRec.H_pipe_wavout, HANDLE_FLAG_INHERIT, 0) )
		return false;

	if ( ! SetHandleInformation(lRec->SDKFileRec.H_pipe_aacin, HANDLE_FLAG_INHERIT, 0) )
		return false;
 
	return true;
}

SECURITY_ATTRIBUTES saAttr; 

DllExport PREMPLUGENTRY xSDKExport (
	csSDK_int32		selector, 
	exportStdParms	*stdParmsP, 
	void			*param1, 
	void			*param2)
{
	prMALError result = exportReturn_Unsupported;
	
	switch (selector)
	{
		case exSelStartup:
			// Sent during application launch, unless the exporter has been cached.
			// A single exporter can support multiple codecs and file extensions.
			// exExporterInfoRec describes the exporter’s attributes, such as the 
			// format display name.
			result = exSDKStartup(	stdParmsP, 
									reinterpret_cast<exExporterInfoRec*>(param1));
			break;

		case exSelBeginInstance:
			// Allocate any private data.
			result = exSDKBeginInstance(stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelEndInstance:
			// Deallocate any private data.
			result = exSDKEndInstance(	stdParmsP,
										reinterpret_cast<exExporterInstanceRec*>(param1));
			break;

		case exSelGenerateDefaultParams:
			// Set the exporter’s default parameters using the Export Param Suite.
			result = exSDKGenerateDefaultParams(stdParmsP,
												reinterpret_cast<exGenerateDefaultParamRec*>(param1));
			break;

		case exSelPostProcessParams:
			// Post process parameters. This is where the localized strings for 
			// the parameter UI must be provided.
			result = exSDKPostProcessParams(stdParmsP,
											reinterpret_cast<exPostProcessParamsRec*>(param1));
			break;

		case exSelGetParamSummary:
			// Provide a text summary of the current parameter settings, which
			// will be displayed in the summary area of the Export Settings dialog.
			result = exSDKGetParamSummary(	stdParmsP,
											reinterpret_cast<exParamSummaryRec*>(param1));
			break;

		case exSelQueryOutputSettings:
			// For exporters that export to more than one file.
			result = exSDKQueryOutputSettings(	stdParmsP,
												reinterpret_cast<exQueryOutputSettingsRec*>(param1));
			break;

		case exSelQueryExportFileExtension:
			// For exporters that support more than one file extension,
			// specify an extension given the file type. If this selector
			// is not supported by the exporter, the extension is specified
			// by the exporter in exExporterInfoRec. fileTypeDefaultExtension.
			result = exSDKFileExtension(stdParmsP,
										reinterpret_cast<exQueryExportFileExtensionRec*>(param1));
			break;

		case exSelQueryOutputFileList:
			// For exporters that support more than one file extension,
			// this is called before an export for the host to find out which
			// files wold need to be overwritten.  It is called after an export
			// so the host will know about all the files created, for any pending
			// encoding tasks, such as FT.  If this selector is not supported
			// by the exporter, the host application will only know about the original
			// file path.
			result = exSDKFileList(stdParmsP,
										reinterpret_cast<exQueryOutputFileListRec*>(param1));
			break;

		case exSelParamButton:
			// Sent if exporter has one or more buttons in its parameter UI,
			// and the user clicks one of the buttons in the Export Settings.
			result = exSDKParamButton(	stdParmsP, 
										reinterpret_cast<exParamButtonRec*>(param1));
			break;

		case exSelValidateParamChanged:
			// Validate any parameters that have changed. Based on a change
			// to a parameter value, the exporter may update other parameter
			// values, or show/hide certain parameter controls, using the
			// Export Param Suite. To notify the host that the plug-in is
			// changing other parameters, set
			// exParam-ChangedRec.rebuildAllParams to a non-zero value.
			result = exSDKValidateParamChanged(	stdParmsP,
												reinterpret_cast<exParamChangedRec*>(param1));
			break;

		case exSelValidateOutputSettings:
			// The host application asks the exporter if it can export
			// with the current settings. The exporter should return
			// exportReturn_ErrLastErrorSet if not, and the error string
			// should be set to a description of the failure.
			result = exSDKValidateOutputSettings( stdParmsP, 
					reinterpret_cast<exValidateOutputSettingsRec*>(param1));
			break;

		case exSelExport:
			// Do the export! Sent when the user starts an export to the format
			// supported by the exporter, or if the exporter is used in an
			// Editing Mode and the user renders the work area.
			result = exSDKExport(	stdParmsP,
									reinterpret_cast<exDoExportRec*>(param1));
			break;
	}
	return result;
}


prMALError exSDKStartup (
	exportStdParms		*stdParmsP, 
	exExporterInfoRec	*infoRecP)
{
	prMALError result = malNoError;
	
	// Note:
	// -----
	// The exporter needs to notify the Adobe app that we can write out multiple
	// file types (*.m4v, *.wav)  Tried to install it here, but this appears
	// to cause multiple instances of NVENC-exporter to appear in the menu.

	copyConvertStringLiteralIntoUTF16(SDK_FILE_NAME, infoRecP->fileTypeName);
	infoRecP->singleFrameOnly = kPrFalse; // exporter does stills only?
	infoRecP->doesNotSupportAudioOnly = kPrFalse; // Sure we support audio-only

	// advertise 2 supported fileTypes
	switch( infoRecP->exportReqIndex ) {

		case 0 : infoRecP->fileType = SDK_FILE_TYPE_M4V; // The filetype FCC (Four Character Code)
			// fileTypeDefaultExtension is used for extension when generating preview files
			copyConvertStringLiteralIntoUTF16( EXPORTER_PLUGIN_NAME, infoRecP->fileTypeName);
//			copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_M4V, infoRecP->fileTypeDefaultExtension);
			copyConvertStringLiteralIntoUTF16(L"", infoRecP->fileTypeDefaultExtension);
			infoRecP->canExportVideo = kPrTrue;// Can compile Video, enables the Video checkbox in File > Export > Movie
			infoRecP->canExportAudio = kPrTrue;// Can compile Auieo, enables the Audio checkbox in File > Export > Movie
//			result = exportReturn_IterateExporter;// caller sets this to exportReturn_IterateExporter? to support multiple file types
			break;
/*
		case 1 : infoRecP->fileType = SDK_FILE_TYPE_WAV; // The filetype FCC (Four Character Code)
			// fileTypeDefaultExtension is used for extension when generating preview files
			copyConvertStringLiteralIntoUTF16(L"NVENC_export WAV", infoRecP->fileTypeName);
			copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_WAV, infoRecP->fileTypeDefaultExtension);
			infoRecP->canExportAudio = kPrTrue;
			result = exportReturn_IterateExporter;// caller sets this to exportReturn_IterateExporter? to support multiple file types
			break;
*/
		default : 
			result = exportReturn_IterateExporterDone;
	}
	
	infoRecP->classID			= SDK_CLSS;		// Class ID of the MAL (media abstraction layer)
	infoRecP->wantsNoProgressBar = kPrFalse;	// Let Premiere provide the progress bar
	infoRecP->hideInUI			= kPrFalse;


	// Tell Premiere which headers the exporter was compiled with
	infoRecP->interfaceVersion	= EXPORTMOD_VERSION;

	return result;
}



prMALError exSDKBeginInstance (
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result				= malNoError;
	typedef union {
		SPErr	spError;
		uint8_t b[sizeof(SPErr)];
	} SPErr_u;
	SPErr_u					a[20];       // store the reutrn-val of each AcquireSuite() call,
										 // so they can all be checked for errors at the end
	SPErr_u					spError_eps; // spError, for exportParamSuite
	ExportSettings			*mySettings;
	PrSDKMemoryManagerSuite	*memorySuite;
	csSDK_int32				exportSettingsSize	= sizeof(ExportSettings);
	SPBasicSuite			*spBasic			= stdParmsP->getSPBasicSuite();
	unsigned				suiteCtr;

	// clear the error-status array a[]
	for(suiteCtr = 0; suiteCtr < (sizeof(a)/sizeof(a[0])); ++suiteCtr )
		a[suiteCtr].spError= kSPNoError;

	suiteCtr = 0;
	if (spBasic != NULL)
	{
		a[suiteCtr++].spError = spBasic->AcquireSuite(
			kPrSDKMemoryManagerSuite,
			kPrSDKMemoryManagerSuiteVersion,
			const_cast<const void**>(reinterpret_cast<void**>(&memorySuite)));
		mySettings = reinterpret_cast<ExportSettings *>(memorySuite->NewPtrClear(exportSettingsSize));

		if (mySettings)
		{
			mySettings->spBasic		= spBasic;
			mySettings->memorySuite	= memorySuite;
// exportStdParamSuite - new in Premiere Pro CS6, but not supported in Premiere Elements 11?
//			a[suiteCtr++].spError = spBasic->AcquireSuite(
//				kPrExportStdParamSuite,
//				kPrExportStdParamSuiteVersion,
//				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportStdParamSuite))));

////////
// Adobe Premiere Elements 11 workaround:
//	CS6 SDK aliases #kPrSDKExportParamSuiteVersion -> kPrSDKExportParamSuiteVersion4, which isn't
//  supported by Elements.  The following loop tries to grab the highest-version suite supported by
//  the Adobe app.
			long eps_version = kPrSDKExportParamSuiteVersion;
			do { 
				spError_eps.spError = spBasic->AcquireSuite(
					kPrSDKExportParamSuite,
					eps_version,
					const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportParamSuite))));
				if ( spError_eps.spError ) eps_version--;
			} while ( (spError_eps.spError != 0) && (eps_version != 0) );
			mySettings->exportParamSuite_version = eps_version; // record the version we're using
			a[suiteCtr++].spError = spError_eps.spError;
// Adobe Premiere Elements 11 workaround, exportparamsuite v4 not supported
////////

			a[suiteCtr++].spError = spBasic->AcquireSuite (
				kPrSDKExportProgressSuite,
				kPrSDKExportProgressSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportProgressSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKExportFileSuite,
				kPrSDKExportFileSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportFileSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKExportInfoSuite,
				kPrSDKExportInfoSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exportInfoSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKExporterUtilitySuite,
				kPrSDKExporterUtilitySuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->exporterUtilitySuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKErrorSuite,
				kPrSDKErrorSuiteVersion3,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->errorSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKClipRenderSuite,
				kPrSDKClipRenderSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->clipRenderSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKMarkerSuite,
				kPrSDKMarkerSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->markerSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKPPixSuite,
				kPrSDKPPixSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppixSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKPPix2Suite,
				kPrSDKPPix2SuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->ppix2Suite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKAudioSuite,
				kPrSDKAudioSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->audioSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKSequenceAudioSuite,
				kPrSDKSequenceAudioSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->sequenceAudioSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKSequenceRenderSuite,
				kPrSDKSequenceRenderSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->sequenceRenderSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKTimeSuite,
				kPrSDKTimeSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->timeSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKWindowSuite,
				kPrSDKWindowSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->windowSuite))));
			a[suiteCtr++].spError = spBasic->AcquireSuite(
				kPrSDKStringSuite,
				kPrSDKStringSuiteVersion,
				const_cast<const void**>(reinterpret_cast<void**>(&(mySettings->stringSuite))));

			// verify no errors occurred during suite-acquisition
			for( suiteCtr = 0; suiteCtr < (sizeof(a)/sizeof(a[0])); ++suiteCtr )
				assert( a[suiteCtr].spError == kSPNoError );

			mySettings->SDKFileRec.width = 0;
			mySettings->SDKFileRec.height = 0;

			// Create the NVENC object.
			mySettings->p_NvEncoder = new CNvEncoderH264();

			/// Initialize mySettings->NvEncodeConfig
			mySettings->p_NvEncoder->initEncoderConfig( &mySettings->NvEncodeConfig );
			mySettings->p_NvEncoder->Register_fwrite_callback( fwrite_callback );
		}


		instanceRecP->privateData = reinterpret_cast<void*>(mySettings);
	}
	else
	{
		result = exportReturn_ErrMemory;
	}
	return result;
}


prMALError exSDKEndInstance (
	exportStdParms			*stdParmsP, 
	exExporterInstanceRec	*instanceRecP)
{
	prMALError				result		= malNoError;
	ExportSettings			*lRec		= reinterpret_cast<ExportSettings*>(instanceRecP->privateData);
	SPBasicSuite			*spBasic	= stdParmsP->getSPBasicSuite();
	PrSDKMemoryManagerSuite	*memorySuite;
	if(spBasic != NULL && lRec != NULL)
	{

		// The NVENC object was dynamically  created, so need to destroy it when the exporter plugin is exited.
		if ( lRec->p_NvEncoder ) {
			lRec->p_NvEncoder->DestroyEncoder();
			delete lRec->p_NvEncoder;
			lRec->p_NvEncoder = NULL;
		}

		
		if (lRec->exportStdParamSuite)
		{
			result = spBasic->ReleaseSuite(kPrExportStdParamSuite, kPrExportStdParamSuiteVersion);
		}
		if (lRec->exportParamSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportParamSuite, kPrSDKExportParamSuiteVersion);
		}
		if (lRec->exportProgressSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportProgressSuite, kPrSDKExportProgressSuiteVersion);
		}
		if (lRec->exportFileSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportFileSuite, kPrSDKExportFileSuiteVersion);
		}
		if (lRec->exportInfoSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExportInfoSuite, kPrSDKExportInfoSuiteVersion);
		}
		if (lRec->exporterUtilitySuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKExporterUtilitySuite, kPrSDKExporterUtilitySuiteVersion);
		}
		if (lRec->errorSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKErrorSuite, kPrSDKErrorSuiteVersion3);
		}
		if (lRec->clipRenderSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKClipRenderSuite, kPrSDKClipRenderSuiteVersion);
		}
		if (lRec->markerSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKMarkerSuite, kPrSDKMarkerSuiteVersion);
		}
		if (lRec->ppixSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPixSuite, kPrSDKPPixSuiteVersion);
		}
		if (lRec->ppix2Suite)
		{
			result = spBasic->ReleaseSuite(kPrSDKPPix2Suite, kPrSDKPPix2SuiteVersion);
		}
		if (lRec->audioSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKAudioSuite, kPrSDKAudioSuiteVersion);
		}
		if (lRec->sequenceAudioSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceAudioSuite, kPrSDKSequenceAudioSuiteVersion);
		}
		if (lRec->sequenceRenderSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKSequenceRenderSuite, kPrSDKSequenceRenderSuiteVersion);
		}
		if (lRec->timeSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKTimeSuite, kPrSDKTimeSuiteVersion);
		}
		if (lRec->windowSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKWindowSuite, kPrSDKWindowSuiteVersion);
		}
		if (lRec->stringSuite)
		{
			result = spBasic->ReleaseSuite(kPrSDKStringSuite, kPrSDKStringSuiteVersion);
		}
		if (lRec->memorySuite)
		{
			memorySuite = lRec->memorySuite;
			memorySuite->PrDisposePtr(reinterpret_cast<PrMemoryPtr>(lRec));
			result = spBasic->ReleaseSuite(kPrSDKMemoryManagerSuite, kPrSDKMemoryManagerSuiteVersion);
		}
	}

	return result;
}


// This selector is necessary so that the AME UI can provide a preview
// The bitrate value is used to provide the Estimated File Size
prMALError exSDKQueryOutputSettings(
	exportStdParms				*stdParmsP,
	exQueryOutputSettingsRec	*outputSettingsP)
{
	prMALError					result			= malNoError;
	csSDK_uint32				exID			= outputSettingsP->exporterPluginID;
	exParamValues				width,
								height,
								frameRate,
								pixelAspectRatio,
								fieldType,
								codec,
								sampleRate,
								channelType,
								videoTargetBitRate,
								audioCodec,
								AAC_bitrate; // for Compressed-formats only (AAC, AC3, MP3, etc.)
	ExportSettings				*privateData	= reinterpret_cast<ExportSettings*>(outputSettingsP->privateData);
	PrSDKExportParamSuite		*paramSuite		= privateData->exportParamSuite;
	csSDK_int32					mgroupIndex		= 0;
	float						fps				= 0.0f;
	
	if (outputSettingsP->inExportVideo)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoWidth, &width);
		outputSettingsP->outVideoWidth = width.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoHeight, &height);
		outputSettingsP->outVideoHeight = height.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFPS, &frameRate);
		outputSettingsP->outVideoFrameRate = frameRate.value.timeValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoAspect, &pixelAspectRatio);
		outputSettingsP->outVideoAspectNum = pixelAspectRatio.value.ratioValue.numerator;
		outputSettingsP->outVideoAspectDen = pixelAspectRatio.value.ratioValue.denominator;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoFieldType, &fieldType);
		outputSettingsP->outVideoFieldType = fieldType.value.intValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoTargetBitrate, &videoTargetBitRate); // Megabps
	}
	if (outputSettingsP->inExportAudio)
	{
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioCodec, &audioCodec);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioRatePerSecond, &sampleRate);
		outputSettingsP->outAudioSampleRate = sampleRate.value.floatValue;
		outputSettingsP->outAudioSampleType = kPrAudioSampleType_32BitFloat;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioNumChannels, &channelType);
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioBitrate, &AAC_bitrate); 
		outputSettingsP->outAudioChannelType = (PrAudioChannelType)channelType.value.intValue;
	}

	// Calculate bitrate
	PrTime			ticksPerSecond	= 0;
	csSDK_uint32	videoBitrate	= 0, // bytes per second
					audioBitrate	= 0; // bytes per second
	if (outputSettingsP->inExportVideo)
	{
		privateData->timeSuite->GetTicksPerSecond(&ticksPerSecond);
		fps = static_cast<float>(ticksPerSecond) / frameRate.value.timeValue;
		paramSuite->GetParamValue(exID, mgroupIndex, ADBEVideoCodec, &codec);
		videoBitrate = static_cast<csSDK_uint32>( videoTargetBitRate.value.floatValue * (1000000.0 / 8.0) );// bytes/sec
	}
	if (outputSettingsP->inExportAudio)
	{
		switch ( audioCodec.value.intValue ) {
			case ADBEAudioCodec_PCM:
				audioBitrate = static_cast<csSDK_uint32>(
					sampleRate.value.floatValue * 2 * // 2 bytes (16-bits) per sample
					GetNumberOfAudioChannels(outputSettingsP->outAudioChannelType));
				break;

			case ADBEAudioCodec_AAC:
				audioBitrate = static_cast<csSDK_uint32>(
					AAC_bitrate.value.intValue * // AAC-bitrate in units of Kbits/sec
					(1024 >> 3) * // convert Kbits/sec into Bytes/second
					GetNumberOfAudioChannels(outputSettingsP->outAudioChannelType));
				break;
			default:
				audioBitrate = 0;
		} // switch
	}
	outputSettingsP->outBitratePerSecond = videoBitrate + audioBitrate;// bytes/sec

	// New in CS5 - return outBitratePerSecond in kbps
	outputSettingsP->outBitratePerSecond = outputSettingsP->outBitratePerSecond * 8 / 1000;

	// Not sure why, but this is the *only* place where setting SDKFileRec.hasAudio/hasVideo
	// has the intended effect.  AME/Adobe Premiere seems to spawn multiple instances of this plugin-object,
	// and the data-structs in the other instances are left with their initial-defaults (instead of
	// being properly overriden.) ?!?
	privateData->SDKFileRec.hasAudio = outputSettingsP->inExportAudio ? kPrTrue : kPrFalse;
	privateData->SDKFileRec.hasVideo = outputSettingsP->inExportVideo ? kPrTrue : kPrFalse;

	return result;
}


// If an exporter supports various file extensions, it would specify which one to use here
prMALError exSDKFileExtension ( // used by selector exSelQueryExportFileExtension
	exportStdParms					*stdParmsP, 
	exQueryExportFileExtensionRec	*exportFileExtensionRecP)
{
	csSDK_uint32	exID		= exportFileExtensionRecP->exporterPluginID;
	ExportSettings *mySettings = reinterpret_cast<ExportSettings*>(exportFileExtensionRecP->privateData);
	prMALError		result	= malNoError;
//	exQueryOutputSettingsRec outOutputSettings;
	csSDK_int32					mgroupIndex		= 0;

//  Note, can't use the ExportAudio/ExportVideo settings from this suite, because they aren't up-to-date
//	use the shadow-versions stored in SDKFileRec. (These are kept up to date.)

// Note causes the following crash in Adobe Premiere Elements 11:
//	"Premiere Elements has encountered an error.
//	 [..\..\Src\Exporter_Accessors.cpp-213"
//
// Furthermore, even in Adobe Media Encoder CS6, the returned values aren't accurate.
//	mySettings->exportStdParamSuite->QueryOutputSettings( exID, &outOutputSettings );
//	bool inExportAudio = (outOutputSettings.inExportAudio != 0);
//	bool inExportVideo = (outOutputSettings.inExportVideo != 0);
	bool inExportAudio = mySettings->SDKFileRec.hasAudio ? true : false;
	bool inExportVideo = mySettings->SDKFileRec.hasVideo ? true : false;

	// If Muxing is enabled, then mux the final output
	exParamValues mux_selection;// adobe parameter ADBEVMCMux_Type
	mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ADBEVMCMux_Type, &mux_selection );

	switch ( exportFileExtensionRecP->fileType ) {
		case SDK_FILE_TYPE_M4V : // Video plugin (H264 by NVENC)
			if ( mux_selection.value.intValue == MUX_MODE_M2T ) // MPEG-2 TS enabled?
				copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_M2T, exportFileExtensionRecP->outFileExtension);
			else if ( mux_selection.value.intValue == MUX_MODE_MP4 ) // MPEG-4 enabled?
				copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_MP4, exportFileExtensionRecP->outFileExtension);
			else if ( inExportVideo )
				copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_M4V, exportFileExtensionRecP->outFileExtension);
			else if ( inExportAudio ) {
				exParamValues audioFormat;//
				mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ADBEAudioCodec, &audioFormat );
				// Format:
				//   (1) M4A (AAC-audiostream wrapped in MPEG-4 file)
				//   (2) WAV (uncompressed PCM in RIFF WAV file)
				if ( audioFormat.value.intValue == ADBEAudioCodec_AAC )
					copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_M4A, exportFileExtensionRecP->outFileExtension);
				else
					copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_WAV, exportFileExtensionRecP->outFileExtension);
			}
			break;
/*
		case SDK_FILE_TYPE_WAV : // Audio plugin (PCM WAVE)
			copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_WAV, exportFileExtensionRecP->outFileExtension);
			break;
*/
		default:
			result = malUnknownError;
	}
	return result;
}


//
// If an exporter outputs more than 1 file, ...
//
prMALError exSDKFileList (  // used by selector exSelQueryOutputFileList()
	exportStdParms					*stdParmsP, 
	exQueryOutputFileListRec		*exportFileListRecP)
{
	prSuiteError suiteError = malNoError;
	prUTF16Char  tempString[1024];
	csSDK_uint32 tempString_length;
	prMALError result = malNoError;
	csSDK_uint32	exID			= exportFileListRecP->exporterPluginID;


	ExportSettings *mySettings = reinterpret_cast<ExportSettings*>(exportFileListRecP->privateData);
//	exQueryOutputSettingsRec outOutputSettings;

//  Note, can't use the ExportAudio/ExportVideo settings from this suite, because they aren't up-to-date
//	use the shadow-versions stored in SDKFileRec. (These are kept up to date.)
//	mySettings->exportStdParamSuite->QueryOutputSettings( exID, &outOutputSettings );// not supported in Premiere Elements 11
//	bool inExportAudio = (outOutputSettings.inExportAudio != 0);
//	bool inExportVideo = (outOutputSettings.inExportVideo != 0);
	bool inExportAudio = mySettings->SDKFileRec.hasAudio  ? true : false;
	bool inExportVideo = mySettings->SDKFileRec.hasVideo  ? true : false;

	// File-count policy:
	// ------------------
	//
	// Note, this policy only specifies the number of "production" files for
	// upload to an FTP-site.  It does not include intermediate files which
	// will be generated during the A/V encoding-process!
	//   
	// Muxing-mode	Audio	Video	Count	Files
	//				enabled	enabled	
	//  NONE		no		yes		1		*.m4v
	//  NONE		yes		no		1		*.wav
	//  NONE		yes		yes		2		*.wav + *.m4v
	//
	//  TS			yes		yes		1		*.ts
	//  TS			no		yes		1		*.ts
	//  TS			yes		no		1		*.ts
	//
	//  MP4			yes		yes		1		*.mp4
	//  MP4			no		yes		1		*.mp4
	//  MP4			yes		no		1		*.mp4

	exParamValues mux_selection;// adobe parameter ADBEVMCMux_Type
	mySettings->exportParamSuite->GetParamValue( exID, 0, ADBEVMCMux_Type, &mux_selection );

	// (1) [first time Adobe-app calls this function]
	//     Set numOutputFiles
	if ( exportFileListRecP->numOutputFiles == 0) {
		if ( mux_selection.value.intValue == MUX_MODE_M2T || mux_selection.value.intValue == MUX_MODE_MP4 )
			exportFileListRecP->numOutputFiles = 1; // All is muxed into single *.ts or *.mp4 file
		else {
			// Mux-Type: none
			//  separate audio/video file(s)
			exportFileListRecP->numOutputFiles = inExportAudio + inExportVideo;
		}
		return result;
	}

	// (1b) After the first time, 
	//     Adobe should have allocated outputFileRec[].  Verify that before continuing.

	assert( exportFileListRecP->outputFileRecs != NULL );
	if ( exportFileListRecP->outputFileRecs == NULL )
		return malUnknownError;

	// Get the original filename length (tempString_length)
	suiteError = mySettings->stringSuite->CopyToUTF16String(
		&(exportFileListRecP->path), 
		tempString,
		&tempString_length
	);

	if ( suiteError != malNoError )
		return suiteError;

	// create a copy of the (exportFileListRecP->Path), then strip out the extension
	wstring origpath = tempString; // origpath = filepath without extension
	nvenc_make_output_filename( 
		tempString, 
		L"",  // no postfix
		L"",  // no extension
		origpath
	);

	copyConvertStringLiteralIntoUTF16( origpath.c_str(), tempString );
	tempString_length = origpath.size();

	for ( unsigned i = 0; i < exportFileListRecP->numOutputFiles; ++i ) {

//		if ( exportFileListRecP->outputFileRecs[i].pathLength == 0 ) {
		if ( exportFileListRecP->outputFileRecs[i].path == NULL ) {
			// pathLength hasn't been set yet.

			// (2) This is the second time Adobe-app has called this selector.
			//     *numOutputFiles has already been set.
			//     We must set pathLength to the length of the filename-string (including the extension

			// Add +4 characters, for the string '.xxx'
			exportFileListRecP->outputFileRecs[i].pathLength = tempString_length + 4;
		}
		else {
			// (3) This is the third time Adobe-app has called this selector.
			//     Finally, we must place the full filename string into outputFileRecs[i].path

			wstring newpath = origpath; // the full filename string (with added extension)

			// (3a) Append the correct file-extension '.m4v' or '.wav'
			//
			//   audio + video:   video is file#0, audio is file#1
			//   audio-only:      file#0 ("wav")
			//   video-only:      file#0 ("m4v")

			if ( mux_selection.value.intValue == MUX_MODE_M2T ) {
				// TS-muxer: single output file (*.ts)
				newpath += SDK_FILE_EXTENSION_M2T;
			}
			else if ( mux_selection.value.intValue == MUX_MODE_MP4 ) {
				// MP4Box: single output file (*.mp4)
				newpath += SDK_FILE_EXTENSION_MP4;
			}
			else if ( (i==0) && (inExportVideo) ) {
				// Video (H264 by NVENC)
				newpath += SDK_FILE_EXTENSION_M4V;
			}
			else if ( inExportAudio ) {
				// Audio (PCM WAVE)
				newpath += SDK_FILE_EXTENSION_WAV;
			}

			// Adobe has already allocated a properly (larger) sized-string in outputFileRecs[].path,
			//   so we can safely copy over it.
			copyConvertStringLiteralIntoUTF16( newpath.c_str(), 
				exportFileListRecP->outputFileRecs[i].path );
		}

	} // for

	return result;
}

// callback function for DoMultiPassExportLoop
prSuiteError NVENC_export_FrameCompletionFunction(
	const csSDK_uint32 inWhichPass,
	const csSDK_uint32 inFrameNumber,
	const csSDK_uint32 inFrameRepeatCount,
	PPixHand inRenderedFrame,
	void* inCallbackData
)
{
	exDoExportRec	*exportInfoP	= reinterpret_cast<exDoExportRec*>(inCallbackData);
	csSDK_uint32	exID			= exportInfoP->exporterPluginID;
	ExportSettings	*mySettings		= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	char			*frameBufferP	= NULL;

	csSDK_int32		rowbytes;
	exParamValues	width, height, temp_param;
	PrPixelFormat   rendered_pixelformat; // pixelformat of the current video-frame

	EncodeFrameConfig nvEncodeFrameConfig = {0};

	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &width);
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &height);

	nvEncodeFrameConfig.height = height.value.intValue;
	nvEncodeFrameConfig.width  = width.value.intValue;

	mySettings->ppixSuite->GetPixelFormat(inRenderedFrame, &rendered_pixelformat );
	const bool		adobe_yuv420 =      // Adobe is sending Planar YUV420 (instead of packed-pixel 422/444)
		            PrPixelFormat_is_YUV420(rendered_pixelformat);

	if ( rendered_pixelformat != mySettings->rendered_PixelFormat0 ) {
		// TODO: ERROR!
	}

	// CNvEncoderH264 must know the source-video's pixelformat, in order to
	//    convert it into an NVENC compatible format (NV12 or YUV444)
	nvEncodeFrameConfig.ppro_pixelformat           = rendered_pixelformat;
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv420 = PrPixelFormat_is_YUV420( rendered_pixelformat) ;
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv444 = PrPixelFormat_is_YUV444( rendered_pixelformat );
	nvEncodeFrameConfig.ppro_pixelformat_is_uyvy422= 
		(rendered_pixelformat == PrPixelFormat_UYVY_422_8u_601) ||
		(rendered_pixelformat == PrPixelFormat_UYVY_422_8u_709);
	nvEncodeFrameConfig.ppro_pixelformat_is_yuyv422= 
		(rendered_pixelformat == PrPixelFormat_YUYV_422_8u_601) ||
		(rendered_pixelformat == PrPixelFormat_YUYV_422_8u_709);

	// NVENC picture-type: Interlaced vs Progressive
	//
	// Note that the picture-type must match the selected encoding-mode.
	// In interlaced or MBAFF-mode, NVENC still requires all sourceFrames to be tagged as fieldPics
	// (even if the sourceFrame is truly progressive.)
	mySettings->exportParamSuite->GetParamValue(exID, 0, ParamID_FieldEncoding, &temp_param);
	nvEncodeFrameConfig.fieldPicflag = ( temp_param.value.intValue == NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME ) ?
		false :
		true;
	nvEncodeFrameConfig.topField = true; 
	if ( nvEncodeFrameConfig.fieldPicflag ) {
		PrSDKExportInfoSuite	*exportInfoSuite	= mySettings->exportInfoSuite;
		PrParam	seqFieldOrder;  // video-sequence field order (top_first/bottom_first)
		exportInfoSuite->GetExportSourceInfo( exID,
										kExportInfo_VideoFieldType,
										&seqFieldOrder);
		nvEncodeFrameConfig.topField = (seqFieldOrder.mInt32 == prFieldsLowerFirst) ? false : true;
	}

	if ( adobe_yuv420 ) {
		// 4:2:0 planar (all 3 yuv[] pointers used)
		size_t       rowsize;
		csSDK_uint32 stride[3]; // #bytes per row (for each of Y/U/V planes)

		// Standard ppixSuite doesn't work with Planar frame(s),
		//    must use ppix2Suite to access them
		mySettings->ppix2Suite->GetYUV420PlanarBuffers(
			inRenderedFrame,
			PrPPixBufferAccess_ReadOnly,
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[0]),
			&stride[0],
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[1]),
			&stride[1],
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[2]),
			&stride[2]
		);
		nvEncodeFrameConfig.stride[0] = stride[0];
		nvEncodeFrameConfig.stride[1] = stride[1];
		nvEncodeFrameConfig.stride[2] = stride[2];
		mySettings->ppix2Suite->GetSize(inRenderedFrame, &rowsize);
		rowbytes = rowsize;
	}
	else {
		// Packed pixel framedata
		//   ... Either 16bpp 4:2:2 or 32bpp 4:4:4
		//
		// In packed-pixel format, only pointer[0] is used,(1 & 2 aren't)
		mySettings->ppixSuite->GetPixels(	inRenderedFrame,
											PrPPixBufferAccess_ReadOnly,
											&frameBufferP);
		mySettings->ppixSuite->GetRowBytes(inRenderedFrame, &rowbytes);
		nvEncodeFrameConfig.stride[0] = rowbytes; // Y-plane

		nvEncodeFrameConfig.yuv[0] = reinterpret_cast<unsigned char *>(&frameBufferP[0]); // Y-plane
		nvEncodeFrameConfig.stride[1] = 0; // U-plane not used
		nvEncodeFrameConfig.stride[2] = 0; // V-plane not used
		nvEncodeFrameConfig.yuv[1] = NULL;
		nvEncodeFrameConfig.yuv[2] = NULL;
	}

	// Submit the Adobe rendered frame to NVENC:
	//   (1) If NvEncoder is operating in 'async_mode', then the call will return as soon
	//       as the frame is placed in the encodeQueue.
	//   (2) if NvEncoder is operating in 'sync_mode', then call will not return until
	//       NVENC has completed encoding of this frame.
	//HRESULT hr = mySettings->p_NvEncoder->EncodeFrame( &nvEncodeFrameConfig, false );
	HRESULT hr = mySettings->p_NvEncoder->EncodeFramePPro( 
		&nvEncodeFrameConfig, 
		false // flush
	);

	return ( hr == S_OK ) ? malNoError : // no error
		malUnknownError;
}


//
// NVENC_mux_m2t() - multiplex the Audio/Video file(s) into a MPEG-2 
//                   transport bitstream by calling the third-party
//                   program "TSMUXER.EXE"
//
BOOL
NVENC_mux_m2t( 
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to TSMUXER.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings,
	const csSDK_int32 audioCodec
) {
	//ExportSettings *mySettings = reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	wstring metafilename; // name of the control-file (*.meta) to operate TSMUXER.EXE
	wstring shellargs;    // command-line shell arguments
	wstring tempdirname;  // temporary-directory
	
	// Set FileRecord_Audio.filename to the *actual* outputfile path: 'XXX.M2T'
//	nvenc_make_output_filename( outpath, SDK_FILE_EXTENSION_M2T, mySettings->SDKFileRec.FileRecord_AV.filename );
	nvenc_make_output_filename(
		outpath,
		L"", // no postfix
		L"ts", // extension
		mySettings->SDKFileRec.FileRecord_AV.filename
	);

	// create a '.meta' file for controlling TSMUXER
	{
		wostringstream postfix; // filename postfix (unique tempID)
		postfix << "_temp_" << std::dec << exporterPluginID;

		nvenc_make_output_filename(
			outpath,
			postfix.str(),  
			L"meta",
			metafilename
		);
		nvenc_make_output_dirname( outpath, tempdirname );
	}

	wofstream metafile;
	metafile.open( metafilename, ios::out | ios::trunc );
	metafile << "MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr --vbv-len=500 ";
	metafile << endl;

	if (mySettings->SDKFileRec.hasVideo ) {
		double frameRate = mySettings->NvEncodeConfig.frameRateNum;
		frameRate /= mySettings->NvEncodeConfig.frameRateDen;
		metafile << "V_MPEG4/ISO/AVC, ";
		metafile << "\"" << mySettings->SDKFileRec.FileRecord_Video.filename << "\"";
		metafile << ", fps=";

		// Configure floating-point output to print up to 5-digits after decimal (.1234)
		metafile.setf( std::ios::fixed, std::ios::floatfield );
		metafile.precision(4);
		metafile << frameRate; 
		metafile << L", insertSEI, contSPS, ar=As source ";
		metafile << endl;
	}

	if (mySettings->SDKFileRec.hasAudio) {
		if ( audioCodec == ADBEAudioCodec_PCM )
			metafile << "A_LPCM, ";
		else if ( audioCodec == ADBEAudioCodec_AAC )
			metafile << "A_AAC, "; 
		else
			metafile << "A_LPCM, "; // unknown, default
		metafile << "\"" << mySettings->SDKFileRec.FileRecord_Audio.filename << "\"";

		// For AAC-audio, neroAacEnc gave us an M4A file (AAC-bitstream wrapped in MP4 file)
		//    select the correct audiotrack, which is assumed to be track#1
		if ( audioCodec == ADBEAudioCodec_AAC )
			metafile << ", track=1"; // audiotrack#1 of the M4A file

		metafile << endl;
	}

	// finalize the metafile
	metafile.close(); 

	// Just in case the output-file already exists, delete it
	DeleteFileW( mySettings->SDKFileRec.FileRecord_AV.filename.c_str() );

	shellargs = L"\"";
	shellargs += metafilename;
	shellargs += L"\"";
	shellargs += L" ";
	shellargs += L"\"";
	shellargs += mySettings->SDKFileRec.FileRecord_AV.filename;
	shellargs += L"\"";

	SHELLEXECUTEINFOW ShExecInfo = {0};
	ShExecInfo.cbSize = sizeof(ShExecInfo);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = muxpath;// TargetFile to execute/open
	ShExecInfo.lpParameters = shellargs.c_str();
	ShExecInfo.lpDirectory = tempdirname.c_str();
	ShExecInfo.nShow = SW_SHOWNORMAL; // SW_SHOW;
	ShExecInfo.hInstApp = NULL;	

	// Now launch the external-program: TSMUXER.EXE
	BOOL rc = ShellExecuteExW(&ShExecInfo);

	// If shellexec was successful, then 
	//		wait for TSMUXER.exe to finish (could take a while...)
	if ( rc ) {
		WaitForSingleObject(ShExecInfo.hProcess,INFINITE);
		DeleteFileW( metafilename.c_str() );
	}

	// done with TS-muxing!
	return rc;
} 

//
// NVENC_mux_mp4() - multiplex the Audio/Video file(s) into a MPEG-4 stream by 
//                   calling an the third-party program "MP4BOX.EXE"
//

BOOL
NVENC_mux_mp4(
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to MP4BOX.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings
) {
	wstring metafilename; // name of the control-file (*.meta) to operate TSMUXER.EXE
	wstring shellargs;    // command-line shell arguments
	wstring tempdirname;  // temporary dir for MP4BOX
	
	// Set FileRecord_Audio.filename to the *actual* outputfile path: 'XXX.MP4'
//	nvenc_make_output_filename( outpath, SDK_FILE_EXTENSION_MP4, mySettings->SDKFileRec.FileRecord_AV.filename );
	nvenc_make_output_filename( 
		outpath,
		L"",		// no postfix (since this is the *final* output file)
		L"mp4",		// mpeg-4 extension
		mySettings->SDKFileRec.FileRecord_AV.filename
	);
	nvenc_make_output_dirname( outpath, tempdirname );

	if (mySettings->SDKFileRec.hasVideo ) {
		shellargs += L" ";
		shellargs += L"-add \"";
		shellargs += mySettings->SDKFileRec.FileRecord_Video.filename;
		shellargs += L"\"";
		shellargs += L" ";
	}

	if (mySettings->SDKFileRec.hasAudio) {
		shellargs += L" ";
		shellargs += L"-add \"";
		shellargs += mySettings->SDKFileRec.FileRecord_Audio.filename;
		shellargs += L"\"";
		shellargs += L" ";
	}

	// Specify a temporary directory (use the output-file's dir)
	shellargs += L" ";
	shellargs += L"-tmp ";
	shellargs += tempdirname;

	// Now add the output filepath
	shellargs += L" ";
	shellargs += L"\"";
	shellargs += mySettings->SDKFileRec.FileRecord_AV.filename;
	shellargs += L"\"";

	// Just in case the output-file already exists, delete it
	DeleteFileW( mySettings->SDKFileRec.FileRecord_AV.filename.c_str() );

	SHELLEXECUTEINFOW ShExecInfo = {0};
	ShExecInfo.cbSize = sizeof(ShExecInfo);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = muxpath;// TargetFile to execute/open
	ShExecInfo.lpParameters = shellargs.c_str();
	ShExecInfo.lpDirectory = tempdirname.c_str();
	ShExecInfo.nShow = SW_SHOWNORMAL; // SW_SHOW;
	ShExecInfo.hInstApp = NULL;	

	// Now launch the external-program: MP4BOX.EXE
	BOOL rc = ShellExecuteExW(&ShExecInfo);

	// If shellexec was successful, then 
	//		wait for MP4BOX.exe to finish (could take a while...)
	if ( rc )
		WaitForSingleObject(ShExecInfo.hProcess,INFINITE);
	
	// done with MP4-muxing!
	return rc;
} 



prMALError RenderAndWriteAllVideo(
	exDoExportRec	*exportInfoP,
	float			progress,
	float			videoProgress,
	PrTime			*exportDuration)
{
	prMALError		result			= malNoError;
	csSDK_uint32	exID			= exportInfoP->exporterPluginID;
	ExportSettings	*mySettings		= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	exParamValues	ticksPerFrame,
					width,
					height,
					pixelAspectRatio;
	PrTime			segmentEnd;
	prtPlaycode		playcode;
	//PrClipID		clipID;
	//ClipFrameFormat	frameFormat;
	//PPixHand		tempFrame;

	// 'PushMode' rendering is new for Adobe CS6 API
	bool			UsePushMode = true; // Assume we can use 'Push mode' (until we know otherwise)

	// logmessage generation
	std::wostringstream os;
	prUTF16Char eventTitle[256];
	prUTF16Char eventDesc[512];

	mySettings->exportParamSuite->GetParamValue (exID, 0, ADBEVideoFPS, &ticksPerFrame);
	mySettings->sequenceRenderSuite->MakeVideoRenderer(	exID,
														&mySettings->videoRenderID,
														ticksPerFrame.value.timeValue);

	// The following code is in progress to test the new custom pixel format support
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &width);
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &height);
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoAspect, &pixelAspectRatio);
	mySettings->videoSequenceParser = new VideoSequenceParser(mySettings->spBasic,
										width.value.intValue,
										height.value.intValue,
										pixelAspectRatio.value.ratioValue.numerator,
										pixelAspectRatio.value.ratioValue.denominator);
	mySettings->videoSequenceParser->ParseSequence(exportInfoP->timelineData);

	copyConvertStringLiteralIntoUTF16(L"Note from RenderAndWriteAllVideo()", eventTitle);

/*	// NVENC doesn't use this
	frameFormat.inPixelFormat = MAKE_THIRD_PARTY_CUSTOM_PIXEL_FORMAT_FOURCC('s', 'd', 'k');
	frameFormat.inWidth = width.value.intValue;
	frameFormat.inHeight = height.value.intValue;
*/
	// The render-loop operation
	// -------------------------
	// The render-loop supports 2 modes of operation:
	//
	//	(1) legacy PULL-mode processing (CS5.5 and earlier)
	//	(2) PUSH-mode processning (new, CS6 and above)
	// 
	// Common to both modes, the first frame (Frame0) is rendered using a call
	// to RenderAndWriteVideoFrame() [which in turn, calls 
	// sequenceRenderSuite->RenderVideoFrame()].  For this first-frame only,
	// the NVENC plugin advertises all compatible PixelFormat(s) ...YUV420
	// After the call returns, the render-loop checks the rendered frame's
	// PrPixelformat:
	//
	//		* If it is one of our requested YUV-format(s), then we assume PUSH-mode
	//        is supported.  (This is true for CS6)
	//        The video will be rendered using push-mode.
	//
	//		* Otherwise, if it is an RGB32-format (which is something we did not request),
	//		  then we conclude the host Adobe-app doesn't support PUSH-mode.
	//        The video will be rendered using pull-mode.
	//
	// ...All subsequent video-rendering uses the selected-mode (PUSH or PULL.)

	// PUSH-mode works as follows: 
	// ---------------------------
	//  (1) For the first frame only (frame0), call RenderAndWriteVideoFrame().
	//      This plugin advertises all compatible PixelFormat(s), then the Adobe
	//      app chooses 1 for us.
	//      (The plugin doesn't actually submit this frame to NVENC,
	//       it just wants Adobe to choose a PixelFormat.)
	//
	//  (2) After the PixelFormat selection (above), then 
	//      call DoMultiPassExportLoop() to actually encode the entire video-sequence
	//      (including Frame#0 again, since it wasn't rendered the first time around.)
	//
	//  (3) break out of for-loop()


	// PULL-mode works as follows: 
	// ---------------------------
	//  (1) For the first frame only (frame0), same as PUSH-mode (i.e. advertise
	//		yuv PixelFormats for Adobe to choose.)  Adobe will return with an 
	//		RGB PixelFormat.
	//
	//		* the loop detects the (unrequested) RGB-format, then disables PUSH-mode.
	//		  And it sets mySettings->rendered_PrPixelFormat0 to the user-requested
	//		  format, in order to manually override all future RenderVideoFrame() ops.
	//
	//  (2) After the PixelFormat override (above), the for-loop proceeds normally.
	//		RenderAndWriteVideoFrame() is called once For each video-frame (including
	//		frame#0 again.) 
	
	// Since the endTime can fall in between frames, make sure to not include any fractional trailing frames
	bool is_frame0 = true; // flag, is this the first frame being rendered?
	bool encoded_at_least_1 = false;// status, we successfully encoded at least 1 frame
	bool pre11_suppress_messages = false; // workaround for Premiere Elements 11, don't call ReportEvent

	// kludge for Premiere Elements 11 - don't call ReportEventA() when running on PRE11
#define _SafeReportEvent( id, eventtype, title, desc) \
	if ( !pre11_suppress_messages ) \
		mySettings->exporterUtilitySuite->ReportEventA( \
			id, eventtype, title, desc \
		);

	////////////////////////////////////////////////////////////////////////////
	// Video render loop (start)
	//

	for (	PrTime videoTime = exportInfoP->startTime;
			videoTime <= (exportInfoP->endTime - ticksPerFrame.value.timeValue);
			videoTime += ticksPerFrame.value.timeValue)
	{
		mySettings->videoSequenceParser->GetRTStatus(videoTime, segmentEnd, playcode);
/* NVENC plugin doesn't use this
		if (playcode == PRT_PLAYCODE_REALTIME)
		{
			clipID = mySettings->videoSequenceParser->FindClipIDAtTime(videoTime);
			mySettings->clipRenderSuite->GetNumCustomPixelFormats(
								clipID,
								&numPixelFormats);
			if (numPixelFormats > 0)
			{
				// (Really lame asynchronous rendering)
				result = mySettings->clipRenderSuite->InitiateAsyncRead(
								clipID,
								&videoTime,
								&frameFormat);
				result = mySettings->clipRenderSuite->FindFrame(
								clipID,
								&videoTime,
								&frameFormat,
								1,
								kPrTrue,
								&tempFrame);
				mySettings->ppixSuite->Dispose(tempFrame);
			}
			else
			{
				result = RenderAndWriteVideoFrame(is_frame0, false, videoTime, exportInfoP);
			}
		}
		else
		{
*/
		// Get 1 video-frame of the sequence:
		//    * If this is frame#0, then render the frame without encoding it,
		//      so that the plugin can analyze the returned PrPixelFormat
		//
		//    * For all other frames, submit the rendered frame to NVENC
		//      for H264-encoding.
		result = RenderAndWriteVideoFrame(
			is_frame0,	// flag, is this frame#0? 
			is_frame0,	// Don't Submit the rendered frame to NVENC for encoding?
			videoTime,	// timeStamp
			exportInfoP	// plugin internal data-struct (p_nvEncoder object)
		);

		// If an error occurred during video-rendering, halt the render.
		if ( result != malNoError ) {
			mySettings->video_encode_fatalerr = true;// video-encode fatal failure
			break; // halt the render (abort the for-loop)
		}

		//
		// for Frame#0 only: analyze the rendered frame's PrPixelFormat,
		//                   (because we will have Adobe render the rest of the video
		//                    to the same format)
		//
		if ( is_frame0 ) {
			PrPixelFormat adobe_selected_prpixelformat = mySettings->rendered_PixelFormat0;

			// Write informational message to Adobe's log:
			//    which pixelformat did Adobe choose for us?
			os.clear();
			os.flush();
			os << "Video Frame#0 info: Adobe rendered_PixelFormat0 = 0x"
				<< std::hex << mySettings->rendered_PixelFormat0 << " '";
			for( unsigned i = 0; i < 4; ++i )
				os <<  (static_cast<char>((adobe_selected_prpixelformat >> (i<<3)) & 0xFFU));
			os << "'" << std::endl;
			copyConvertStringLiteralIntoUTF16( os.str().c_str(), eventDesc);
			// Did Adobe-app accept or reject our requested YUV PrPixelFormat?

			if ( adobe_selected_prpixelformat == PrPixelFormat_BGRA_4444_8u ||
				adobe_selected_prpixelformat == PrPixelFormat_BGRX_4444_8u )
			{
				// Adobe rejected the YUV-pixelformat because we never requested RGB32.
				// * Assume we're running an Adobe-app that uses an older API-version 
				//   that does NOT support PushMode (such as Adobe Premiere Elements 11.)

				// Actions:
				//  *  disable PushMode, and
				//  *  manually *force* Adobe-app to render all future frames in user-chosen YUV-format.
				UsePushMode = false;// assume this Adobe-app doesn't support push-mode
				pre11_suppress_messages = true; // assume we can't even call ReportEvent() safely

				if ( mySettings->forced_PixelFormat0 )
					mySettings->rendered_PixelFormat0 = mySettings->requested_PixelFormat0;// user-selection
				else
					mySettings->rendered_PixelFormat0 = PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709;

				os.flush();
				os.clear();
				os << "*** Adobe-app rendered first video-frame using RGB instead of YUV." << std::endl;
				os << "*** NVENC plugin will force Adobe to render all video using PrPixelFormat = 0x"
					<< std::hex << mySettings->rendered_PixelFormat0 << " '";
				for( unsigned i = 0; i < 4; ++i )
					os << static_cast<unsigned char>((mySettings->rendered_PixelFormat0 >> (i<<3)) & 0xFFU);
				os << "'" << std::endl;

				copyConvertStringLiteralIntoUTF16( os.str().c_str(), eventDesc);
				_SafeReportEvent( 
					exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
				);

			} // not YUV
			else {
				// Adobe rendered 1st-frame with a compatible YUV-format, print it to a log message.
				_SafeReportEvent(
					exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
				);
			}

			// attempt to initialize the NVENC-hardware.  Failure could be due to an
			// invalid/expired license-key.
			result = nvenc_initialize_h264_session( mySettings->rendered_PixelFormat0, exportInfoP );
			if ( result != malNoError ) {
				break; // halt the render (abort the for-loop)
			}
		} // if ( is_frame0 )

		if ( is_frame0 && !UsePushMode ) {
			//
			// PULL-mode (for frame#0 only)
			//

			copyConvertStringLiteralIntoUTF16( L"Using PULL-mode to render video", eventDesc);
			_SafeReportEvent( 
				exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
			);

			// For Frame#0 only, call the renderer a second time, to 
			// render frame#0 for *real* this time (this time it will be submitted to NVENC)
			result = RenderAndWriteVideoFrame(
				false,	// flag, is this frame#0? (lie because we want to render for real this time)
				false,	// Don't Submit the rendered frame to NVENC for encoding?
				videoTime,	// timeStamp
				exportInfoP	// plugin internal data-struct (p_nvEncoder object)
			);

			// If the first frame failed to render, then this is a fatal error condition.
			// Set the fatal-flag (to signal the remainder of nvenc_export to quit),
			// and halt render now.
			if ( PrSuiteErrorFailed(result) ) {
				mySettings->video_encode_fatalerr = true;// video-encode fatal failure
				break; // halt the render (abort the for-loop)
			}
			else
				encoded_at_least_1 = true;
		} // if ( is_frame0 && !UsePushMode)

		if ( is_frame0 && UsePushMode ) {
			//
			// PUSH-mode (for all frames)
			//
			ExportLoopRenderParams ep;

			// (CS6 and above) PUSH-mode processing
			// ------------------------------------
			//
			// In push-mode: 
			//  (1) For the first frame only (frame0), call RenderAndWriteVideoFrame().
			//      This plugin advertises all compatible PixelFormat(s), then the Adobe
			//      app chooses 1 for us.
			//      (The plugin doesn't actually submit this frame to NVENC,
			//       it just wants Adobe to choose a PixelFormat.)
			//
			//  (2) After the PixelFormat selection (above), then 
			//      call DoMultiPassExportLoop() to actually encode the entire video-sequence
			//      (starting from Frame#0 again)
			//
			//  (3) break out of for-loop()

			memset( (void *)&ep, 0, sizeof(ep) );
			ep.inEndTime = exportInfoP->endTime;

			// kludge: with the above ep.inEndTime, push-mode renders +1 extra-frame 
			//         compared to pull-mode.
			//    workaround: match the #frames encoded by shortening the endtime
			//    by 1 timetick.
			//
			//  On second-thought, Adobe Premiere Pro/Media-Encoder end up rendering +1 extra-frame
			//  compared to Premiere Elements 11, so maybe the extra-frame is correct behavior?
			//ep.inEndTime -= ticksPerFrame.value.timeValue; // adjustment -1 frame
			
			// render the entire video using the PrPixelFormat from frame#0
			ep.inFinalPixelFormat = mySettings->rendered_PixelFormat0;

			ep.inRenderParamsSize = sizeof(ep);
			ep.inRenderParamsVersion = 1; // ?!? TODO
			ep.inReservedProgressPostRender = 0;
			ep.inReservedProgressPostRender = 0;
			ep.inStartTime = exportInfoP->startTime;

			copyConvertStringLiteralIntoUTF16( L"Using PUSH-mode to render video", eventDesc);
			_SafeReportEvent( 
				exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
			);

			result = mySettings->exporterUtilitySuite->DoMultiPassExportLoop(
				exID,
				&ep,
				1,
				NVENC_export_FrameCompletionFunction, // callback to plugin's completion-Fn
				(void *)exportInfoP
			);
			
			// done with encoding the entire video-sequence!  Now break out of the for-loop()
			encoded_at_least_1 = true;
			is_frame0 = false;
			break;
		} // if ( is_frame0 && UsePushMode )

		//
		// If using legacy PULL-mode (i.e. not using PUSH-mode),
		//  then each frame of the video-sequence executes the code below
		//
		progress = static_cast<float>(videoTime - exportInfoP->startTime) / static_cast<float>(*exportDuration) * videoProgress;
		result = mySettings->exportProgressSuite->UpdateProgressPercent(exID, progress);
		if (result == suiteError_ExporterSuspended)
		{
			mySettings->exportProgressSuite->WaitForResume(exID);
		}
		else if (result == exportReturn_Abort)
		{
			// Pass back the actual length exported so far
			// Since the endTime can fall in between frames, we go with the lower of the two values
			*exportDuration = videoTime + ticksPerFrame.value.timeValue - exportInfoP->startTime < *exportDuration ?
								videoTime + ticksPerFrame.value.timeValue - exportInfoP->startTime : *exportDuration;
			break; // abort further video-processing (abort the for-loop)
		}

		// clear the 'frame#0 flag' after rendering first frame
		is_frame0 = false;
	} // for

	//
	// Video render loop (end)
	////////////////////////////////////////////////////////////////////////////

	// If we successfully encoded 1 or more frame(s), then
	// notify NVENC to close out the encoded bitstream,
	if ( encoded_at_least_1 )
		mySettings->p_NvEncoder->EncodeFrame(NULL, true );

	// Free up GPU-resources allocated by NVENC
	mySettings->p_NvEncoder->DestroyEncoder();

	mySettings->sequenceRenderSuite->ReleaseVideoRenderer(exID, mySettings->videoRenderID);
	return result;
}


// Export markers and return warning
void HandleOptionalExportSetting(
	exportStdParms	*stdParmsP,
	exDoExportRec	*exportInfoP,
	ExportSettings	*mySettings,
	prMALError		*result)
{
	CodecSettings	codecSettings;
	csSDK_int32		codecSettingsSize	= static_cast<csSDK_int32>(sizeof(CodecSettings));

	mySettings->exportParamSuite->GetArbData(	exportInfoP->exporterPluginID,
												0,
												ADBEVideoCodecPrefsButton,
												&codecSettingsSize,
												NULL);
	if (codecSettingsSize)
	{
		// Settings valid.  Let's get them.
		mySettings->exportParamSuite->GetArbData(	exportInfoP->exporterPluginID,
													0,
													ADBEVideoCodecPrefsButton,
													&codecSettingsSize,
													reinterpret_cast<void*>(&codecSettings));
	}

	if (codecSettings.sampleSetting == kPrTrue)
	{
		// Write another file which contains the marker data (if any)
		if (mySettings->markerSuite)
		{
			WriteMarkerAndProjectDataToFile(stdParmsP, exportInfoP);
		}

		// Fake a warning and a info
		prUTF16Char title[256],
					description[256];
		copyConvertStringLiteralIntoUTF16(WARNING_TITLE, title);
		copyConvertStringLiteralIntoUTF16(WARNING_DESC, description);
		mySettings->errorSuite->SetEventStringUnicode(PrSDKErrorSuite3::kEventTypeWarning, title, description);
		copyConvertStringLiteralIntoUTF16(INFO_TITLE, title);
		copyConvertStringLiteralIntoUTF16(INFO_DESC, description);
		mySettings->errorSuite->SetEventStringUnicode(PrSDKErrorSuite3::kEventTypeInformational, title, description);
		if (*result == 0)
		{
			*result = exportReturn_ErrLastWarningSet;
		}
	}
}

// The main export function
prMALError exSDKExport( // used by selector exSelExport
	exportStdParms	*stdParmsP,
	exDoExportRec	*exportInfoP)
{
	prMALError					result					= malNoError;
	PrTime						exportDuration			= exportInfoP->endTime - exportInfoP->startTime;
	csSDK_uint32				exID					= exportInfoP->exporterPluginID;
	csSDK_int32					mgroupIndex		= 0;
	float						progress				= 0.0,
								videoProgress,
								audioProgress;
	ExportSettings				*mySettings				= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	PrSDKExportParamSuite		*paramSuite	= mySettings->exportParamSuite;
	prUTF16Char					filePath[1024];
	csSDK_int32					filePath_length;
	csSDK_int32					muxType, audioCodec;
	exParamValues exParamValues_muxPath;// path to muxer tool (TSMUXER or MP4BOX)
	exParamValues exParamValue;

	// Get some UI-parameter selections
	paramSuite->GetParamValue( exID, mgroupIndex, ADBEVMCMux_Type, &exParamValue );
	muxType = exParamValue.value.intValue;

	paramSuite->GetParamValue( exID, mgroupIndex, ADBEAudioCodec, &exParamValue );
	audioCodec = exParamValue.value.intValue;

	///////////////////////
	//
	// Create a string 'postfix' to be appended to any temporary-filenames; 
	// this will reduce the chance that nvenc_export inadvertently overwrites
	// other user files in the output-directory.
	//
	// Postfixes are generated as follows:
	//  (1) If muxing is enabled, then all audio/video bitstreams are tempfiles,
	//      and thus given postfixes 
	//  (2) If audio-output is enabled and output-format is AAC,
	//      then the wav-filename is given a postfix (since it becomes a tempfile)

	wostringstream wos_postfix; // filename postfixes (unique tempID)
	wos_postfix << "_temp_" << std::dec << exportInfoP->exporterPluginID; 

	const wstring postfix_str = wos_postfix.str();
	wstring v_postfix_str, wav_postfix_str, aac_postfix_str;

	// uniquify the *.aac and *.m4v filenames if output-muxer is enabled
	if ( muxType != MUX_MODE_NONE ) {
		aac_postfix_str = postfix_str;// postfix for *.m4a audio-tempfile
		v_postfix_str = postfix_str; // postfix for *.m4v/*.264 video-tempfile
	}

	if ( muxType != MUX_MODE_NONE || audioCodec == ADBEAudioCodec_AAC)
		wav_postfix_str = postfix_str;// postfix for *.wav audio-tempfile


	// Note, we expect exportAudio/exportVideo to match the most recent 
//	assert( mySettings->SDKFileRec.hasAudio == ( exportInfoP->exportAudio ? kPrTrue : kPrFalse) );
//	assert( mySettings->SDKFileRec.hasVideo == ( exportInfoP->exportVideo ? kPrTrue : kPrFalse) );
	mySettings->SDKFileRec.hasAudio = exportInfoP->exportAudio ? kPrTrue : kPrFalse;
	mySettings->SDKFileRec.hasVideo = exportInfoP->exportVideo ? kPrTrue : kPrFalse;
/*
	if ( mySettings->SDKFileRec.hasAudio != ( exportInfoP->exportAudio ? kPrTrue : kPrFalse) )
		return malUnknownError;
	else if ( mySettings->SDKFileRec.hasVideo != ( exportInfoP->exportVideo ? kPrTrue : kPrFalse) )
		return malUnknownError;
*/

	// filePath - get the path of the output-file from the Adobe-application
	//    The plugin might need to write out 2 files, so here we need to generate the
	//    paths for the *actual* output files:
	//     (1) if audio is enabled, one of the output-file's path will be "xxx.WAV"
	//     (2) if video is enabled, one of the output-file's path will be "xxx.M4V"
//	mySettings->exportFileSuite->Open(exportInfoP->fileObject);
	mySettings->exportFileSuite->GetPlatformPath(exportInfoP->fileObject, &filePath_length, filePath);

	// For progress meter, calculate how much video and audio should contribute to total progress
	if (exportInfoP->exportVideo && exportInfoP->exportAudio)
	{
		videoProgress = 0.9f;
		audioProgress = 0.1f;
	}
	else if (exportInfoP->exportVideo && !exportInfoP->exportAudio)
	{
		videoProgress = 1.0;
		audioProgress = 0.0;
	}
	else if (!exportInfoP->exportVideo && exportInfoP->exportAudio)
	{
		videoProgress = 0.0;
		audioProgress = 1.0;
	}

	//
	// (1) First step: Render and write out the Video
	//

	if (exportInfoP->exportVideo && !result )
	{
		// transfer the plugin UI settings to mySettings->NvEncodeConfig
		NVENC_ExportSettings_to_EncodeConfig( exportInfoP->exporterPluginID, mySettings );

		// Set FileRecord_Video.filename to the *actual* outputfile path:
		//   (1) '*.264'  (if Multiplexer == MP4, because mp4box requires '.264' extension)
		//   (2) '*.M4V'  (all other choices)
		nvenc_make_output_filename(
			filePath,
			v_postfix_str,          // string to uniquify this filename (if necessary)
			(muxType == MUX_MODE_MP4) ? 
				L"264" :				// mp4box requires H264-input file to have extension *.264
				SDK_FILE_EXTENSION_M4V, // other: use default extension (.m4v)
			mySettings->SDKFileRec.FileRecord_Video.filename
		);

		// Delete existing file, just in case it already exists
		DeleteFileW( mySettings->SDKFileRec.FileRecord_Video.filename.c_str() );

		// Create a new file:  the NVENC-encoder class will write the encoded video to this file
		/*
		mySettings->SDKFileRec.FileRecord_Video.fp = _wfopen( 
			mySettings->SDKFileRec.FileRecord_Video.filename.c_str(),
			L"wb"
		);*/
		mySettings->SDKFileRec.FileRecord_Video.hfp = CreateFileW(
			mySettings->SDKFileRec.FileRecord_Video.filename.c_str(),
			GENERIC_WRITE,
			0, // don't share
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
			NULL
		);

		// if videofile-creation failed, then abort the Export!
		if ( mySettings->SDKFileRec.FileRecord_Video.hfp == NULL )
			return exportReturn_ErrInUse;

		result = RenderAndWriteAllVideo(exportInfoP, progress, videoProgress, &exportDuration);
		//fclose( mySettings->SDKFileRec.FileRecord_Video.fp );
		CloseHandle( mySettings->SDKFileRec.FileRecord_Video.hfp );

		// If the video-encode failed catastrophically, quit out of everything now.
		if ( mySettings->video_encode_fatalerr )
			return result;
	} // exportVideo

	///////////////////////////////////////////////////////////
	//
	// (2) Second step: Render and write out the Audio
	//
	const bool aac_pipe_mode = false;// didn't get the Windows-pipe to work, keep this false

	// Even if user aborted export during video rendering, we'll just finish the audio to that point since it is really fast
	// and will make the export complete. How your exporter handles an abort, of course, is up to your implementation
	if (exportInfoP->exportAudio )
	{
		// AAC-output has two different output-modes:
		//   These both generate exactly the same AAC-audio file, they only differ in the
		//   use of an intermediate file or a windows-pipe.
		//
		// pipe-mode
		// ----------
		// nvenc_export WAVoutput  ---> | pipe | ---> neroAacEnc.exe <stdin> ----> *.m4a file
		//
		//   Here nvenc_export inserts the '|pipe|' in between wav-output subssystem, and the
		//   <stdin> of neroAacEnc.exe.  The neroAacEnc.exe is spawned using createProcess.
		//  Didn't work.
		//
		// Non pipe-mode
		// --------------
		//  nvenc_export WAVoutput  ---> *.wav file 
		//  ShellExecute:  neroAacEnc.exe *.wav inputfile ----> *.m4a file
		//
		//   Here nvenc_export inserts the '|pipe|' in between wav-output subssystem, and the
		//   <stdin> of neroAacEnc.exe.  Didn't work.


		// Set FileRecord_Audio.filename to the *actual* outputfile path, which is one of the following:
		//	  (1) for PCM-audio: *.wav
		//	  (2) for AAC-audio: *.aac
		nvenc_make_output_filename( 
			filePath, 
			wav_postfix_str,          // string to uniquify this filename (if necessary)
			SDK_FILE_EXTENSION_WAV,
			mySettings->SDKFileRec.FileRecord_Audio.filename
		);

		if ( !aac_pipe_mode ) {
			DeleteFileW( mySettings->SDKFileRec.FileRecord_Audio.filename.c_str() );

			// PCM-audio output:
			//   Create the *.wav output-file
			mySettings->SDKFileRec.FileRecord_Audio.hfp = CreateFileW(
				mySettings->SDKFileRec.FileRecord_Audio.filename.c_str(),
				GENERIC_WRITE,
				0, // don't share
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL
			);
		} //  if ( !aac_pipe_mode )

		// AAC-audio output:
		//   In AAC-mode, we don't create the audio output file directly, 
		//   because neroAacEnc will do that automatically. Instead,
		//   create a "logfile" for neroAacEnc.
		if (audioCodec == ADBEAudioCodec_AAC) {

			// Generate the output-filename (*.aac) by combining the 
			//   source filePath + ".aac"
			nvenc_make_output_filename(
				filePath,
				aac_postfix_str,          // string to uniquify this filename (if necessary)
				SDK_FILE_EXTENSION_M4A, // MPEG-4 audio file (AAC wrapped in an MPEG-4 stream)
				mySettings->SDKFileRec.FileRecord_Audio.filename // generate the output-filename (*.aac)
			);

			if ( aac_pipe_mode ) {

				// Create stdin/stdout pipe.  When we run neroAacEnc.exe, the pipe
				// will redirect output of nvenc_export's wav-writer into 
				// the neroAacEnc.exe process.
				if ( !nvenc_create_neroaac_pipe( mySettings ) )
					return exportReturn_ErrInUse;// failed to create pipe

				nvenc_make_output_filename( 
					filePath, 
					postfix_str, // string to uniquify this filename (if necessary)
					L"log",
					mySettings->SDKFileRec.FileRecord_AAClog.filename
				);

				// Create the logfile that neroAacEnc.exe will write to 
				DeleteFileW( mySettings->SDKFileRec.FileRecord_AAClog.filename.c_str() );
				mySettings->SDKFileRec.FileRecord_AAClog.hfp = CreateFileW(
					mySettings->SDKFileRec.FileRecord_AAClog.filename.c_str(),
					GENERIC_WRITE,
					0, // don't share
					NULL,
					CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL
				);

				// Connect the pipe-output to the FileRecord_Audio's file-handle.
				// When nvenc_export's wav-writer writes to FileRecord_Audio, 
				// the data will be piped into the neroAacEnc process.
				mySettings->SDKFileRec.FileRecord_Audio.hfp = mySettings->SDKFileRec.H_pipe_wavout;

				// Nero-AAC does not write 'raw' AAC (ADTS) files,
				//   it will wrap the AAC audio-stream in an MPEG-4 container (M4A)
				bool aac_result = NVENC_spawn_neroaacenc(
					exID,
					mySettings,
					mySettings->SDKFileRec.FileRecord_Audio.filename.c_str() // output filename
				);

				// If AAC-file doesn't exist, then something went severely wrong
				if ( !aac_result )
					return exportReturn_InternalError;
			} //  if ( aac_pipe_mode )
		} // AAC

		// sanity-check: if audiofile-creation failed, then abort the Export!
		if ( mySettings->SDKFileRec.FileRecord_Audio.hfp == NULL )
			return exportReturn_ErrInUse;

		///////////////////////////////
		//
		// Write the audio
		//

		// (1) First, create the audio-file's WAV-header, 
		//    which is written to the first 20-30 bytes of file
		result = WriteSDK_WAVHeader(stdParmsP, exportInfoP, exportDuration);

		// If header creation failed, then quit now.
		if ( result != malNoError ) {
			CloseHandle( mySettings->SDKFileRec.FileRecord_Audio.hfp );
			return result; // exportAudio encountered an error, abort now
		}

		// (2) Now render the remaining audio
		result = RenderAndWriteAllAudio(exportInfoP, exportDuration);

		//
		// Write the audio
		//
		///////////////////////////////

		CloseHandle( mySettings->SDKFileRec.FileRecord_Audio.hfp );
	} // exportAudio

	// Verify the exportAudio operation succeeded.  If it failed, then quit now.
	if ( result != malNoError )
		return result; // exportAudio encountered an error, abort now

	//
	// kludge: AAC-audio requires execution of external third-party app: neroAacenc.exe
	//
	if (exportInfoP->exportAudio && (audioCodec == ADBEAudioCodec_AAC) && aac_pipe_mode) {
		// in pipe-mode, NeroAacEnc has already been spawned.  Just need to wait for completion.
		bool aac_result = NVENC_wait_neroaacenc(
			mySettings,
			mySettings->SDKFileRec.FileRecord_Audio.filename.c_str() // output filename
		);
	}

	if (exportInfoP->exportAudio && (audioCodec == ADBEAudioCodec_AAC) && !aac_pipe_mode) {
		// Generate the input-filename (*.wav) by combining the 
		//   source filePath + ".wav"
		wstring wav_infilename;
		nvenc_make_output_filename(
			filePath,
			wav_postfix_str,      // string to uniquify this filename (if necessary)
			SDK_FILE_EXTENSION_WAV, // MPEG-4 audio file (AAC wrapped in an MPEG-4 stream)
			wav_infilename			// generate the output-filename (*.aac)
		);

		// Generate the output-filename (*.aac) by combining the 
		//   source filePath + ".aac"
		nvenc_make_output_filename(
			filePath,
			aac_postfix_str,      // string to uniquify this filename (if necessary)
			SDK_FILE_EXTENSION_M4A, // MPEG-4 audio file (AAC wrapped in an MPEG-4 stream)
			mySettings->SDKFileRec.FileRecord_Audio.filename // generate the output-filename (*.aac)
		);

		// Nero-AAC does not write 'raw' AAC (ADTS) files,
		//   it will wrap the AAC audio-stream in an MPEG-4 container (M4A)
		bool aac_result = NVENC_run_neroaacenc(
			exID,
			mySettings,
			wav_infilename.c_str(), // input filename
			mySettings->SDKFileRec.FileRecord_Audio.filename.c_str() // output filename
		);

		// If AAC-file doesn't exist, then something went severely wrong
		if ( !aac_result )
			return exportReturn_InternalError;
		
		// Once we are done with the WAV input file,
		// delete it
		DeleteFileW( wav_infilename.c_str() );
	} // ADBEAudioCodec_AAC

	//
	// (2) Done with Second step: Render and write out the Audio
	//
	///////////////////////////////////////////////////////////


	//
	// (3) Final step: Mux (or don't mux) the elementary Audio/Video file(s)
	//

	// If Muxing is enabled, then mux the final output
	BOOL mux_result = true; // assume muxing succeeded
	switch( muxType ) {
		case MUX_MODE_M2T:
			paramSuite->GetParamValue(exID, mgroupIndex, ParamID_BasicMux_TSMUXER_Path, &exParamValues_muxPath);
			mux_result = NVENC_mux_m2t(
				exID, // exporterID (used to generate unique tempfilename)
				exParamValues_muxPath.paramString, // path to TSMUXER.exe
				filePath,	// output filepath (*.ts)
				mySettings,
				audioCodec	// (if audio is present) audioFormat: *.AAC or *.WAV 
			);
			break;

		case MUX_MODE_MP4:
			paramSuite->GetParamValue(exID, mgroupIndex, ParamID_BasicMux_MP4BOX_Path, &exParamValues_muxPath);
			mux_result = NVENC_mux_mp4(
				exID, // exporterID (used to generate unique tempfilename)
				exParamValues_muxPath.paramString, // path to MP4BOX.exe
				filePath,	// output filepath (*.mp4)
				mySettings
			);
			break;
	}

	// If muxing failed, give Adobe-app a generic error 
	if ( !mux_result )
		result = exportReturn_InternalError;

	// TODO MP4
	if (result != suiteError_CompilerCompileAbort)
	{
		HandleOptionalExportSetting(stdParmsP, exportInfoP, mySettings, &result);
	}

	// Once we are done with muxing the raw audio/video input bitstreams,
	//    delete them
	if ( muxType != MUX_MODE_NONE ) {
		if (mySettings->SDKFileRec.hasVideo )
			DeleteFileW( mySettings->SDKFileRec.FileRecord_Video.filename.c_str() );
		if (mySettings->SDKFileRec.hasAudio )
			DeleteFileW( mySettings->SDKFileRec.FileRecord_Audio.filename.c_str() );
	}

//	mySettings->exportFileSuite->Close(exportInfoP->fileObject);

	return result;
}
