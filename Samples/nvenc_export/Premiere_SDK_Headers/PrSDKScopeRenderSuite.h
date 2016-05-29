/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2001 Adobe Systems Incorporated                       */
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

#ifndef PRSDKSCOPERENDERSUITE_H
#define PRSDKSCOPERENDERSUITE_H

#ifndef PRSDKTYPES_H
#include "PrSDKTypes.h"
#endif

#ifndef PRSDKMALERRORS_H
#include "PrSDKMALErrors.h"
#endif

#ifndef PRSDKPIXELFORMAT_H
#include "PrSDKPixelFormat.h"
#endif 

#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif

#define kPrSDKScopeRenderSuite			"Premiere Scope Render Suite"
#define kPrSDKScopeRenderSuiteVersion	2

typedef float PrScopeDisplayIntensity;

typedef enum
{
	kPrVideoDisplayType_Vectorscope,
	kPrVideoDisplayType_Waveform,
	kPrVideoDisplayType_RGBParade,
	kPrVideoDisplayType_YUVParade,
	kPrVideoDisplayType_VectWaveYParade,
	kPrVideoDisplayType_VectWaveRParade,
	kPrVideoDisplayType_Alpha,
	kPrVideoDisplayType_All,
	kPrVideoDisplayType_Off,
	kPrVideoDisplayType_DirectManipulation,
	kPrVideoDisplayType_Composite,					// Just display composite video
	kPrVideoDisplayType_AudioOnly					//	An audio-only clip is playing. Times should not be rounded to video frames,
													//	and no video display should be drawn.
} PrVideoDisplayType;

typedef struct
{
	PrVideoDisplayType		displayType;
	PrScopeDisplayIntensity	scopeIntensity;
	prBool					scopeUseSetup;
	prBool					scopeUseIRE;
	prBool					scopeShowChroma;
	prBool					scopeMagnify;
	csSDK_int32				reserved[23];
} PrVideoDisplayParameters;

typedef struct 
{
	prSuiteError (*RenderScope)(
		prRect								inFrameBounds,			/* bounds (always 0,0 origin!) */
		int									inRowBytes,				/* rowbytes */
		char*								inFrameBuffer,			/* the frame pixel data */
		prFloatRect							inFrameSubRect,			/* normalized sub-rect of video frame used for zooming */
		PrPixelFormat						inPixelFormat,			/* RGBA, YUV, etc */
		const PrVideoDisplayParameters* 	inScopeParameters,		/* Parameters for render */
		prWnd								inWindow,				/* Window to render into */
		prRect								inDisplayBounds);		/* Display video in this rect */	

} PrSDKScopeRenderSuite;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif // PRSDKSCOPERENDERSUITE_H
