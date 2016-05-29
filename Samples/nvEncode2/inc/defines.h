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


#if defined(WIN32) || defined(_WIN32) || defined(WIN64)
  #include <windows.h>
  #ifndef _CRT_SECURE_NO_DEPRECATE
  #define _CRT_SECURE_NO_DEPRECATE
  #endif
#else
  #include <stdio.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <string.h>
  #include <unistd.h>
#endif

typedef struct
{
    unsigned int nDeviceID;
    unsigned int startFrame;
    unsigned int endFrame;
    unsigned int numFramesToEncode;
    unsigned int mvc;
    unsigned int maxNumberEncoders;
    unsigned int dynamicResChange;
    unsigned int dynamicBitrateChange;
    unsigned int showCaps;

    char *input_file;
    char *output_file;
    char output_base_file[256], output_base_ext[8];
    char *psnr_file;

} EncoderAppParams;

