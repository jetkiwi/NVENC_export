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

#include "defines.h"

#include <nvEncodeAPI.h>
#include "CNVEncoderH264.h"
#include "xcodeutil.h"
#include <platform/NvTypes.h>

#include <include/helper_string.h>
#include <include/helper_timer.h>
#include <include/nvFileIO.h>

#include <cuviddec.h> // cudaVideoChromaFormat

#pragma warning (disable:4189)

extern "C"
void printHelp()
{
    printf("[ NVENC 2.0 Alpha Command Line Encoder] <%s> %s\n", __TIME__ , __DATE__ );

    printf("Usage: nvEncoder -infile=input.yuv -outfile=output.264 <optional params>\n\n");

    printf("> Encoder Test Application Parameters\n");
    printf(" <<Required Parameters>>\n");
    printf("   [-infile=input.mp4]    MPEG bitstream input (*.MP4, *.TS, *.M2TS)\n");
    printf("   [-outfile=output.264]  Output elementary-stream H.264(HEVC) Video filename\n");
//  printf("   [-width=n]             Width  of video source (i.e. n = (704, 1280, 1920))\n");
//  printf("   [-height=n]            Height of video source (i.e. n = (480,  720, 1080))\n");
    printf(" <<Optional Parameters>>\n");
	printf("   [-device=n]            Select the CUDA-devicenum(GPUID#) to run decode/encode (default==0) \n");
	printf("                   ...overrides display GPU device that is used to create an Encoder context.\n");
    printf("   [-darwidth=n]          DisplayAspectRatio Width  (default=infile's)\n");
    printf("   [-darheight=n]         DisplayAspectRatio Height (default=infile's)\n");
    printf("   [-psnr=out_psnr.txt]   Output File Peak SNR\n");
    printf("   [-interface=n]         0=DX9, 1=DX11, 2=CUDA, 3=DX10 (default=2, don't change)\n");
    printf("   [-showCaps]            Display NVENC Hardware Capabilities detected\n");
//  printf("   [-syncMode=n]          1=Enable Asynchronous Mode, Windows OS only\n");// always in async-mode
    printf("   {-useMappedResources]  Enable NVENC buffer interop with DirectX or CUDA\n"); 
    printf("   [-maxNumberEncoders=n] n=number of encoders to use when multiple GPUs are detected\n");
    printf("\n\n");

    printf("> NVENC Hardware Parameters\n");
    printf(" <<Required Parameters>>: See Video Encoder Interface Guide or (nvEncodeAPI.h) <\n");
    printf("   [-codec=n]          Specify which codec to use, %0d = H.264/AVC (default)\n", NV_ENC_H264);
	printf("                         %0d = H.265/HEVC (requires Maxwell Gen2 or later hardware)\n", NV_ENC_H265);
	printf("   [-bitrate=n]        [VBR, CBR/2CBR, VBR_MinQP] Avg Video Bitrate of output file (eg: n=6000000 for 6 mbps)\n");
    printf("      or [-kbitrate=n] ... Bitrate of output file (units=1000 bits, eg: n=6000 for 6 mbps)\n");
    printf("      or [-mbitrate=n] ... Bitrate of output file (units=million bits, eg: n=6 for 6 mbps)\n");
	printf("   [-maxbitrate=n]     [not CBR/2CBR] Maximum Video Bitrate of output file (eg: n=6000000 for 6 mbps)\n");
    printf("      or [-maxkbitrate=n] ... Bitrate of output file (units=1000 bits, eg: n=6000 for 6 mbps)\n");
    printf("      or [-maxmbitrate=n] ... Bitrate of output file (units=million bits, eg: n=6 for 6 mbps)\n");
    printf("   [-rcmode=n]         Rate Control Mode (0=Constant QP, 1=VBR, 2=CBR, 4=VBR_MinQP, 8=2pass Qual, 32=2pass VBR\n");
    printf(" <<Optional Parameters>>: See Video Encoder Interface Guide or (nvEncodeAPI.h) <\n");
    printf("   [-bdirectmode=n]    Specify B-slice Direct-mode (0=auto, 1=disable, 2=temporal, 3=spatial)\n");
    printf("   [-startframe=n]     Start Frame (within input file)\n");
    printf("   [-endframe=n]       End Frame (within input file)\n");
    printf("   [-numerator=n]      Frame Rate numerator   (default = 30000)  (numerator/denominator = 29.97fps)\n");
    printf("   [-denomonator=n]    Frame Rate denominator (default =  1001)\n");
	printf("   [-num_b_frames=n]   Specify Max #B-frames (default = 0, max = 4)\n");
    printf("   [-goplength=n]      Specify GOP length (N=distance between I-Frames)\n");
    printf("                         (i.e. n=12 for GOP IBBPBBPBBPBBP)\n");
    printf("   [-monoChromeEncoding] (force black&white encoding, default==off)\n");
	printf("       for all *qp* settings, <n> ranges from {0 .. 51}: 0=best quality, 51=lowest quality\n");
    printf("   [-qpALL=n]          [ConstantQP only] Quantization value for all frames\n");
    printf("   [-qpI=n]            [ConstantQP only] QP value for I-frames (overrides -qpALL)\n");
    printf("   [-qpB=n]            [ConstantQP only] QP value for B-frames (...)\n");
    printf("   [-qpP=n]            [ConstantQP only] QP value for P-frames (...)\n");
    printf("   [-min_qpALL=n]      [VBR_MinQP only] Min QP value for all frames\n");
	printf("                       for 4kvideo, you should use -min_qpALL=0\n");
    printf("   [-min_qpI=n]        [VBR_MinQP only] Min QP value for I-frames (overrides -min_qpALL)\n");
    printf("   [-min_qpB=n]        [VBR_MinQP only] Min QP value for B-frames (...)\n");
    printf("   [-min_qpP=n]        [VBR_MinQP only] Min QP value for P-frames (...)\n");
    printf("   [-max_qpALL=n]      [?!?] Max QP value for all frames\n");
	printf("                       for 4kvideo, you should use -max_qpALL=51\n");
    printf("   [-max_qpI=n]        [?!?] Max QP value for I-frames (overrides -max_qpALL)\n");
    printf("   [-max_qpB=n]        [?!?] Max QP value for B-frames (...)\n");
    printf("   [-max_qpP=n]        [?!?] Max QP value for P-frames (...)\n");
	printf("   [-initial_qpALL=n]  [?!?] initial QP value for all frames\n");
	printf("   [-initial_qpI=n]    [?!?] initial QP value for I-frames\n");
	printf("   [-initial_qpB=n]    [?!?] initial QP value for B-frames\n");
	printf("   [-initial_qpP=n]    [?!?] initial QP value for P-frames\n");
    printf("   [-hiearchicalP=n]   Hiearchical value for P\n");
    printf("   [-hiearchicalB=n]   Hiearchical value for B\n");
    printf("   [-svcTemporal=n]    SVC Temporal (1=enable, 0=disable)\n");
    printf("   [-numlayers=n]      Number of layers for SVC (requires flag -svcTemporal=1) \n");
    printf("   [-outbandspspps=n]  Outband SPSPPS (1=enable, 0=disable)\n");
//  printf("   [-mvc=n]            MultiView Codec (1=enable, 0=disable)\n");// not supported
    printf("   [-profile=n]        Codec Profile (n=profile #)\n");
	printf("                           %0d = (H264/HEVC Autoseelct Profile) DEFAULT\n", NV_ENC_CODEC_PROFILE_AUTOSELECT);
    printf("                           %0d = (H264 Baseline)\n", NV_ENC_H264_PROFILE_BASELINE);
    printf("                           %0d = (H264 Main Profile)\n", NV_ENC_H264_PROFILE_MAIN);
    printf("                           %0d = (H264 High Profile)\n", NV_ENC_H264_PROFILE_HIGH);
	printf("                           %0d = (H264 High444 Predictive)\n", NV_ENC_H264_PROFILE_HIGH_444);
	printf("                           %0d = (H264 Constrained high)\n", NV_ENC_H264_PROFILE_CONSTRAINED_HIGH);
	printf("                           %0d = (HEVC Main Profile)\n", NV_ENC_HEVC_PROFILE_MAIN);
//    printf("   [-stereo3dMode=n]   Stereo Mode Packing Mode\n");
//    printf("                          0 = No Stereo Packing\n");
//    printf("                          1 = Checkerboard\n");
//    printf("                          2 = Column Interleave\n");
//    printf("                          3 = Row Interleave\n");
//    printf("                          4 = Side by Side\n");
//    printf("                          5 = Top-Bottom\n");
//    printf("                          6 = Frame Sequential\n");
//  printf("   [-stereo3dEnable=n] 3D Stereo (1=enable, 0=disable)\n");// not supported
	printf("   [-sliceMode=n]      determines how sliceModeData is interpreted\n");
	printf("                       n=3  (# slices per picture)  DEFAULT\n");
    printf("   [-sliceModeData=n]  Specify Number of Slices to be encoded per Frame\n");
	printf("                       (default=1, lower#s give better efficiency.  Bluray 1080p\n");
	printf("                       requires 4 slices\n");
	printf("   [-preset=n]         Specify the encoding preset (NVENC API %0u.%0u)\n",
		NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION
	);
    printf("                         -1 = No preset\n");
    printf("                          0 = Default\n");
    printf("                          1 = Low Latency Default\n");
    printf("                          2 = High Performance\n");
    printf("                          3 = High Quality\n");
    printf("                          4 = BluRay\n");
    printf("                          5 = Low Latency High Quality (HQ)\n");
    printf("                          6 = Low Latency High Performance (HP)\n");
    printf("                          8 = Lossless Default *(requires -profile=%0u)\n", NV_ENC_H264_PROFILE_HIGH_444);
    printf("                          9 = Lossless (HP) (same as above)\n");
    printf("   [-disableCodecCfg=n] Disabling codec config allows the application to pick up the parameters\n");
    printf("                          0 = Codec specific parameter is provided by application.\n");
    printf("                          1 = Required if using manual NVENC parameter settings\n");
    printf("   [-fieldmode=n]     Field Encoding Mode (0=Frame, 2=Field, 3=MBAFF)\n"); 
    printf("   [-maxNumRefFrames=n] Reference Frames (1..4: if Bframes enabled must be >= 2)\n");
	printf("   [-ltrNumFrames=n]    #long term reference frames (if supported), 0=default\n");
    printf("   [-mvprecision=n]   Motion Vector Precision (1=full, 2=half, 3=quarter pixel) \n"); 
    printf("   [-enableFMO=n]     FMO Mode (0=Autoselect, 1=Enabled, 2=Disabled)\n"); 
    printf("   [-outputseiBufferPeriod=n]\n"); 
    printf("   [-outputseiPictureTime=n]\n"); 
    printf("   [-outputAud=n]\n"); 
	printf("   [-level_idc=n] %0d=auto (DEFAULT),\n", NV_ENC_LEVEL_AUTOSELECT);
	printf("                  (H264: %0d = bluray/1080p30, %0d = 1080p60, %0d = 4k/2160p30)\n",
		NV_ENC_LEVEL_H264_41, NV_ENC_LEVEL_H264_42, NV_ENC_LEVEL_H264_51
	); 
	printf("                  (HEVC: %0d = 1080p60, %0d = 4k/2160p30, %0d = 4k/2160p60)\n",
		NV_ENC_LEVEL_HEVC_41, NV_ENC_LEVEL_HEVC_5, NV_ENC_LEVEL_HEVC_51
	);
	printf("   [-tier=n] <HEVC only>: %0d=main (default), %0d=high\n", NV_ENC_TIER_HEVC_MAIN, NV_ENC_TIER_HEVC_HIGH);
	printf("   [-minCUsize=n], [-maxCUsize=n] <HEVC only>: %0d=auto (default)\n", NV_ENC_HEVC_CUSIZE_AUTOSELECT);
	printf("                   %0d=8x8, %0d=16x16, %0d=32x32\n", NV_ENC_HEVC_CUSIZE_8x8, 
		NV_ENC_HEVC_CUSIZE_16x16, NV_ENC_HEVC_CUSIZE_32x32
	);
	printf("   [-idrperiod=n]\n");
    printf("   [-cabacenable=n] (0=auto, 1=cabac, 2=cavlc)\n"); 
	printf("   [-chromaformatIDC=n] (default=4:2:0, to encode in 4:4:4 instead, use n=%0d)\n", cudaVideoChromaFormat_444);
    printf("   [-separateColourPlaneFlag] (Requires PROFILE_HIGH_444)\n");
    printf("   [-reportsliceoffsets=n]\n"); 
    printf("   [-enableSubFrameWrite]\n"); 
    printf("   [-adaptiveTransformMode=n]  Adaptive Transform 8x8 mode (0=Autoselect, 1=Disabled, 2=Disabled)\n\n"); 
    printf("   [-disableDeblock=n]  disable deblocking (default=0) (for H264: 0..2, for HEVC: 0..1)\n");
    printf("   [-disablePTD]\n"); 
//    printf("   [-dynamicResChange]     (Enable Dynamic Resolution Changes)\n"); 
//    printf("   [-dynamicBitrateChange] (Enable Dynamic Bitrate Changes)\n"); 
    printf("   [-enableVFR]            (Enable Variable Frame Rate Mode)\n"); 
    printf("   [-enableAQ]             Enable adaptive quantization (QP)\n"); 
	printf("                           (ignored if rcmode=ConstQP)\n");
    printf("   [-qpPrimeYZeroTransformBypassFlag] (required for lossless presets)\n");
	printf("   [-vbvBufferSize=n]      (default==0)\n");
	printf("   [-vbvInitialDelay=n]    (default==0)\n");

}

// These are the initial parameters for the NVENC encoder
extern "C" 
void initEncoderParams(EncoderAppParams *pEncodeAppParams, EncodeConfig *p_nvEncoderConfig)
{
    // These are Application Encoding Parameters
    if (pEncodeAppParams) 
    {
        pEncodeAppParams->nDeviceID              = 0;
        pEncodeAppParams->maxNumberEncoders      = 1;
        pEncodeAppParams->startFrame             = 0;
        pEncodeAppParams->endFrame               = 0;
        pEncodeAppParams->numFramesToEncode      = 0;
        pEncodeAppParams->mvc                    = 0;
        pEncodeAppParams->dynamicResChange       = 0;
        pEncodeAppParams->dynamicBitrateChange   = 0;
        pEncodeAppParams->showCaps               = 0;
    }

    if (p_nvEncoderConfig) 
    {
		// wipe clean the struct
		memset((void *)p_nvEncoderConfig, 0, sizeof(*p_nvEncoderConfig));

        // Parameters that are send to NVENC hardware
        p_nvEncoderConfig->codec            = NV_ENC_H264;
        p_nvEncoderConfig->rateControl      = NV_ENC_PARAMS_RC_CBR; // Constant Bitrate
        p_nvEncoderConfig->avgBitRate       = 0;
        p_nvEncoderConfig->gopLength        = 30;
        p_nvEncoderConfig->frameRateNum     = 30000;
        p_nvEncoderConfig->frameRateDen     = 1001;
        p_nvEncoderConfig->width            = 704;
        p_nvEncoderConfig->height           = 480;
        p_nvEncoderConfig->svcTemporal      = 0;
        p_nvEncoderConfig->numlayers        = 0;
        p_nvEncoderConfig->outBandSPSPPS    = 0;

        p_nvEncoderConfig->hierarchicalP    = 0;
        p_nvEncoderConfig->hierarchicalB    = 0;
        //p_nvEncoderConfig->qp               = 26;
        p_nvEncoderConfig->qpI              = 25;
        p_nvEncoderConfig->qpP              = 28;
        p_nvEncoderConfig->qpB              = 31;
        p_nvEncoderConfig->preset           = NV_ENC_PRESET_BD; // set to Bluray disc
        p_nvEncoderConfig->disableCodecCfg  = 0;
        p_nvEncoderConfig->profile          = NV_ENC_CODEC_PROFILE_AUTOSELECT;
        p_nvEncoderConfig->stereo3dEnable   = 0;
        p_nvEncoderConfig->stereo3dMode     = 0;
        p_nvEncoderConfig->mvPrecision      = NV_ENC_MV_PRECISION_QUARTER_PEL;
        p_nvEncoderConfig->enableFMO        = NV_ENC_H264_FMO_AUTOSELECT;
        p_nvEncoderConfig->sliceMode        = 3;
        p_nvEncoderConfig->sliceModeData    = 1;   // (sliceMode=3): #slices per frame
        p_nvEncoderConfig->FieldEncoding    = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME; // either set to 0 or 2
		p_nvEncoderConfig->chromaFormatIDC  = cudaVideoChromaFormat_420; // 1=4:2:0, 3=4:4:4
        p_nvEncoderConfig->output_sei_BufferPeriod = 0;
        p_nvEncoderConfig->output_sei_PictureTime  = 0;
        p_nvEncoderConfig->level                   = NV_ENC_LEVEL_AUTOSELECT;
		p_nvEncoderConfig->idr_period = 30;
        p_nvEncoderConfig->vle_entropy_mode        = NV_ENC_H264_ENTROPY_CODING_MODE_AUTOSELECT;
        p_nvEncoderConfig->aud_enable              = 0;
        p_nvEncoderConfig->report_slice_offsets    = 0; // Default dont report slice offsets for nvEncodeAPP.
        p_nvEncoderConfig->enableSubFrameWrite     = 0; // Default do not flust to memory at slice end
        p_nvEncoderConfig->adaptive_transform_mode = NV_ENC_H264_ADAPTIVE_TRANSFORM_AUTOSELECT;
        p_nvEncoderConfig->bdirectMode             = NV_ENC_H264_BDIRECT_MODE_SPATIAL;
        p_nvEncoderConfig->disableDeblock          = 0;
        p_nvEncoderConfig->disable_ptd             = 0;
        p_nvEncoderConfig->useMappedResources      = 0;
        p_nvEncoderConfig->interfaceType           = NV_ENC_CUDA;  // Windows R304 (DX9 only), Windows R310 (DX10/DX11/CUDA), Linux R310 (CUDA only)
        p_nvEncoderConfig->monoChromeEncoding      = 0;
        p_nvEncoderConfig->syncMode                = 1;
        p_nvEncoderConfig->enableVFR               = 0;
		p_nvEncoderConfig->qpPrimeYZeroTransformBypassFlag = 0;
		p_nvEncoderConfig->enableAQ                = 0;

		p_nvEncoderConfig->enableLTR               = 0; // enable long term reference frames
		p_nvEncoderConfig->ltrNumFrames            = 0;
		p_nvEncoderConfig->ltrTrustMode            = 0;

		// (HEVC only)
		p_nvEncoderConfig->tier = NV_ENC_TIER_HEVC_MAIN;
		p_nvEncoderConfig->minCUsize = NV_ENC_HEVC_CUSIZE_AUTOSELECT;
		p_nvEncoderConfig->maxCUsize = NV_ENC_HEVC_CUSIZE_AUTOSELECT;
	}
}

// These are the initial parameters for the NVENC encoder
extern "C" 
void parseCmdLineArguments(int argc, const char *argv[], EncoderAppParams *pEncodeAppParams, EncodeConfig *p_nvEncoderConfig)
{
    if (argc > 1) 
    {
		bool rc;
        // These are parameters specific to the encoding application
        getCmdLineArgumentString( argc, (const char **)argv, "infile",               &pEncodeAppParams->input_file          );
        getCmdLineArgumentString( argc, (const char **)argv, "outfile",              &pEncodeAppParams->output_file         );
        getCmdLineArgumentString( argc, (const char **)argv, "psnr",                 &pEncodeAppParams->psnr_file           );
        getCmdLineArgumentValue ( argc, (const char **)argv, "device",               &pEncodeAppParams->nDeviceID           );
        getCmdLineArgumentValue ( argc, (const char **)argv, "startframe",           &pEncodeAppParams->startFrame          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "endframe",             &pEncodeAppParams->endFrame            );
        getCmdLineArgumentValue ( argc, (const char **)argv, "startf",               &pEncodeAppParams->startFrame          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "endf",                 &pEncodeAppParams->endFrame            );
        getCmdLineArgumentValue ( argc, (const char **)argv, "mvc",                  &pEncodeAppParams->mvc                 );
        getCmdLineArgumentValue ( argc, (const char **)argv, "maxNumberEncoders",    &pEncodeAppParams->maxNumberEncoders   );

        pEncodeAppParams->dynamicBitrateChange = checkCmdLineFlag ( argc, (const char **)argv, "dynamicBitrateChange" );
        pEncodeAppParams->dynamicResChange     = checkCmdLineFlag ( argc, (const char **)argv, "dynamicResChange" );
        pEncodeAppParams->showCaps             = checkCmdLineFlag ( argc, (const char **)argv, "showCaps");

        // These are NVENC encoding parameters built in with the structure
        getCmdLineArgumentValue ( argc, (const char **)argv, "interface",            &p_nvEncoderConfig->interfaceType      );
        getCmdLineArgumentValue ( argc, (const char **)argv, "fieldmode",            &p_nvEncoderConfig->FieldEncoding      );
        getCmdLineArgumentValue ( argc, (const char **)argv, "bdirectmode",          &p_nvEncoderConfig->bdirectMode        );
        getCmdLineArgumentValue ( argc, (const char **)argv, "codec",                &p_nvEncoderConfig->codec              );

		// If user selects HEVC codec, change the default profile to HEVC_PROFILE_MAIN
		if (p_nvEncoderConfig->codec == NV_ENC_H265)
			p_nvEncoderConfig->profile = NV_ENC_HEVC_PROFILE_MAIN;
        getCmdLineArgumentValue ( argc, (const char **)argv, "width",                &p_nvEncoderConfig->width              );
        getCmdLineArgumentValue ( argc, (const char **)argv, "height",               &p_nvEncoderConfig->height             );
        getCmdLineArgumentValue ( argc, (const char **)argv, "width",                &p_nvEncoderConfig->maxWidth           );
        getCmdLineArgumentValue ( argc, (const char **)argv, "height",               &p_nvEncoderConfig->maxHeight          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "width",                &p_nvEncoderConfig->darRatioX          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "height",               &p_nvEncoderConfig->darRatioY          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "darwidth",             &p_nvEncoderConfig->darRatioX          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "darheight",            &p_nvEncoderConfig->darRatioY          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "bitrate",              &p_nvEncoderConfig->avgBitRate         );
        if ( getCmdLineArgumentValue ( argc, (const char **)argv, "kbitrate",             &p_nvEncoderConfig->avgBitRate         ) )
			p_nvEncoderConfig->avgBitRate *= 1000;
        if ( getCmdLineArgumentValue ( argc, (const char **)argv, "mbitrate",             &p_nvEncoderConfig->avgBitRate         ) )
			p_nvEncoderConfig->avgBitRate *= 1000000;
        getCmdLineArgumentValue ( argc, (const char **)argv, "maxbitrate",           &p_nvEncoderConfig->peakBitRate        );
        if ( getCmdLineArgumentValue ( argc, (const char **)argv, "maxkbitrate",          &p_nvEncoderConfig->peakBitRate        ) )
			p_nvEncoderConfig->peakBitRate *= 1000;
        if ( getCmdLineArgumentValue ( argc, (const char **)argv, "maxmbitrate",          &p_nvEncoderConfig->peakBitRate        ) )
			p_nvEncoderConfig->peakBitRate *= 1000000;
        getCmdLineArgumentValue ( argc, (const char **)argv, "rcmode",               &p_nvEncoderConfig->rateControl        );
        getCmdLineArgumentValue ( argc, (const char **)argv, "num_b_frames",         &p_nvEncoderConfig->numBFrames         );
        getCmdLineArgumentValue ( argc, (const char **)argv, "numerator",            &p_nvEncoderConfig->frameRateNum       );
        getCmdLineArgumentValue ( argc, (const char **)argv, "denominator",          &p_nvEncoderConfig->frameRateDen       );
        getCmdLineArgumentValue ( argc, (const char **)argv, "goplength",            &p_nvEncoderConfig->gopLength          );
        p_nvEncoderConfig->monoChromeEncoding = checkCmdLineFlag ( argc, (const char **)argv, "monoChromeEncoding" );

		// NVENC API 4
		p_nvEncoderConfig->qpPrimeYZeroTransformBypassFlag = checkCmdLineFlag ( argc, (const char **)argv, "qpPrimeYZeroTransformBypassFlag" );
		p_nvEncoderConfig->enableAQ = checkCmdLineFlag ( argc, (const char **)argv, "enableAQ" );
		// need to check qpPrimeYZero before parsing 'qpP', 

		///////////
		// ConstQP rate-control parameters
		//
		// qpALL replaces all three qpI/qpB/qpP
        getCmdLineArgumentValue ( argc, (const char **)argv, "qpALL",                &p_nvEncoderConfig->qpI                );
        getCmdLineArgumentValue ( argc, (const char **)argv, "qpALL",                &p_nvEncoderConfig->qpB                );
        getCmdLineArgumentValue ( argc, (const char **)argv, "qpALL",                &p_nvEncoderConfig->qpP                );

		// and individual command-line arguments qpI/qpB/qpP override qpALL
        getCmdLineArgumentValue ( argc, (const char **)argv, "qpI",                  &p_nvEncoderConfig->qpI                );
        getCmdLineArgumentValue ( argc, (const char **)argv, "qpB",                  &p_nvEncoderConfig->qpB                );
		
		// If -qpPrimeYZeroTransformBypassFlag is present, then the qpP gets inadvertently set to 0.
		//   To avoid this, don't check qpP when PrimeyZero is present.
		if ( !p_nvEncoderConfig->qpPrimeYZeroTransformBypassFlag )
			getCmdLineArgumentValue ( argc, (const char **)argv, "qpP",                  &p_nvEncoderConfig->qpP                );

		///////////
		// VariableQP rate-control parameters
		//
        rc =  getCmdLineArgumentValue ( argc, (const char **)argv, "min_qpALL",      &p_nvEncoderConfig->min_qpI            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "min_qpALL",      &p_nvEncoderConfig->min_qpB            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "min_qpALL",      &p_nvEncoderConfig->min_qpP            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "min_qpI",        &p_nvEncoderConfig->min_qpI            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "min_qpB",        &p_nvEncoderConfig->min_qpB            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "min_qpP",        &p_nvEncoderConfig->min_qpP            );
		if ( rc) p_nvEncoderConfig->min_qp_ena = true;

        rc =  getCmdLineArgumentValue ( argc, (const char **)argv, "max_qpALL",      &p_nvEncoderConfig->max_qpI            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "max_qpALL",      &p_nvEncoderConfig->max_qpB            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "max_qpALL",      &p_nvEncoderConfig->max_qpP            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "max_qpI",        &p_nvEncoderConfig->max_qpI            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "max_qpB",        &p_nvEncoderConfig->max_qpB            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "max_qpP",        &p_nvEncoderConfig->max_qpP            );
		if ( rc) p_nvEncoderConfig->max_qp_ena = true;

        rc =  getCmdLineArgumentValue ( argc, (const char **)argv, "initial_qpALL",      &p_nvEncoderConfig->initial_qpI            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "initial_qpALL",      &p_nvEncoderConfig->initial_qpB            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "initial_qpALL",      &p_nvEncoderConfig->initial_qpP            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "initial_qpI",        &p_nvEncoderConfig->initial_qpI            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "initial_qpB",        &p_nvEncoderConfig->initial_qpB            );
        rc |= getCmdLineArgumentValue ( argc, (const char **)argv, "initial_qpP",        &p_nvEncoderConfig->initial_qpP            );
		if ( rc) p_nvEncoderConfig->initial_qp_ena = true;

        getCmdLineArgumentValue ( argc, (const char **)argv, "hierarchicalP",        &p_nvEncoderConfig->hierarchicalP      );
        getCmdLineArgumentValue ( argc, (const char **)argv, "hierarchicalB",        &p_nvEncoderConfig->hierarchicalB      );
        getCmdLineArgumentValue ( argc, (const char **)argv, "svcTemporal",          &p_nvEncoderConfig->svcTemporal        );
        getCmdLineArgumentValue ( argc, (const char **)argv, "numlayers",            &p_nvEncoderConfig->numlayers          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "outbandspspps",        &p_nvEncoderConfig->outBandSPSPPS      );
        getCmdLineArgumentValue ( argc, (const char **)argv, "profile",              &p_nvEncoderConfig->profile            ); //  H264: 66=baseline, 77=main, 100=high, 128=stereo (need to also set stereo3d bits below)
        p_nvEncoderConfig->stereo3dEnable = checkCmdLineFlag ( argc, (const char **)argv, "stereo3dEnable" );
        getCmdLineArgumentValue ( argc, (const char **)argv, "stereo3dMode",         &p_nvEncoderConfig->stereo3dMode       );
		if ( p_nvEncoderConfig->stereo3dMode ) 
			p_nvEncoderConfig->stereo3dEnable = 1;
        getCmdLineArgumentValue ( argc, (const char **)argv, "sliceMode",            &p_nvEncoderConfig->sliceMode          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "sliceModeData",        &p_nvEncoderConfig->sliceModeData      );
        getCmdLineArgumentValue ( argc, (const char **)argv, "preset",               &p_nvEncoderConfig->preset             );
        getCmdLineArgumentValue ( argc, (const char **)argv, "disableCodecCfg"      ,&p_nvEncoderConfig->disableCodecCfg    );
        getCmdLineArgumentValue ( argc, (const char **)argv, "mvprecision"          ,&p_nvEncoderConfig->mvPrecision        );
        getCmdLineArgumentValue ( argc, (const char **)argv, "enableFMO"            ,&p_nvEncoderConfig->enableFMO          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "outputseiBufferPeriod", &p_nvEncoderConfig->output_sei_BufferPeriod );
        getCmdLineArgumentValue ( argc, (const char **)argv, "outputseiPictureTime" , &p_nvEncoderConfig->output_sei_PictureTime  );
        getCmdLineArgumentValue ( argc, (const char **)argv, "outputAud"            , &p_nvEncoderConfig->aud_enable          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "level_idc"            , &p_nvEncoderConfig->level               );
        getCmdLineArgumentValue ( argc, (const char **)argv, "idrperiod"            , &p_nvEncoderConfig->idr_period          );
        getCmdLineArgumentValue ( argc, (const char **)argv, "cabacenable"          , &p_nvEncoderConfig->vle_entropy_mode    );
        getCmdLineArgumentValue ( argc, (const char **)argv, "chromaformatIDC"      , &p_nvEncoderConfig->chromaFormatIDC     );
		p_nvEncoderConfig->separateColourPlaneFlag= checkCmdLineFlag ( argc, (const char **)argv, "separateColourPlaneFlag" );
        getCmdLineArgumentValue ( argc, (const char **)argv, "reportsliceoffsets"   , &p_nvEncoderConfig->report_slice_offsets);
        getCmdLineArgumentValue ( argc, (const char **)argv, "enableSubFrameWrite"  , &p_nvEncoderConfig->enableSubFrameWrite );
        getCmdLineArgumentValue ( argc, (const char **)argv, "adaptiveTransformMode", &p_nvEncoderConfig->adaptive_transform_mode                 );
        getCmdLineArgumentValue ( argc, (const char **)argv, "syncMode"             , &p_nvEncoderConfig->syncMode            );
        getCmdLineArgumentValue ( argc, (const char **)argv, "maxNumRefFrames"      , &p_nvEncoderConfig->max_ref_frames      );

        rc = getCmdLineArgumentValue ( argc, (const char **)argv, "ltrNumFrames"         , &p_nvEncoderConfig->ltrNumFrames        );
        rc |=getCmdLineArgumentValue ( argc, (const char **)argv, "ltrTrustMode"         , &p_nvEncoderConfig->ltrTrustMode        );
		if (rc)
			p_nvEncoderConfig->enableLTR = 1;

		p_nvEncoderConfig->disable_ptd        = checkCmdLineFlag ( argc, (const char **)argv, "disablePTD" );
        p_nvEncoderConfig->disableDeblock     = checkCmdLineFlag ( argc, (const char **)argv, "disableDeblock"  );
        p_nvEncoderConfig->useMappedResources = checkCmdLineFlag ( argc, (const char **)argv, "useMappedResources" );

		getCmdLineArgumentValue(argc, (const char **)argv, "vbvBufferSize",         &p_nvEncoderConfig->vbvBufferSize);
		getCmdLineArgumentValue(argc, (const char **)argv, "vbvInitialDelay",       &p_nvEncoderConfig->vbvInitialDelay);

		// NVENC API 3
		p_nvEncoderConfig->enableVFR = checkCmdLineFlag ( argc, (const char **)argv, "enableVFR" );

		// NVENC API 5 (HEVC codec only)
		getCmdLineArgumentValue(argc, (const char **)argv, "tier",                  &p_nvEncoderConfig->tier     );
		getCmdLineArgumentValue(argc, (const char **)argv, "minCUsize",             &p_nvEncoderConfig->minCUsize);
		getCmdLineArgumentValue(argc, (const char **)argv, "maxCUsize",             &p_nvEncoderConfig->maxCUsize);
	}
    // if peakBitrate or bufferSize are not specified, then we compute the recommended ones them
    if (p_nvEncoderConfig->peakBitRate == 0) {
        p_nvEncoderConfig->peakBitRate  = (p_nvEncoderConfig->avgBitRate * 4);
    }
    //if (p_nvEncoderConfig->bufferSize == 0) {
    //    p_nvEncoderConfig->bufferSize   = (p_nvEncoderConfig->avgBitRate / 2);
    //}
    // Validate the Rate Control Parameters
    if((p_nvEncoderConfig->rateControl < NV_ENC_PARAMS_RC_CONSTQP) || 
       (p_nvEncoderConfig->rateControl > NV_ENC_PARAMS_RC_2_PASS_VBR))
    {
        p_nvEncoderConfig->rateControl = NV_ENC_PARAMS_RC_2_PASS_VBR;
    }

    if (argc == 1 || !pEncodeAppParams->input_file || !pEncodeAppParams->output_file) 
    {
        printHelp();
        return;
    }

    // Validate the (width,height) Parameters
    if (p_nvEncoderConfig->width == 0 || p_nvEncoderConfig->height == 0 )
    {
        fprintf(stderr, "Error width=%d, height=%d settings are invalid\n\n", p_nvEncoderConfig->width, p_nvEncoderConfig->height);
        printHelp();
        exit(EXIT_FAILURE);
    }

    p_nvEncoderConfig->FieldEncoding   = p_nvEncoderConfig->FieldEncoding;
    if (pEncodeAppParams->mvc || (p_nvEncoderConfig->profile == NV_ENC_H264_PROFILE_STEREO))
    {
        pEncodeAppParams->mvc               = 1;
        p_nvEncoderConfig->profile          = NV_ENC_H264_PROFILE_STEREO;
        p_nvEncoderConfig->numEncFrames     = (pEncodeAppParams->numFramesToEncode) << 1;
        pEncodeAppParams->numFramesToEncode = (pEncodeAppParams->numFramesToEncode) << 1;
    }
}

extern "C"
void displayEncodingParams(EncoderAppParams *pEncodeAppParams, EncodeConfig *p_nvEncoderConfig, int GPUID)
{
	const bool is_h264 = p_nvEncoderConfig->codec == NV_ENC_H264;
	const bool is_h265 = p_nvEncoderConfig->codec == NV_ENC_H265;

	std::string str, str2;

	if (pEncodeAppParams == NULL)
		printf("displayEncodingParams(): Error, NULL\n");
#define NOT_NULL(x) ((x) == NULL) ? "NULL" : (x)

	printf("> NVEncode configuration parameters for Encoder[%d]\n", 0);
	printf("> GPU Device ID             = (#%d) %d", GPUID, pEncodeAppParams->nDeviceID);
	printf(" %s\n", p_nvEncoderConfig[GPUID].enabled ? "(enabled)" : "(disabled)");
	printf("> Input File                = %s\n", NOT_NULL(pEncodeAppParams->input_file));
	printf("> Output File               = %s\n", NOT_NULL(pEncodeAppParams->output_file));
	printf("> Frame Rate (Num/Denom)    = (%d/%d) %4.4f fps", p_nvEncoderConfig[GPUID].frameRateNum, p_nvEncoderConfig[GPUID].frameRateDen, (float)p_nvEncoderConfig[GPUID].frameRateNum / (float)p_nvEncoderConfig[GPUID].frameRateDen);
	if (is_h264 && p_nvEncoderConfig[GPUID].enableVFR) printf(" (enableVFR variable)");
	printf("\n");
	printf("> Frames [%03d-%03d]          = %d frames \n", pEncodeAppParams->startFrame, pEncodeAppParams->endFrame - 1, pEncodeAppParams->numFramesToEncode);

	desc_nv_enc_codec_names.value2string(p_nvEncoderConfig[GPUID].codec, str);
	printf("> Video Output Codec        = %d - %s_GUID\n", p_nvEncoderConfig[GPUID].codec, str.c_str() );
	//  printf("> Multi-View Codec          = %s\n",           pEncodeAppParams->mvc ? "Yes" : "No");

	desc_nv_enc_preset_names.value2string(p_nvEncoderConfig[GPUID].preset, str);
	printf("> Encoder Preset            = %d - %s_GUID \"%s\"\n", p_nvEncoderConfig[GPUID].preset,
		str.c_str(), preset_names[p_nvEncoderConfig[GPUID].preset].name );

	desc_nv_enc_profile_names.value2string(p_nvEncoderConfig[GPUID].profile, str);
	printf("> Video codec profile       = %d - %s_GUID\n", p_nvEncoderConfig[GPUID].profile, str.c_str());

	if ( is_h265 ) {
		desc_nv_enc_level_hevc_names.value2string(p_nvEncoderConfig[GPUID].level, str);
		desc_nv_enc_tier_hevc_names.value2string(p_nvEncoderConfig[GPUID].tier, str2);
		printf(">     level                 = %0d - %s\n", p_nvEncoderConfig[GPUID].level, str.c_str() );
		printf(">     tier                  = %0d - %s\n", p_nvEncoderConfig[GPUID].tier, str2.c_str());
	}
	else {
		desc_nv_enc_level_h264_names.value2string(p_nvEncoderConfig[GPUID].level, str);
		printf(">     level                 = %0d - %0s\n", p_nvEncoderConfig[GPUID].level, str.c_str());
	}

	desc_nv_enc_ratecontrol_names.value2string(p_nvEncoderConfig[GPUID].rateControl, str);
	printf("> Rate Control Mode         = %d - %s \"%s\"\n", p_nvEncoderConfig[GPUID].rateControl,
		str.c_str(), ratecontrol_names[p_nvEncoderConfig[GPUID].rateControl].name);

    printf("> Width x Height            = [%04d x %04d]\n",  p_nvEncoderConfig[GPUID].maxWidth, p_nvEncoderConfig[GPUID].maxHeight);
	printf("> Average:Peak Bitrate      = %d : %d (bps/sec)\n", p_nvEncoderConfig[GPUID].avgBitRate, p_nvEncoderConfig[GPUID].peakBitRate);
    //printf("> BufferSize                = %d\n",           p_nvEncoderConfig[GPUID].bufferSize);
	printf("> vbvBufferSize             = %d\n",           p_nvEncoderConfig[GPUID].vbvBufferSize);
	printf("> vbvInitialDelay           = %d\n",           p_nvEncoderConfig[GPUID].vbvInitialDelay);

	printf("> GOP Length (#frames)      = %d\n",           p_nvEncoderConfig[GPUID].gopLength);
    printf("> Number of BFrames(per GOP)= %d\n",           p_nvEncoderConfig[GPUID].numBFrames);
    printf("> Display Aspect Ratio X/Y  = %d/%d\n",        p_nvEncoderConfig[GPUID].darRatioX, p_nvEncoderConfig[GPUID].darRatioY);

	desc_nv_enc_h264_bdirect_mode_names.value2string(p_nvEncoderConfig[GPUID].bdirectMode, str);
	printf("> bdirectMode               = %d (%s)\n", p_nvEncoderConfig[GPUID].bdirectMode, str.c_str() );

	desc_nv_enc_h264_fmo_names.value2string(p_nvEncoderConfig[GPUID].enableFMO, str);
	printf("> enableFMO                 = %d (%s)\n", p_nvEncoderConfig[GPUID].enableFMO, str.c_str());
	//printf("> QP (All Frames)           = %d\n",           p_nvEncoderConfig[GPUID].qp);

	if ( is_h264 ) {
		desc_nv_enc_h264_entropy_coding_mode_names.value2string(p_nvEncoderConfig[GPUID].vle_entropy_mode, str);
		printf("> vle_entropy_mode          = %d (%s)\n", p_nvEncoderConfig[GPUID].vle_entropy_mode, str.c_str());
	}

	desc_nv_enc_mv_precision_names.value2string(p_nvEncoderConfig[GPUID].mvPrecision, str);
	printf("> mvPrecision               = %d (%s)\n", p_nvEncoderConfig[GPUID].mvPrecision, str.c_str());

	desc_nv_enc_adaptive_transform_names.value2string(p_nvEncoderConfig[GPUID].adaptive_transform_mode, str);
	printf("> adaptive_transform_mode   = %d (%s)\n", p_nvEncoderConfig[GPUID].adaptive_transform_mode, str.c_str());

#define PRINT_QP_MIN_MAX(x) \
	if (p_nvEncoderConfig[GPUID].min_qp_ena) \
		printf(", min=%2d", p_nvEncoderConfig[GPUID].min_ ## x); \
	else \
		printf("          " ); \
	if (p_nvEncoderConfig[GPUID].max_qp_ena) \
		printf(", max=%2d", p_nvEncoderConfig[GPUID].max_ ## x); \
	else \
		printf("          "); \
	printf("\n");

    printf("> QP (I-Frames)             = %2d",           p_nvEncoderConfig[GPUID].qpI);
		PRINT_QP_MIN_MAX(qpI)
	printf("> QP (P-Frames)             = %2d", p_nvEncoderConfig[GPUID].qpP);
		PRINT_QP_MIN_MAX(qpP)
    printf("> QP (B-Frames)             = %2d",           p_nvEncoderConfig[GPUID].qpB);
		PRINT_QP_MIN_MAX(qpB)

    printf("> Enable Hiearchical P/B Fra= %d/%d\n",      p_nvEncoderConfig[GPUID].hierarchicalP,
        p_nvEncoderConfig[GPUID].hierarchicalB);
//    printf("> SVC Temporal Scalability  = %d\n",           p_nvEncoderConfig[GPUID].svcTemporal);
//    printf("> Number of Temporal Layers = %d\n",           p_nvEncoderConfig[GPUID].numlayers);
    printf("> Outband SPSPPS            = %d\n",           p_nvEncoderConfig[GPUID].outBandSPSPPS);
//    printf("> Stereo 3D Mode            = %d\n",           p_nvEncoderConfig[GPUID].stereo3dMode);
//    printf("> Stereo 3D Enable          = %s\n",           p_nvEncoderConfig[GPUID].stereo3dEnable  ? "Yes" : "No");
	printf("> sliceMode/Data            = %d/%d\n",        p_nvEncoderConfig[GPUID].sliceMode, p_nvEncoderConfig[GPUID].sliceModeData);
#if defined (NV_WINDOWS)
    printf("> Asynchronous Mode         = %s\n",           p_nvEncoderConfig[GPUID].syncMode ? "Yes" : "No");
#endif
    printf("> NVENC API Interface       = %d - %s\n",      p_nvEncoderConfig[GPUID].interfaceType, nvenc_interface_names[p_nvEncoderConfig[GPUID].interfaceType].name);
    printf("> Map Resource API Demo     = %s\n",           p_nvEncoderConfig[GPUID].useMappedResources ? "Yes" : "No");
    printf("> enableAQ                  = %s\n",           p_nvEncoderConfig[GPUID].enableAQ ? "Yes" : "No");

	if ( is_h265 ) {
		desc_nv_enc_hevc_cusize_names.value2string(p_nvEncoderConfig->minCUsize, str);
		desc_nv_enc_hevc_cusize_names.value2string(p_nvEncoderConfig->maxCUsize, str2);
		printf("> minCUsize, maxCUsize      = %0d, %0d (%0s, %0s)",
			p_nvEncoderConfig->minCUsize, p_nvEncoderConfig->maxCUsize, str.c_str(), str2.c_str()
		);
	}
	getchar();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Here, the use of chromaFormatIdc refers only to the source video-frames, not the encode bufferformat.
// Format 1 = YV12? format 3 = YUV444?
extern "C"
HRESULT LoadCurrentFrame (unsigned char *yuvInput[3] , HANDLE hInputYUVFile, unsigned int dwFrmIndex,
                          unsigned int dwFileWidth, unsigned int dwFileHeight, unsigned int dwSurfWidth, unsigned int dwSurfHeight,
                          bool bFieldPic, bool bTopField, int FrameQueueSize, int chromaFormatIdc = 1)
{
    U64 fileOffset;
    U32 numBytesRead = 0;
    U32 result;
    unsigned int dwFrameStrideY    = (dwFrmIndex % FrameQueueSize) *  dwFileWidth * dwFileHeight;
    unsigned int dwFrameStrideCbCr = (dwFrmIndex % FrameQueueSize) * (dwFileWidth * dwFileHeight)/4;
    unsigned int dwInFrameSize = dwFileWidth*dwFileHeight + (dwFileWidth*dwFileHeight)/2;

    if (chromaFormatIdc == 3) // YUV 4:4:4
    {
        dwInFrameSize = dwFileWidth * dwFileHeight * 3;
    }
    else
    { // YUV 4:2:0
        dwInFrameSize = dwFileWidth * dwFileHeight + (dwFileWidth * dwFileHeight) / 2;
    }
    fileOffset = ((U64)dwInFrameSize * (U64)dwFrmIndex);
    result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_BEGIN);
    if (result == INVALID_SET_FILE_POINTER)
    {
        return E_FAIL;
    }
    if (chromaFormatIdc == 3)
    {
        for (unsigned int i = 0 ; i < dwFileHeight; i++ )
        {
            nvReadFile(hInputYUVFile, yuvInput[0] + dwFrameStrideY    + i * dwSurfWidth, dwFileWidth, &numBytesRead, NULL);
        }
        // read U
        for ( unsigned int i = 0 ; i < dwFileHeight; i++ )
        {
            nvReadFile(hInputYUVFile, yuvInput[1] + dwFrameStrideCbCr + i * dwSurfWidth, dwFileWidth, &numBytesRead, NULL);
        }
        // read V
        for ( unsigned int i = 0 ; i < dwFileHeight; i++ )
        {
            nvReadFile(hInputYUVFile, yuvInput[2] + dwFrameStrideCbCr + i * dwSurfWidth, dwFileWidth, &numBytesRead, NULL);
        }
    }
    else if (bFieldPic)
    {
        if (!bTopField)
        {
            // skip the first line
            fileOffset  = (U64)dwFileWidth;
            result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_CURRENT);
            if (result == INVALID_SET_FILE_POINTER)
            {
                return E_FAIL;
            }
        }
        // read Y
        for ( unsigned int i = 0 ; i < dwFileHeight/2; i++ )
        {
            nvReadFile(hInputYUVFile, yuvInput[0] + dwFrameStrideY + i*dwSurfWidth, dwFileWidth, &numBytesRead, NULL);
            // skip the next line
            fileOffset  = (U64)dwFileWidth;
            result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_CURRENT);
            if (result == INVALID_SET_FILE_POINTER)
            {
                return E_FAIL;
            }
        }
        // read U,V
        for (int cbcr = 0; cbcr < 2; cbcr++)
        {
            //put file pointer at the beginning of chroma
            fileOffset  = ((U64)dwInFrameSize*dwFrmIndex + (U64)dwFileWidth*dwFileHeight + (U64)cbcr*(( dwFileWidth*dwFileHeight)/4));
            result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_BEGIN);
            if (result == INVALID_SET_FILE_POINTER)
            {
                return E_FAIL;
            }
            if (!bTopField)
            {
                fileOffset  = (U64)dwFileWidth/2;
                result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_CURRENT);
                if (result == INVALID_SET_FILE_POINTER)
                {
                    return E_FAIL;
                }
            }
            for ( unsigned int i = 0 ; i < dwFileHeight/4; i++ )
            {
                nvReadFile(hInputYUVFile, yuvInput[cbcr + 1] + dwFrameStrideCbCr + i*(dwSurfWidth/2), dwFileWidth/2, &numBytesRead, NULL);
                fileOffset = (U64)dwFileWidth/2;
                result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_CURRENT);
                if (result == INVALID_SET_FILE_POINTER)
                {
                    return E_FAIL;
                }
            }
        }
    }
    else if (dwFileWidth != dwSurfWidth)
    {
        // load the whole frame
        // read Y
        for ( unsigned int i = 0 ; i < dwFileHeight; i++ )
        {
            nvReadFile(hInputYUVFile, yuvInput[0] + dwFrameStrideY + i*dwSurfWidth, dwFileWidth, &numBytesRead, NULL);
        }
        // read U,V
        for (int cbcr = 0; cbcr < 2; cbcr++)
        {
            // move in front of chroma
            fileOffset = (U32)(dwInFrameSize*dwFrmIndex + dwFileWidth*dwFileHeight + cbcr*((dwFileWidth* dwFileHeight)/4));
            result = nvSetFilePointer64(hInputYUVFile, fileOffset, NULL, FILE_BEGIN);
            if (result == INVALID_SET_FILE_POINTER)
            {
                return E_FAIL;
            }
            for ( unsigned int i = 0 ; i < dwFileHeight/2; i++ )
            {
                 nvReadFile(hInputYUVFile, yuvInput[cbcr + 1] + dwFrameStrideCbCr + i*dwSurfWidth/2, dwFileWidth/2, &numBytesRead, NULL);
            }
        }
    }
    else
    {
        // direct file read
        nvReadFile(hInputYUVFile, &yuvInput[0][dwFrameStrideY   ], dwFileWidth * dwFileHeight, &numBytesRead, NULL);
        nvReadFile(hInputYUVFile, &yuvInput[1][dwFrameStrideCbCr], dwFileWidth * dwFileHeight/4, &numBytesRead, NULL);
        nvReadFile(hInputYUVFile, &yuvInput[2][dwFrameStrideCbCr], dwFileWidth * dwFileHeight/4, &numBytesRead, NULL);
    }
    return S_OK;
}

