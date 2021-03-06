/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2000 Adobe Systems Incorporated                       */
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

//	This file is part of the public Premiere 6.0 SDK.


#ifndef PRSDKPLUGWINDOW_H
#define PRSDKPLUGWINDOW_H

#ifndef PRSDKTYPES_H
#include "PrSDKTypes.h"
#endif

#pragma pack(push, 1)

// window functions

typedef void (*plugUpdateAllWindowsFunc)(void);
typedef void* (*plugGetMainWindFunc)(void);


typedef struct
{
	plugUpdateAllWindowsFunc	updateAllWindows;
	plugGetMainWindFunc			getMainWnd;
} PlugWindowFuncs, *PlugWindowFuncsPtr;

#pragma pack(pop)

#endif /* PRSDKPLUGWINDOW_H */
