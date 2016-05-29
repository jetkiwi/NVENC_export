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

#ifndef	PRSDKSTOCKIMAGESUITE_H
#define PRSDKSTOCKIMAGESUITE_H

#ifndef PRSDKTYPES_H
#include "PrSDKTypes.h"
#endif

#ifndef PRSDKPIXELFORMAT_H
#include "PrSDKPixelFormat.h"
#endif 

#ifndef PRSDKMALERRORS_H
#include "PrSDKMALErrors.h"
#endif

#ifndef PRSDKPPIXSUITE_H
#include "PrSDKPPixSuite.h"
#endif 

#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif

#define kPrSDKStockImageSuite				"Premiere Stock Image Suite"
#define kPrSDKStockImageSuiteVersion		1

enum
{
	PrSDK_StockImage_PlayingOnHardware = 1,
	PrSDK_StockImage_NotYetRendered,
	PrSDK_StockImage_ColorBars,
	PrSDK_StockImage_CapturePreviewOnHardware,
	PrSDK_StockImage_MediaPending,
	PrSDK_StockImage_MediaOffline,
	PrSDK_StockImage_END					// check value, always comes last in enum, do not use
};

typedef struct 
{
	/**
	*
	**/
	prSuiteError (*GetStockImage)(
		PPixHand*		outPPixHand,
		int				inWhichImage,
		int				inWidth,
		int				inHeight,
		PrPixelFormat	inPixelFormat,
		int				inPARNumerator,
		int				inPARDenominator);


} PrSDKStockImageSuite;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif
