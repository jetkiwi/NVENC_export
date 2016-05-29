/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007 Adobe Systems Incorporated                       */
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

//	This file is part of the public Premiere and After Effects SDKs.


#ifndef	PRSDKAPPINFOSUITE_H
#define PRSDKAPPINFOSUITE_H

#ifndef PRSDKMALERRORS_H
#include "PrSDKMALErrors.h"
#endif

#pragma pack(push, 1)

#ifdef __cplusplus
extern "C" {
#endif

#define kPrSDKAppInfoSuite				"MediaCore App Info Suite"
#define kPrSDKAppInfoSuiteVersion		1


// Used for kAppInfo_AppFourCC
#define kAppPremierePro			'PPro'
#define kAppPremiereElements	'PrEl'
#define	kAppAfterEffects		'FXTC'
#define kAppEncoreDVD			'EncD'
#define kAppSoundBooth			'SndB'
#define kAppMediaEncoder		'AME '
#define kAppCottonwood			'COWO'
#define kAppPrelude				'PRLD'

// Used for kAppInfo_Version
typedef struct
{
	unsigned int major;
	unsigned int minor;
	unsigned int build;
} VersionInfo;

typedef struct 
{
// Used for settingsSelector
	enum {
		kAppInfo_AppFourCC,
		kAppInfo_Version
	};

// GetAppInfo - Get information on the host application.
//		Plug-ins such as importers and compilers are supported in different
//		host applications.  This suite can be used to distinguish between hosts.

	prSuiteError (*GetAppInfo)(
		int settingsSelector,
		void *appInfo);

} PrSDKAppInfoSuite;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* PRSDKAPPINFOSUITE_H */

