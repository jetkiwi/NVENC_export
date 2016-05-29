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
#ifndef SDK_EXPORTER_H
#define SDK_EXPORTER_H

#define SDK_FILE_NAME			L"NVENC_export 1.11"

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

size_t
fwrite_callback(
	_In_count_x_(_Size*_Count) void * _Str,
	size_t _Size, 
	size_t _Count, 
	FILE * _File, 
	void *privateData
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

#endif // SDK_EXPORTER_H