/*
 * Copyright 1993-2013 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

//---------------------------------------------------------------------------
// NOTES:
//
// Don't include this file directly. Use NvResult.h or NvResultStrings.h.
//
// IMPORTANT: Make sure you put "OK" results before RESULT_OK_LAST.
//
//---------------------------------------------------------------------------
//! \file NvResultTable.h
//! \brief Definitions for the \e NvResult enumeration values.
//!
//! This file contains the definitions of enumeration values for the
//! \e NvResult enum defined in NvResult.h.
//! \warning Never include this header file directly; include NvResult.h.
//---------------------------------------------------------------------------

#ifndef _NV_RESULT_TABLE_H
#define _NV_RESULT_TABLE_H

//! \brief Table of all the defined NvResult enumeration values.
//!
//! See the source file for the complete list.
#define NV_RESULT_TABLE \
\
    NV_RESULT_ENTRY(RESULT_OK, "The operation completed successfully.") \
\
    NV_RESULT_ENTRY(RESULT_FALSE,    "The operation completed successfully (False).") \
    NV_RESULT_ENTRY(RESULT_OK_ASYNC, "The operation completed successfully (Async).") \
    NV_RESULT_ENTRY(RESULT_OK_BREAK, "The operation completed successfully (Break).") \
\
    NV_RESULT_ENTRY(RESULT_OK_RESUME, "The operation completed successfully (Resume).") \
    NV_RESULT_ENTRY(RESULT_OK_DELETE, "The operation completed successfully (Delete).") \
    NV_RESULT_ENTRY(RESULT_OK_PICTURE_FILE, "The operation completed successfully (Picture File).") \
    NV_RESULT_ENTRY(RESULT_OK_BLOCK, "The operation completed successfully (Input Blocked).") \
    NV_RESULT_ENTRY(RESULT_OK_STUB,  "The operation completed successfully (Using stub implementation).") \
\
    NV_RESULT_ENTRY(RESULT_OK_LAST, "The operation completed successfully (Unknown).") \
\
    NV_RESULT_ENTRY_VALUE(RESULT_FAIL, 0x80000000 + RESULT_OK_LAST + 1, "Unspecified error.") \
\
    NV_RESULT_ENTRY(RESULT_ERROR,                 "Unspecified error.") \
    NV_RESULT_ENTRY(RESULT_NOT_IMPLEMENTED,       "Not implemented.") \
    NV_RESULT_ENTRY(RESULT_NO_INTERFACE,          "No such interface supported.") \
    NV_RESULT_ENTRY(RESULT_INVALID_POINTER,       "Invalid pointer.") \
    NV_RESULT_ENTRY(RESULT_INVALID_ARGUMENT,      "One or more arguments are invalid.") \
    NV_RESULT_ENTRY(RESULT_OUT_OF_MEMORY,         "Ran out of memory.") \
    NV_RESULT_ENTRY(RESULT_ACCESS_DENIED,         "General access denied error.") \
    NV_RESULT_ENTRY(RESULT_QUEUE_FULL,            "Queue full.") \
    NV_RESULT_ENTRY(RESULT_ABORTED,               "Operation was aborted without completing.") \
    NV_RESULT_ENTRY(RESULT_TIMEOUT,               "Operation timed out.") \
    NV_RESULT_ENTRY(RESULT_OUT_OF_HANDLES,        "Ran out of operating system handles.") \
    NV_RESULT_ENTRY(RESULT_INVALID_HANDLE,        "Invalid handle passed in as argument.") \
    NV_RESULT_ENTRY(RESULT_ERROR_NO_MEDIA,        "No media in drive.") \
    NV_RESULT_ENTRY(RESULT_ERROR_INVALID_MEDIA,   "Invalid or unsupported media.") \
    NV_RESULT_ENTRY(RESULT_FILE_NOT_FOUND,        "File not found.") \
    NV_RESULT_ENTRY(RESULT_INVALID_STATE,         "Invalid state.") \
    NV_RESULT_ENTRY(RESULT_INVALID_FILE_FORMAT,   "Invalid or unsupported file format.") \
    NV_RESULT_ENTRY(RESULT_END_OF_FILE,           "No more data - end of file.") \
    NV_RESULT_ENTRY(RESULT_ERROR_FILE_READ,       "Error reading file.") \
    NV_RESULT_ENTRY(RESULT_ERROR_FILE_WRITE,      "Error writing file.") \
    NV_RESULT_ENTRY(RESULT_VIDEO_TOO_LATE,        "Video is behind and is unable to catch up.") \
    NV_RESULT_ENTRY(RESULT_OUT_OF_CPU,            "Not enough CPU for operation.") \
    NV_RESULT_ENTRY(RESULT_OUT_OF_BANDWIDTH,      "Not enough bandwidth for operation.") \
    NV_RESULT_ENTRY(RESULT_DRM_UNSUPPORTED,       "Unable to decrypt content." ) \
    NV_RESULT_ENTRY(RESULT_DRM_NO_LICENSE,        "Unable to acquire License." ) \
    NV_RESULT_ENTRY(RESULT_DRM_RESTRICTED,        "Unable to play because of DRM playback restrictions." ) \
    NV_RESULT_ENTRY(RESULT_DRM_NEEDS_VALIDATION,  "Needs to do proximity detection on the device." ) \
\
    NV_RESULT_ENTRY(RESULT_INCOMPLETE_INSTALL_MISSING_COMPONENT, "One or more required components is missing from the installation.") \
    NV_RESULT_ENTRY(RESULT_INCOMPLETE_INSTALL_REBOOT,            "The installation process requires a reboot to complete.") \
\
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_MEDIA,                    "Invalid or unsupported media.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_UOP_BLOCK,                "Operation not permitted by content.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_DOMAIN,                   "Operation not valid in this domain.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_STATE,                    "Operation not valid in this state.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_RESTRICTED,               "Operation is restricted by content.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARENTAL,                 "Content is parental protected.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_MENU,               "Invalid menu request.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_TITLE,              "Invalid title argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_CHAPTER,            "Invalid chapter argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_GROUP,              "Invalid group argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_TRACK,              "Invalid track argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_TIME,               "Invalid time argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_SPEED,              "Invalid speed argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_BUTTON,             "Invalid button argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_AUDIO_STREAM,       "Invalid audio stream argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_SUB_PICTURE_STREAM, "Invalid sub picture stream argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_ANGLE,              "Invalid angle argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_PARENTAL_LEVEL,     "Invalid parental level argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_VIDEO_DISPLAY,      "Invalid video display argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_KARAOKE_MODE,       "Invalid karaoke mode argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_PARAM_HIDDEN_CODE,        "Invalid hidden code argument.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_GOUP,                     "Home operation not valid at this time.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_REGION,                   "DVD region mismatch.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_KEY,                      "Unable to authenticate DVD.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_DISC_MINOR,               "Minor disc read error occurred.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_DISC_MEDIUM,              "Medium disc read error occurred.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_DISC_MAJOR,               "Major disc read error occurred.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_DATA,                     "Corrupt data detected on disc.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_NAV_CMD,                  "Invalid command.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_HAL,                      "Hardware error.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_FILE,                     "File error.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_RESUME,                   "Resume is invalid.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_MENU,                     "Menu is invalid.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_FIRST_PLAY,               "First play is invalid.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_BUSY,                     "DVD navigator is busy.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_OSAL,                     "Operating system error.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_NO_BUTTON,                "No button at selected position.") \
    NV_RESULT_ENTRY(RESULT_DVD_ERROR_SKIP_CHAPTER,             "Disc Read Error Skip the current chapter.") \
\
    NV_RESULT_ENTRY(RESULT_NVSOCKET_WOULDBLOCK, "This operation would block the socket.") \
    NV_RESULT_ENTRY(RESULT_ERROR_DISCONNECTED, "The server is disconnected.") \
\
    NV_RESULT_ENTRY(RESULT_ERROR_LAST, "Unspecified error.")

#endif
