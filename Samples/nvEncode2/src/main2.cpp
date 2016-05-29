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

////////////////////////////////////////////////////////////
// This is the main application for using the NV Encode API
//

#if defined(LINUX) || defined (NV_LINUX)
  // This is required so that fopen will use the 64-bit equivalents for large file access
  #define _FILE_OFFSET_BITS 64  
  #define _LARGEFILE_SOURCE
  #define _LARGEFILE64_SOURCE
#endif
#define pprintf(...) printf( "%0s(%0u):", __FILE__, __LINE__ ),printf(__VA_ARGS__)

#include <nvEncodeAPI.h>                // the NVENC common API header
#include "CNVEncoderH264.h"             // class definition for the H.264 encoding class
#include "CNVEncoderH265.h"             // class definition for the HEVC encoding class
#include "xcodeutil.h"                  // class helper functions for video encoding
#include <platform/NvTypes.h>           // type definitions
#include "defines.h"                    // common headers and definitions

#include "VideoDecode.h"

#include <string>
#include <sstream>

#include <cuda.h>                       // include CUDA header for CUDA/NVENC interop
#include <include/helper_cuda_drvapi.h> // helper functions for CUDA driver API
#include <include/helper_string.h>      // helper functions for string parsing
#include <include/helper_timer.h>       // helper functions for timing
#include <include/nvFileIO.h>           // helper functions for large file access
#include <cstdio> // getchar
#include <include/videoFormats.h>  // IsYV12, IsNV12


#pragma warning (disable:4189)

#define FRAME_QUEUE 32 // Use a maximum of 16 frames to buffer the decoder

const char *sAppName = "nvEncoder";

StopWatchInterface *timer[MAX_ENCODERS];

// Utilities.cpp 
//
// printHelp()       - prints all the command options for the NVENC sample application
extern "C" void    printHelp();
// initEncoderParams - function to set the initial parameters for NVENC encoder
extern "C" void    initEncoderParams(EncoderAppParams *pEncodeAppParams, EncodeConfig *p_nvEncoderConfig);
// parseCmdLineArguments - parsing command line arguments for EncodeConfig Struct
extern "C" void    parseCmdLineArguments(int argc, const char *argv[], EncoderAppParams *pEncodeAppParams, EncodeConfig *p_nvEncoderConfig);
// displayEncodingParams
extern "C" void    displayEncodingParams(EncoderAppParams *pEncodeAppParams, EncodeConfig *p_nvEncoderConfig, int GPUID);

// LoadCurrentFrame  - function to load the current frame into system memory
extern "C" HRESULT LoadCurrentFrame( unsigned char *yuvInput[3] , HANDLE hInputYUVFile, 
                                     unsigned int dwFrmIndex,
                                     unsigned int dwFileWidth, unsigned int dwFileHeight, 
                                     unsigned int dwSurfWidth, unsigned int dwSurfHeight,
                                     bool bFieldPic, bool bTopField, 
                                     int FrameQueueSize, int chromaFormatIdc );

//
// fwrite_callback() - CNvEncoder calls this function whenever it wants to write bits to the output file.
//
size_t
fwrite_callback(_In_count_x_(_Size*_Count) void * _Str, size_t _Size, size_t _Count, FILE * _File, void *privateData)
{
	// privateData isn't used by this win32 app
	return fwrite(_Str, _Size, _Count, _File );
}


// MsgProc() - The window's message handler
// This function is passed to the create
static LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_KEYDOWN:
            switch (wParam)
            {
                    // use ESC to quit application
                case VK_ESCAPE:
                    {
                        //g_bDone = true;
                        PostQuitMessage(0);
                        return 0;
                    }
                    break;

                    // use space to pause playback
                case VK_SPACE:
                    {
                        //g_bRunning = !g_bRunning;
                    }
                    break;
            }

            break;
			/*
            // resize the window
            // even though we disable resizing (see next event handler below)
            // we need to be able to handle this event properly because the
            // windowing system calls it.
        case WM_SIZE:
            {
                // store new window size in our globals.
                g_nClientAreaWidth  = GET_X_LPARAM(lParam);
                g_nClientAreaHeight = GET_Y_LPARAM(lParam);

                D3DVIEWPORT9 oViewport;
                oViewport.X = 0;
                oViewport.Y = 0;
                oViewport.Width  = g_nClientAreaWidth;
                oViewport.Height = g_nClientAreaHeight;
                oViewport.MaxZ = 1.0f;
                oViewport.MinZ = 0.0f;

                g_pD3DDevice->SetViewport(&oViewport);

                D3DMATRIX oViewMatrix = {1.0f, 0.0f, 0.0f, 0.0f,
                                         0.0f, 1.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 1.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f, 1.0f
                                        };
                // scale viewport to be of window width and height
                oViewMatrix._11 = 2.0f/g_nClientAreaWidth;
                oViewMatrix._22 = 2.0f/g_nClientAreaHeight;
                // translate viewport so that lower left corner represents
                // (0, 0) and upper right corner is (width, height)
                oViewMatrix._41 = -1.0f - 1.0f / g_nClientAreaWidth;
                oViewMatrix._42 = -1.0f + 1.0f / g_nClientAreaHeight;

                if (0 != g_pD3DDevice)
                {
                    g_pD3DDevice->SetTransform(D3DTS_VIEW, &oViewMatrix);
                }

                renderVideoFrame(hWnd, g_bUseInterop);

                return 0;   // Jump Back
            }

            // disallow resizing.
        case WM_GETMINMAXINFO:
            {
                MINMAXINFO *pMinMaxInfo = reinterpret_cast<MINMAXINFO *>(lParam);

                pMinMaxInfo->ptMinTrackSize.x = g_nWindowWidth;
                pMinMaxInfo->ptMinTrackSize.y = g_nWindowHeight;

                pMinMaxInfo->ptMaxTrackSize.x = g_nWindowWidth;
                pMinMaxInfo->ptMaxTrackSize.y = g_nWindowHeight;

                return 0;
            }
			*/
        case WM_DESTROY:
            //g_bDone = true;
            PostQuitMessage(0);
            return 0;

        case WM_PAINT:
            ValidateRect(hWnd, NULL);
            return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void queryAllEncoderCaps(CNvEncoder *pEncoder, string &s)
{
	ostringstream os;
	string        stemp;
	nv_enc_caps_s caps;
	char tmp[256];
	int result;
	os << "queryAlllEncoderCaps( deviceID=" << std::dec << pEncoder->m_deviceID << " )\n";
	pEncoder->PrintEncodeFormats(stemp); // print supported HW-codecs (MPEG-2, VC1, H264, etc.)
	os << stemp;
	pEncoder->PrintEncodeProfiles(stemp);// print supported encoding profiles (Baseline, Main, High, etc.)
	os << stemp;
	pEncoder->PrintEncodePresets(stemp);
	os << stemp;
	pEncoder->PrintBufferFormats(stemp); // print supported (framebuffer) Input Formats
	os << stemp;
	os << endl;

#define QUERY_PRINT_CAPS(CAP) sprintf(tmp, "Query %s = %d\n", #CAP, caps.value_ ## CAP); os << tmp
	
    if (pEncoder) 
    {
        pEncoder->QueryEncoderCaps( caps );
        QUERY_PRINT_CAPS(NV_ENC_CAPS_NUM_MAX_BFRAMES);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_FIELD_ENCODING);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_MONOCHROME);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_FMO);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_QPELMV);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_BDIRECT_MODE);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_CABAC);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM);
//      QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_STEREO_MVC);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_LEVEL_MAX);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_LEVEL_MIN);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SEPARATE_COLOUR_PLANE);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_WIDTH_MAX);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_HEIGHT_MAX);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_INTRA_REFRESH);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_PREPROC_SUPPORT);
        QUERY_PRINT_CAPS(NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT);
		QUERY_PRINT_CAPS(NV_ENC_CAPS_MB_NUM_MAX);     
		QUERY_PRINT_CAPS(NV_ENC_CAPS_MB_PER_SEC_MAX );
		QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_YUV444_ENCODE );
		QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE );
		QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_SAO);   // NVENC 6.0
		QUERY_PRINT_CAPS(NV_ENC_CAPS_SUPPORT_MEONLY_MODE); // NVENC 6.0
		//QUERY_PRINT_CAPS(NV_ENC_CAPS_EXPOSED_COUNT);
    }

	s = os.str();
}

// Initialization code that checks the GPU encoders available and fills a table
unsigned int checkNumberEncoders(EncoderGPUInfo *encoderInfo)
{
    CUresult cuResult = CUDA_SUCCESS;
    CUdevice cuDevice = 0;

    char gpu_name[100];
    int  deviceCount = 0;
    int  SMminor = 0, SMmajor = 0;
    int  NVENC_devices = 0;

    printf("\n");

    // CUDA interfaces
    cuResult = cuInit(0);
    if (cuResult != CUDA_SUCCESS) {
        printf(">> GetNumberEncoders() - cuInit() failed error:0x%x\n", cuResult);
        exit(EXIT_FAILURE);
    }

    checkCudaErrors(cuDeviceGetCount(&deviceCount));
    if (deviceCount == 0) {
        printf( ">> GetNumberEncoders() - reports no devices available that support CUDA\n");
        exit(EXIT_FAILURE);
    } else {
        printf(">> GetNumberEncoders() has detected %d CUDA capable GPU device(s) <<\n", deviceCount);
        for (int currentDevice=0; currentDevice < deviceCount; currentDevice++) {
            checkCudaErrors(cuDeviceGet(&cuDevice, currentDevice));
            checkCudaErrors(cuDeviceGetName(gpu_name, 100, cuDevice));
            checkCudaErrors(cuDeviceComputeCapability(&SMmajor, &SMminor, currentDevice));
            printf("  [ GPU #%d - < %s > has Compute SM %d.%d, NVENC %0u.%0u API is %s ]\n", 
                            currentDevice, gpu_name, SMmajor, SMminor, 
							NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION,
                            (((SMmajor << 4) + SMminor) >= 0x30) ? "Available" : "Not Available");

            if (((SMmajor << 4) + SMminor) >= 0x30)
            {
                encoderInfo[NVENC_devices].device = currentDevice;
                strcpy(encoderInfo[NVENC_devices].gpu_name, gpu_name);
                NVENC_devices++;
            }
        }
    }
    return NVENC_devices;
}


// Main Console Application for NVENC
int main(const int argc, char *argv[])
{
	CUVIDEOFORMAT inCuvideoformat[MAX_ENCODERS]; // input-file's format information
    NV_ENC_CONFIG_H264_VUI_PARAMETERS vui;    // H264-Encoder's video-usability struct (for color info)
	NV_ENC_CONFIG_HEVC_VUI_PARAMETERS vui265; // H265-Encoder's video-usability struct (for color info)

	NVENCSTATUS nvencstatus = NV_ENC_SUCCESS; // OpenEncodeSession() return code
	FILE *fOutput[MAX_ENCODERS] = {NULL};
    int retval        = 1;

	CNvEncoder     *pEncoder[MAX_ENCODERS] = {NULL};
    EncodeConfig   nvEncoderConfig[MAX_ENCODERS];
    EncoderGPUInfo encoderInfo[MAX_ENCODERS];

    EncoderAppParams nvAppEncoderParams = {0};
	videoDecode *pVideoDecode[MAX_ENCODERS] = {NULL};
	string       output_filename_strings[MAX_ENCODERS];

//    int          botFieldFirst    = 0;
    int          filename_length  = 0;
    bool useall_gpus              = false;// if true, use ALL detected GPUs
	unsigned int use_gpuid        = 0;// The GPUID# to use (if multiple GPUs are installed, only 1 will be used)
	unsigned char *yuv[3] = {NULL, NULL, NULL};

    HANDLE hInput;

#if defined __linux || defined __APPLE_ || defined __MACOSX
    U32 num_bytes_read;
    NvPthreadABIInit();
#endif

    memset(&nvEncoderConfig, 0 , sizeof(EncodeConfig)*MAX_ENCODERS);
    memset(&vui   , 0, sizeof(NV_ENC_CONFIG_H264_VUI_PARAMETERS));
	memset(&vui265, 0, sizeof(NV_ENC_CONFIG_HEVC_VUI_PARAMETERS));
	memset(inCuvideoformat, 0, sizeof(inCuvideoformat));
	memset(fOutput, NULL, sizeof(fOutput));

    // Initialize Encoder Configurations
	pprintf("calling initEncoderParams()\n");
    initEncoderParams(&nvAppEncoderParams, &nvEncoderConfig[0]);
	nvAppEncoderParams.maxNumberEncoders = 16;// bandaid, most likely no one is running with more than 16 gpus
    
    // Find out the source-video's characteristics (format, size, frame-rate, etc.)
	//
	//  Note, the *videoDecode object can modify *argv[].
	pVideoDecode[0] = new videoDecode(argc, argv);
	pVideoDecode[0]->parseCommandLineArguments(); // set videoDecode's input-filename

    pVideoDecode[0]->loadVideoSource( &inCuvideoformat[0] );


    nvEncoderConfig[0].width = inCuvideoformat[0].display_area.right - inCuvideoformat[0].display_area.left; 
    nvEncoderConfig[0].height= inCuvideoformat[0].display_area.bottom- inCuvideoformat[0].display_area.top;
    nvEncoderConfig[0].maxHeight = inCuvideoformat[0].coded_height; // nvEncoderConfig[0].height;
    nvEncoderConfig[0].maxWidth  = inCuvideoformat[0].coded_width; // nvEncoderConfig[0].width;
    nvEncoderConfig[0].darRatioX = inCuvideoformat[0].display_aspect_ratio.x;
    nvEncoderConfig[0].darRatioY = inCuvideoformat[0].display_aspect_ratio.y;

    nvEncoderConfig[0].frameRateDen = inCuvideoformat[0].frame_rate.denominator;
    nvEncoderConfig[0].frameRateNum = inCuvideoformat[0].frame_rate.numerator;
    nvEncoderConfig[0].FieldEncoding = inCuvideoformat[0].progressive_sequence ? NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME : NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD;

	// kludge: when input file is encoded in H265/HEVC, the cuvid decoder API
	//         fails to report the correct framerate, leaving it 0/0.
	//         Check the frame-rate and notify the user to take action.
	if (nvEncoderConfig[0].frameRateDen == 0 || nvEncoderConfig[0].frameRateNum == 0) {
		printf( "ERROR, input file has a reported frameRate (Num/Den) = %0u/%0u\n",
			nvEncoderConfig[0].frameRateNum, nvEncoderConfig[0].frameRateDen
		);
		printf( "Auto-detection of input-file framerate has failed.  You must manually\n");
		printf( "specify the framerate (m/n) using the following command-line options:\n");
		printf("-numerator=<m>, -denominator=<n>\n");
		exit(EXIT_FAILURE);
	}

	///////////////////////////////////////////////////////////
	//
	// create window hWnd (after we know the size of the input file size)
	//
	// In the original NVCUVID sample project, 'hWnd' was the on-screen output 
	// window for the decoded video.  Here, we don't output to a window.
	// Instead we use hWnd for a totally different reason; it's a required argument
	// for IDirect3D9::CreateDevice().
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L,
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      sAppName, NULL
                    };
    RegisterClassEx(&wc);

    // figure out the window size we must create to get a *client* area
    // that is of the size requested by m_dimensions.
    RECT adjustedWindowSize;
    DWORD dwWindowStyle;
    HWND hWnd = NULL;

    {   // create a dummy window for the direct3d9 interop-object we'll create later.
        dwWindowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetRect(&adjustedWindowSize, 0, 0, inCuvideoformat[0].coded_width, inCuvideoformat[0].coded_height);
        AdjustWindowRect(&adjustedWindowSize, dwWindowStyle, false);

        LONG g_nWindowWidth  = adjustedWindowSize.right  - adjustedWindowSize.left;
        LONG g_nWindowHeight = adjustedWindowSize.bottom - adjustedWindowSize.top;

        // Create the application's window
        hWnd = CreateWindow(wc.lpszClassName, sAppName,
                            dwWindowStyle,
                            0, 0,
                            g_nWindowWidth,
                            g_nWindowHeight,
                            NULL, NULL, wc.hInstance, NULL);
    }


    // Parse the command line parameters for the application and NVENC
	// This step allows some of the above settings (like aspect ratio) to be overriden from the command-line
	pprintf("calling parseCmdLineArguments()\n");
    parseCmdLineArguments(argc, (const char **)argv, &nvAppEncoderParams, &nvEncoderConfig[0]);

	// Kludge: override certain settings based on the input-file:
	switch( inCuvideoformat[0].chroma_format ) {
	case cudaVideoChromaFormat_Monochrome: // 4:2:0 (any of the following: base/main/high/high422/high444/high444p)
	case cudaVideoChromaFormat_420:  // 4:2:0 (any of the following: base/main/high/high422/high444/high444p)
		nvEncoderConfig[0].chromaFormatIDC = cudaVideoChromaFormat_420;// 4:2:0
		break;

	case cudaVideoChromaFormat_422:  // 4:2:2 (High422/High444/High444p)
		pprintf("input-videofile: ERROR, chroma_format cudaVideoChromaFormat_422 is not supported!\n");
		exit(EXIT_FAILURE);
		break;

	case cudaVideoChromaFormat_444:  // 4:4:4 (High444/High444p)
		nvEncoderConfig[0].chromaFormatIDC = cudaVideoChromaFormat_444;// 4:4:4
		if ( nvEncoderConfig[0].profile < NV_ENC_H264_PROFILE_HIGH_444 ) {
			pprintf("FIXUP: forcing nvEncoderConfig[0].profile = NV_ENC_H264_PROFILE_HIGH_444\n");
			nvEncoderConfig[0].profile = NV_ENC_H264_PROFILE_HIGH_444;
		}
		break;
	default:
		pprintf("input-videofile: ERROR, unknown chroma_format: %0u!\n", inCuvideoformat[0].chroma_format);
		exit(EXIT_FAILURE);
	} // switch( inCuvideoformat[0].chroma_format ) 

    // Show a summary of all the parameters for GPU 0
	pprintf("calling displayEncodingParams()\n");
    displayEncodingParams(&nvAppEncoderParams, nvEncoderConfig, 0);

    // If there is a failure to open the file, this function will quit and print an error message
/*
	pprintf("calling nvOpenFile()\n");
    hInput = nvOpenFile (nvAppEncoderParams.input_file);

	pprintf("calling nvGetFileSize()\n");
    nvGetFileSize (hInput, NULL);
*/
    // We don't know how many frames the source-bitstream contains, so just set it to an arbitrary large number
    unsigned int inputEndFrame = 1<<30;

    nvAppEncoderParams.input_file = 0;
	// If the Number of input farmes are not specified, encoding to the computed frame number
    if (nvAppEncoderParams.endFrame == 0 || (nvAppEncoderParams.endFrame > inputEndFrame))
    {
        printf("Input File <%s> auto setting (nvAppEncoderParams.endFrame = %d frames)\n", nvAppEncoderParams.input_file, inputEndFrame);
        nvAppEncoderParams.endFrame = inputEndFrame;
    }
    nvAppEncoderParams.numFramesToEncode = MAX(1,(nvAppEncoderParams.endFrame - nvAppEncoderParams.startFrame));
    printf("\n");

    // Clear all counters
    double total_encode_time[MAX_ENCODERS], sum[MAX_ENCODERS];

    for (int i=0; i < MAX_ENCODERS; i++) 
    {
        total_encode_time[i] = sum[i] = 0.0;

        // Make a copy of all of the Encoder Configurations based on 0
        if (i > 0) {
            memcpy(&nvEncoderConfig[i], &nvEncoderConfig[0], sizeof(EncodeConfig));
        }
    }

    // Query the number of GPUs that have NVENC encoders
	vector<bool> encoder_disable_mask;
    vector<unsigned int> srcFrameNumber;

    unsigned int numEncoders = MIN(checkNumberEncoders(encoderInfo), nvAppEncoderParams.maxNumberEncoders);

	for(unsigned i = 0; i < numEncoders; ++i ) {
		encoder_disable_mask.push_back( true );
		srcFrameNumber.push_back( 0 );
	}

	// GPU selection: 
	// --------------
	// More than 1 GPU may be installed in the system.  All are disabled by default.
	//  command-line argument "-gpuid=<x>" chooses one gpuID.

	use_gpuid = 0; // default GPUID (if none specified on command-line)
	getCmdLineArgumentValue ( argc, (const char **)argv, "device", &use_gpuid );
	encoder_disable_mask[use_gpuid] = false; // turn on the user-selected GPU

	// optional, turn on ALL gpus
//	useall_gpus = checkCmdLineFlag ( argc, (const char **)argv, "useall_gpus");
//	if ( useall_gpus )
//		for(unsigned i = 0; i < encoder_disable_mask.size(); ++i )
//			encoder_disable_mask[i] = false;


	pprintf("Checkpoint 2: numEncoders=%0u\n", numEncoders);

    if ( nvAppEncoderParams.output_file == NULL ) {
        pprintf("output_file not specified: --outfile=?\n");
        exit(EXIT_FAILURE);
    }
    else
        pprintf("--outfile=%s\n", nvAppEncoderParams.output_file);

    // We already created pVideoDecode[0] (to get the source-video characteristics.)
	// Now create the remaining pVideoDecode objects.
	for (unsigned int encoderID=1; encoderID < numEncoders; encoderID++) {
		// mask-control: use encoderID# only if it is not disabled
		if ( encoder_disable_mask[encoderID] ) continue; // it's masked, don't use it

		pVideoDecode[encoderID] = new videoDecode(argc, argv);
		pVideoDecode[encoderID]->parseCommandLineArguments(); // set videoDecode's input-filename

		// note, only inCuvideoformat[0] is used, we discard inCuvideoformat[i] returned by
		// all the other encoderIDs (1..xx)
	    pVideoDecode[encoderID]->loadVideoSource( &inCuvideoformat[encoderID] );
	}

    // Depending on the number of available encoders and the maximum encoders, we open multiple FILEs (one per GPU)
    for (unsigned int encoderID=0; encoderID < numEncoders; encoderID++) 
    {
        char output_filename[256], *output_ext = NULL;
        int filename_length = (int)strlen(nvAppEncoderParams.output_file);
        int extension_index = getFileExtension(nvAppEncoderParams.output_file, &output_ext);

		// mask-control: use encoderID# only if it is not disabled
		if ( encoder_disable_mask[encoderID] ) continue; // it's masked, don't use it

		pprintf("Checkpoint loop encoderID=%0u, extension_index=%0d\n", encoderID, extension_index);
		if ( extension_index )
			pprintf("\toutput_ext = %0s\n", output_ext );

        if (encoderID == 0) 
        {
            strncpy(nvAppEncoderParams.output_base_file, nvAppEncoderParams.output_file, extension_index-1);
			pprintf("Checkpoint loop1b\n");
            nvAppEncoderParams.output_base_file[extension_index-1] = '\0';
			pprintf("Checkpoint loop1c\n");
            strcpy (nvAppEncoderParams.output_base_ext, output_ext);
        }
		pprintf("Checkpoint loop2\n");
        strncpy(output_filename, nvAppEncoderParams.output_file, extension_index-1);
        output_filename[extension_index-1] = '\0';
        sprintf(output_filename, "%s.gpu%d.%s", output_filename, encoderID, output_ext);

		// remember this encoder's output filename (need this to pritn end-of-run stats)
		output_filename_strings[encoderID] = output_filename;

		pprintf("Checkpoint loop3\n");
        fOutput[encoderID] = fopen(output_filename, "wb+");
        if (!fOutput[encoderID])
        {
            printf("Failed to open encoderID[%d], output file\"%s\"\n", encoderID, output_filename);
            exit(EXIT_FAILURE);
        }
        nvEncoderConfig[encoderID].fOutput         = fOutput[encoderID];
    }

    // Set the 'Video Usability Info' struct -
    //   identifies the exact colorspace geometry (eg. BT-709)
	if ( 1 )  { // ( inCuvideoformat.video_signal_description.video_format < 5 )
        vui.videoSignalTypePresentFlag = 1;
        vui.videoFormat = inCuvideoformat[0].video_signal_description.video_format;
        vui.colourDescriptionPresentFlag = 1;
        vui.colourMatrix = inCuvideoformat[0].video_signal_description.matrix_coefficients;
        vui.colourPrimaries = inCuvideoformat[0].video_signal_description.color_primaries;
        vui.transferCharacteristics = inCuvideoformat[0].video_signal_description.transfer_characteristics;

#define COPY_VUI_FIELD(f) vui265.f = vui.f;
		COPY_VUI_FIELD(videoSignalTypePresentFlag)
		COPY_VUI_FIELD(videoFormat)
		COPY_VUI_FIELD(colourDescriptionPresentFlag)
		COPY_VUI_FIELD(colourMatrix)
		COPY_VUI_FIELD(colourPrimaries)
		COPY_VUI_FIELD(transferCharacteristics)
    }

	///////////////////////////////////////////////////////////////////////////
	// Using the default creation-flag (cudaVideoCreate_PreferCUVID) appears to 
	// have problems with some types of video-bitstreams:
	//
	// (1) all HEVC bitstreams (Kepler/Maxwell GPUs, which rely on 'hybrid' decoder)
	//     (can't even create the decoder object)
	// (2) some H264 bitstreams (usually 4k-resolution with high# ref. frames)
	//     (decoder object is created, but fails on first frame with the error
	//      'not enough resources')
	//
	// To workaround this problem, we use the D3D9 interoperability mode.  In this
	// mode, we create our CUDA-context with its associated ID3D9Device handle.
	// This requires that the NVidia GPU is an enumerated D3D9 device (i.e. it
	// must be physically attached to a display, it cannot be running in
	// 'headless' mode.)
	//
	// The interop-mode enables us to use the 'DXVA' creation-flag, which 
	// seems to handle the aforementioned problematic bitstreams without
	// trouble.

    // Create the H.264 Encoder instances
    for (unsigned int encoderID=0; encoderID < numEncoders; encoderID++) 
    {
		// mask-control: use encoderID# only if it is not disabled
		if ( encoder_disable_mask[encoderID] ) continue; // it's masked, don't use it

		////////////////
		// create a D3D9 interoperabilty context (even if we don't need it later)
		// 
		//  Note: the NVidia GPU *must* be attached to a display
		//        (i.e. it cannot be running in a 'headless' config.)
		//
		// Since we don't know if we're actually going to use interop-mode, it's ok
		// if initD3D9() fails. 
		CUdevice device = 0;
		int bTCC;
		const bool interop_available = pVideoDecode[encoderID]->initD3D9(
			hWnd, // only used to create a IDirect3D9 device
			nvEncoderConfig[encoderID].maxWidth,
			nvEncoderConfig[encoderID].maxHeight,
			&bTCC
		);
			
//		if (pVideoDecode[encoderID]->initCudaResources(true , 0, &device, NULL) == E_FAIL)
		const bool useinterop = (inCuvideoformat[0].codec == cudaVideoCodec_HEVC) ?
			true:   // On Kepler/Maxwell GPUs, HEVC requires interop-mode
			interop_available;  // don't need interop, use it only if available

		if (pVideoDecode[encoderID]->initCudaResources(useinterop, 0, NULL, NULL) == E_FAIL)
		{
//        m_bAutoQuit  = true;
//        m_bException = true;
//        m_bWaived    = true;
	        my_printf("pVideoDecode[0]: Unable to initCudaResources!\n");
			pVideoDecode[encoderID]->cleanup(true);
			exit(EXIT_FAILURE);
		}
		CUcontext cudaContext = pVideoDecode[encoderID]->GetCudaContext();

        // Create H.264 based encoder
		switch (nvEncoderConfig[0].codec) {

			case NV_ENC_H264: 
				pEncoder[encoderID] = new CNvEncoderH264();
				break;

			case NV_ENC_H265:
				pEncoder[encoderID] = new CNvEncoderH265();
				break;
					
			default:
				printf("main2(): ERROR, unknown codec(%0d)\n", nvEncoderConfig[0].codec);
				exit(EXIT_FAILURE);
		} // switch
        
		pEncoder[encoderID]->Register_fwrite_callback(fwrite_callback);

		// Configure the encoder to use the videoDecoder's cuda-context.
		pEncoder[encoderID]->UseExternalCudaContext(cudaContext, encoderInfo[encoderID].device);

        // Section 2.1 (Opening an Encode Session on nDeviceID)
        pEncoder[encoderID]->OpenEncodeSession( nvEncoderConfig[encoderID], encoderInfo[encoderID].device, nvencstatus );

//        if ( S_OK != pEncoder[encoderID]->InitializeEncoder() )
		HRESULT hr = S_FALSE;
		switch (nvEncoderConfig[0].codec) {
			case NV_ENC_H264:
				hr = pEncoder[encoderID]->InitializeEncoderCodec((void *)&vui);
				break;

			case NV_ENC_H265:
				hr = pEncoder[encoderID]->InitializeEncoderCodec((void *)&vui265);
				break;

			default:
				printf("main2(): ERROR, unknown codec(%0d)\n", nvEncoderConfig[0].codec);
				exit(EXIT_FAILURE);
		}

        if ( hr != S_OK )
        {
            printf("\nmain2(): nvEncoder Error InitializeEncoderCodec(): encoder initialization failure(%0X)! Check input params!\n", hr);
            return 1;
        }

//        if (encoderID == 0) {
//        if (nvAppEncoderParams.showCaps == 1) 
		{
			string stmp;
            queryAllEncoderCaps(pEncoder[encoderID], stmp);
			cout << stmp;
			printf("Press enter to continue.\n");
			getchar();
        }

        sdkCreateTimer(&timer[encoderID]);
        sdkResetTimer (&timer[encoderID]);  

		pVideoDecode[encoderID]->Start();
    } // for ( encoderID

	printf("Checkpoint 4\n");
        
    unsigned int picHeight ;
    int lumaPlaneSize      ;
    int chromaPlaneSize    ;
	unsigned int oDecodedFrame_pitch;// #bytes per scanline

	//
	// Create picture buffers for the encoder
	//
    picHeight     = (nvEncoderConfig[0].FieldEncoding == NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME) ?
                   nvEncoderConfig[0].height : (nvEncoderConfig[0].height >> 1);
    lumaPlaneSize = (nvEncoderConfig[0].width * nvEncoderConfig[0].height);

	if ( nvEncoderConfig[0].chromaFormatIDC == cudaVideoChromaFormat_444 )
		chromaPlaneSize = lumaPlaneSize;// separate Y,U,V planes: each is full size
	else 
		chromaPlaneSize = lumaPlaneSize >> 1;// NV12 format: chromaplane is combined U+V (1/4 + 1/4 = 1/2)

	/*
    // In the original nvEncoderApp, video-frames were sourced from hostMemory.
	// Since we are feeding the output of CUVID (Purevideo) decoder into NVENC,
	// the frames are already allocated in GPU deviceMemory, so we completely
	// bypass hostMemory. 
    yuv[0] = new unsigned char[FRAME_QUEUE * lumaPlaneSize  ];
    yuv[1] = new unsigned char[FRAME_QUEUE * chromaPlaneSize];
    yuv[2] = new unsigned char[FRAME_QUEUE * chromaPlaneSize];

	// clear the buffers before starting (wow, takes way too long)
	for(int i = 0; i < FRAME_QUEUE; ++i ) {
		memset( yuv[0], 0  , FRAME_QUEUE * lumaPlaneSize );
		memset( yuv[1], 128, FRAME_QUEUE * chromaPlaneSize );
		memset( yuv[2], 128, FRAME_QUEUE * chromaPlaneSize );
	}
	*/
    fprintf(stderr, "\n ** Start Encode <%s> ** \n", nvAppEncoderParams.input_file);
    
    int viewId = 0;
    unsigned int curEncWidth  = nvEncoderConfig[0].width;
    unsigned int curEncHeight = nvEncoderConfig[0].height;
    bool bTopField = false;//!botFieldFirst;
    bool not_end_of_source = true;
    for (unsigned int frameNumber = 0 ; not_end_of_source && (frameNumber < nvAppEncoderParams.numFramesToEncode); ++frameNumber)
    {
        // outputs from the cuvid decoder-
        bool got_source_frame;
        CUVIDPICPARAMS oDecodedPicParams;   // decoded frame metainfo (I/B/P frame, etc.)
        CUVIDPARSERDISPINFO oDecodedDispInfo; // decoded frame information (pic-index)
        CUdeviceptr oDecodedFrame[3];       // handle - decoded framebuffer (or fields)
                                        // progressive uses 1
                                        // interlaced uses 2
                                        // interlaced + repeat_field uses 3


        // Decode *1* frame (or a pair of interlaced-fields) from the source-video.
        // The decoded frames will be placed in Cuda (Device) Memory.
/*
        bool got_frame;
        // On this case we drive the display with a while loop (no openGL calls)
        while (pVideoDecode->GetFrame(&got_frame, &oDecodedPicParams, &oDecodedDispInfo, oDecodedFrame)) {
            // Once we are done with processing, must release the hw/mem resources that
            // are held by the decoded-picture
            ++srcFrameNumber;
            if ( got_frame )
                pVideoDecode->GetFrameFinish( &oDecodedDispInfo, oDecodedFrame);
        }
*/


/*
        printf("\nLoading Frames [%d,%d] into system memory queue (%d frames)\n", frameNumber,
              (MIN(frameNumber+FRAME_QUEUE, nvAppEncoderParams.numFramesToEncode))-1, 
              (MIN(frameNumber+FRAME_QUEUE, nvAppEncoderParams.numFramesToEncode))-frameNumber);

        for (unsigned int frameCount = frameNumber ; frameCount < MIN(frameNumber+FRAME_QUEUE, nvAppEncoderParams.numFramesToEncode); frameCount++) 
        {
            EncodeFrameConfig stEncodeFrame;
            memset(&stEncodeFrame, 0, sizeof(stEncodeFrame));
            stEncodeFrame.yuv[0] = yuv[0];
            stEncodeFrame.yuv[1] = yuv[1];
            stEncodeFrame.yuv[2] = yuv[2];
    
            stEncodeFrame.stride[0] = nvEncoderConfig[0].width;
            stEncodeFrame.stride[1] = nvEncoderConfig[0].width/2;
            stEncodeFrame.stride[2] = nvEncoderConfig[0].width/2;
            if (nvAppEncoderParams.mvc == 1) {
                stEncodeFrame.viewId = viewId;
            }

            // Ability to set Dynamic Bitrate Changes
            if (nvAppEncoderParams.dynamicResChange) {
                if (frameNumber == nvAppEncoderParams.numFramesToEncode/4) {
                    stEncodeFrame.dynResChangeFlag = DYN_DOWNSCALE;
                    stEncodeFrame.newWidth  = curEncWidth  = nvEncoderConfig[0].width/2;
                    stEncodeFrame.newHeight = curEncHeight = nvEncoderConfig[0].height/2;
                } 
                else if (frameNumber == nvAppEncoderParams.numFramesToEncode*3/4) {
                    stEncodeFrame.dynResChangeFlag = DYN_UPSCALE;
                    stEncodeFrame.newWidth  = curEncWidth  = nvEncoderConfig[0].width;
                    stEncodeFrame.newHeight = curEncHeight = nvEncoderConfig[0].height;
                }
            }
            if (nvAppEncoderParams.dynamicBitrateChange) {
                if (frameNumber == nvAppEncoderParams.numFramesToEncode/4) {
                    stEncodeFrame.dynBitrateChangeFlag = DYN_DOWNSCALE;                
                }
                else if (frameNumber == nvAppEncoderParams.numFramesToEncode*3/4) {
                    stEncodeFrame.dynBitrateChangeFlag = DYN_UPSCALE;
                }
            }

            // Field based source inputs
            if (nvEncoderConfig[0].FieldEncoding == 2) {
                for (int field = 0; field < 2; field++) {
                    LoadCurrentFrame(yuv, hInput, frameCount, 
                                     nvEncoderConfig[0].width, nvEncoderConfig[0].height, 
                                     nvEncoderConfig[0].width, picHeight, 
                                     !nvEncoderConfig[0].FieldEncoding, bTopField, FRAME_QUEUE, nvEncoderConfig[0].chromaFormatIDC);
                }
            }
            else {
                // Frame Based source inputs
                LoadCurrentFrame (yuv, hInput, frameCount, 
                                  nvEncoderConfig[0].width, nvEncoderConfig[0].height, 
                                  nvEncoderConfig[0].width, nvEncoderConfig[0].height, 
                                  false, false, FRAME_QUEUE, nvEncoderConfig[0].chromaFormatIDC);
            }
            viewId = viewId ^ 1 ;
        }
*/

        // We are only timing the Encoding time (YUV420->NV12 Tiled and the Encoding)
        // Now start the encoding process
        printf("Encoding Frames [%d,%d]\n", frameNumber, (MIN(frameNumber+FRAME_QUEUE, nvAppEncoderParams.numFramesToEncode)-1) );

        for (unsigned int encoderID=0; encoderID < numEncoders; encoderID++) 
        {
            // Start measuring performance of the encode
            sdkStartTimer(&timer[encoderID]);
            sdkResetTimer(&timer[encoderID]);
        }

        // Now send the decoded source frame to the encoder, directly from the buffer (up to 240 frames)
        unsigned int frameCount = frameNumber;
        
        EncodeFrameConfig stEncodeFrame;
        memset(&stEncodeFrame, 0, sizeof(stEncodeFrame));

        for (unsigned int encoderID=0; encoderID < numEncoders; encoderID++) {
			unsigned int fboffset;

			// mask-control: use encoderID# only if it is not disabled
			if ( encoder_disable_mask[encoderID] ) continue; // it's masked, don't use it

	        do {
		        ++srcFrameNumber[encoderID];
	            not_end_of_source = pVideoDecode[encoderID]->GetFrame(
					&got_source_frame,
					&oDecodedPicParams,
					&oDecodedDispInfo,
					oDecodedFrame,
					&oDecodedFrame_pitch
				);
				// Once we are done with processing, must release the hw/mem resources that
				// are held by the decoded-picture
			} while ( not_end_of_source && !got_source_frame );

			// If we decoded an interlaced video frame: update bTopField
			if ( !oDecodedDispInfo.progressive_frame && (srcFrameNumber[encoderID] <= 1) )
				bTopField = oDecodedDispInfo.top_field_first ? true : false;

			if ( not_end_of_source && got_source_frame ) {
/*
				// [0] = luma
//				stEncodeFrame.yuv[0] = &yuv[0][(frameCount % FRAME_QUEUE)*nvEncoderConfig[encoderID].width*nvEncoderConfig[encoderID].height];
                stEncodeFrame.stride[0] = nvEncoderConfig[encoderID].width;

                // [1] = chroma U
                // [2] = chroma V
				if ( IsYUV444Format( nvEncoderConfig[0].chromaFormatIDC ) ) {
					// YUV444: Each chroma plane is same size as luma plane
	                stEncodeFrame.yuv[1] = &yuv[1][(frameCount % FRAME_QUEUE)*nvEncoderConfig[encoderID].width*nvEncoderConfig[encoderID].height];
					stEncodeFrame.yuv[2] = &yuv[2][(frameCount % FRAME_QUEUE)*nvEncoderConfig[encoderID].width*nvEncoderConfig[encoderID].height];
	                stEncodeFrame.stride[1] = nvEncoderConfig[encoderID].width;
					stEncodeFrame.stride[2] = nvEncoderConfig[encoderID].width;
				}
				else if ( IsNV12Format( nvEncoderConfig[0].chromaFormatIDC ) ) {
					// NV12: One chroma plane
	                stEncodeFrame.yuv[1] = &yuv[1][(frameCount % FRAME_QUEUE)*nvEncoderConfig[encoderID].width*nvEncoderConfig[encoderID].height/4];
					stEncodeFrame.yuv[2] = &yuv[2][(frameCount % FRAME_QUEUE)*nvEncoderConfig[encoderID].width*nvEncoderConfig[encoderID].height/4];
	                stEncodeFrame.stride[1] = nvEncoderConfig[encoderID].width/2;
					stEncodeFrame.stride[2] = nvEncoderConfig[encoderID].width/2;
				}
				else {
					// YUV420: Each chroma plane is 1/4 size of luma plane
	                stEncodeFrame.yuv[1] = &yuv[1][(frameCount % FRAME_QUEUE)*nvEncoderConfig[encoderID].width*nvEncoderConfig[encoderID].height/4];
					stEncodeFrame.yuv[2] = &yuv[2][(frameCount % FRAME_QUEUE)*nvEncoderConfig[encoderID].width*nvEncoderConfig[encoderID].height/4];
	                stEncodeFrame.stride[1] = nvEncoderConfig[encoderID].width/2;
					stEncodeFrame.stride[2] = nvEncoderConfig[encoderID].width/2;
				}
*/
                if (nvAppEncoderParams.mvc == 1)
                {
                    stEncodeFrame.viewId = viewId;
                }
                
                if (nvAppEncoderParams.dynamicResChange) {
                    if (frameNumber == nvAppEncoderParams.numFramesToEncode/4) {
                        stEncodeFrame.dynResChangeFlag = DYN_DOWNSCALE;
                        stEncodeFrame.newWidth  = curEncWidth  = nvEncoderConfig[encoderID].width/2;
                        stEncodeFrame.newHeight = curEncHeight = nvEncoderConfig[encoderID].height/2;
                    }
                    else if (frameNumber == nvAppEncoderParams.numFramesToEncode*3/4) {
                        stEncodeFrame.dynResChangeFlag = DYN_UPSCALE;
                        stEncodeFrame.newWidth  = curEncWidth  = nvEncoderConfig[encoderID].width;
                        stEncodeFrame.newHeight = curEncHeight = nvEncoderConfig[encoderID].height;
                    }
                }

                if (nvAppEncoderParams.dynamicBitrateChange) {
                    if (frameCount == nvAppEncoderParams.numFramesToEncode/4) {
                        stEncodeFrame.dynBitrateChangeFlag = DYN_DOWNSCALE;                
                    }
                    else if (frameCount == nvAppEncoderParams.numFramesToEncode*3/4) {
                        stEncodeFrame.dynBitrateChangeFlag = DYN_UPSCALE;
                    }
                }

                stEncodeFrame.width  = nvEncoderConfig[encoderID].width;
                stEncodeFrame.height = nvEncoderConfig[encoderID].height;
				unsigned fbbase = (frameCount % FRAME_QUEUE) * stEncodeFrame.width * stEncodeFrame.height;

				if ( nvEncoderConfig[encoderID].FieldEncoding == NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME ) {
					// frame encoding mode
					stEncodeFrame.fieldPicflag = false;  // progressive-scan frame
					stEncodeFrame.topField = false;       // not used
				}
				else {
					// (field or MBAFF) encoding mode
					// In interlaced-encoding mode, fieldPicflag must always be set to true,
					//    even when passing a progressive-frame.
					stEncodeFrame.fieldPicflag = true;
					stEncodeFrame.topField = oDecodedDispInfo.top_field_first;
					//
					//  rff - NOT SUPPORTED YET! 
					//if ( oDecodedDispInfo.repeat_first_field )
				}

/*
                stEncodeFrame.yuv[0] = &yuv[0][ fbbase ];
				if ( IsYUV444Format( nvEncoderConfig[0].chromaFormatIDC ) ) {
					stEncodeFrame.yuv[1] = &yuv[1][ fbbase ];
					stEncodeFrame.yuv[2] = &yuv[2][ fbbase ];
				}
				else if ( IsNV12Format( nvEncoderConfig[0].chromaFormatIDC ) ) {
					stEncodeFrame.yuv[1] = &yuv[1][ fbbase >> 2];
					stEncodeFrame.yuv[2] = &yuv[2][ fbbase >> 2];
				}
				else {
					stEncodeFrame.yuv[1] = &yuv[1][ fbbase >> 2];
					stEncodeFrame.yuv[2] = &yuv[2][ fbbase >> 2];
				}
*/
                //pEncoder[encoderID]->EncodeFrame(&stEncodeFrame, false);
                pEncoder[encoderID]->EncodeCudaMemFrame(&stEncodeFrame,oDecodedFrame,oDecodedFrame_pitch,false);

                // tell the Cuda video-decoder that the Decoded-Frames are no longer needed.
                // (Frees them up to be re-used.)
                pVideoDecode[encoderID]->GetFrameFinish( &oDecodedDispInfo, oDecodedFrame);

                if (nvAppEncoderParams.maxNumberEncoders > 1) {
                    printf("[%d]%d, ", encoderID, frameCount);
                } else {
//                    printf("%d, ", frameCount);
                    printf("%d, %08X", frameCount, oDecodedFrame[0]);
                }
            }
            viewId = viewId ^ 1 ; // stereoscopic not supported, does this go here?
//        } // for (unsigned int encoderID=0;

        // At the end-of-source video, or end of session, call the encoder(s) one last time to flush them.
//        for (unsigned int encoderID=0; encoderID < numEncoders; encoderID++) 
//        {
            if ((!not_end_of_source) || (frameNumber == nvAppEncoderParams.numFramesToEncode-1))  {
                printf("EncoderID[%d] - Last Encoded Frame flushed\n", encoderID);
				pEncoder[encoderID]->EncodeCudaMemFrame(NULL, oDecodedFrame, oDecodedFrame_pitch,true);// flush the encoder
            }

            sdkStopTimer(&timer[encoderID]);
            sum[encoderID] = sdkGetTimerValue(&timer[encoderID]);
            total_encode_time[encoderID] += sum[encoderID];

            printf("\nencodeID[%d], Frames [%d,%d] Encode Time = %6.2f (ms)\n", encoderID, frameNumber, 
                                                    MIN(frameNumber, nvAppEncoderParams.numFramesToEncode)-1, sum[encoderID]);
        }
    }

    // Encoding Complete, now print statistics
    for (unsigned int encoderID=0; encoderID < numEncoders; encoderID++) 
    {
		unsigned int numFramesToEncode;

		// if this encoder isn't enabled, skip to the next one
		if ( encoder_disable_mask[encoderID] == true )
			continue;// skip to next encoder

	    // update the numFrames to the *actual* number of frames we decoded, so that the statistics print out correctly.
	    if ( nvAppEncoderParams.numFramesToEncode > pVideoDecode[encoderID]->GetFrameDecodeCount() )
			numFramesToEncode = pVideoDecode[encoderID]->GetFrameDecodeCount();
		else
			numFramesToEncode = nvAppEncoderParams.numFramesToEncode;

        pprintf("** EncoderID[%d] - Summary of Results **\n", encoderID);
		pprintf("  NVCUVID decodedframe_pitch : %0u (#bytes per scanline)\n", oDecodedFrame_pitch);
        pprintf("  Frames Encoded     : %d\n", numFramesToEncode);
        pprintf("  Total Encode Time  : %6.2f (sec)\n", total_encode_time[encoderID] / 1000.0f );
        pprintf("  Average Time/Frame : %6.2f (ms)\n",  total_encode_time[encoderID] / numFramesToEncode );
        pprintf("  Average Frame Rate : %4.2f (fps)\n", numFramesToEncode * 1000.0f / total_encode_time[encoderID]);

        sdkDeleteTimer(&timer[encoderID]);

		// Release the resources grabbed by the Encoder
        pEncoder[encoderID]->DestroyEncoder();

        if (fOutput[encoderID])
        {
            fclose(fOutput[encoderID]);

            char output_filename[256];
            strcpy( output_filename, output_filename_strings[encoderID].c_str() );

            fOutput[encoderID] = fopen(output_filename, "rb");
			if ( fOutput[encoderID] == NULL ) {
				pprintf("  ERROR, unable to re-open encoded output (output_filename: %s)\n", output_filename );
				continue; // skip to next encoder
			}

//          fseek(fOutput[encoderID], 0, SEEK_END);
            int rc = _fseeki64(fOutput[encoderID], 0, SEEK_END);
			if ( fOutput[encoderID] == NULL ) {
				pprintf("  ERROR, unable to seek to end of file (output_filename: %s)\n", output_filename );
				continue; // skip to next encoder
			}

            __int64 file_size = _ftelli64(fOutput[encoderID]);
            fclose(fOutput[encoderID]);
            printf("  OutputFile[%d] <%s>\n", encoderID, output_filename);
            printf("  Filesize[%d] = %lld\n", encoderID, file_size);
            printf("  Average Bitrate[%d] (%4.2f seconds) %4.3f (Kbps)\n", encoderID,
                    (double)numFramesToEncode / ((double)nvEncoderConfig[encoderID].frameRateNum / (double)nvEncoderConfig[encoderID].frameRateDen), 
                    (double)file_size*(8.0/1024.0) * (double)nvEncoderConfig[encoderID].frameRateNum / ((double)numFramesToEncode * (double)nvEncoderConfig[encoderID].frameRateDen) );
        }
    }

//  nvCloseFile( hInput );

    for (unsigned int i = 0; i < 3; i++)
    {
        if (yuv[i]) {
            delete [] yuv[i];
            yuv[i] = NULL;
        }
    }
/*
    for (unsigned int encoderID=0; encoderID < numEncoders; encoderID++) 
    {
        char output_filename[256];
        sprintf(output_filename, "%s.gpu%d.%s", nvAppEncoderParams.output_base_file, encoderID, nvAppEncoderParams.output_base_ext);
        printf("\nNVENC completed encoding H.264 video, saved as <%s> \n", output_filename);
    }
*/
    for (unsigned int i=0; i < numEncoders; i++)
    {
        if (pEncoder[i])
        {
            delete pEncoder[i];
            pEncoder[i] = NULL;
        }
    }

    return 0;
}
