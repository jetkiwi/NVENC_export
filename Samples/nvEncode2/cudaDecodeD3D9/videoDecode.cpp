/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

/* This example demonstrates how to use the Video Decode Library with CUDA
 * bindings to interop between CUDA and DX9 textures for the purpose of post
 * processing video.
 */

#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#endif
//#include <d3dx9.h>

#include "VideoDecode.h"

#include <cuda.h>
//#include <cudad3d9.h>
#include <builtin_types.h>

// CUDA utilities and system includes
#include <helper_functions.h>
#include <helper_cuda_drvapi.h>    // helper file for CUDA Driver API calls and error checking

// cudaDecodeD3D9 related helper functions
#include "FrameQueue.h"
#include "VideoSource.h"
#include "VideoParser.h"
#include "VideoDecoder.h"

#include "cudaProcessFrame.h"
#include "cudaModuleMgr.h"

// Include files
#include <math.h>
#include <memory>
#include <iostream>
#include <cassert>

#define VIDEO_SOURCE_FILE "plush1_720p_10s.m2v"
//#define VIDEO_SOURCE_FILE "D:\\shared\\heat_bd_rip\\bank.m2ts"



////////////////////////////////////////////////////////////////////////////////////////////

videoDecode::videoDecode(const int argc, char *argv[] ) {

    sAppName     = "CUDA Video Decode";
    sAppFilename = "cudaDecode";
    sSDKname     = "cudaDecode";

    m_frame_timer  = NULL;
    m_global_timer = NULL;

    m_DeviceID    = 0;
    m_bWindowed   = true;
    m_bDeviceLost = false;
    m_bDone       = false;
    m_bRunning    = false;
    m_bAutoQuit   = false;
    m_bUseVsync   = false;
    m_bFrameRepeat= false;
    m_bFrameStep  = false;
    m_bQAReadback = false;
    m_bFirstFrame = true;
    m_bLoop       = false;
    m_bUpdateCSC  = true;
    m_bUpdateAll  = false;
    m_bUseDisplay = false; // this flag enables/disables video on the window
    m_bUseInterop = false;
    m_bReadback   = false; // this flag enables/disables reading back of a video from a window
    m_bIsProgressive = true; // assume it is progressive, unless otherwise noted
    m_bException  = false;
    m_bWaived     = false;

    m_iRepeatFactor = 1; // 1:1 assumes no frame repeats

    m_argc = argc;
    m_argv = (char **) new char*[m_argc];
    for (unsigned i = 0; i < m_argc; ++i )
        m_argv[i] = argv[i];

    m_eVideoCreateFlags = cudaVideoCreate_PreferCUVID;
    m_CtxLock = NULL;

    present_fps = decoded_fps = total_time = 0.0f;

    m_pD3DDevice = NULL;

    cuModNV12toARGB  = 0;
    gfpNV12toARGB    = 0;
    gfpPassthru      = 0;

    m_oContext = 0;
    m_oDevice  = 0;

    m_ReadbackSID = 0, m_KernelSID = 0;

    m_eColorSpace = ITU709;
    m_nHue        = 0.0f;

// System Memory surface we want to readback to
    for( unsigned i = 0; i < sizeof(m_bFrameData) / sizeof(m_bFrameData[0]); ++i )
        m_bFrameData[i] = NULL;
    m_pFrameQueue   = NULL;
    m_pVideoSource  = NULL;
    m_pVideoParser  = NULL;
    m_pVideoDecoder = NULL;

//ImageDX       *m_pImageDX      = 0;
    m_pImageDX      = NULL;
    for( unsigned i = 0; i < sizeof(m_pInteropFrame) / sizeof(m_pInteropFrame[0]); ++i )
        m_pInteropFrame[i] = NULL;

    m_nWindowWidth  = 0;
    m_nWindowHeight = 0;

    m_nClientAreaWidth  = 0;
    m_nClientAreaHeight = 0;

    m_nVideoWidth  = 0;
    m_nVideoHeight = 0;

    m_FrameCount = 0;
    m_DecodeFrameCount = 0;
    m_fpsCount = 0;      // FPS count for averaging
    m_fpsLimit = 16;     // FPS limit for sampling timer;

    sdkCreateTimer(&m_frame_timer);
    sdkResetTimer(&m_frame_timer);
    sdkCreateTimer(&m_global_timer);
    sdkResetTimer(&m_global_timer);
}

videoDecode::~videoDecode() {
    cleanup(true);

    delete m_argv;
}

bool videoDecode::Start() {
    if ( m_bRunning ) {
        my_printf("ERROR, videoDecode is already running!\n");
        return false;
    }

    m_pVideoSource->start();
    m_bRunning = true;

    sdkStartTimer(&m_frame_timer);
    sdkResetTimer(&m_frame_timer);

    sdkStartTimer(&m_global_timer);
    sdkResetTimer(&m_global_timer);

    return true;
}

bool videoDecode::checkHW(char *name, char *gpuType, int dev)
{
    char deviceName[256];
    checkCudaErrors(cuDeviceGetName(deviceName, 256, dev));

    strcpy(name, deviceName);

    if (!strnicmp(deviceName, gpuType, strlen(gpuType)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

int videoDecode::findGraphicsGPU(char *name)
{
    int nGraphicsGPU = 0;
    int deviceCount = 0;
    bool bFoundGraphics = false;
    char firstGraphicsName[256], temp[256];

    CUresult err = cuInit(0);
    checkCudaErrors(cuDeviceGetCount(&deviceCount));

    // This function call returns 0 if there are no CUDA capable devices.
    if (deviceCount == 0)
    {
        printf("> There are no device(s) supporting CUDA\n");
        return false;
    }
    else
    {
        printf("> Found %d CUDA Capable Device(s)\n", deviceCount);
    }

    for (int dev = 0; dev < deviceCount; ++dev)
    {
        bool bGraphics = !checkHW(temp, "Tesla", dev);
        printf("> %s\t\tGPU %d: %s\n", (bGraphics ? "Graphics" : "Compute"), dev, temp);

        if (bGraphics)
        {
            if (!bFoundGraphics)
            {
                strcpy(firstGraphicsName, temp);
            }

            nGraphicsGPU++;
        }
    }

    if (nGraphicsGPU)
    {
        strcpy(name, firstGraphicsName);
    }
    else
    {
        strcpy(name, "this hardware");
    }

    return nGraphicsGPU;
}

void videoDecode::printStatistics()
{
    int   hh, mm, ss, msec;

    present_fps = 1.f / (total_time / (m_FrameCount * 1000.f));
    decoded_fps = 1.f / (total_time / (m_DecodeFrameCount * 1000.f));

    msec = ((int)total_time % 1000);
    ss   = (int)(total_time/1000) % 60;
    mm   = (int)(total_time/(1000*60)) % 60;
    hh   = (int)(total_time/(1000*60*60)) % 60;

    printf("\n[%s] statistics\n", sSDKname);
    printf("\t Video Length (hh:mm:ss.msec)   = %02d:%02d:%02d.%03d\n", hh, mm, ss, msec);

    printf("\t Frames Presented (inc repeats) = %d\n", m_FrameCount);
    printf("\t Average Present FPS            = %4.2f\n", present_fps);

    printf("\t Frames Decoded   (hardware)    = %d\n", m_DecodeFrameCount);
    printf("\t Average Decoder FPS            = %4.2f\n", decoded_fps);

}

void videoDecode::computeFPS(HWND hWnd)
{
    sdkStopTimer(&m_frame_timer);

    if (m_bRunning)
    {
        m_fpsCount++;

        if (!m_pFrameQueue->isEndOfDecode())
        {
            m_FrameCount++;
        }
    }

    char sFPS[256];
    std::string sDecodeStatus;

    if (m_bDeviceLost)
    {
        sDecodeStatus = "DeviceLost!\0";
        sprintf(sFPS, "%s [%s] - [%s %d]",
                sAppName, sDecodeStatus.c_str(),
                (m_bIsProgressive ? "Frame" : "Field"),
                m_DecodeFrameCount);

        if (m_bUseInterop && (!m_bQAReadback))
        {
            SetWindowText(hWnd, sFPS);
            UpdateWindow(hWnd);
        }

        sdkResetTimer(&m_frame_timer);
        m_fpsCount = 0;
        return;
    }

    if (m_pFrameQueue->isEndOfDecode())
    {
        sDecodeStatus = "STOP (End of File)\0";

        // we only want to record this once
        if (total_time == 0.0f)
        {
            total_time = sdkGetTimerValue(&m_global_timer);
        }

        sdkStopTimer(&m_global_timer);

        if (m_bAutoQuit)
        {
            m_bRunning = false;
            m_bDone    = true;
        }
    }
    else
    {
        if (!m_bRunning)
        {
            sDecodeStatus = "PAUSE\0";
            sprintf(sFPS, "%s [%s] - [%s %d] - Video Display %s / Vsync %s",
                    sAppName, sDecodeStatus.c_str(),
                    (m_bIsProgressive ? "Frame" : "Field"), m_DecodeFrameCount,
                    m_bUseDisplay ? "ON" : "OFF",
                    m_bUseVsync   ? "ON" : "OFF");

            if (m_bUseInterop && (!m_bQAReadback))
            {
                SetWindowText(hWnd, sFPS);
                UpdateWindow(hWnd);
            }
        }
        else
        {
            if (m_bFrameStep)
            {
                sDecodeStatus = "STEP\0";
            }
            else
            {
                sDecodeStatus = "PLAY\0";
            }
        }

        if (m_fpsCount == m_fpsLimit)
        {
            float ifps = 1.f / (sdkGetAverageTimerValue(&m_frame_timer) / 1000.f);

            sprintf(sFPS, "[%s] [%s] - [%3.1f fps, %s %d] - Video Display %s / Vsync %s",
                    sAppName, sDecodeStatus.c_str(), ifps,
                    (m_bIsProgressive ? "Frame" : "Field"), m_DecodeFrameCount,
                    m_bUseDisplay ? "ON" : "OFF",
                    m_bUseVsync   ? "ON" : "OFF");

            if (m_bUseInterop && (!m_bQAReadback))
            {
                SetWindowText(hWnd, sFPS);
                UpdateWindow(hWnd);
            }

            printf("[%s] - [%s: %04d, %04.1f fps, time: %04.2f (ms) ]\n",
                   sSDKname, (m_bIsProgressive ? "Frame" : "Field"), m_FrameCount, ifps, 1000.f/ifps);

            sdkResetTimer(&m_frame_timer);
            m_fpsCount = 0;
        }
    }

    sdkStartTimer(&m_frame_timer);
}

HRESULT videoDecode::initCudaResources(int bUseInterop, int bTCC, CUdevice *pcudevice, CUcontext *pcucontext)
{
    HRESULT hr = S_OK;
    
    // Initialize CUDA
    cuInit(0);

    CUdevice cuda_device;
    m_bUseInterop = bUseInterop;
    
if ( pcudevice == NULL ) {
    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "device"))
    {
        cuda_device = getCmdLineArgumentInt(m_argc, (const char **) m_argv, "device");

        // If interop is disabled, then we need to create a CUDA context w/o the GL context
        if (m_bUseInterop && !bTCC)
        {
            cuda_device = findCudaDeviceDRV(m_argc, (const char **)m_argv);
        }
        else
        {
            cuda_device = findCudaGLDeviceDRV(m_argc, (const char **)m_argv);
        }

        if (cuda_device < 0)
        {
            printf("No CUDA Capable devices found, exiting...\n");
            exit(EXIT_SUCCESS);
        }

        checkCudaErrors(cuDeviceGet(&m_oDevice, cuda_device));
    }
    else
    {
        // If we want to use Graphics Interop, then choose the GPU that is capable
        if (m_bUseInterop)
        {
            cuda_device = gpuGetMaxGflopsGLDeviceIdDRV();
            checkCudaErrors(cuDeviceGet(&m_oDevice, cuda_device));
        }
        else
        {
            cuda_device = gpuGetMaxGflopsDeviceIdDRV();
            checkCudaErrors(cuDeviceGet(&m_oDevice, cuda_device));
        }
    }
} // if ( pcudevice == NULL )
else {
    m_oDevice = *pcudevice;// use caller-supplied CudaDeviceID
}

    // get compute capabilities and the devicename
    int major, minor;
    size_t totalGlobalMem;
    char deviceName[256];
    checkCudaErrors(cuDeviceComputeCapability(&major, &minor, m_oDevice));
    checkCudaErrors(cuDeviceGetName(deviceName, 256, m_oDevice));
    printf("> Using GPU Device %d: %s has SM %d.%d compute capability\n", cuda_device, deviceName, major, minor);

    checkCudaErrors(cuDeviceTotalMem(&totalGlobalMem, m_oDevice));
    printf("  Total amount of global memory:     %4.4f MB\n", (float)totalGlobalMem/(1024*1024));

    // Create CUDA Device w/ D3D9 interop (if WDDM), otherwise CUDA w/o interop (if TCC)
    // (use CU_CTX_BLOCKING_SYNC for better CPU synchronization)
    if (m_bUseInterop)
    {
        //checkCudaErrors(cuD3D9CtxCreate(&m_oContext, &m_oDevice, CU_CTX_BLOCKING_SYNC, m_pD3DDevice));
    }
    else
    {
        if ( pcucontext ) {
            m_oContext = *pcucontext; // use caller supplied context
            checkCudaErrors(cuCtxPushCurrent(m_oContext));
        } else
            checkCudaErrors(cuCtxCreate(&m_oContext, CU_CTX_BLOCKING_SYNC, m_oDevice)); // get new context
    }

    try
    {
        // Initialize CUDA releated Driver API (32-bit or 64-bit), depending the platform running
        if (sizeof(void *) == 4)
        {
            m_pCudaModule = NULL;//new CUmoduleManager("NV12ToARGB_drvapi_Win32.ptx", exec_path, 2, 2, 2);
        }
        else
        {
            m_pCudaModule = NULL;//new CUmoduleManager("NV12ToARGB_drvapi_x64.ptx", exec_path, 2, 2, 2);
        }
    }
    catch (char const *p_file)
    {
        // If the CUmoduleManager constructor fails to load the PTX file, it will throw an exception
        printf("\n>> CUmoduleManager::Exception!  %s not found!\n", p_file);
        printf(">> Please rebuild NV12ToARGB_drvapi.cu or re-install this sample.\n");
        return E_FAIL;
    }

    //g_pCudaModule->GetCudaFunction("NV12ToARGB_drvapi",   &gfpNV12toARGB);
    //g_pCudaModule->GetCudaFunction("Passthru_drvapi",     &gfpPassthru);

    /////////////////Change///////////////////////////
    // Now we create the CUDA resources and the CUDA decoder context
    initCudaVideo();

    if (m_bUseInterop)
    {
        my_printf("skipping initD3D9Surface()\n");

        //initD3D9Surface(m_pVideoDecoder->targetWidth(),
//                        m_pVideoDecoder->targetHeight());
    }
    else
    {
        checkCudaErrors(cuMemAlloc(&m_pInteropFrame[0], m_pVideoDecoder->targetWidth() * m_pVideoDecoder->targetHeight() * 2));
        checkCudaErrors(cuMemAlloc(&m_pInteropFrame[1], m_pVideoDecoder->targetWidth() * m_pVideoDecoder->targetHeight() * 2));
    }

    CUcontext cuCurrent = NULL;
    CUresult result = cuCtxPopCurrent(&cuCurrent);

    if (result != CUDA_SUCCESS)
    {
        printf("cuCtxPopCurrent: %d\n", result);
        assert(0);
    }

    /////////////////////////////////////////
    //return ((m_pCudaModule && m_pVideoDecoder && (m_pImageDX || m_pInteropFrame[0])) ? S_OK : E_FAIL);
    return ((m_pVideoDecoder && (m_pImageDX || m_pInteropFrame[0])) ? S_OK : E_FAIL);
}

HRESULT videoDecode::reinitCudaResources()
{
    // Free resources
    cleanup(false);

    CUVIDEOFORMAT fmt;
    std::auto_ptr<FrameQueue> apFrameQueue(new FrameQueue);
    std::auto_ptr<VideoSource> apVideoSource(new VideoSource(m_sFileName, apFrameQueue.get()));


    // Reinit VideoSource and Frame Queue
    m_bIsProgressive = loadVideoSource( &fmt );

    apVideoSource->getSourceDimensions(m_nVideoWidth, m_nVideoHeight);
    apVideoSource->getSourceDimensions(m_nWindowWidth, m_nWindowHeight);

    /////////////////Change///////////////////////////
    initCudaVideo();
    /////////////////////////////////////////

    return S_OK;
}

void videoDecode::displayHelp()
{
    printf("\n");
    printf("%s - Help\n\n", sAppName);
    printf("  %s [parameters] [video_file]\n\n", sAppFilename);
    printf("Program parameters:\n");
    printf("\t-decodecuda   - Use CUDA for MPEG-2 (Available with 64+ CUDA cores)\n");
    printf("\t-decodedxva   - Use VP for MPEG-2, VC-1, H.264 decode.\n");
    printf("\t-decodecuvid  - Use VP for MPEG-2, VC-1, H.264 decode (optimized)\n");
    printf("\t-vsync        - Enable vertical sync.\n");
    printf("\t-novsync      - Disable vertical sync.\n");
    printf("\t-repeatframe  - Enable frame repeats.\n");
    printf("\t-updateall    - always update CSC matrices.\n");
    printf("\t-displayvideo - display video frames on the window\n");
    printf("\t-nointerop    - create the CUDA context w/o using graphics interop\n");
    printf("\t-readback     - enable readback of frames to system memory\n");
    printf("\t-device=n     - choose a specific GPU device to decode video with\n");
}

void videoDecode::parseCommandLineArguments()
{
    char *video_file;

    printf("Command Line Arguments:\n");

    for (int n=0; n < m_argc; n++)
    {
        printf("m_argv[%d] = %s\n", n, m_argv[n]);
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "help"))
    {
        displayHelp();
        exit(EXIT_SUCCESS);
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "decodecuda"))
    {
        m_eVideoCreateFlags = cudaVideoCreate_PreferCUDA;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "decodedxva"))
    {
        printf("[%s]: option not supported!", "decodedxva");
        exit(EXIT_FAILURE);
        m_eVideoCreateFlags = cudaVideoCreate_PreferDXVA;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "decodecuvid"))
    {
        m_eVideoCreateFlags = cudaVideoCreate_PreferCUVID;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "vsync"))
    {
        printf("[%s]: option not supported!", "vsync" );
        exit(EXIT_FAILURE);
        m_bUseVsync = true;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "novsync"))
    {
        printf("[%s]: option not supported!", "novsync" );
        exit(EXIT_FAILURE);
        m_bUseVsync = false;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "repeatframe"))
    {
        m_bFrameRepeat = true;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "framestep"))
    {
        printf("[%s]: option not supported!", "framestep" );
        exit(EXIT_FAILURE);
        m_bFrameStep     = true;
        m_bUseDisplay    = true;
        m_bUseInterop    = true;
        m_fpsLimit = 1;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "updateall"))
    {
        m_bUpdateAll     = true;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "displayvideo"))
    {
        printf("[%s]: option not supported!", "displayvideo" );
        exit(EXIT_FAILURE);
        m_bUseDisplay   = true;
        m_bUseInterop   = true;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "nointerop"))
    {
        m_bUseInterop   = false;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "readback"))
    {
        m_bReadback     = true;
    }

    if (checkCmdLineFlag(m_argc, (const char **)m_argv, "device"))
    {
        m_DeviceID = getCmdLineArgumentInt(m_argc, (const char **)m_argv, "device");
        m_bUseDisplay    = true;
        m_bUseInterop    = true;
    }

    if (m_bUseDisplay == false)
    {
        m_bQAReadback    = true;
        m_bUseInterop    = false;
    }

    if (m_bLoop == false)
    {
        m_bAutoQuit = true;
    }

    // Search all command file parameters for video files with extensions:
    // mp4, avc, mkv, 264, h264. vc1, wmv, mp2, mpeg2, mpg
    char *file_ext = NULL;
    for (int i=1; i < m_argc; i++) 
    {
        if (getFileExtension(m_argv[i], &file_ext) > 0) 
        {
            video_file = m_argv[i];
            break;
        }
    }

    // We load the default video file for the SDK sample
    if (file_ext == NULL)
    {
        video_file = sdkFindFilePath(VIDEO_SOURCE_FILE, m_argv[0]);
    }

    // Now verify the video file is legit
    FILE *fp = fopen(video_file, "r");
    if (video_file == NULL && fp == NULL)
    {
        printf("[%s]: unable to find file: <%s>\nExiting...\n", sAppFilename, VIDEO_SOURCE_FILE);
        exit(EXIT_FAILURE);
    }
    if (fp)
    {
        fclose(fp);
    }

    // default video file loaded by this sample
    m_sFileName = video_file;

    // store the current path so we can reinit the CUDA context
    strcpy(exec_path, m_argv[0]);

    printf("[%s]: input file: <%s>\n", sAppFilename, m_sFileName.c_str());
}


bool
videoDecode::loadVideoSource( CUVIDEOFORMAT *fmt )
{
    std::auto_ptr<FrameQueue> apFrameQueue(new FrameQueue);
    std::auto_ptr<VideoSource> apVideoSource(new VideoSource(m_sFileName, apFrameQueue.get()));

    // retrieve the video source (width,height)
    apVideoSource->getSourceDimensions(m_nVideoWidth, m_nVideoHeight);
    apVideoSource->getSourceDimensions(m_nWindowWidth, m_nWindowHeight);

    *fmt = apVideoSource->format();

    std::cout << (*fmt) << std::endl;

    if (m_bFrameRepeat)
    {
        m_iRepeatFactor = (60.0f / ceil((float)apVideoSource->format().frame_rate.numerator / (float)apVideoSource->format().frame_rate.denominator));
        my_printf("Frame Rate Playback Speed = %d fps\n", 60 / m_iRepeatFactor);
    }

    m_pFrameQueue  = apFrameQueue.release();
    m_pVideoSource = apVideoSource.release();

    if (m_pVideoSource->format().codec == cudaVideoCodec_JPEG ||
        m_pVideoSource->format().codec == cudaVideoCodec_MPEG2)
    {
        m_eVideoCreateFlags = cudaVideoCreate_PreferCUDA;
    }

    bool IsProgressive = 0;
    m_pVideoSource->getProgressive(IsProgressive);
    m_bIsProgressive = IsProgressive;
    return IsProgressive;
}

void
videoDecode::initCudaVideo()
{
    // bind the context lock to the CUDA context
    CUresult result = cuvidCtxLockCreate(&m_CtxLock, m_oContext);

    if (result != CUDA_SUCCESS)
    {
        printf("cuvidCtxLockCreate failed: %d\n", result);
        assert(0);
    }

    std::auto_ptr<VideoDecoder> apVideoDecoder(new VideoDecoder(m_pVideoSource->format(), m_oContext, m_eVideoCreateFlags, m_CtxLock));
    std::auto_ptr<VideoParser> apVideoParser(new VideoParser(apVideoDecoder.get(), m_pFrameQueue));
    m_pVideoSource->setParser(*apVideoParser.get());

    m_pVideoParser  = apVideoParser.release();
    m_pVideoDecoder = apVideoDecoder.release();

    // Create a Stream ID for handling Readback
    if (m_bReadback)
    {
        checkCudaErrors(cuStreamCreate(&m_ReadbackSID, 0));
        checkCudaErrors(cuStreamCreate(&m_KernelSID,   0));
        my_printf("> initCudaVideo()\n");
        printf("  CUDA Streams (%s) <g_ReadbackSID = %p>\n", ((m_ReadbackSID == 0) ? "Disabled" : "Enabled"), m_ReadbackSID);
        printf("  CUDA Streams (%s) <g_KernelSID   = %p>\n", ((m_KernelSID   == 0) ? "Disabled" : "Enabled"), m_KernelSID);
    }
}


void
videoDecode::freeCudaResources(bool bDestroyContext)
{
    if (m_pVideoParser)
    {
        delete m_pVideoParser;
        m_pVideoParser = NULL;
    }

    if (m_pVideoDecoder)
    {
        delete m_pVideoDecoder;
        m_pVideoDecoder = NULL;
    }

    if (m_pVideoSource)
    {
        delete m_pVideoSource;
        m_pVideoSource = NULL;
    }

    if (m_pFrameQueue)
    {
        delete m_pFrameQueue;
        m_pFrameQueue = NULL;
    }

    if (m_CtxLock)
    {
        checkCudaErrors(cuvidCtxLockDestroy(m_CtxLock));
        m_CtxLock = NULL;
    }

    if (m_oContext && bDestroyContext)
    {
        checkCudaErrors(cuCtxDestroy(m_oContext));
        m_oContext = NULL;
    }

    if (m_ReadbackSID)
    {
        cuStreamDestroy(m_ReadbackSID);
        m_ReadbackSID = NULL;
    }

    if (m_KernelSID)
    {
        cuStreamDestroy(m_KernelSID);
        m_KernelSID = NULL;
    }
}

// Run the Cuda part of the computation
bool videoDecode::copyDecodedFrameToTexture(unsigned int &nRepeats, 
    CUVIDPICPARAMS *pDecodedPicInfo,   // decoded frame metainfo (I/B/P frame, etc.)
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[]       // handle - decoded framebuffer (or fields)
)
{

    if (m_pFrameQueue->dequeue(pDisplayInfo,pDecodedPicInfo))
    {
        CCtxAutoLock lck(m_CtxLock);
        // Push the current CUDA context (only if we are using CUDA decoding path)
        CUresult result = cuCtxPushCurrent(m_oContext);

        pDecodedFrame[0] = 0;
        pDecodedFrame[1] = 0;
        CUdeviceptr  pInteropFrame[2] = { 0, 0 };

        int num_fields = (pDisplayInfo->progressive_frame ? (1) : (2+pDisplayInfo->repeat_first_field));
        m_bIsProgressive = pDisplayInfo->progressive_frame ? true : false;

        for (int active_field=0; active_field<num_fields; active_field++)
        {
            nRepeats = pDisplayInfo->repeat_first_field;
            CUVIDPROCPARAMS oVideoProcessingParameters;
            memset(&oVideoProcessingParameters, 0, sizeof(CUVIDPROCPARAMS));

            oVideoProcessingParameters.progressive_frame = pDisplayInfo->progressive_frame;
            oVideoProcessingParameters.second_field      = active_field;
            oVideoProcessingParameters.top_field_first   = pDisplayInfo->top_field_first;
            oVideoProcessingParameters.unpaired_field    = (num_fields == 1);

            unsigned int nDecodedPitch = 0;
            unsigned int nWidth = 0;
            unsigned int nHeight = 0;

            // map decoded video frame to CUDA surfae
            m_pVideoDecoder->mapFrame(pDisplayInfo->picture_index, &pDecodedFrame[active_field], &nDecodedPitch, &oVideoProcessingParameters);
            nWidth  = m_pVideoDecoder->targetWidth();
            nHeight = m_pVideoDecoder->targetHeight();
            // map DirectX texture to CUDA surface
            size_t nTexturePitch = 0;

            // If we are Encoding and this is the 1st Frame, we make sure we allocate system memory for readbacks
/*
            if (m_bReadback && m_bFirstFrame && m_ReadbackSID)
            {
                CUresult result;
                checkCudaErrors(result = cuMemAllocHost((void **)&m_bFrameData[0], (nDecodedPitch * nHeight * 3 / 2)));
                checkCudaErrors(result = cuMemAllocHost((void **)&m_bFrameData[1], (nDecodedPitch * nHeight * 3 / 2)));
                m_bFirstFrame = false;

                if (result != CUDA_SUCCESS)
                {
                    printf("cuMemAllocHost returned %d\n", (int)result);
                }
            }

            // If streams are enabled, we can perform the readback to the host while the kernel is executing
            if (m_bReadback && m_ReadbackSID)
            {
                CUresult result = cuMemcpyDtoHAsync(m_bFrameData[active_field], pDecodedFrame[active_field], (nDecodedPitch * nHeight * 3 / 2), m_ReadbackSID);

                if (result != CUDA_SUCCESS)
                {
                    printf("cuMemAllocHost returned %d\n", (int)result);
                }
            }
*/
#if ENABLE_DEBUG_OUT
            printf("%02d ", m_DecodeFrameCount );

            printf("%s", pDisplayInfo->progressive_frame ? "Frame " : "Field");
            if ( pDisplayInfo->progressive_frame )
                printf( pDecodedPicInfo->second_field ? "+" : " ");

            printf( "nNumSlices=%4u", pDecodedPicInfo->nNumSlices );
            printf(" PicIndex = %02d, OutputPTS = %08d ",
                   pDisplayInfo->picture_index, pDisplayInfo->timestamp);

            if ( pDecodedPicInfo->intra_pic_flag && pDecodedPicInfo->ref_pic_flag )
                printf( "I" );
            else if ( pDecodedPicInfo->intra_pic_flag )
                printf( " i" );
            else if ( pDecodedPicInfo->ref_pic_flag )
                printf( " P" );
            else
                printf( "  B" );

            printf(" [%0u]", pDecodedPicInfo->CurrPicIdx );
            if ( pDecodedPicInfo->CurrPicIdx < 0 )
                printf(" !BAD!"); // uh oh, bad entry
            printf("\n");
#endif

            if (m_pImageDX)
            {
                // map the texture surface
                //g_pImageDX->map(&pInteropFrame[active_field], &nTexturePitch, active_field);
                printf("ERROR, m_pImageDx not supported!\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                pInteropFrame[active_field] = m_pInteropFrame[active_field];
                nTexturePitch = m_pVideoDecoder->targetWidth() * 2;
            }

            // perform post processing on the CUDA surface (performs colors space conversion and post processing)
            // comment this out if we inclue the line of code seen above

//            cudaPostProcessFrame(&pDecodedFrame[active_field], nDecodedPitch, &pInteropFrame[active_field], nTexturePitch, m_pCudaModule->getModule(), gfpNV12toARGB, m_KernelSID);

            if (m_pImageDX)
            {
                // unmap the texture surface
                //g_pImageDX->unmap(active_field);
                my_printf("ERROR, m_pImageDx not supported!\n");
                exit(EXIT_FAILURE);

            }

            // unmap video frame
            // unmapFrame() synchronizes with the VideoDecode API (ensures the frame has finished decoding)
            // note, the actions below must now be done by the aller

            //m_pVideoDecoder->unmapFrame(pDecodedFrame[active_field]);
            // release the frame, so it can be re-used in decoder
            //m_pFrameQueue->releaseFrame(pDisplayInfo);
            m_DecodeFrameCount++;
        }

        // Detach from the Current thread
        checkCudaErrors(cuCtxPopCurrent(NULL));
    }
    else
    {
        return false;
    }



    return true;
}


// Launches the CUDA kernels to fill in the texture data
bool videoDecode::renderVideoFrame(HWND hWnd,
    CUVIDPICPARAMS *pDecodedPicInfo,   // decoded frame metainfo (I/B/P frame, etc.)
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[]       // handle - decoded framebuffer (or fields))
) {
    static unsigned int nRepeatFrame = 0;
    int repeatFactor = m_iRepeatFactor;
    int bFPSComputed = 0;
    bool bFramesDecoded = false;

    m_bIsProgressive = 1;
    if (0 != m_pFrameQueue)
    {
        // if not running, we simply don't copy
        // new frames from the decoder
        if (!m_bDeviceLost && m_bRunning)
        {
            bFramesDecoded = copyDecodedFrameToTexture(nRepeatFrame, pDecodedPicInfo, pDisplayInfo, pDecodedFrame);
        }
    }
    else
    {
        return false;
    }

    if (bFramesDecoded)
    {
        while (repeatFactor-- > 0)
        {
            // draw the scene using the copied textures
            if (m_bUseDisplay && m_bUseInterop)
            {
                // We will always draw field/frame 0
                //drawScene(0);

                if (!repeatFactor)
                {
                    computeFPS(hWnd);
                }

                // If interlaced mode, then we will need to draw field 1 separately
                if (!m_bIsProgressive)
                {
                    //drawScene(1);

                    if (!repeatFactor)
                    {
                        computeFPS(hWnd);
                    }
                }

                bFPSComputed = 1;
            }
        }

        // Pass the Windows handle to show Frame Rate on the window title
        if (!bFPSComputed)
        {
            computeFPS(hWnd);
        }
    }

    if (bFramesDecoded && m_bFrameStep)
    {
        if (m_bRunning)
        {
            m_bRunning = false;
        }
    }

    return bFramesDecoded;
}


// Release all previously initd objects
HRESULT videoDecode::cleanup(bool bDestroyContext)
{
    if ( m_bRunning ) {
        m_pFrameQueue->endDecode();
        m_pVideoSource->stop();
        m_bRunning = false;
    }

    // Attach the CUDA Context (so we may properly free memroy)
    if (bDestroyContext)
    {
        checkCudaErrors(cuCtxPushCurrent(m_oContext));

        if (m_pInteropFrame[0])
        {
            checkCudaErrors(cuMemFree(m_pInteropFrame[0]));
        }

        if (m_pInteropFrame[1])
        {
            checkCudaErrors(cuMemFree(m_pInteropFrame[1]));
        }

        // Detach from the Current thread
        checkCudaErrors(cuCtxPopCurrent(NULL));
    }

    if (m_pImageDX)
    {
        delete m_pImageDX;
        m_pImageDX = NULL;
    }

    freeCudaResources(bDestroyContext);

    // destroy the D3D device
    //if (m_pD3DDevice)
    //{
    //    m_pD3DDevice->Release();
    //    m_pD3DDevice = NULL;
    //}


    return S_OK;
}

//    if (!m_bUseInterop)
bool videoDecode::GetFrame(
    bool *got_frame,
    CUVIDPICPARAMS *pDecodedPicInfo,   // decoded frame metainfo (I/B/P frame, etc.)
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[]       // handle - decoded framebuffer (or fields)
)
{
    bool new_frame = false;
    *got_frame = false;
    bool isStarted = m_pVideoSource->isStarted();
    bool isEnd     = m_pFrameQueue->isEndOfDecode();
    // On this case we drive the display with a while loop (no openGL calls)
    if (isStarted && !isEnd)
    {
            //renderVideoFrame(hWnd, false);
            *got_frame = renderVideoFrame(NULL, pDecodedPicInfo, pDisplayInfo, pDecodedFrame);
            new_frame = true;
    }


    return new_frame;
}

bool 
videoDecode::GetFrameFinish( 
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[]       // handle - decoded framebuffer (or fields)
) { // decoded frame information (pic-index)
    int num_fields = (pDisplayInfo->progressive_frame ? (1) : (2+pDisplayInfo->repeat_first_field));

    for (int active_field=0; active_field<num_fields; active_field++)
    {
        m_pVideoDecoder->unmapFrame(pDecodedFrame[active_field]);
        // release the frame, so it can be re-used in decoder
        m_pFrameQueue->releaseFrame(pDisplayInfo);
    }

        // check if decoding has come to an end.
    // if yes, signal the app to shut down.
    if (!m_pVideoSource->isStarted() || m_pFrameQueue->isEndOfDecode())
    {
        // Let's free the Frame Data
        if (m_ReadbackSID && m_bFrameData)
        {
            cuMemFreeHost((void *)m_bFrameData[0]);
            cuMemFreeHost((void *)m_bFrameData[1]);
            m_bFrameData[0] = NULL;
            m_bFrameData[1] = NULL;
        }

        // Let's just stop, and allow the user to quit, so they can at least see the results
        m_pVideoSource->stop();

        // If we want to loop reload the video file and restart
        if (m_bLoop && !m_bAutoQuit)
        {
            reinitCudaResources();
            m_FrameCount = 0;
            m_DecodeFrameCount = 0;
            m_pVideoSource->start();
        }

        if (m_bAutoQuit)
        {
            m_bDone = true;
        }
    }
    return true;
}
