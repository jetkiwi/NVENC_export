/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 1999-2008 Adobe Systems Incorporated                  */
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
//
//		SDK_File.h - Used to read/write a custom file format
//		.sdk files are simple media files consisting of a header,
//		followed by video, then audio, both optional.

#ifndef SDKFILE_H
#define SDKFILE_H

#include	"PrSDKStructs.h"
#include	"PrSDKImport.h"
#include	"PrSDKExport.h"
#include	"PrSDKExportStdParamSuite.h" // for querying output-settings
#include	"PrSDKExportFileSuite.h"
#include	"PrSDKExportInfoSuite.h"
#include	"PrSDKExportParamSuite.h"
#include	"PrSDKExportProgressSuite.h"
#include	"PrSDKErrorSuite.h"
#include	"PrSDKMALErrors.h"
#include	"PrSDKMarkerSuite.h"
#include	"PrSDKSequenceRenderSuite.h"
#include	"PrSDKAudioSuite.h"
#include	"PrSDKSequenceAudioSuite.h"
#include	"PrSDKClipRenderSuite.h"
#include	"PrSDKPPixCreatorSuite.h"
#include	"PrSDKPPixCacheSuite.h"
#include	"PrSDKMemoryManagerSuite.h"
#include	"PrSDKWindowSuite.h"
#include	"PrSDKPPix2Suite.h"
#include	"PrSDKExporterUtilitySuite.h"
#include	"PrSDKStringSuite.h"
#include	"SDK_Segment_Utils.h"
#ifdef		PRMAC_ENV
#include	<wchar.h>
#endif

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdint.h>
#include	<string>

#include	<cuda.h>
#include "CNvEncoder.h"

#include <MMReg.h> // for GUID KSDATAFORMAT_SUBTYPE_PCM, WAVE_FORMAT_EXTENSIBLE

#ifndef SDK_FILE_CURRENT_VERSION	
#define SDK_FILE_CURRENT_VERSION	39			// The current file version number. When making a change
												// to the file structure, increment this value.
#endif

#define SDK_FILETYPE				'SDK_'		// The four character code for our filetype
#define SDK_FILE_EXTENSION_M4A		L"m4a"		// file extension for MPEG-4 Audio (AAC) output [in an MP4 wrapper]
#define SDK_FILE_EXTENSION_M4V		L"m4v"		// file extension for H264 (video) output
#define SDK_FILE_EXTENSION_HEVC		L"hevc"		// file extension for H265 (video) output
#define SDK_FILE_EXTENSION_WAV		L"wav"		// file extension for pcm (audio) output
#define SDK_FILE_EXTENSION_M2T		L"ts"		// file extension for muxed (A+V) output
												//  ^^^ Caution: must be 'ts' or 'm2ts', becauase TSMUXER.EXE
												//               will analyze this filename-extension to
												//               choose the proper mux--output type.
#define SDK_FILE_EXTENSION_MP4		L"mp4"		// file extension for muxed (A+V) output
#define SDK_FILE_EXTENSION_MKV		L"mkv"		// file extension for muxed (A+V) output

// exSDKStartup(): the following defines enumerate the different codecs in this exporter
//                 (Note, we only support SDK_FILE_TYPE_M4V)
#define SDK_FILE_TYPE_M4V			'H264'		// file extension for H264 (video) output
//#define SDK_FILE_TYPE_WAV			'WAVE'		// file extension for pcm (audio) output

// Display strings for menus
#define EXPORTER_PLUGIN_NAME		SDK_FILE_NAME
#define SDK_8_BIT_RGB_NAME			L"Uncompressed 8-bit RGB"
#define SDK_10_BIT_YUV_NAME			L"Uncompressed 10-bit YUV (v410)"
#define	SDK_RLE_NAME				L"RLE Compressed 8-bit RGB"

#define	SDK_NAME					"NVidia NVENC SDK 6.0 (Dec 2015) Exporter"	// This string is used in the file header
#define	SDK_CLSS					'DTEK'		// ClassID four character code, used in the Editing Mode XML

// Codec (subtype) fourCCs
#define SDK_8_BIT_RGB				'RAW '
#define SDK_10_BIT_YUV				'10yu'
#define SDK_RLE						'RLE_'

#define	AUDIO_SAMPLE_SIZE			4			// Currently, only 32-bit float is supported
#define	PLUS_LINE					"++++"		// Buffer for padding between video frames
#define	PLUS_LINE_LENGTH			4

//////////////////
// NVENC specific structures

typedef struct
{
    int device; // GPU-Index
    char gpu_name[100];
	uint64_t vram_total; // total video RAM (MB)
	uint64_t vram_free;  // total video RAM (MB)
	int vram_clock; // peak VRAM Clock frequency
	int vram_width; // data-bus width
	
	// Cuda Revision: <major>.<minor>
	int driver_version; // value returned by cudaDriverGetVersion()
	int cuda_major;
	int cuda_minor;

	bool nvenc_supported;

	// Capabilities queried during a live EncoderSession()
	nv_enc_caps_s nv_enc_caps; // queryable caps
} NvEncoderGPUInfo_s;

///////////////////////////////////////////////////////////////////////////////
// SDK header structure

typedef struct {
	wstring filename;
	
	// Only *one* of the following pointers will be used.
	//FILE *fp;   // legacy C-standard library file-pointer
	HANDLE hfp; // Win32 file handle (for those routines that use it)
} FileRecord_t;

#pragma pack(1)
typedef struct {
	// Note, the NVENC plugin doesn't use all of these vars
	// (Many are leftover from the Adobe SDK sample exporter)
	char				name[18];		// SDK_NAME (see above)
	csSDK_int32			version;		// File version #
	prBool				hasAudio;
	PrFourCC			audioSubtype;
	prBool				hasVideo;
	PrFourCC			videoSubtype;	// SDK_8_BIT_RGB, SDK_10_BIT_YUV, or SDK_RLE (not implemented)
										// NOTE: for demo purposes - the compiler does NOT compress audio
	PrPixelFormat		pixelFormat;
	csSDK_int32			tvformat;		// 0=NTSC,1=PAL,2=SECAM
	csSDK_int32			depth;			// Bit depth of video
	csSDK_int32			width;
	csSDK_int32			height;
	csSDK_uint32		numFrames;		// Number of video frames
	PrAudioSample		numSampleFrames;// Number of audio sample frames. Note that audio samples may extend
										// past the last video frame.
	csSDK_int32			channelType;	// Audio channel type; uses same enum as PrSDKAudioSuite.h
										// - kAudioChannelType_Mono, kAudioChannelType_Stereo, or kAudioChannelType_51
	PrTime				frameRate;
	double				sampleRate;		// Can be any sample rate supported by Premiere
	csSDK_int32			fieldType;		// Uses same enum as compiler API
										// - compFieldsNone, compFieldsUpperFirst, or compFieldsLowerFirst
	csSDK_uint32		pixelAspectNum;	// Numerator of pixel aspect ratio
	csSDK_uint32		pixelAspectDen; // Denominator of pixel aspect ratio
	char				orgtime[18];	// These fields map directly to those in imTimeInfoRec.
	char				alttime[18];
	char				orgreel[40];
	char				altreel[40];
	char				logcomment[256];
	csSDK_int32			magic;
	csSDK_int32			unused[32];		// For future expansion

	// FileRecords for writing to the actual output-files
	//   Plugin doesn't use the Adobe provided fileObject, instead we manage the direct file
	// access ourselves (because we need to support writing to multiple files.)
	FileRecord_t		FileRecord_Audio;// the audio (WAV) file record
	FileRecord_t		FileRecord_Video;// the video (M4V) file record
	FileRecord_t		FileRecord_AV;   // muxed file (A+V) not supported yet
	FileRecord_t		FileRecord_AAClog;// the logfile written by neroAacEnc.exe (for AAC only)
	HANDLE				H_pipe_wavout;	 // PIPE input  (nvenc_export wav output)
	HANDLE				H_pipe_aacin;	 // PIPE output (neroaacenc stdin input)
	PROCESS_INFORMATION child_piProcInfo; 
} SDK_File, *SDK_FileP, **SDK_FileH;
#pragma pack()


///////////////////////////////////////////////////////////////////////////////
// Importer local data structure, defined here for convenience,
// and shared between the various importer SDK samples

typedef struct
{	
	SDK_File				theFile;
	PrAudioSample			audioPosition;
	// fileName is used as an example of saving a filename in private data,
	// and could be used if a clip has child files that Premiere doesn't know about
	prUTF16Char				fileName[256];
	imFileRef				fileRef;
	#ifdef PRWIN_ENV
	HANDLE					ioCompletionPort;
	#endif
	PlugMemoryFuncsPtr		memFuncs;
	csSDK_int32				importerID;
	SPBasicSuite			*BasicSuite;
	PrSDKPPixCreatorSuite	*PPixCreatorSuite;
	PrSDKPPixCacheSuite		*PPixCacheSuite;
	PrSDKPPixSuite			*PPixSuite;
	PrSDKTimeSuite			*TimeSuite;
} ImporterLocalRec8, *ImporterLocalRec8Ptr, **ImporterLocalRec8H;


///////////////////////////////////////////////////////////////////////////////
// Exporter local data structure, defined here for convenience
typedef struct ExportSettings
{
	SDK_File					SDKFileRec;	// The struct of the file header
	VideoSequenceParser			*videoSequenceParser;
	SPBasicSuite				*spBasic;
	PrSDKExportStdParamSuite	*exportStdParamSuite;
	PrSDKExportParamSuite		*exportParamSuite;
	long						exportParamSuite_version; // workaround for Premiere Elements 11
	PrSDKExportProgressSuite	*exportProgressSuite;
	PrSDKExportInfoSuite		*exportInfoSuite;
	PrSDKExportFileSuite		*exportFileSuite;
	PrSDKExporterUtilitySuite	*exporterUtilitySuite;
	PrSDKErrorSuite3			*errorSuite;
	PrSDKClipRenderSuite		*clipRenderSuite;
	PrSDKMarkerSuite			*markerSuite;
	PrSDKPPixSuite				*ppixSuite;
	PrSDKPPix2Suite				*ppix2Suite;
	PrSDKTimeSuite				*timeSuite;
	PrSDKMemoryManagerSuite		*memorySuite;
	PrSDKAudioSuite				*audioSuite;
	PrSDKSequenceAudioSuite		*sequenceAudioSuite;
	PrSDKSequenceRenderSuite	*sequenceRenderSuite;
	PrSDKWindowSuite			*windowSuite;
	PrSDKStringSuite			*stringSuite;
	csSDK_uint32				videoRenderID;

	// NVENC management
	NvEncoderGPUInfo_s          NvGPUInfo;
	EncodeConfig				NvEncodeConfig;
	CNvEncoder					*p_NvEncoder;
	
	// frame#0 PixelFormat advertisement behavior:
	//   true(forced) = user supplies the PixelFormat to use for frame#0, 
	//                  and by extension, the remaining video-frames.
	//   false        = NVENC advertises all supported formats, and lets Adobe 
	//                  autoselect the format for frame#0. NVENC will use
	//                  the selected format for the rest of the video.
	bool						forced_PixelFormat0;    // flag: if 0 then Adobe will autoselect, 
														// otherwise (NVENC provided) list of formats
	PrPixelFormat				requested_PixelFormat0; // user-requested Pixelformat, must be initialized to
														// a valid format
	PrPixelFormat				rendered_PixelFormat0;  // the actual Pixelformat Adobe chose to render frame#0

	// Status the encode-session (spawned by the Adobe Premiere app)
	bool                        video_encode_fatalerr;  // status, video-encode operation suffered a fatal
														// unrecoverable error.  (This causes nvenc_export
														// to skip subsequent audio-encoding and muxing.)
} ExportSettings;


// Set during exSelParamButton, and passed during exSelExport
typedef struct CodecSettings
{
	prBool		sampleSetting;	// Sample setting to demonstrate how to set and get custom settings
} CodecSettings;

typedef union {
	uint32_t  dword[1];
	uint16_t  word[2];
	uint8_t   byte[4];
} dword_word_byte_u; // a 32-bit glob that is addressable as byte/word/dword


///////////////////////////////////////////////////////////////////////////////
// Video import-related calls
#ifdef PRWIN_ENV
unsigned char ReadSDKFileAsync(	imFileRef	SDKfileRef, 
								csSDK_int32	frameBytes, 
								csSDK_int32	theFrame, 
								char		*inFrameBuffer,
								OVERLAPPED	*overlapped);
#endif

unsigned char ReadSDK_File(	imFileRef	SDKfileRef, 
							csSDK_int32	frameBytes, 
							csSDK_int32	theFrame, 
							char		*inFrameBuffer);

void AddRowPadding(	char			*srcFrameZ,
					char			*dstFrameZ, 
					csSDK_uint32	rowBytesL, 
					csSDK_uint32	pixelSize,
					csSDK_uint32	widthL, 
					csSDK_uint32	heightL);

void ScaleAndBltFrame(imStdParms		*stdParms,
					  SDK_File			fileHeader,
					  csSDK_uint32		frameBytes,
					  char				*inFrameBuffer, 
					  imImportImageRec	*imageRec);

csSDK_int32 GetSrcPix(	char			*inFrameBuffer,
						SDK_File		fileHeader,
						csSDK_uint32	dstCoorW, 
						csSDK_uint32	dstCoorH, 
						float		ratioW, 
						float		ratioH);

int prWriteFile (imFileRef			refNum, 
				 const void			*data, 
				 csSDK_uint32		*bytes);
/*
void WriteRLE	(long			*src, 
				 compFileRef	ref, 
				 long			totalPix);
*/


///////////////////////////////////////////////////////////////////////////////
// Export-related calls

void RemoveRowPadding(	char		*srcFrameZ,
						char		*dstFrameZ, 
						csSDK_int32 rowBytesL,
						csSDK_int32 pixelSize,
						csSDK_int32 widthL, 
						csSDK_int32 heightL);

void WriteMarkerAndProjectDataToFile(
	exportStdParms		*stdParms, 
	exDoExportRec		*exportInfoP);


///////////////////////////////////////////////////////////////////////////////
// Miscellaneous helper funcs

csSDK_int32 GetPixelFormatSize(PrFourCC subtype);
csSDK_int32 GetPixelFormatSize(PrPixelFormat pixelFormat);

void ConvertFrom8uTo32f(char *buffer8u, char *buffer32f, csSDK_int32 width, csSDK_int32 height);
void ConvertFromBGRA32fToVUYA32f(char *buffer32f, csSDK_int32 width, csSDK_int32 height);
void ConvertFrom32fToV410(char *buffer32f, char *bufferV210, csSDK_int32 width, csSDK_int32 height);
void ConvertFromV410To32f(char *bufferV210, char *buffer32f, csSDK_int32 width, csSDK_int32 height);

void ConvertPrTimeToScaleSampleSize(
	PrSDKTimeSuite	*timeSuite,
	PrTime			prTime,
	csSDK_int32		*scale,
	csSDK_int32		*sampleSize);

void ConvertScaleSampleSizeToPrTime(
	PrSDKTimeSuite	*timeSuite,
	csSDK_int32		*scale,
	csSDK_int32		*sampleSize,
	PrTime			*prTime);
	
void copyConvertStringLiteralIntoUTF16(const wchar_t* inputString, prUTF16Char* destination);

void safeStrCpy (char *destStr, int size, const char *srcStr);
void safeWcscat (wchar_t *destStr, int size, const wchar_t *srcStr);

bool Check_prSuiteError(const prSuiteError errval, string &str);

//	This format does not support audio interleaving or "smart" RLE encoding.

//	Frames end with \n\n\n\n, audio "blips" start with ++++
//	So, read until you get to the first \n, that will be the total amount
//	of bytes for the first frame.  Likewise for a "frame" or "blip" of audio.

//	RLE Compression is done on a pixel repeat per frame basis.  
//		So if a row has all one color such as 
//		[fa,fb,fc,00], it would look like 
//		(num_repeat)(fafbfc00)
//		where num_repeat is the number of pixels from 1 to totalPixels (pixWidth * pixHeight)

// Node struct, used in RLE compression 
typedef struct Node{
	csSDK_int32	count;
	csSDK_int32	pixel;
} Node;


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
nvenc_make_output_filename(
	const wstring &src,
	const wstring &postfix,
	const wstring &ext,
	wstring &dst
);

void
nvenc_make_output_dirname(
	const wstring &src,
	wstring &dst
);

#endif
