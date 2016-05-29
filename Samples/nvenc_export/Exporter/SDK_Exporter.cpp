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
#include "SDK_File_video.h"  // video-export routines
#include "SDK_File_audio.h"  // audio-export routines
#include "SDK_File_mux.h"    // TS, MP4, MKV muxing routines

#include "CNVEncoder.h"
#include "CNVEncoderH264.h"
#include "CNVEncoderH265.h"
#include <sstream>
#include <cwchar>
#include <Shellapi.h> // for ShellExecute()
#include <iostream>
#include <fstream>
#include <windows.h> // CreateFile(), CloseHandle()

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
			//   ...Later on, if user switches to different codec (eg. HEVC), then this
			//      object will be destroyed and replaced.
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
		fps = static_cast<double>(ticksPerSecond) / frameRate.value.timeValue;
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


//
// Returns the file-extensions supported by the exporter.  If multiple file-extensions are supported, the
//   exporter chooses one in the code below
//
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
	exParamValues exParamValue_temp;// adobe parameter ADBEVMCMux_Type

	mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ADBEVMCMux_Type, &exParamValue_temp );
	const csSDK_int32 mux_selection = exParamValue_temp.value.intValue;

	switch ( exportFileExtensionRecP->fileType ) {
		case SDK_FILE_TYPE_M4V : // Video plugin (H264 by NVENC)
			if (mux_selection == MUX_MODE_M2T) { // MPEG-2 TS enabled?
				// MPEG-2 muxing (*.ts) - output extension is always *.ts
				//  (regardless of what combination of audio & video is enabled)
				copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_M2T, exportFileExtensionRecP->outFileExtension);
			} 
			else if (mux_selection == MUX_MODE_MP4) { // MPEG-4 enabled?
				// MPEG-4 muxing (*.mp4) - output extension is always *.mp4
				//  (regardless of what combination of audio & video is enabled)
				copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_MP4, exportFileExtensionRecP->outFileExtension);
			}
			else if (mux_selection == MUX_MODE_MKV) { // Matroska muxing enabled?
				// MKV muxing (*.MKV) - output extension is always *.mkv
				//  (regardless of what combination of audio & video is enabled)
				copyConvertStringLiteralIntoUTF16(SDK_FILE_EXTENSION_MKV, exportFileExtensionRecP->outFileExtension);
			}
			else if (inExportVideo) {
				// Muxing is disabled, and video-export is enabled. 
				//   (Ignore whether or not audio-export is enabled)
				mySettings->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_NV_ENC_CODEC, &exParamValue_temp);
				const csSDK_int32 videoCodec = exParamValue_temp.value.intValue;// NV_ENC_H264 | NV_ENC_H265

				switch (videoCodec) {
					case NV_ENC_H264 : copyConvertStringLiteralIntoUTF16(
							SDK_FILE_EXTENSION_M4V, exportFileExtensionRecP->outFileExtension);
						break;
					case NV_ENC_H265: copyConvertStringLiteralIntoUTF16(
							SDK_FILE_EXTENSION_HEVC, exportFileExtensionRecP->outFileExtension);
						break;
					default: // unknown?!?
								copyConvertStringLiteralIntoUTF16(
							SDK_FILE_EXTENSION_M4V, exportFileExtensionRecP->outFileExtension);
				}
			}
			else if ( inExportAudio ) {
				// Format:
				//   (1) M4A (AAC-audiostream wrapped in MPEG-4 file)
				//   (2) WAV (uncompressed PCM in RIFF WAV file)

				mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ADBEAudioCodec, &exParamValue_temp );
				const csSDK_int32 audioFormat = exParamValue_temp.value.intValue;

				if (audioFormat == ADBEAudioCodec_AAC)
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
	const NvEncodeCompressionStd videoCodec = static_cast<NvEncodeCompressionStd>(mySettings->NvEncodeConfig.codec);
	const bool codec_is_h264 = (videoCodec == NV_ENC_H264);
	const bool codec_is_hevc = (videoCodec == NV_ENC_H265);

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
	//  NONE		no		yes		1		*.m4v (or *.hevc)
	//  NONE		yes		no		1		*.wav
	//  NONE		yes		yes		2		*.wav + *.m4v (or *.wav + *.hevc)
	//
	//  TS			yes		yes		1		*.ts
	//  TS			no		yes		1		*.ts
	//  TS			yes		no		1		*.ts
	//
	//  MP4			yes		yes		1		*.mp4
	//  MP4			no		yes		1		*.mp4
	//  MP4			yes		no		1		*.mp4
	//
	//  MKV			yes		yes		1		*.mkv
	//  MKV			no		yes		1		*.mkv
	//  MKV			yes		no		1		*.mkv

	exParamValues mux_selection;// adobe parameter ADBEVMCMux_Type
	mySettings->exportParamSuite->GetParamValue( exID, 0, ADBEVMCMux_Type, &mux_selection );

	// (1) [first time Adobe-app calls this function]
	//     Set numOutputFiles
	if ( exportFileListRecP->numOutputFiles == 0) {
		if ( mux_selection.value.intValue == MUX_MODE_M2T || 
			mux_selection.value.intValue == MUX_MODE_MP4 ||
			mux_selection.value.intValue == MUX_MODE_MKV )
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

			// Add +5 characters, to cover the string maximum-length suffix '.hevc'
			//     (The other suffxies need fewer chars: .mp4/.m4v/.264/.ts)
			exportFileListRecP->outputFileRecs[i].pathLength = tempString_length + 5;
		}
		else {
			// (3) This is the third time Adobe-app has called this selector.
			//     Finally, we must place the full filename string into outputFileRecs[i].path

			wstring newpath = origpath; // the full filename string (with added extension)

			// (3a) Append the correct file-extension '.m4v' or '.wav'
			//
			//   audio + video:   video is file#0, audio is file#1
			//   audio-only:      file#0 ("wav")
			//   video-only:      file#0 ("m4v") or ("hevc")

			if ( mux_selection.value.intValue == MUX_MODE_M2T ) {
				// TS-muxer: single output file (*.ts)
				newpath += SDK_FILE_EXTENSION_M2T;
			}
			else if ( mux_selection.value.intValue == MUX_MODE_MP4 ) {
				// MP4Box: single output file (*.mp4)
				newpath += SDK_FILE_EXTENSION_MP4;
			}
			else if (mux_selection.value.intValue == MUX_MODE_MKV) {
				// MP4Box: single output file (*.mkv)
				newpath += SDK_FILE_EXTENSION_MKV;
			}
			else if ( (i==0) && (inExportVideo) ) {
				// Video
				if ( codec_is_hevc )
					newpath += SDK_FILE_EXTENSION_HEVC; // H265
				else
					newpath += SDK_FILE_EXTENSION_M4V;  // H264
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
	csSDK_int32					muxType, audioCodec, videoCodec;
	exParamValues exParamValues_muxPath;// path to muxer tool (TSMUXER or MP4BOX)
	exParamValues exParamValue;

	// Get some UI-parameter selections
	paramSuite->GetParamValue( exID, mgroupIndex, ADBEVMCMux_Type, &exParamValue );
	muxType = exParamValue.value.intValue;

	paramSuite->GetParamValue(exID, mgroupIndex, ParamID_NV_ENC_CODEC, &exParamValue);
	videoCodec = exParamValue.value.intValue; // NV_ENC_H264 | NV_ENC_H265

	paramSuite->GetParamValue(exID, mgroupIndex, ADBEAudioCodec, &exParamValue);
	audioCodec = exParamValue.value.intValue;

	//
	// During initialization, the export-plugin always constructs an object 
	// of type CNvEncoderH264.  If necessary, change to the correct object-type.
	// 
	NVENC_switch_codec(mySettings); // switch to the correct {CNvEncoderH264 or CNvEncoderH265}

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

	// if output-muxer is enabled, uniquify the *.aac and *.m4v filenames 
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
		//
		//   (1) '*.hevc' (if codec==h265)
		//   (2) '*.264'  (else if Multiplexer == MP4, and codec==h264,  because mp4box requires '.264' extension)
		//   (3) '*.M4V'  (else all other choices)
		nvenc_make_output_filename(
			filePath,
			v_postfix_str,          // string to uniquify this filename (if necessary)
			(videoCodec == NV_ENC_H265) ? 
				SDK_FILE_EXTENSION_HEVC :
			((muxType == MUX_MODE_MP4) ? 
				L"264" :				// mp4box requires H264-input file to have extension *.264
				SDK_FILE_EXTENSION_M4V), // other: use default extension (.m4v)
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
				if ( !NVENC_create_neroaac_pipe( mySettings ) )
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
		result = NVENC_WriteSDK_WAVHeader(stdParmsP, exportInfoP, exportDuration);

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

		case MUX_MODE_MKV:
			paramSuite->GetParamValue(exID, mgroupIndex, ParamID_BasicMux_MKVMERGE_Path, &exParamValues_muxPath);
			mux_result = NVENC_mux_mkv(
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
