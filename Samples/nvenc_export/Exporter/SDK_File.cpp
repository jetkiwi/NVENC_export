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
//		SDK_File.cpp - Used to read/write the SDK file format.
//		
//		Description: SDK Files can contain audio and video
//		In addition they can be RLE compressed
//
//		Purpose: This "artificial" file format is used as
//		a part of the SDK "Media Abstraction Layer".  By writing
//		an import and compile module, and optionally a
//		real-time playback module and assigning a Filetype, 
//		Subtype, and a unique ClassID, your plug-ins can 
//		work cooperatively with one another.
//
//		Created by Adobe Developer Technologies for 
//		Adobe Systems, Inc.
//		Part of the Adobe Premiere Pro SDK
//		_______________________________________
//		Version 1.0 - eks
//				1.1	- bbb -	(.c -> .cpp)
//				1.2 - zal - Fixed file write, work area export
//				1.3 - zal - Added support for arbitrary audio sample rates, multi-channel audio,
//							pixel aspect ratio, and fields
//				1.4 - zal - Support for 24-bit video (no alpha channel), versioning
//				2.0 - zal - Generic routines for rendering and writing video, audio, and markers
//				2.5 - zal - High-bit video support (v410)

#include <nvEncodeAPI.h>                // the NVENC common API header
#include "CNVEncoderH264.h"             // class definition for the H.264 encoding class
#include "xcodeutil.h"                  // class helper functions for video encoding
#include <platform/NvTypes.h>           // type definitions

#include <cuda.h>                       // include CUDA header for CUDA/NVENC interop
#include "SDK_File.h"
#include "SDK_Exporter.h" // nvenc_make_output_dirname()
#include "SDK_Exporter_Params.h"
#include <Windows.h> // SetFilePointer(), WriteFile()
#include <sstream>  // ostringstream
#include <cstdio>

//////////////////////////////////////////////////////////////////
//
//	prWriteFile - File write.
//

int 
prWriteFile (
	imFileRef		refNum, 
	const void 		*data, 
	csSDK_uint32	*bytes)
{
	csSDK_uint32		orig_bytes = *bytes;
	csSDK_int32		err;
	#ifdef PRWIN_ENV
	err = !WriteFile(refNum, data, (DWORD)*bytes, (LPDWORD)bytes, NULL);
	if (err)
	{
		err = GetLastError ();
	}
	#else
	err = FSWriteFork(	reinterpret_cast<intptr_t>(refNum),
						fsAtMark,
						0,
						orig_bytes,
						data,
						reinterpret_cast<ByteCount*>(bytes));
	#endif
	
	if (!err && (*bytes != orig_bytes)) {
		err = exportReturn_OutOfDiskSpace;
	}
	return err;
}

//////////////////////////////////////////////////////////////////
//
//	WriteRLE - Simple RLE function, frame by frame.
//
//	RLE Compress the data in the frame, then write to file
//
/*
void WriteRLE(long *src, compFileRef ref, long totalPix)
{
	long			totalNodes 	= 1;
	unsigned long	in_bytes = 0;		
	Node *  nodes = (Node*)malloc((sizeof(Node) * totalPix));	// Create Nodes
	register Node * N = nodes;
	register long * s1 = src;
	register long * s2 = src;
	
	++s2;				

	if (!N)				// Make sure I've got the memory...
		return;
	
	N->count = 1;		// initialize the first element in our node
	N->pixel = *src;	// get the value of the first pixel from src

	while (--totalPix)
	{	
		// Look at the source buffer, compare to next. pixel

		if (*s1++ == *s2++)
		{
			// If the current and next pixel values match,
			// increment the count of the already stored
			// pixel.
			++(N->count);
		}
		else
		{
			//	If the current and next pixel don't match
			//	Increment the node position, start a new node count
			//	assign the value of the current pixel and increment
			//	the total number of nodes created.
			++N;
			N->count = 1;
			N->pixel = *s1;
			++totalNodes;
		}
	}
	in_bytes = totalNodes * sizeof(Node);
	prWriteFile(ref,
				nodes,
				&in_bytes);

	free(nodes);
	
}
*/

//////////////////////////////////////////////////////////////////
//
//	ReadSDKFileAsync - Read theFrame from the indicated SDK file,
//	Returns a populated inFrameBuffer. 
//	Only for uncompressed SDK files
//

#ifdef PRWIN_ENV
unsigned char ReadSDKFileAsync(
	imFileRef	SDKfileRef, 
	csSDK_int32	frameBytes, 
	csSDK_int32	theFrame, 
	char		*inFrameBuffer,
	OVERLAPPED	*overlapped)
{
	prBool			didRead		= 0;
	DWORD			lastError	= 0;
	csSDK_int32		offset		= sizeof(SDK_File);
	csSDK_uint32	bytesRead	= 0;

	offset += theFrame * frameBytes;
	if( theFrame > 0)
	{	
		// take the extra 4 bytes (\n\n\n\n) at the end of each frame into account
		offset += (4 * (theFrame));
	}

	overlapped->Offset = offset;
	overlapped->OffsetHigh = 0;
	didRead = ReadFile(	SDKfileRef,
						inFrameBuffer,
						frameBytes,
						NULL,
						overlapped);

	return imNoErr;
}
#endif


//////////////////////////////////////////////////////////////////
//
//	ReadSDK_File - Read theFrame from the indicated SDK file,
//	Returns a populated inFrameBuffer. 
//	Only for uncompressed SDK files
//

unsigned char ReadSDK_File(	imFileRef		SDKfileRef, 
							csSDK_int32		frameBytes, 
							csSDK_int32		theFrame, 
							char			*inFrameBuffer)
{
	csSDK_int32		offset		= sizeof(SDK_File);
	csSDK_uint32	bytesRead	= 0;

	#ifdef PRWIN_ENV
	char			didRead		= 0;
	#else
	OSErr			returnValue = 0;
	#endif

	offset += theFrame * frameBytes;
	if( theFrame > 0)
	{	
		// Take the extra bytes at the end of each frame into account
		offset += (PLUS_LINE_LENGTH * (theFrame));
	}
	
	#ifdef PRWIN_ENV
	SetFilePointer(SDKfileRef,offset,NULL,FILE_BEGIN);
	didRead = ReadFile(	SDKfileRef,
						inFrameBuffer,
						frameBytes,
						reinterpret_cast<LPDWORD>(&bytesRead),
						NULL);
	if(!didRead)
	{
		return imBadFile;
	}						
	#else
	returnValue = FSReadFork(	reinterpret_cast<intptr_t>(SDKfileRef),
								fsFromStart,
								offset,
								frameBytes,
								inFrameBuffer,
								reinterpret_cast<ByteCount*>(&bytesRead));
	if(returnValue)
	{
		return imBadFile;
	}							
	#endif

	return imNoErr;
}


//////////////////////////////////////////////////////////////////
//
//	ScaleAndBltFrame - Scaling Function
//		
//	Designed to work with SDK format files, modify for your own importer needs
//		

void ScaleAndBltFrame(imStdParms		*stdParms,
					  SDK_File			fileHeader,
					  csSDK_uint32		frameBytes,
					  char				*inFrameBuffer, 
					  imImportImageRec	*imageRec)
{
	// original source and dest in pixels
	csSDK_int32	srcWidth, srcHeight, dstWidth, dstHeight, dstCoorW,dstCoorH;
	float		ratioW, ratioH;
	char		paddingBytes = 0;
	
	char		*tempPix;
	csSDK_int32	*dstPix = (csSDK_int32*)imageRec->pix; 
	
	paddingBytes = (imageRec->rowbytes) - (imageRec->dstWidth * 4); 

	srcWidth	= fileHeader.width;;
	srcHeight	= fileHeader.height;
	dstWidth	= imageRec->dstWidth;
	dstHeight	= imageRec->dstHeight;

	// coordinate numbers, ratios
	
	ratioW		= (float)srcWidth / (float)dstWidth;
	ratioH		= (float)srcHeight / (float)dstHeight;
	
	// loop through the destination coordinate grid, find the "virtual" pixel in source grid

	for(dstCoorH = 0; dstCoorH < dstHeight; ++dstCoorH)
	{
		for(dstCoorW = 0; dstCoorW < dstWidth; ++dstCoorW)
		{
			*dstPix = GetSrcPix(inFrameBuffer, fileHeader,dstCoorW,dstCoorH,ratioW,ratioH);
			++dstPix;
		}
		// add the padding bytes to the dst after it's scaled
		tempPix = (char*)dstPix;
		tempPix += paddingBytes;
		dstPix = (csSDK_int32*)tempPix;
	}
	return;
}

//////////////////////////////////////////////////////////////////
//
//	GetSrcPix - Utility function used by the scaling functions
//	

csSDK_int32 GetSrcPix(char			*inFrameBuffer, 
			   SDK_File		fileHeader,
			   csSDK_uint32	dstCoorW, 
			   csSDK_uint32	dstCoorH, 
			   float		ratioW, 
			   float		ratioH)
{
	csSDK_uint32	w, h;
	csSDK_uint32	*thePixel = reinterpret_cast<csSDK_uint32*>(inFrameBuffer);

	// The translated coordinates

	w = static_cast<csSDK_uint32>(dstCoorW * ratioW);
	h = static_cast<csSDK_uint32>(dstCoorH * ratioH);
	
	thePixel += (h * fileHeader.width) + w; 

	return *thePixel;
}


// Source and destination frames may be the same
void RemoveRowPadding(	char		*srcFrame,
						char		*dstFrame, 
						csSDK_int32 rowBytes, 
						csSDK_int32 pixelSize,
						csSDK_int32 widthL, 
						csSDK_int32 heightL)
{
	csSDK_int32 widthBytes = widthL * pixelSize;

	if (widthBytes < rowBytes)
	{
		for(csSDK_int32 hL = 0; hL < heightL; ++hL)
		{
			memcpy (&dstFrame[hL * widthBytes], &srcFrame[hL * rowBytes], widthBytes);
		}
	}

	return;
}


// Source and destination frames may be the same
void AddRowPadding(	char			*srcFrame,
					char			*dstFrame, 
					csSDK_uint32	rowBytesL, 
					csSDK_uint32	pixelSize,
					csSDK_uint32	widthL, 
					csSDK_uint32	heightL)
{
	csSDK_uint32 widthBytes = widthL * pixelSize;

	if (widthBytes < rowBytesL)
	{
		// Expand rows starting from last row, so that we can handle an in-place operation
		for(csSDK_int32 hL = heightL - 1; hL >= 0; --hL)
		{
			memcpy (&dstFrame[hL * rowBytesL], &srcFrame[hL * widthBytes], widthBytes);
		}
	}

	return;
}


//	Writes an HTML File that contains the marker info in the same folder as the output 
void WriteMarkerAndProjectDataToFile(
	exportStdParms		*stdParmsP, 
	exDoExportRec		*exportInfoP)
{
	#ifdef PRWIN_ENV
	FILE			*fileP					= NULL;
	prMarkerRef		marker					= 0;
	char			*nameZ					= NULL,
					*commentZ				= NULL,
					*chapterZ				= NULL,
					*hrefZ					= NULL,
					*targetZ				= NULL;
	wchar_t			htmlOutputFilePath[256]	= {'\0'};
	char			settingsA[256]			= {'\0'};
	prBool			firstMarker				= kPrTrue;
	ExportSettings	*mySettings				= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);

	char			HTML_begin[]			= "<html>\n<head>\n<title>SDK Exporter - Sequence Marker Data Output\n</title>\n</head>\n",
					HTML_body[]				= "<body>",
					HTML_end[]				= "</body>\n</html>",
					noMarkers[]				= "<center>There were no markers found in the Adobe Premiere Pro Sequence";
	csSDK_int32		filepathLen				= 255;
	csSDK_uint32	markerType				= 0,
					DVDmarkerType			= 0,
					numMarkers				= 0;
	PrTime			ticksPerSecond			= 0,
					markerTime				= 0,
					markerDuration			= 0;
	float			markerTimeFloat			= 0.0,
					markerDurationFloat		= 0.0;

	mySettings->exportFileSuite->GetPlatformPath(exportInfoP->fileObject, &filepathLen, htmlOutputFilePath);
	mySettings->timeSuite->GetTicksPerSecond (&ticksPerSecond);

	#ifdef PRWIN_ENV
	wcscat_s(htmlOutputFilePath, sizeof (htmlOutputFilePath) / sizeof (wchar_t), L".html");
	_wfopen_s(&fileP, htmlOutputFilePath, L"w");
	#else
	wcscat(htmlOutputFilePath, L".html");
	fileP = _wfopen(htmlOutputFilePath, L"w");
	#endif

	mySettings->markerSuite->GetMarkerCount(exportInfoP->timelineData, &numMarkers);
	marker = mySettings->markerSuite->GetFirstMarker(exportInfoP->timelineData);
	
	// If no markers in the timeline, create default "no markers" 
	if (numMarkers == 0)
	{
		fprintf(fileP, HTML_begin);
		fprintf(fileP, HTML_body);
		fprintf(fileP, settingsA);
		fprintf(fileP, "%s", &noMarkers);
		fprintf(fileP, HTML_end);
		fclose(fileP);
		
		// Exit the function, nothing else to do
		return;
	}
	
	while (marker != kInvalidMarkerRef)
	{
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_VALUE64, &markerTime);
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_DURATION64, &markerDuration);
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_NAME, reinterpret_cast<void*>(&nameZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_COMMENT, reinterpret_cast<void*>(&commentZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_CHAPTER, reinterpret_cast<void*>(&chapterZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_HREF,	reinterpret_cast<void*>(&hrefZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_TARGET, reinterpret_cast<void*>(&targetZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_TYPE, reinterpret_cast<void*>(&markerType));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_DVD_MARKER_TYPE, reinterpret_cast<void*>(&DVDmarkerType));

		// Create an HTML table of marker information, make links active
		if (firstMarker)
		{
			fprintf(fileP, HTML_begin);
			fprintf(fileP, HTML_body);
			fprintf(fileP, "<center>\nSequence Marker Data Output<p>\n");
			fprintf(fileP, "<table border=\"4\" cellpadding=\"0\" cellspacing=\"2\" width=\"350\">\n");
			firstMarker = false;
		}

		markerTimeFloat = static_cast<float>(markerTime) / static_cast<float>(ticksPerSecond);
		fprintf(fileP, "<tr><td>Time</td><td>%.2f sec</td></tr>", markerTimeFloat);
		markerDurationFloat = static_cast<float>(markerDuration) / static_cast<float>(ticksPerSecond);
		fprintf(fileP, "<tr><td>Duration</td><td>%.2f sec</td></tr>\n", markerDurationFloat);
		fprintf(fileP, "<tr><td>Name</td><td>%s</td></tr>\n", nameZ);
		fprintf(fileP, "<tr><td>Comment</td><td>%s</td></tr>\n", commentZ);
		fprintf(fileP, "<tr><td>Chapter</td><td>%s</td></tr>\n", chapterZ);
		fprintf(fileP, "<tr><td>HREF</td><td><a href=\"%s\">%s</a></td></tr>\n", hrefZ, hrefZ);
		fprintf(fileP, "<tr><td>Frame Target</td><td>%s</td></tr>\n", targetZ);
		if (markerType == kMarkerType_Timeline)
		{
			fprintf(fileP, "<tr><td>Marker Type</td><td>Timeline Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
		}
		else if (markerType == kMarkerType_DVD)
		{
			if (DVDmarkerType == kDVDMarkerType_Main)
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>DVD Chapter Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
			else if (DVDmarkerType == kDVDMarkerType_Scene)
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>DVD Scene Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
			else if (DVDmarkerType == kDVDMarkerType_Stop)
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>DVD Stop Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
			else
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>Unknown DVD Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
		}
		else
		{
			fprintf(fileP, "<tr><td>Marker Type</td><td>Unknown Marker Type</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
		}

		marker = mySettings->markerSuite->GetNextMarker(exportInfoP->timelineData, marker);
	}

	fprintf(fileP, "</table>\n</center>\n</body>\n</html>");
	fclose(fileP);
	
	#endif

	return;
}


csSDK_int32 GetPixelFormatSize(PrFourCC subtype)
{
	csSDK_int32 formatSize = 4; // Default to size of 8-bit pixel formats

	if (subtype == SDK_10_BIT_YUV)
	{
		formatSize = 4;
	}
	return formatSize;
}


csSDK_int32 GetPixelFormatSize(PrPixelFormat pixelFormat) // #bytes per pixel
{
	switch ( pixelFormat ) {
		case PrPixelFormat_VUYX_4444_8u_709:
		case PrPixelFormat_VUYA_4444_8u_709:
		case PrPixelFormat_VUYX_4444_8u:
		case PrPixelFormat_VUYA_4444_8u:
			return 4;
	}

	// assume we're using a YUV420 planar mode, 1 byte per pixel
	return 1;
}


//float max(float a, float b)
//{
//	return (a > b ? a : b);
//}


//float min(float a, float b)
//{
//	return (a < b ? a : b);
//}


void ConvertFrom8uTo32f(
  	char		*buffer8u,
	char		*buffer32f,
	csSDK_int32 width,
	csSDK_int32 height)
{
	csSDK_uint32 *tempSrcBuffer = (csSDK_uint32 *)buffer8u;
	float *tempDestBuffer = (float *)buffer32f;
	csSDK_uint32 X, Y, Z, A;
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			Z = ((*tempSrcBuffer) << 24) >> 24;
			Y = ((*tempSrcBuffer) << 16) >> 24;
			X = ((*tempSrcBuffer) << 8) >> 24;
			A = (*tempSrcBuffer) >> 24;
			tempDestBuffer[0] = (float)Z / 255.0f;
			tempDestBuffer[1] = (float)Y / 255.0f;
			tempDestBuffer[2] = (float)X / 255.0f;
			tempDestBuffer[3] = (float)A / 255.0f;
			tempSrcBuffer++;
			tempDestBuffer += 4;
		}
	}
}


// This uses ITU-R Recommendation BT.601
void ConvertFromBGRA32fToVUYA32f(
  	char		*buffer32f,
	csSDK_int32	width,
	csSDK_int32	height)
{
	float *tempBuffer		= (float *)buffer32f;
	float Y, Cb, Cr;
	// The luma component float range is 0.0 = black to 1.0 = white
	float Y_RGBtoYCbCr[3]	= { 0.299f, 0.587f, 0.114f};
	// The Cb and Cr float range is -0.5 to 0.5
	float Cb_RGBtoYCbCr[3]	= { -0.168736f, -0.331264f, 0.5f}; 
	float Cr_RGBtoYCbCr[3]	= { 0.5f, -0.418688f, -0.081312f};
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			// BGR -> VUY
			Y =		Y_RGBtoYCbCr[0] * tempBuffer[2] +	// Red
					Y_RGBtoYCbCr[1] * tempBuffer[1] +	// Green
					Y_RGBtoYCbCr[2] * tempBuffer[0];	// Blue
			Cb =	Cb_RGBtoYCbCr[0] * tempBuffer[2] +
					Cb_RGBtoYCbCr[1] * tempBuffer[1] +
					Cb_RGBtoYCbCr[2] * tempBuffer[0];
			Cr =	Cr_RGBtoYCbCr[0] * tempBuffer[2] +
					Cr_RGBtoYCbCr[1] * tempBuffer[1] +
					Cr_RGBtoYCbCr[2] * tempBuffer[0];

			tempBuffer[0] = Cr;
			tempBuffer[1] = Cb;
			tempBuffer[2] = Y;

			tempBuffer += 4;
		}
	}
}


// Converts a 32f VUYA buffer to the v410 format described at
// http://developer.apple.com/quicktime/icefloe/dispatch019.html#v410
void ConvertFrom32fToV410(
	char *buffer32f,
	char *bufferV410,
	csSDK_int32 width,
	csSDK_int32 height)
{
	float *tempSrcBuffer = (float *)buffer32f;
	csSDK_int32 *tempDestBuffer = (csSDK_int32 *)bufferV410;
	float fY, fCr, fCb;
	csSDK_uint32 Y, Cr, Cb;
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			fCr = (*(tempSrcBuffer + 1) * 896.0f + 512.5f);
			Cr = (csSDK_uint32)max(64, min(960, fCr));
			fY = (*(tempSrcBuffer + 2) * 876.0f + 64.5f);
			Y = (csSDK_uint32)max(64, min(940, fY));
			fCb = (*tempSrcBuffer * 896.0f + 512.5f);
			Cb = (csSDK_uint32)max(64, min(960, fCb));
			*tempDestBuffer = (Cr << 22) + (Y << 12) + (Cb << 2);
			tempSrcBuffer += 4;
			tempDestBuffer++;
		}
	}
}


// Converts to a 32f VUYA buffer from the v410 format described at
// http://developer.apple.com/quicktime/icefloe/dispatch019.html#v410
void ConvertFromV410To32f(
  	char *bufferV410,
	char *buffer32f,
	csSDK_int32 width,
	csSDK_int32 height)
{
	csSDK_uint32 *tempSrcBuffer = (csSDK_uint32 *)bufferV410;
	float *tempDestBuffer = (float *)buffer32f;
	csSDK_uint32 Y, Cr, Cb; // Y != y
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			Cr = (*tempSrcBuffer) >> 22;
			Y = ((*tempSrcBuffer) << 10) >> 22;
			Cb = ((*tempSrcBuffer) << 20) >> 22;
			tempDestBuffer[0] = ((float)Cb - 512.0f) / 896.0f;
			tempDestBuffer[1] = ((float)Cr - 512.0f) / 896.0f;
			tempDestBuffer[2] = ((float)Y - 64.0f) / 876.0f;
			tempDestBuffer[3] = 1.0f;
			tempSrcBuffer++;
			tempDestBuffer += 4;
		}
	}
}


// Assumes that prTime is a framerate < ticksPerSecond
void ConvertPrTimeToScaleSampleSize(
	PrSDKTimeSuite	*timeSuite,
	PrTime			prTime,
	csSDK_int32		*scale,
	csSDK_int32		*sampleSize)
{
	PrTime	ticksPerSecond = 0,
			tempFrameRate = 0;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);
	if (ticksPerSecond % prTime == 0) // a nice round frame rate
	{
		*scale = static_cast<csSDK_int32>(ticksPerSecond / prTime);
		*sampleSize = 1;
	}
	else
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC, &tempFrameRate);
		if (tempFrameRate == prTime)
		{
			*scale = 30000;
			*sampleSize = 1001;
		}
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC_HD, &tempFrameRate);
		if (tempFrameRate == prTime)
		{
			*scale = 60000;
			*sampleSize = 1001;
		}
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_24Drop, &tempFrameRate);
		if (tempFrameRate == prTime)
		{
			*scale = 24000;
			*sampleSize = 1001;
		}
	}
}


void ConvertScaleSampleSizeToPrTime(
	PrSDKTimeSuite	*timeSuite,
	csSDK_int32		*scale,
	csSDK_int32		*sampleSize,
	PrTime			*prTime)
{
	if ((*scale == 24000 && *sampleSize == 1001) ||
			(*scale == 23976 && *sampleSize == 1000) ||
			(*scale == 2397 && *sampleSize == 100))
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_24Drop, prTime);
	}
	else if (*scale == 24)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_24, prTime);
	}
	else if (*scale == 25)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_PAL, prTime);
	}
	else if ((*scale == 30000 && *sampleSize == 1001) ||
		(*scale == 29970 && *sampleSize == 1000) ||
		(*scale == 2997 && *sampleSize == 100))
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC, prTime);
	}
	else if (*scale == 30)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_30, prTime);
	}
	else if (*scale == 50)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_PAL_HD, prTime);
	}
	else if ((*scale == 60000 && *sampleSize == 1001) ||
			(*scale == 5994 && *sampleSize == 100))
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC_HD, prTime);
	}
	else if (*scale == 60)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_60, prTime);
	}
}


// Function to convert and copy string literals to the format expected by the exporter API.
// On Win: Pass the input directly to the output
// On Mac: All conversion happens through the CFString format
void copyConvertStringLiteralIntoUTF16(const wchar_t* inputString, prUTF16Char* destination)
{
#ifdef PRMAC_ENV
	int length = wcslen(inputString);
	CFRange	range = {0, kPrMaxPath};
	range.length = length;
	CFStringRef inputStringCFSR = CFStringCreateWithBytes(	kCFAllocatorDefault,
															reinterpret_cast<const UInt8 *>(inputString),
															length * sizeof(wchar_t),
															kCFStringEncodingUTF32LE,
															kPrFalse);
	CFStringGetBytes(	inputStringCFSR,
						range,
						kCFStringEncodingUTF16,
						0,
						kPrFalse,
						reinterpret_cast<UInt8 *>(destination),
						length * (sizeof (prUTF16Char)),
						NULL);
	destination[length] = 0; // Set NULL-terminator, since CFString calls don't set it, and MediaCore hosts expect it
	CFRelease(inputStringCFSR);
#elif defined PRWIN_ENV
	size_t length = wcslen(inputString);
	wcscpy_s(destination, length + 1, inputString);
#endif
}


// Utility function to merge strcpy_s on Win and strcpy on Mac into one call
void safeStrCpy (char *destStr, int size, const char *srcStr)
{
#ifdef PRWIN_ENV
	strcpy_s (destStr, size, srcStr);
#elif defined PRMAC_ENV
	strcpy (destStr, srcStr);
#endif
}


// Utility function to merge wcscat_s on Win and wcscat on Mac into one call
void safeWcscat (wchar_t *destStr, int size, const wchar_t *srcStr)
{
#ifdef PRWIN_ENV
	wcscat_s (destStr, size, srcStr);
#elif defined PRMAC_ENV
	wcscat (destStr, srcStr);
#endif
}

bool Check_prSuiteError( const prSuiteError errval, string &str )
{
	ostringstream oss;
	bool   found_error = true;
	str.clear();

	// top of error-check
	if ( PrSuiteErrorSucceeded(errval) )
		return false;

#define case_prSuiteError_VALUE(e) case e : str = #e; break

	switch( errval ) {
		//case_prSuiteError_VALUE(suiteError_NoError);	// Method succeeded
/*
**	General error results.
*/
		case_prSuiteError_VALUE(suiteError_Fail				);	// Method failed
		case_prSuiteError_VALUE(suiteError_InvalidParms		);	// A parameter to this method is invalid
		case_prSuiteError_VALUE(suiteError_OutOfMemory		);	// There is not enough memory to complete this method
		case_prSuiteError_VALUE(suiteError_InvalidCall		);	// Usually this means this method call is not appropriate at this time
		case_prSuiteError_VALUE(suiteError_NotImplemented	);	// The requested action is not implemented
		case_prSuiteError_VALUE(suiteError_IDNotValid		);	// The passed in ID (pluginID, clipID...) is not valid


/*
**	RenderSuite results
*/

/*	<private>
**	RenderSuite ErrorCategory == 1
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_RenderPending				);	// Render is pending
		case_prSuiteError_VALUE(suiteError_RenderedFrameNotFound		);	// A cached frame was not found.
		case_prSuiteError_VALUE(suiteError_RenderedFrameCanceled		);	// A render was canceled

		case_prSuiteError_VALUE(suiteError_RenderInvalidPixelFormat		);	// Render output pixel format list is invalid
		case_prSuiteError_VALUE(suiteError_RenderCompletionProcNotSet	);	// The render completion proc was not set for an async request

/*
**	TimeSuite results
*/

/*	<private>
**	TimeSuite ErrorCategory == 2
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_TimeRoundedAudioRate			);	// Audio rate returned was rounded

/*
**	Compiler{Render,Audio,Settings}Suite results
**
**	NOTE: If this list is changed in any way, you must also
**	update:
**
**	1.) SuiteErrorToCompilerError() and CompilerErrorToSuiteError()
**		in \Plugins\MediaCommon\MediaUtils\Src\Compilers\CompilerErrorUtils.cpp
**	2.)	CompilerErrorToSuiteError() in \MediaLayer\Src\Compilers\CompilerModuleCallbacks.cpp
*/

/*	<private>
**	Compiler{Render,Audio,Settings}Suite ErrorCategory == 3
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_CompilerCompileAbort				);	// User aborted the compile
		case_prSuiteError_VALUE(suiteError_CompilerCompileDone				);	// Compile finished normally
		case_prSuiteError_VALUE(suiteError_CompilerOutputFormatAccept		);	// The output format is valid
		case_prSuiteError_VALUE(suiteError_CompilerOutputFormatDecline		);	// The compile module cannot compile to the output format
		case_prSuiteError_VALUE(suiteError_CompilerRebuildCutList			);	// Return value from compGetFilePrefs used to force Premiere to bebuild its cutlist
		case_prSuiteError_VALUE(suiteError_CompilerIterateCompiler			);	// 6.0 Return value from compInit to request compiler iteration
		case_prSuiteError_VALUE(suiteError_CompilerIterateCompilerDone		);	// 6.0 Return value from compInit to indicate there are no more compilers
		case_prSuiteError_VALUE(suiteError_CompilerInternalErrorSilent		);	// 6.0 Silent error code; Premiere will not display an error message on screen.
																					// Compilers can return this error code from compDoCompile if they wish to
																					// put their own customized error message on screen just before returning 
																					// control to Premiere
		case_prSuiteError_VALUE(suiteError_CompilerIterateCompilerCacheable );	// 7.0 Return value from compInit to request compiler iteration and indicating that this
																					// compiler is cacheable.

		case_prSuiteError_VALUE(suiteError_CompilerBadFormatIndex			);	// Invalid format index - used to stop compGetIndFormat queries
		case_prSuiteError_VALUE(suiteError_CompilerInternalError			);	// 
		case_prSuiteError_VALUE(suiteError_CompilerOutOfDiskSpace			);	// Out of disk space error
		case_prSuiteError_VALUE(suiteError_CompilerBufferFull				);	// The offset into the audio buffer would overflow it
		case_prSuiteError_VALUE(suiteError_CompilerErrOther					);	// Someone set gCompileErr
		case_prSuiteError_VALUE(suiteError_CompilerErrMemory				);	// Ran out of memory
		case_prSuiteError_VALUE(suiteError_CompilerErrFileNotFound			);	// File not found
		case_prSuiteError_VALUE(suiteError_CompilerErrTooManyOpenFiles		);	// Too many open files
		case_prSuiteError_VALUE(suiteError_CompilerErrPermErr				);	// Permission violation
		case_prSuiteError_VALUE(suiteError_CompilerErrOpenErr				);	// Unable to open the file
		case_prSuiteError_VALUE(suiteError_CompilerErrInvalidDrive			);	// Drive isn't valid.
		case_prSuiteError_VALUE(suiteError_CompilerErrDupFile				);	// Duplicate Filename
		case_prSuiteError_VALUE(suiteError_CompilerErrIo					);	// File io error
		case_prSuiteError_VALUE(suiteError_CompilerErrInUse					);	// File is in use
		case_prSuiteError_VALUE(suiteError_CompilerErrCodecBadInput			);	// A video codec refused the input format
		case_prSuiteError_VALUE(suiteError_ExporterSuspended				);	// The host has suspended the export
		case_prSuiteError_VALUE(suiteError_ExporterNoMoreFrames			);	// Halt export early skipping all remaining frames including this one. AE uses

/*
**	FileSuite results
*/

/*	<private>
**	FileSuite ErrorCategory == 4
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_FileBufferTooSmall			);
		case_prSuiteError_VALUE(suiteError_FileNotImportableFileType	);	// Not an importable file type

/*
**	LegacySuite results
*/

/*	<private>
**	LegacySuite ErrorCategory == 5
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_LegacyInvalidVideoRate		);	// Invalid video rate (scale and sample rate don't match a valid rate)

/*
**	PlayModuleAudioSuite results
*/

/*	<private>
**	PlayModuleAudioSuite ErrorCategory == 6
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioInitFailure			);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioIllegalPlaySetting	);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioNotInitialized		);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioNotStarted			);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioIllegalAction			);

/*
**	PlayModuleDeviceControlSuite
*/

/*	<private>
**	PlayModuleDeviceControlSuite ErrorCategory == 7
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_PlayModuleDeviceControlSuiteIllegalCallSequence	);

/*
**	MediaAcceleratorSuite ErrorCategory == 8
*/
		case_prSuiteError_VALUE(suiteError_MediaAcceleratorSuitePathNotFound	);
		case_prSuiteError_VALUE(suiteError_MediaAcceleratorSuiteRegisterFailure	);


/*
**	Royalty Activation ErrorCategory == 9
*/
		case_prSuiteError_VALUE(suiteError_RepositoryReadFailed					);
		case_prSuiteError_VALUE(suiteError_RepositoryWriteFailed				);
		case_prSuiteError_VALUE(suiteError_NotActivated							);
		case_prSuiteError_VALUE(suiteError_DataNotPresent						);
		case_prSuiteError_VALUE(suiteError_ServerCommunicationFailed			);
		case_prSuiteError_VALUE(suiteError_Internal								);

/*
**	PrSDKStringSuite ErrorCategory == A
*/
		case_prSuiteError_VALUE(suiteError_StringNotFound						);
		case_prSuiteError_VALUE(suiteError_StringBufferTooSmall					);


/*
**	PrSDKVideoSegmentSuite ErrorCategory == B
*/
		case_prSuiteError_VALUE(suiteError_NoKeyframeAfterInTime				);

/*
**	PrSDKCaptioningSuite ErrorCategory == C
*/
		case_prSuiteError_VALUE(suiteError_NoMoreData							);

/*
**	PrSDKThreadedWorkSuite ErrorCategory == D
*/
		case_prSuiteError_VALUE(suiteError_InstanceDestroyed					);

		default:
			oss << "Unknown:" << std::hex << errval;
			str.clear();
			found_error = false;
	} // switch( errval )

	if ( errval == suiteError_NoError )
		found_error = false;

	return found_error;
}


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
nvenc_make_output_filename(const wstring &src, const wstring &postfix, const wstring &ext, wstring &dst)
{
	dst = src;
	size_t dst_newlen = dst.rfind(L".");
	if (dst_newlen != string::npos)
		dst.erase(dst_newlen, dst.size());

	dst += postfix;
	dst += L".";
	dst += ext;
}

void
nvenc_make_output_dirname(const wstring &src, wstring &dst)
{
	dst = src;
	// Hazardous --  assume the src fileaname includes at least one '\' character 
	//               We search for that '\' character, erase it and everything after,
	//               then append ext to it.
	//
	// Example:
	//    src = "C:\TEMP\abcd\hello.mpeg"
	//    Result:  dst = "C:\TEMP\acbd"

	size_t dst_newlen = dst.rfind(L"\\");
	dst.erase(dst_newlen, dst.size());
}
