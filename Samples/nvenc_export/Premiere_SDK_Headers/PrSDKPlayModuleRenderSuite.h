/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2002 Adobe Systems Incorporated                       */
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

#ifndef	PRSDKPLAYMODULERENDERSUITE_H
#define PRSDKPLAYMODULERENDERSUITE_H

#ifndef PRSDKTYPES_H
#include "PrSDKTypes.h"
#endif

#ifndef PRSDKPIXELFORMAT_H
#include "PrSDKPixelFormat.h"
#endif 

#ifndef PRSDKMALERRORS_H
#include "PrSDKMALErrors.h"
#endif

#ifndef PRSDKPLAYMODULE_H
#include "PrSDKPlayModule.h"
#endif

#ifndef PRSDKQUALITY_H
#include "PrSDKQuality.h"
#endif

#ifndef PRSDKTIMESUITE_H
#include "PrSDKTimeSuite.h"
#endif

#ifndef PRSDKRENDERCACHETYPE_H
#include "PrSDKRenderCacheType.h"
#endif

#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*PrSDKPlayModuleRenderSuite_AsyncCompletionProc)(void* inAsyncCompletionData, csSDK_int32 inRequestID, PPixHand inPPixHand);

#define kPrSDKPlayModuleRenderSuite					"PremierePlaymodRenderSuite"
#define kPrSDKPlayModuleRenderSuiteVersion			4

#define kPrSDKPlayModuleRenderSuiteVersion_3		3
#define kPrSDKPlayModuleRenderSuiteVersion_2		2

typedef struct 
{
	/**
	**	This will render the video frame synchronously. It will return will not return until the requested
	**	video frame has been rendered or an error has occured.
	**
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**	@param	inFrameTime							The time of the video frame number.
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	**	@param	inCacheFlags						The cache flags.
	*/
	prSuiteError (*RenderVideoFrame)(
		csSDK_int32				inPlayID,								
		const PrTime*			inFrameTime, 
		PPixHand*				outRenderedFrame,					
		const PrPixelFormat*	inRequestedPixelFormatArray,		
		csSDK_int32				inRequestedPixelFormatArrayCount,	
		const prRect*			inFrameRect,						
		csSDK_uint32			inPixelAspectRatioNumerator,		
		csSDK_uint32			inPixelAspectRatioDenominator,		
		PrRenderQuality			inRenderQuality,
		PrRenderCacheType		inCacheFlags,
		prBool					inRenderFields);

	/**
	**	This will queue a render of a video frame. It will return immediately. If the frame was available in the cache
	**		it will be returned from this call and the completion proc will not be called. If the frame is not available, 
	**		the request will be queued and the completion proc will be called when the request is complete. 
	**	Note: while inside your completion proc, no other video frames will be rendered so do not do any time consuming
	**		work in your completion proc. 
	**	The async render completion proc must be set before calling this function.
	**
	**	@param	inAsyncCompletionData				User specific data sent to the async completion proc
	**	@param	inFrameTime							The time of the video frame number.
	**	@param	outRequestID						The request ID, if the frame is rendered async
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	**	@param	inCacheFlags						The cache flags.
	*/
	prSuiteError (*QueueAsyncVideoFrameRender)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrameTime,
		void*					inAsyncCompletionData,
		csSDK_int32*			outRequestID,
		PPixHand*				outRenderedFrame,

		const PrPixelFormat*	inRequestedPixelFormatArray,																	
		csSDK_int32				inRequestedPixelFormatArrayCount,
		const prRect*			inFrameRect,
		csSDK_uint32			inPixelAspectRatioNumerator,
		csSDK_uint32			inPixelAspectRatioDenominator,
		PrRenderQuality			inRenderQuality,
		PrRenderCacheType		inCacheFlags,
		prBool					inRenderFields);
	
	/**
	**	Sets the completion function that will be called when an async render request is completed.
	**	Note: while inside your completion proc, no other video frames will be rendered so do not do any time consuming
	**		work in your completion proc. 
	**	Instance data is sent into each async render call, and will be returned to this completion proc.
	**
	**	@param	inCompletionProc	This method will be called when async renders are completed.
	*/
	prSuiteError (*SetAsyncRenderCompletionProc)(
		csSDK_int32										inPlayID,	
		PrSDKPlayModuleRenderSuite_AsyncCompletionProc	inCompletionProc);		
	
	/*
	**	Cancel ALL pending render requests for this RenderFrame ref.
	*/
	prSuiteError (*CancelAllOutstandingAsyncRequests)(
		csSDK_int32				inPlayID);

	/*
	** Cancel a specific render request.
	**
	** @param inAsyncRequestID	Identifies the render request.
	*/
	prSuiteError (*CancelOneOutstandingAsyncRequest)(
		csSDK_int32				inPlayID,
		csSDK_int32				inAsyncRequestID);

	/**
	**	Returns the video frame if it is already in the cache. This does not cause a render to occur.
	**
	**	@param	inFrame								The video frame number.
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	*/
	prSuiteError (*FetchRenderedFrameFromCache)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame, 
		PPixHand*				outRenderedFrame,					
		
		const PrPixelFormat*	inRequestedPixelFormatArray,		 
																	
		csSDK_int32				inRequestedPixelFormatArrayCount,	
		const prRect*			inFrameRect,						
		csSDK_uint32			inPixelAspectRatioNumerator,		
		csSDK_uint32			inPixelAspectRatioDenominator,				 
		PrRenderQuality			inRenderQuality);					

	/**
	**	Pre-fetches the media needed to render this frame. 
	**	This is a hint to the importers to begin reading media needed to render this video frame.
	**
	**	@param	inFrame								The video frame number.
	*/
	prSuiteError (*PrefetchMedia)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame);

	/**
	**	Pre-fetches the media needed to render this frame, using all of the parameters used to render the frame. 
	**	This is a hint to the importers to begin reading media needed to render this video frame.
	**
	**	@param	inFrameTime							The time of the video frame number.
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	*/
	prSuiteError (*PrefetchMediaWithRenderParameters)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrameTime,
		const PrPixelFormat*	inRequestedPixelFormatArray,																	
		csSDK_int32				inRequestedPixelFormatArrayCount,
		const prRect*			inFrameRect,
		csSDK_uint32			inPixelAspectRatioNumerator,
		csSDK_uint32			inPixelAspectRatioDenominator,
		PrRenderQuality			inRenderQuality,
		prBool					inRenderFields);


	/**
	**	This will cancel all media pre-fetches that are still outstanding. 
	*/
	prSuiteError (*CancelAllOutstandingMediaPrefetches)(
		csSDK_int32				inPlayID);

	/**
	**	Is all the prefetched media ready?
	*/
	prSuiteError (*IsPrefetchedMediaReady)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame,
		prBool*					outMediaReady);

	/*
	**	This will add a video frame to the cache. 
	**	As an example, this can be used to add a video frame of another pixel format back to the cache. 
	**		Then future requests for this video frame may be satisfied in this pixel format.
	**
	**	@param inOriginalPPix		The original PPix that was rendered.
	**	@param inNewPPix			The new PPix that also represents this frame.
	*/
	prSuiteError (*AddFrameToCache)(
		csSDK_int32				inPlayID,
		PPixHand				inOriginalPPix, 
		PPixHand				inNewPPix);

	/**
	**	Set the type of video frame returned. If set to TRUE the video frame returned may have transparent
	**		alpha. Note that this type of video frame still needs to be composited on black. This is an 
	**		option if you can perform the final composite in hardware, otherwise set to FALSE and the 
	**		final composite onto black will be performed during the render
	**
	**	@param inAllowTransparentVideoFrames	If TRUE, the resulting video frame will not be composited onto black.
	*/
	prSuiteError (*AllowTransparentVideoFrames)(
		csSDK_int32				inPlayID,							
		prBool					inAllowTransparentVideoFrames);		



	/**
	**	Cancels Pre-fetches for the media needed to render this frame, using all of the parameters used to render the frame. 
	**
	**	@param	inFrameTime							The time of the video frame number.
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	*/
	prSuiteError (*CancelPrefetchMediaWithRenderParameters)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrameTime,
		const PrPixelFormat*	inRequestedPixelFormatArray,																	
		csSDK_int32				inRequestedPixelFormatArrayCount,
		const prRect*			inFrameRect,
		csSDK_uint32			inPixelAspectRatioNumerator,
		csSDK_uint32			inPixelAspectRatioDenominator,
		PrRenderQuality			inRenderQuality,
		prBool					inRenderFields);
	
	/**
	**	Force a refresh of the RT segment status for a given player
	*/
	prSuiteError (*RefreshRTStatus)(
		csSDK_int32				inPlayID);

	/**
	**	Query the RT status of an accelerated renderer if used
	*/
	prSuiteError (*GetAcceleratedRendererRTStatusForTime)(
		csSDK_int32				inPlayID,
		prtPlayableRangePtr		outplayableRange);

} PrSDKPlayModuleRenderSuite;

typedef PrSDKPlayModuleRenderSuite PrSDKPlayModuleRenderSuite_3;

typedef struct 
{
	/**
	**	This will render the video frame synchronously. It will return will not return until the requested
	**	video frame has been rendered or an error has occured.
	**
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**	@param	inFrameTime							The time of the video frame number.
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	**	@param	inCacheFlags						The cache flags.
	*/
	prSuiteError (*RenderVideoFrame)(
		csSDK_int32				inPlayID,								
		const PrTime*			inFrameTime, 
		PPixHand*				outRenderedFrame,					
		const PrPixelFormat*	inRequestedPixelFormatArray,		
		csSDK_int32				inRequestedPixelFormatArrayCount,	
		const prRect*			inFrameRect,						
		csSDK_uint32			inPixelAspectRatioNumerator,		
		csSDK_uint32			inPixelAspectRatioDenominator,		
		PrRenderQuality			inRenderQuality,
		PrRenderCacheType		inCacheFlags,
		prBool					inRenderFields);

	/**
	**	This will queue a render of a video frame. It will return immediately. If the frame was available in the cache
	**		it will be returned from this call and the completion proc will not be called. If the frame is not available, 
	**		the request will be queued and the completion proc will be called when the request is complete. 
	**	Note: while inside your completion proc, no other video frames will be rendered so do not do any time consuming
	**		work in your completion proc. 
	**	The async render completion proc must be set before calling this function.
	**
	**	@param	inAsyncCompletionData				User specific data sent to the async completion proc
	**	@param	inFrameTime							The time of the video frame number.
	**	@param	outRequestID						The request ID, if the frame is rendered async
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	**	@param	inCacheFlags						The cache flags.
	*/
	prSuiteError (*QueueAsyncVideoFrameRender)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrameTime,
		void*					inAsyncCompletionData,
		csSDK_int32*			outRequestID,
		PPixHand*				outRenderedFrame,

		const PrPixelFormat*	inRequestedPixelFormatArray,																	
		csSDK_int32				inRequestedPixelFormatArrayCount,
		const prRect*			inFrameRect,
		csSDK_uint32			inPixelAspectRatioNumerator,
		csSDK_uint32			inPixelAspectRatioDenominator,
		PrRenderQuality			inRenderQuality,
		PrRenderCacheType		inCacheFlags,
		prBool					inRenderFields);
	
	/**
	**	Sets the completion function that will be called when an async render request is completed.
	**	Note: while inside your completion proc, no other video frames will be rendered so do not do any time consuming
	**		work in your completion proc. 
	**	Instance data is sent into each async render call, and will be returned to this completion proc.
	**
	**	@param	inCompletionProc	This method will be called when async renders are completed.
	*/
	prSuiteError (*SetAsyncRenderCompletionProc)(
		csSDK_int32										inPlayID,	
		PrSDKPlayModuleRenderSuite_AsyncCompletionProc	inCompletionProc);		
	
	/*
	**	Cancel ALL pending render requests for this RenderFrame ref.
	*/
	prSuiteError (*CancelAllOutstandingAsyncRequests)(
		csSDK_int32				inPlayID);

	/*
	** Cancel a specific render request.
	**
	** @param inAsyncRequestID	Identifies the render request.
	*/
	prSuiteError (*CancelOneOutstandingAsyncRequest)(
		csSDK_int32				inPlayID,
		csSDK_int32				inAsyncRequestID);

	/**
	**	Returns the video frame if it is already in the cache. This does not cause a render to occur.
	**
	**	@param	inFrame								The video frame number.
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	*/
	prSuiteError (*FetchRenderedFrameFromCache)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame, 
		PPixHand*				outRenderedFrame,					
		
		const PrPixelFormat*	inRequestedPixelFormatArray,		 
																	
		csSDK_int32				inRequestedPixelFormatArrayCount,	
		const prRect*			inFrameRect,						
		csSDK_uint32			inPixelAspectRatioNumerator,		
		csSDK_uint32			inPixelAspectRatioDenominator,				 
		PrRenderQuality			inRenderQuality);					

	/**
	**	Pre-fetches the media needed to render this frame. 
	**	This is a hint to the importers to begin reading media needed to render this video frame.
	**
	**	@param	inFrame								The video frame number.
	*/
	prSuiteError (*PrefetchMedia)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame);

	/**
	**	Pre-fetches the media needed to render this frame, using all of the parameters used to render the frame. 
	**	This is a hint to the importers to begin reading media needed to render this video frame.
	**
	**	@param	inFrameTime							The time of the video frame number.
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	*/
	prSuiteError (*PrefetchMediaWithRenderParameters)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrameTime,
		const PrPixelFormat*	inRequestedPixelFormatArray,																	
		csSDK_int32				inRequestedPixelFormatArrayCount,
		const prRect*			inFrameRect,
		csSDK_uint32			inPixelAspectRatioNumerator,
		csSDK_uint32			inPixelAspectRatioDenominator,
		PrRenderQuality			inRenderQuality,
		prBool					inRenderFields);


	/**
	**	This will cancel all media pre-fetches that are still outstanding. 
	*/
	prSuiteError (*CancelAllOutstandingMediaPrefetches)(
		csSDK_int32				inPlayID);

	/**
	**	Is all the prefetched media ready?
	*/
	prSuiteError (*IsPrefetchedMediaReady)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame,
		prBool*					outMediaReady);

	/*
	**	This will add a video frame to the cache. 
	**	As an example, this can be used to add a video frame of another pixel format back to the cache. 
	**		Then future requests for this video frame may be satisfied in this pixel format.
	**
	**	@param inOriginalPPix		The original PPix that was rendered.
	**	@param inNewPPix			The new PPix that also represents this frame.
	*/
	prSuiteError (*AddFrameToCache)(
		csSDK_int32				inPlayID,
		PPixHand				inOriginalPPix, 
		PPixHand				inNewPPix);

	/**
	**	Set the type of video frame returned. If set to TRUE the video frame returned may have transparent
	**		alpha. Note that this type of video frame still needs to be composited on black. This is an 
	**		option if you can perform the final composite in hardware, otherwise set to FALSE and the 
	**		final composite onto black will be performed during the render
	**
	**	@param inAllowTransparentVideoFrames	If TRUE, the resulting video frame will not be composited onto black.
	*/
	prSuiteError (*AllowTransparentVideoFrames)(
		csSDK_int32				inPlayID,							
		prBool					inAllowTransparentVideoFrames);		


} PrSDKPlayModuleRenderSuite_2;



/*
**	Older RenderSuite 1.0
**	Please use the new render suite.
*/

#define kPrSDKPlayModuleRenderSuite_1				"Premiere Playmod Render Suite"
#define kPrSDKPlayModuleRenderSuiteVersion_1		1

typedef struct 
{
	/**
	**	This will render the video frame synchronously. It will return will not return until the requested
	**	video frame has been rendered or an error has occured.
	**
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**	@param	inFrameTime							The time of the video frame number.
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	**	@param	inCacheFlags						The cache flags.
	*/
	prSuiteError (*RenderVideoFrame)(
		csSDK_int32				inPlayID,								
		const PrTime*			inFrameTime, 
		PPixHand*				outRenderedFrame,					
		const PrPixelFormat*	inRequestedPixelFormatArray,		
		csSDK_int32				inRequestedPixelFormatArrayCount,	
		const prRect*			inFrameRect,						
		csSDK_uint32			inPixelAspectRatioNumerator,		
		csSDK_uint32			inPixelAspectRatioDenominator,		
		PrRenderQuality			inRenderQuality,
		PrRenderCacheType		inCacheFlags,
		prBool					inRenderFields);

	/**
	**	This will queue a render of a video frame. It will return immediately. If the frame was available in the cache
	**		it will be returned from this call and the completion proc will not be called. If the frame is not available, 
	**		the request will be queued and the completion proc will be called when the request is complete. 
	**	Note: while inside your completion proc, no other video frames will be rendered so do not do any time consuming
	**		work in your completion proc. 
	**	The async render completion proc must be set before calling this function.
	**
	**	@param	inAsyncCompletionData				User specific data sent to the async completion proc
	**	@param	inFrameTime							The time of the video frame number.
	**	@param	outRequestID						The request ID, if the frame is rendered async
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	**	@param	inCacheFlags						The cache flags.
	*/
	prSuiteError (*QueueAsyncVideoFrameRender)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrameTime,
		void*					inAsyncCompletionData,
		csSDK_int32*			outRequestID,
		PPixHand*				outRenderedFrame,

		const PrPixelFormat*	inRequestedPixelFormatArray,																	
		csSDK_int32				inRequestedPixelFormatArrayCount,
		const prRect*			inFrameRect,
		csSDK_uint32			inPixelAspectRatioNumerator,
		csSDK_uint32			inPixelAspectRatioDenominator,
		PrRenderQuality			inRenderQuality,
		PrRenderCacheType		inCacheFlags,
		prBool					inRenderFields);
	
	/**
	**	Sets the completion function that will be called when an async render request is completed.
	**	Note: while inside your completion proc, no other video frames will be rendered so do not do any time consuming
	**		work in your completion proc. 
	**	Instance data is sent into each async render call, and will be returned to this completion proc.
	**
	**	@param	inCompletionProc	This method will be called when async renders are completed.
	*/
	prSuiteError (*SetAsyncRenderCompletionProc)(
		csSDK_int32										inPlayID,	
		PrSDKPlayModuleRenderSuite_AsyncCompletionProc	inCompletionProc);		
	
	/*
	**	Cancel ALL pending render requests for this RenderFrame ref.
	*/
	prSuiteError (*CancelAllOutstandingAsyncRequests)(
		csSDK_int32				inPlayID);

	/*
	** Cancel a specific render request.
	**
	** @param inAsyncRequestID	Identifies the render request.
	*/
	prSuiteError (*CancelOneOutstandingAsyncRequest)(
		csSDK_int32				inPlayID,
		csSDK_int32				inAsyncRequestID);

	/**
	**	Returns the video frame if it is already in the cache. This does not cause a render to occur.
	**
	**	@param	inFrame								The video frame number.
	**	@param	outRenderedFrame					The output ppix. The caller is responsible to destroy this
	**
	**	@param	inRequestedPixelFormatArray			An array of PrPixelFormats that list your format preferences in order.
	**												This list must end with PrPixelFormat_BGRA_4444_8u.
	**	@param	inRequestedPixelFormatArrayCount	Number of formats in the format array
	**	@param	inFrameRect							Video frame size
	**	@param	inPixelAspectRatioNumerator			The numerator for the pixel aspect ratio.
	**	@param	inPixelAspectRatioDenominator		The denominator for the pixel aspect ratio.										 
	**	@param	inRenderQuality						The render quality of this frame.
	*/
	prSuiteError (*FetchRenderedFrameFromCache)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame, 
		PPixHand*				outRenderedFrame,					
		
		const PrPixelFormat*	inRequestedPixelFormatArray,		 
																	
		csSDK_int32				inRequestedPixelFormatArrayCount,	
		const prRect*			inFrameRect,						
		csSDK_uint32			inPixelAspectRatioNumerator,		
		csSDK_uint32			inPixelAspectRatioDenominator,				 
		PrRenderQuality			inRenderQuality);					

	/**
	**	Pre-fetches the media needed to render this frame. 
	**	This is a hint to the importers to begin reading media needed to render this video frame.
	**
	**	@param	inFrame								The video frame number.
	*/
	prSuiteError (*PrefetchMedia)(
		csSDK_int32				inPlayID,
		const PrTime*			inFrame);

	/**
	**	This will cancel all media pre-fetches that are still outstanding. 
	*/
	prSuiteError (*CancelAllOutstandingMediaPrefetches)(
		csSDK_int32				inPlayID);

	/*
	**	This will add a video frame to the cache. 
	**	As an example, this can be used to add a video frame of another pixel format back to the cache. 
	**		Then future requests for this video frame may be satisfied in this pixel format.
	**
	**	@param inOriginalPPix		The original PPix that was rendered.
	**	@param inNewPPix			The new PPix that also represents this frame.
	*/
	prSuiteError (*AddFrameToCache)(
		csSDK_int32				inPlayID,
		PPixHand				inOriginalPPix, 
		PPixHand				inNewPPix);

	/**
	**	Set the type of video frame returned. If set to TRUE the video frame returned may have transparent
	**		alpha. Note that this type of video frame still needs to be composited on black. This is an 
	**		option if you can perform the final composite in hardware, otherwise set to FALSE and the 
	**		final composite onto black will be performed during the render
	**
	**	@param inAllowTransparentVideoFrames	If TRUE, the resulting video frame will not be composited onto black.
	*/
	prSuiteError (*AllowTransparentVideoFrames)(
		csSDK_int32				inPlayID,							
		prBool					inAllowTransparentVideoFrames);		

} PrSDKPlayModuleRenderSuite_1;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
