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

/**
* \file nvEncoder.cpp
* \brief nvEncoder is the Class interface for the Hardware Encoder (NV Encode API)
* \date 2011-2013
*  This file contains the implementation of the CNvEncoder class
*/

#include <cstdio>
#include <cstdlib>
#include <sstream>

#define my_printf(...) printf( "%0s(%0u):", __FILE__, __LINE__ ),printf(__VA_ARGS__)
#define delete_array(x) if ( x ) delete [] x, x = NULL

#if defined(WIN32) || defined(_WIN32) || defined(WIN64)
  #define NV_WINDOWS
#endif

#include <nvEncodeAPI.h>
#include <include/videoFormats.h>
#include "CNVEncoderH264.h"
#include "xcodeutil.h"

#include "guidutil2.h"

#if defined (NV_WINDOWS)
  #include <d3dx9.h>
  #include <include/dynlink_d3d10.h>
  #include <include/dynlink_d3d11.h>

  static char __NVEncodeLibName32[] = "nvEncodeAPI.dll";
  static char __NVEncodeLibName64[] = "nvEncodeAPI64.dll";
#elif defined __linux
  #include <dlfcn.h>
  static char __NVEncodeLibName[] = "libnvidia-encode.so";
#endif

#include <cuda.h>
#include <include/helper_cuda_drvapi.h>
#include <include/helper_nvenc.h>

#pragma warning (disable:4189)
#pragma warning (disable:4311)
#pragma warning (disable:4312)

#define MAKE_FOURCC( ch0, ch1, ch2, ch3 )                               \
                ( (unsigned int)(unsigned char)(ch0) | ( (unsigned int)(unsigned char)(ch1) << 8 ) |    \
                ( (unsigned int)(unsigned char)(ch2) << 16 ) | ( (unsigned int)(unsigned char)(ch3) << 24 ) )

inline BOOL Is64Bit()
{
    return (sizeof(void *)!=sizeof(DWORD));
}


CNvEncoder::CNvEncoder() :
    m_privateData(NULL), m_hEncoder(NULL), m_deviceID(0), m_dwEncodeGUIDCount(0), m_stEncodeGUIDArray(NULL), m_dwInputFmtCount(0), m_pAvailableSurfaceFmts(NULL),
    m_bEncoderInitialized(0), m_dwMaxSurfCount(0), m_dwCurrentSurfIdx(0), m_dwFrameWidth(0), m_dwFrameHeight(0),
    m_bEncodeAPIFound(false), m_pEncodeAPI(NULL), m_cuContext(NULL), m_pEncoderThread(NULL), m_bAsyncModeEncoding(true),
    m_fOutput(NULL), m_fInput(NULL), m_dwCodecProfileGUIDCount(0), m_stCodecProfileGUIDArray(NULL), 
	m_dwCodecPresetGUIDCount(0), m_stCodecPresetGUIDArray(NULL), m_uRefCount(0)
#if defined (NV_WINDOWS)
    , m_pD3D(NULL), m_pD3D9Device(NULL), m_pD3D10Device(NULL),  m_pD3D11Device(NULL)
#endif
{
	m_fwrite_callback        = NULL;
    m_dwInputFormat          = NV_ENC_BUFFER_FORMAT_NV12_TILED64x16;
    memset(&m_stInitEncParams,   0, sizeof(m_stInitEncParams));
    memset(&m_stEncoderInput,    0, sizeof(m_stEncoderInput));
    memset(&m_stInputSurface,    0, sizeof(m_stInputSurface));
    memset(&m_stBitstreamBuffer, 0, sizeof(m_stBitstreamBuffer));
    memset(&m_spspps, 0, sizeof(m_spspps));
    SET_VER(m_stInitEncParams, NV_ENC_INITIALIZE_PARAMS);
    memset(&m_stEncodeConfig, 0, sizeof(m_stEncodeConfig));
    SET_VER(m_stEncodeConfig, NV_ENC_CONFIG);

    memset(&m_stPresetConfig, 0, sizeof(NV_ENC_PRESET_CONFIG));
    SET_VER(m_stPresetConfig, NV_ENC_PRESET_CONFIG);
    SET_VER(m_stPresetConfig.presetCfg, NV_ENC_CONFIG);

	// all NVENC hardware at a minimum supports H264, so H264 is the default
	m_stEncodeGUID = NV_ENC_CODEC_H264_GUID;

    m_dwCodecProfileGUIDCount = 0;
    memset(&m_stCodecProfileGUID, 0, sizeof(GUID)) ;
    memset(&m_stPresetGUID, 0, sizeof(GUID));
	memset( (void *)&m_nv_enc_caps, 0, sizeof(m_nv_enc_caps) );
    
    m_pYUV[0] = m_pYUV[1] = m_pYUV[2] = NULL;

	m_useExternalContext = false;

    NVENCSTATUS nvStatus;
    MYPROC nvEncodeAPICreateInstance; // function pointer to create instance in nvEncodeAPI

#if defined (NV_WINDOWS)
    if (Is64Bit())
    {
        m_hinstLib = LoadLibrary(TEXT(__NVEncodeLibName64));
    }
    else
    {
        m_hinstLib = LoadLibrary(TEXT(__NVEncodeLibName32));
    }
#else
    m_hinstLib = dlopen(__NVEncodeLibName, RTLD_LAZY);
#endif

    if (m_hinstLib != NULL)
    {
#if defined (NV_WINDOWS)
        nvEncodeAPICreateInstance = (MYPROC) GetProcAddress(m_hinstLib, "NvEncodeAPICreateInstance");
#else
        nvEncodeAPICreateInstance = (MYPROC) dlsym(m_hinstLib, "NvEncodeAPICreateInstance");
#endif
        if (NULL != nvEncodeAPICreateInstance) 
        {
            m_pEncodeAPI = new NV_ENCODE_API_FUNCTION_LIST;
            if (m_pEncodeAPI)
            {
                memset(m_pEncodeAPI, 0, sizeof(NV_ENCODE_API_FUNCTION_LIST));
                m_pEncodeAPI->version = NV_ENCODE_API_FUNCTION_LIST_VER;
                nvStatus = nvEncodeAPICreateInstance(m_pEncodeAPI);
                m_bEncodeAPIFound = true;
            }
            else
            {
                m_bEncodeAPIFound = false;
            }
        }
        else 
        {
            PRINTERR(("CNvEncoder::CNvEncoder() failed to find NvEncodeAPICreateInstance"));
        }
    }
    else
    {
        m_bEncodeAPIFound = false;

#if defined (NV_WINDOWS)
        if (Is64Bit()) {
            PRINTERR(("CNvEncoder::CNvEncoder() failed to load %s!", __NVEncodeLibName64));
        } else {
            PRINTERR(("CNvEncoder::CNvEncoder() failed to load %s!", __NVEncodeLibName32));
        }
#else
        PRINTERR(("CNvEncoder::CNvEncoder() failed to load %s!", __NVEncodeLibName));
#endif
        throw((const char *)("CNvEncoder::CNvEncoder() was unable to load nvEncoder Library"));
    }
    memset(&m_stEOSOutputBfr, 0, sizeof(m_stEOSOutputBfr));
}


CNvEncoder::~CNvEncoder()
{
    // clean up encode API resources here
    if (m_hinstLib) {
#if defined (NV_WINDOWS)
        FreeLibrary(m_hinstLib); 
#else
        dlclose(m_hinstLib);
#endif
    }
	
	// clear out all dynamically allocated arrays
	m_dwEncodeGUIDCount = 0;
	delete_array(m_stEncodeGUIDArray);

	m_dwCodecProfileGUIDCount = 0;
	memset( (void *)&m_stCodecProfileGUID, 0, sizeof(m_stCodecProfileGUID));
	delete_array(m_stCodecProfileGUIDArray);
		
	m_dwInputFmtCount = 0;
	delete_array(m_pAvailableSurfaceFmts);

	m_dwCodecPresetGUIDCount = 0;
	delete_array(m_stCodecPresetGUIDArray);

	DestroyEncodeSession();

	if ( !m_useExternalContext )
		cuCtxDestroy( m_cuContext );
}

GUID CNvEncoder::GetCodecGUID(const NvEncodeCompressionStd codec) const
{
	switch (codec) {
		case NV_ENC_H264 : return NV_ENC_CODEC_H264_GUID;
			break;

		case NV_ENC_H265: return NV_ENC_CODEC_HEVC_GUID;
			break;

		default: return NV_ENC_PRESET_GUID_NULL; // unknown codec
	}
}

unsigned int CNvEncoder::GetCodecType(const GUID &encodeGUID) const
{
    NvEncodeCompressionStd eEncodeCompressionStd = NV_ENC_Unknown;
    if (compareGUIDs(encodeGUID, NV_ENC_CODEC_H264_GUID))
    {
        eEncodeCompressionStd = NV_ENC_H264;
    }
	else if (compareGUIDs(encodeGUID, NV_ENC_CODEC_HEVC_GUID)) {
		eEncodeCompressionStd = NV_ENC_H265;
	}
	else
    {
		printf(" unsupported codec GUID ");
		PrintGUID(encodeGUID);
		printf("\n");
    }
    return eEncodeCompressionStd;
}

unsigned int CNvEncoder::GetCodecProfile(const GUID &encodeProfileGUID) const
{
	guidutil::inttype value;
	//printf("CNvEncoder::GetCodecProfile() show supported codecs (MPEG-2, VC1, H264, etc.) \n");

	if ( desc_nv_enc_profile_names.guid2value( encodeProfileGUID, value) ) {
		// found it
		return value;
	}

/*
    if (compareGUIDs(encodeProfileGUID, NV_ENC_H264_PROFILE_BASELINE_GUID))
    {
        return 66;
    }
    else if(compareGUIDs(encodeProfileGUID, NV_ENC_H264_PROFILE_MAIN_GUID ))
    {
        return 77;
    }
    else if(compareGUIDs(encodeProfileGUID, NV_ENC_H264_PROFILE_HIGH_GUID ))
    {
        return 100;
    }
    else if(compareGUIDs(encodeProfileGUID, NV_ENC_H264_PROFILE_STEREO_GUID ))
    {
        return 128;
    }
    else
    {
        // unknown profile
        printf("CNvEncoder::GetCodecProfile is an unspecified GUID\n");
        return 0;
    }
*/
	printf("CNvEncoder::GetCodecProfile is an unspecified GUID: ");
	PrintGUID(encodeProfileGUID);
	printf("\n");
    return 0;
}

#if defined (NV_WINDOWS)
HRESULT CNvEncoder::InitD3D9(unsigned int deviceID)
{
    HRESULT hr = S_OK;
    D3DPRESENT_PARAMETERS d3dpp;

    unsigned int iAdapter    = NULL; // Our adapter

    // Create a Context for interfacing to DirectX9
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (m_pD3D == NULL)
    {
        assert(m_pD3D);
        return E_FAIL;
    }

    D3DADAPTER_IDENTIFIER9 adapterId;

    printf("\n* Detected %d available D3D9 Devices *\n", m_pD3D->GetAdapterCount());
    for(iAdapter = deviceID; iAdapter < m_pD3D->GetAdapterCount(); iAdapter++)  
    {
        HRESULT hr = m_pD3D->GetAdapterIdentifier(iAdapter, 0, &adapterId);  
        if (FAILED(hr)) continue; 
        printf("> Direct3D9 Display Device #%d: \"%s\"",  
            iAdapter, adapterId.Description);
        if (iAdapter == deviceID) {
            printf(" (selected)\n");
        } else {
            printf("\n");
        }
    }

    if (deviceID >= m_pD3D->GetAdapterCount()) {
        my_printf( "CNvEncoder::InitD3D() - deviceID=%d is outside range [%d,%d]\n", deviceID, 0, m_pD3D->GetAdapterCount() );
        return E_FAIL;
    }

    // Create the Direct3D9 device and the swap chain. In this example, the swap
    // chain is the same size as the current display mode. The format is RGB-32.
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = true;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferWidth  = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferCount  = 1;
    d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;//D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    DWORD dwBehaviorFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED | D3DCREATE_HARDWARE_VERTEXPROCESSING;

    hr = m_pD3D->CreateDevice(deviceID,
        D3DDEVTYPE_HAL,
        NULL,
        dwBehaviorFlags,
        &d3dpp,
        &m_pD3D9Device);

    return hr;
}

HRESULT CNvEncoder::InitD3D10(unsigned int deviceID)
{
    // TODO
    HRESULT hr = S_OK;

    assert(0);

    bool bCheckD3D10 = dynlinkLoadD3D10API();
    // If D3D10 is not present, print an error message and then quit    if (!bCheckD3D10) {
    if (!bCheckD3D10)
    {
        printf("> nvEncoder did not detect a D3D10 device, exiting...\n");
        dynlinkUnloadD3D10API();
        return E_FAIL;
    }


    hr = sFnPtr_D3D10CreateDevice( NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, 
                       D3D10_SDK_VERSION, &m_pD3D10Device ); 

    return hr;
}


HRESULT CNvEncoder::InitD3D11(unsigned int deviceID)
{
    // TODO
    HRESULT hr = S_OK;

    assert(0);

    bool bCheckD3D11 = dynlinkLoadD3D11API();
    // If D3D10 is not present, print an error message and then quit    if (!bCheckD3D10) {
    if (!bCheckD3D11)
    {
        printf("> nvEncoder did not detect a D3D11 device, exiting...\n");
        dynlinkUnloadD3D11API();
        return E_FAIL;
    }

    hr = sFnPtr_D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 
                            0, D3D11_SDK_VERSION, &m_pD3D11Device, NULL, NULL);
    return hr;
}
#endif

HRESULT CNvEncoder::InitCuda(unsigned int deviceID)
{
    CUresult        cuResult            = CUDA_SUCCESS;
    CUdevice        cuDevice            = 0;
    CUcontext       cuContextCurr;
    char gpu_name[100];
    int  deviceCount = 0;
    int  SMminor = 0, SMmajor = 0;

    printf("\n");

	// If encoder is configured to use an externally supplied Cuda-context, then this
	// function should never be called
	if ( m_useExternalContext )
		return E_FAIL;

    // CUDA interfaces
    cuResult = cuInit(0);
    if (cuResult != CUDA_SUCCESS) {
        my_printf(">> InitCUDA() - cuInit() failed error:0x%x\n", cuResult);
        return E_FAIL;
    }

    checkCudaErrors(cuDeviceGetCount(&deviceCount));
    if (deviceCount == 0) {
        my_printf( ">> InitCuda() - reports no devices available that support CUDA\n");
        exit(EXIT_FAILURE);
    } else {
        my_printf(">> InitCUDA() has detected %d CUDA capable GPU device(s)<<\n", deviceCount);
        for (int currentDevice=0; currentDevice < deviceCount; currentDevice++) {
            checkCudaErrors(cuDeviceGet(&cuDevice, currentDevice));
            checkCudaErrors(cuDeviceGetName(gpu_name, 100, cuDevice));
            checkCudaErrors(cuDeviceComputeCapability(&SMmajor, &SMminor, currentDevice));
            my_printf("  [ GPU #%d - < %s > has Compute SM %d.%d, %s NVENC ]\n", 
                            currentDevice, gpu_name, SMmajor, SMminor, 
                            (((SMmajor << 4) + SMminor) >= 0x30) ? "Available" : "Not Available");
        }
    }

    // If dev is negative value, we clamp to 0
    if (deviceID < 0) 
        deviceID = 0;
    if (deviceID > (unsigned int)deviceCount-1) {
        fprintf(stderr, ">> InitCUDA() - nvEncoder (-device=%d) is not a valid GPU device. <<\n\n", deviceID);
        exit(EXIT_FAILURE);
    }

    // Now we get the actual device
    checkCudaErrors(cuDeviceGet(&cuDevice, deviceID));
    checkCudaErrors(cuDeviceGetName(gpu_name, 100, cuDevice));
    checkCudaErrors(cuDeviceComputeCapability(&SMmajor, &SMminor, deviceID));
    printf("\n>> Select GPU #%d - < %s > supports SM %d.%d and NVENC\n", deviceID, gpu_name, SMmajor, SMminor);

    if (((SMmajor << 4) + SMminor) < 0x30) {
        my_printf("  [ GPU %d does not have NVENC capabilities] exiting\n", deviceID);
        exit(EXIT_FAILURE);
    }

	m_deviceID = deviceID;

	// If a context already exists, destroy it (to free resources) before creating the new one.
	// (if we fail to do this, memory-leak!)
	if (m_cuContext)
    {
		my_printf("InitCuda(): old m_cuContext exists, deleting it...\n");
        cuResult = cuCtxDestroy(m_cuContext);
        if (cuResult != CUDA_SUCCESS) 
            my_printf("InitCuda(): cuCtxDestroy error:0x%x\n", cuResult);
		m_cuContext = NULL;
    }

    // Create the CUDA Context and Pop the current one
    checkCudaErrors(cuCtxCreate(&m_cuContext, 0, cuDevice));
    checkCudaErrors(cuCtxPopCurrent(&cuContextCurr));

    return S_OK;
}


HRESULT CNvEncoder::AllocateIOBuffers(unsigned int dwInputWidth, unsigned int dwInputHeight, unsigned int maxFrmCnt)
{
    m_dwMaxSurfCount = maxFrmCnt;
    NVENCSTATUS status = NV_ENC_SUCCESS;

    printf(" > CNvEncoder::AllocateIOBuffers() = Size (%dx%d @ %d frames)\n", dwInputWidth, dwInputHeight, maxFrmCnt);
    for (unsigned int i = 0; i < m_dwMaxSurfCount; i++)
    {
        m_stInputSurface[i].dwWidth  = dwInputWidth;
        m_stInputSurface[i].dwHeight = dwInputHeight;
        if (m_stEncoderInput.useMappedResources)
        {
            if (m_stEncoderInput.interfaceType == NV_ENC_CUDA)
            {
                if (i==0) {
                    printf(" > CUDA+NVENC InterOp using %d buffers.\n", m_dwMaxSurfCount);
                }
                // Illustrate how to use a Cuda buffer not allocated using NvEncCreateInputBuffer as input to the encoder.
                cuCtxPushCurrent(m_cuContext);
                CUcontext   cuContextCurr;
                CUdeviceptr devPtrDevice;
                CUresult    result = CUDA_SUCCESS;                          

				// For each buffer, allocate a host-memory space and a device-memory space.

                // (1) Allocate Cuda buffer. We will use this to hold the input YUV data.
				unsigned row_count;
				if ( m_stEncoderInput.chromaFormatIDC == cudaVideoChromaFormat_420 ) {
					m_stInputSurface[i].bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;
					row_count = dwInputHeight*3/2; // enough rows for NV12 frame
				}
				else {
					// YUV 4:4:4
					m_stInputSurface[i].bufferFmt      = NV_ENC_BUFFER_FORMAT_YUV444_PL;
					row_count = dwInputHeight*3; // enough rows for YUV444 frame
				}
                result = cuMemAllocPitch(&devPtrDevice, (size_t *)&m_stInputSurface[i].dwCuPitch, dwInputWidth, row_count, 16);
                m_stInputSurface[i].pExtAlloc      = (void*)devPtrDevice;
				cuMemsetD8( devPtrDevice, 128, m_stInputSurface[i].dwCuPitch*row_count);// clear the memory

                // (2) Allocate Cuda buffer in host memory. We will use this to load data onto the Cuda buffer we want to use as input.
                result = cuMemAllocHost((void**)&m_stInputSurface[i].pExtAllocHost, m_stInputSurface[i].dwCuPitch*row_count);

                m_stInputSurface[i].type           = NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
				memset( (void *)m_stInputSurface[i].pExtAllocHost, 128, m_stInputSurface[i].dwCuPitch*row_count);// clear the memory

                cuCtxPopCurrent(&cuContextCurr);
            }
#if defined(NV_WINDOWS)
            if (m_stEncoderInput.interfaceType == NV_ENC_DX9)
            {
                if (i==0) {
                    printf(" > DirectX+NVENC InterOp using %d buffers.\n", m_dwMaxSurfCount);
                }
                // Illustrate how to use an externally allocated IDirect3DSurface9* as input to the encoder.
                IDirect3DSurface9 *pSurf = NULL;
                unsigned int dwFormat = MAKE_FOURCC('N', 'V', '1', '2');;
                HRESULT hr = S_OK;
                if (IsNV12Format(m_dwInputFormat))
                {
                    dwFormat = MAKE_FOURCC('N', 'V', '1', '2');
                }
                else if (IsYV12Format(m_dwInputFormat))
                {
                    dwFormat = MAKE_FOURCC('Y', 'V', '1', '2');
                }
                
                hr = m_pD3D9Device->CreateOffscreenPlainSurface(dwInputWidth, dwInputHeight, (D3DFORMAT)dwFormat, D3DPOOL_DEFAULT, (IDirect3DSurface9 **)&m_stInputSurface[i].pExtAlloc, NULL);
                m_stInputSurface[i].bufferFmt      = NV_ENC_BUFFER_FORMAT_NV12_PL;
                m_stInputSurface[i].type           = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
            }
#endif
            // Register the allocated buffer with NvEncodeAPI
            NV_ENC_REGISTER_RESOURCE stRegisterRes;
            memset(&stRegisterRes, 0, sizeof(NV_ENC_REGISTER_RESOURCE));
            SET_VER(stRegisterRes, NV_ENC_REGISTER_RESOURCE);
            stRegisterRes.resourceType = m_stInputSurface[i].type;
			stRegisterRes.bufferFormat = m_stInputSurface[i].bufferFmt;
            // Pass the resource handle to be registered and mapped during registration.
            // Do not pass this handle while mapping
            stRegisterRes.resourceToRegister       = m_stInputSurface[i].pExtAlloc;
            stRegisterRes.width                    = m_stInputSurface[i].dwWidth;
            stRegisterRes.height                   = m_stInputSurface[i].dwHeight;
            stRegisterRes.pitch                    = m_stInputSurface[i].dwCuPitch;
            
            status = m_pEncodeAPI->nvEncRegisterResource(m_hEncoder, &stRegisterRes);
            checkNVENCErrors(status);

            // Use this registered handle to retrieve an encoder-understandable mapped resource handle, through NvEncMapInputResource.
            // The mapped handle can be directly used with NvEncEncodePicture.
            m_stInputSurface[i].hRegisteredHandle = stRegisterRes.registeredResource;
        }
        else
        {
			// Premiere Plugin: allocate non-mapped resources
            if (i==0) {
                printf(" > System Memory with %d buffers.\n", m_dwMaxSurfCount);
            }
            // Allocate input surface
            NV_ENC_CREATE_INPUT_BUFFER stAllocInputSurface;
            memset(&stAllocInputSurface, 0, sizeof(stAllocInputSurface));
            SET_VER(stAllocInputSurface, NV_ENC_CREATE_INPUT_BUFFER);
            stAllocInputSurface.width              = (m_dwFrameWidth  + 31)&~31;//dwFrameWidth;
            stAllocInputSurface.height             = (m_dwFrameHeight + 31)&~31; //dwFrameHeight;
#if defined (NV_WINDOWS)
            stAllocInputSurface.memoryHeap         = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;
            stAllocInputSurface.bufferFmt          = NV_ENC_BUFFER_FORMAT_NV12_PL;
#else
            stAllocInputSurface.memoryHeap         = NV_ENC_MEMORY_HEAP_SYSMEM_UNCACHED;
            stAllocInputSurface.bufferFmt          = m_dwInputFormat;
#endif

			//
			// Strange, the host-buffer created must be either NV12_PL or YUV444_PL.
			// Attempting to create a NV12_Tiled* or YUV444_Tiled* buffer results in blank-output from NVENC.
			//
			if (m_stEncoderInput.chromaFormatIDC == cudaVideoChromaFormat_420)
				stAllocInputSurface.bufferFmt = NV_ENC_BUFFER_FORMAT_NV12_PL;
			else
				stAllocInputSurface.bufferFmt = NV_ENC_BUFFER_FORMAT_YUV444_PL;
//            else // assume it's YUV444 format
//                stAllocInputSurface.bufferFmt      = IsNV12Tiled64x16Format(m_dwInputFormat) ? NV_ENC_BUFFER_FORMAT_NV12_PL : m_dwInputFormat;

            status = m_pEncodeAPI->nvEncCreateInputBuffer(m_hEncoder, &stAllocInputSurface);
            checkNVENCErrors(status);

            m_stInputSurface[i].hInputSurface      = stAllocInputSurface.inputBuffer;
            m_stInputSurface[i].bufferFmt          = stAllocInputSurface.bufferFmt;
            m_stInputSurface[i].dwWidth            = (m_dwFrameWidth  + 31)&~31;
            m_stInputSurface[i].dwHeight           = (m_dwFrameHeight + 31)&~31;
        }

        m_stInputSurfQueue.Add(&m_stInputSurface[i]);

        //Allocate output surface
        m_stBitstreamBuffer[i].dwSize = 4*1024*1024;
        NV_ENC_CREATE_BITSTREAM_BUFFER stAllocBitstream;
        memset(&stAllocBitstream, 0, sizeof(stAllocBitstream));
        SET_VER(stAllocBitstream, NV_ENC_CREATE_BITSTREAM_BUFFER);
        stAllocBitstream.size                      =  m_stBitstreamBuffer[i].dwSize;
        stAllocBitstream.memoryHeap                = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;

        status = m_pEncodeAPI->nvEncCreateBitstreamBuffer(m_hEncoder, &stAllocBitstream);
        checkNVENCErrors(status);

        m_stBitstreamBuffer[i].hBitstreamBuffer    = stAllocBitstream.bitstreamBuffer;
        m_stBitstreamBuffer[i].pBitstreamBufferPtr = stAllocBitstream.bitstreamBufferPtr;

        // TODO : need to fix the ucode to set the bitstream position
        if (m_stEncoderInput.outBandSPSPPS == 0)
            m_stBitstreamBuffer[i].pBitstreamBufferPtr = NULL;

        NV_ENC_EVENT_PARAMS nvEventParams = {0};
        SET_VER(nvEventParams, NV_ENC_EVENT_PARAMS);

#if defined (NV_WINDOWS)
        m_stBitstreamBuffer[i].hOutputEvent        = CreateEvent(NULL, FALSE, FALSE, NULL);
        nvEventParams.completionEvent              = m_stBitstreamBuffer[i].hOutputEvent;
#else
        m_stBitstreamBuffer[i].hOutputEvent = NULL;
#endif
        // Register the resource for interop with NVENC
        nvEventParams.completionEvent              = m_stBitstreamBuffer[i].hOutputEvent;
        m_pEncodeAPI->nvEncRegisterAsyncEvent(m_hEncoder, &nvEventParams);

        m_stOutputSurfQueue.Add(&m_stBitstreamBuffer[i]);
    }

    m_stEOSOutputBfr.bEOSFlag = true;
    NV_ENC_EVENT_PARAMS nvEventParams = {0};
    SET_VER(nvEventParams, NV_ENC_EVENT_PARAMS);

#if defined (NV_WINDOWS)
    m_stEOSOutputBfr.hOutputEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    nvEventParams.completionEvent = m_stEOSOutputBfr.hOutputEvent;
#else
    m_stEOSOutputBfr.hOutputEvent = NULL;
#endif
    nvEventParams.completionEvent = m_stEOSOutputBfr.hOutputEvent;
    m_pEncodeAPI->nvEncRegisterAsyncEvent(m_hEncoder, &nvEventParams);

    return S_OK;
}


HRESULT CNvEncoder::ReleaseIOBuffers()
{
	if ( m_hEncoder == NULL )
		return S_OK;

    for (unsigned int i = 0; i < m_dwMaxSurfCount; i++)
    {
		m_pEncodeAPI->nvEncDestroyInputBuffer(m_hEncoder, m_stInputSurface[i].hInputSurface);
        m_stInputSurface[i].hInputSurface = NULL;

		// why is this done twice?
        //m_pEncodeAPI->nvEncDestroyInputBuffer(m_hEncoder, m_stInputSurface[i].hInputSurface);
        //m_stInputSurface[i].hInputSurface = NULL;

        if (m_stEncoderInput.useMappedResources)
        {
            // Unregister the registered resource handle before destroying the allocated Cuda buffer[s]
            m_pEncodeAPI->nvEncUnregisterResource(m_hEncoder, m_stInputSurface[i].hRegisteredHandle);
            if (m_stEncoderInput.interfaceType == NV_ENC_CUDA)
            {
                cuCtxPushCurrent(m_cuContext);
                CUcontext cuContextCurrent;
                cuMemFree((CUdeviceptr) m_stInputSurface[i].pExtAlloc);
                cuMemFreeHost(m_stInputSurface[i].pExtAllocHost);
                cuCtxPopCurrent(&cuContextCurrent);
            }
#if defined(NV_WINDOWS)
            if (m_stEncoderInput.interfaceType == NV_ENC_DX9)
            {
                IDirect3DSurface9 *pSurf = (IDirect3DSurface9 *)m_stInputSurface[i].pExtAlloc;
                if (pSurf)
                {
                    pSurf->Release();
                }
            }
#endif
			m_stInputSurface[i].pExtAlloc = NULL;
			m_stInputSurface[i].pExtAllocHost = NULL;
        }
        else
        {
            m_pEncodeAPI->nvEncDestroyInputBuffer(m_hEncoder, m_stInputSurface[i].hInputSurface);
            m_stInputSurface[i].hInputSurface = NULL;
        }
        m_pEncodeAPI->nvEncDestroyBitstreamBuffer(m_hEncoder, m_stBitstreamBuffer[i].hBitstreamBuffer);
        m_stBitstreamBuffer[i].hBitstreamBuffer = NULL;

        NV_ENC_EVENT_PARAMS nvEventParams = {0};
        SET_VER(nvEventParams, NV_ENC_EVENT_PARAMS);
        nvEventParams.completionEvent =  m_stBitstreamBuffer[i].hOutputEvent;
        m_pEncodeAPI->nvEncUnregisterAsyncEvent(m_hEncoder, &nvEventParams);

#if defined (NV_WINDOWS)
        CloseHandle(m_stBitstreamBuffer[i].hOutputEvent);
#endif
    }

    if (m_stEOSOutputBfr.hOutputEvent)
    {
        NV_ENC_EVENT_PARAMS nvEventParams = {0};
        SET_VER(nvEventParams, NV_ENC_EVENT_PARAMS);
        nvEventParams.completionEvent =  m_stEOSOutputBfr.hOutputEvent ;
        m_pEncodeAPI->nvEncUnregisterAsyncEvent(m_hEncoder, &nvEventParams);
#if defined (NV_WINDOWS)
        CloseHandle( m_stEOSOutputBfr.hOutputEvent );
#endif
        m_stEOSOutputBfr.hOutputEvent  = NULL;
    }
    return S_OK;
}


unsigned char* CNvEncoder::LockInputBuffer(void * hInputSurface, unsigned int *pLockedPitch)
{
    HRESULT hr = S_OK;
    NV_ENC_LOCK_INPUT_BUFFER stLockInputBuffer;
    memset(&stLockInputBuffer, 0, sizeof(stLockInputBuffer));
    SET_VER(stLockInputBuffer, NV_ENC_LOCK_INPUT_BUFFER);
    stLockInputBuffer.inputBuffer = hInputSurface;
    hr = m_pEncodeAPI->nvEncLockInputBuffer(m_hEncoder,&stLockInputBuffer);
    if ( hr != S_OK)
    {
        printf("\n unable to lock buffer");
    }
    *pLockedPitch = stLockInputBuffer.pitch;
    return (unsigned char*)stLockInputBuffer.bufferDataPtr;
}


HRESULT CNvEncoder::UnlockInputBuffer(void * hInputSurface)
{
    HRESULT hr = S_OK;
    m_pEncodeAPI->nvEncUnlockInputBuffer(m_hEncoder, hInputSurface);
    if ( hr != S_OK)
    {
        printf("\n unable to unlock buffer");
    }
    return hr;
}


HRESULT CNvEncoder::CopyBitstreamData(EncoderThreadData stThreadData)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    HRESULT hr = S_OK;

    if (stThreadData.pOutputBfr->hBitstreamBuffer == NULL && stThreadData.pOutputBfr->bEOSFlag == false)
    {
        return E_FAIL;
    }

    if (stThreadData.pOutputBfr->bDynResChangeFlag)
    {        
        NV_ENC_SEQUENCE_PARAM_PAYLOAD spsppsBuf = {0};
        char buf[256] = {0};
        unsigned int bufSz = 0;
        SET_VER(spsppsBuf, NV_ENC_SEQUENCE_PARAM_PAYLOAD);
        spsppsBuf.spsppsBuffer = (void *)(&buf);
        spsppsBuf.outSPSPPSPayloadSize = &bufSz;
        spsppsBuf.inBufferSize = 256*sizeof(char);

        nvStatus = m_pEncodeAPI->nvEncGetSequenceParams(m_hEncoder, &spsppsBuf);
        if (nvStatus == NV_ENC_SUCCESS)
        {
            (*m_fwrite_callback)(spsppsBuf.spsppsBuffer, sizeof(char), bufSz, m_fOutput, m_privateData);
        }
        nvStatus = NV_ENC_SUCCESS;
    }

    if (stThreadData.pOutputBfr->bWaitOnEvent == true)
    {
        if (!stThreadData.pOutputBfr->hOutputEvent)
        {
            return E_FAIL;
        }
#if defined (NV_WINDOWS)
        WaitForSingleObject(stThreadData.pOutputBfr->hOutputEvent, INFINITE);
#endif
    }

    if (stThreadData.pOutputBfr->bEOSFlag)
        return S_OK;

    if (m_stEncoderInput.useMappedResources)
    {
        // unmap the mapped resource ptr
        nvStatus = m_pEncodeAPI->nvEncUnmapInputResource(m_hEncoder, stThreadData.pInputBfr->hInputSurface);
        stThreadData.pInputBfr->hInputSurface = NULL;
    }

    NV_ENC_LOCK_BITSTREAM lockBitstreamData;
    nvStatus = NV_ENC_SUCCESS;
    memset(&lockBitstreamData, 0, sizeof(lockBitstreamData));
    SET_VER(lockBitstreamData, NV_ENC_LOCK_BITSTREAM);

    if(m_stInitEncParams.reportSliceOffsets)
        lockBitstreamData.sliceOffsets = new unsigned int[m_stEncoderInput.sliceModeData];

    lockBitstreamData.outputBitstream = stThreadData.pOutputBfr->hBitstreamBuffer;
    lockBitstreamData.doNotWait = false;

    if (!stThreadData.pOutputBfr->pBitstreamBufferPtr)
    {
        nvStatus = m_pEncodeAPI->nvEncLockBitstream(m_hEncoder, &lockBitstreamData);
        if (nvStatus == NV_ENC_SUCCESS)
        {
            (*m_fwrite_callback)(lockBitstreamData.bitstreamBufferPtr, 1, lockBitstreamData.bitstreamSizeInBytes, m_fOutput, m_privateData);
            nvStatus = m_pEncodeAPI->nvEncUnlockBitstream(m_hEncoder, stThreadData.pOutputBfr->hBitstreamBuffer);
            checkNVENCErrors(nvStatus);
        }
    }
    else
    {
        NV_ENC_STAT stEncodeStats;
        memset(&stEncodeStats, 0, sizeof(stEncodeStats));
        SET_VER(stEncodeStats, NV_ENC_STAT);
        stEncodeStats.outputBitStream = stThreadData.pOutputBfr->hBitstreamBuffer;
        nvStatus = m_pEncodeAPI->nvEncGetEncodeStats(m_hEncoder, &stEncodeStats);
        (*m_fwrite_callback)(stThreadData.pOutputBfr->pBitstreamBufferPtr, 1, stEncodeStats.bitStreamSize, m_fOutput, m_privateData);
    }

    if (!m_stOutputSurfQueue.Add(stThreadData.pOutputBfr))
    {
        assert(0);
    }

    if (!m_stInputSurfQueue.Add(stThreadData.pInputBfr))
    {
        assert(0);
    }

    if(lockBitstreamData.sliceOffsets)
        delete(lockBitstreamData.sliceOffsets);

    if (nvStatus != NV_ENC_SUCCESS)
        hr = E_FAIL;

    return hr;
}


HRESULT CNvEncoder::CopyFrameData(FrameThreadData stFrameData)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    HRESULT hr = S_OK;

    FILE_64BIT_HANDLE  fileOffset;
    DWORD numBytesRead = 0;

#if defined(WIN32) || defined(WIN64)
    fileOffset.QuadPart = (LONGLONG)stFrameData.dwFileWidth;
#else
    fileOffset = (LONGLONG)stFrameData.dwFileWidth; 
#endif

    DWORD result;
    unsigned int dwInFrameSize     = stFrameData.dwFileWidth*stFrameData.dwFileHeight + (stFrameData.dwFileWidth*stFrameData.dwFileHeight)/2;
    unsigned char *yuv[3];

    bool bFieldPic = false, bTopField = false;

    yuv[0] = new unsigned char[stFrameData.dwFileWidth*stFrameData.dwFileHeight];
    yuv[1] = new unsigned char[stFrameData.dwFileWidth*stFrameData.dwFileHeight/4];
    yuv[2] = new unsigned char[stFrameData.dwFileWidth*stFrameData.dwFileHeight/4];

#if defined(WIN32) || defined(WIN64)
    fileOffset.QuadPart = (LONGLONG)(dwInFrameSize * stFrameData.dwFrmIndex);
    result = SetFilePointer(stFrameData.hInputYUVFile, fileOffset.LowPart, &fileOffset.HighPart, FILE_BEGIN);
#else
    fileOffset = (LONGLONG)(dwInFrameSize * stFrameData.dwFrmIndex);
    result = lseek64((LONGLONG)stFrameData.hInputYUVFile, fileOffset, SEEK_SET); 
#endif

    if (result == FILE_ERROR_SET_FP)
    {
        return E_FAIL;
    }

    if (bFieldPic)
    {
        if (!bTopField)
        {
            // skip the first line
#if defined(WIN32) || defined(WIN64)
            fileOffset.QuadPart  = (LONGLONG)stFrameData.dwFileWidth;
            result = SetFilePointer(stFrameData.hInputYUVFile, fileOffset.LowPart, &fileOffset.HighPart, FILE_CURRENT);
#else
            fileOffset = (LONGLONG)stFrameData.dwFileWidth;
            result = lseek64((LONGLONG)stFrameData.hInputYUVFile, fileOffset, SEEK_CUR); 
#endif
            if (result == FILE_ERROR_SET_FP)
            {
                return E_FAIL;
            }
        }

        // read Y
        for ( unsigned int i = 0 ; i < stFrameData.dwFileHeight/2; i++ )
        {
#if defined(WIN32) || defined(WIN64)
            ReadFile(stFrameData.hInputYUVFile, yuv[0] + i*stFrameData.dwSurfWidth, stFrameData.dwFileWidth, &numBytesRead, NULL);
            // skip the next line
            fileOffset.QuadPart  = (LONGLONG)stFrameData.dwFileWidth;
            result = SetFilePointer(stFrameData.hInputYUVFile, fileOffset.LowPart, &fileOffset.HighPart, FILE_CURRENT);
#else
            numBytesRead = read((LONGLONG)stFrameData.hInputYUVFile, yuv[0] + i*stFrameData.dwSurfWidth, stFrameData.dwFileWidth);
            fileOffset   = (LONGLONG)stFrameData.dwFileWidth;
            result       = lseek64((LONGLONG)stFrameData.hInputYUVFile, fileOffset, SEEK_CUR); 
#endif
            if (result == FILE_ERROR_SET_FP)
            {
                return E_FAIL;
            }
        }

        // read U,V
        for (int cbcr = 0; cbcr < 2; cbcr++)
        {
            //put file pointer at the beginning of chroma
#if defined(WIN32) || defined(WIN64)
            fileOffset.QuadPart  = (LONGLONG)(dwInFrameSize*stFrameData.dwFrmIndex + stFrameData.dwFileWidth*stFrameData.dwFileHeight + cbcr*(( stFrameData.dwFileWidth*stFrameData.dwFileHeight)/4));
            result = SetFilePointer(stFrameData.hInputYUVFile, fileOffset.LowPart, &fileOffset.HighPart, FILE_BEGIN);
#else
            fileOffset   = (LONGLONG)(dwInFrameSize*stFrameData.dwFrmIndex + stFrameData.dwFileWidth*stFrameData.dwFileHeight + cbcr*(( stFrameData.dwFileWidth*stFrameData.dwFileHeight)/4));
            result       = lseek64((LONGLONG)stFrameData.hInputYUVFile, fileOffset, SEEK_CUR); 
#endif
            if (result == FILE_ERROR_SET_FP)
            {
                return E_FAIL;
            }
            if (!bTopField)
            {
#if defined(WIN32) || defined(WIN64)
                fileOffset.QuadPart  = (LONGLONG)(stFrameData.dwFileWidth/2);
                result = SetFilePointer(stFrameData.hInputYUVFile, fileOffset.LowPart, &fileOffset.HighPart, FILE_CURRENT);
#else
                fileOffset = (LONGLONG)(stFrameData.dwFileWidth/2);
                result     = lseek64((LONGLONG)stFrameData.hInputYUVFile, fileOffset, SEEK_CUR); 
#endif

                if (result == FILE_ERROR_SET_FP)
                {
                    return E_FAIL;
                }
            }

            for ( unsigned int i = 0 ; i < stFrameData.dwFileHeight/4; i++ )
            {
#if defined(WIN32) || defined(WIN64)
                ReadFile(stFrameData.hInputYUVFile, yuv[cbcr + 1] + i*(stFrameData.dwSurfWidth/2), stFrameData.dwFileWidth/2, &numBytesRead, NULL);
                fileOffset.QuadPart  = (LONGLONG)stFrameData.dwFileWidth/2;
                result = SetFilePointer(stFrameData.hInputYUVFile, fileOffset.LowPart, &fileOffset.HighPart, FILE_CURRENT);
#else
                numBytesRead = read((LONGLONG)stFrameData.hInputYUVFile, yuv[0] + i*stFrameData.dwSurfWidth, stFrameData.dwFileWidth);
                fileOffset   = (LONGLONG)stFrameData.dwFileWidth;
                result       = lseek64((LONGLONG)stFrameData.hInputYUVFile, fileOffset, SEEK_CUR); 
#endif
                if (result == FILE_ERROR_SET_FP)
                {
                    return E_FAIL;
                }
            }
        }
    }
    else if (stFrameData.dwFileWidth != stFrameData.dwSurfWidth)
    {
        // load the whole frame
        // read Y
        for ( unsigned int i = 0 ; i < stFrameData.dwFileHeight; i++ )
        {
#if defined(WIN32) || defined(WIN64)
            ReadFile(stFrameData.hInputYUVFile, yuv[0] + i*stFrameData.dwSurfWidth, stFrameData.dwFileWidth, &numBytesRead, NULL);
#else
            numBytesRead = read((LONGLONG)stFrameData.hInputYUVFile, yuv[0] + i*stFrameData.dwSurfWidth, stFrameData.dwFileWidth);
#endif
        }

        // read U,V
        for (int cbcr = 0; cbcr < 2; cbcr++)
        {
            // move in front of chroma
#if defined(WIN32) || defined(WIN64)
             fileOffset.QuadPart  = (LONGLONG)(dwInFrameSize*stFrameData.dwFrmIndex + stFrameData.dwFileWidth*stFrameData.dwFileHeight + cbcr*((stFrameData.dwFileWidth* stFrameData.dwFileHeight)/4));
             result = SetFilePointer(stFrameData.hInputYUVFile, fileOffset.LowPart, &fileOffset.HighPart, FILE_BEGIN);
#else
             fileOffset   = (LONGLONG)(dwInFrameSize*stFrameData.dwFrmIndex + stFrameData.dwFileWidth*stFrameData.dwFileHeight + cbcr*((stFrameData.dwFileWidth* stFrameData.dwFileHeight)/4));
             result       = lseek64((LONGLONG)stFrameData.hInputYUVFile, fileOffset, SEEK_CUR); 
#endif
             if (result == FILE_ERROR_SET_FP)
             {
                return E_FAIL;
             }
             for ( unsigned int i = 0 ; i < stFrameData.dwFileHeight/2; i++ )
             {
#if defined(WIN32) || defined(WIN64)
                 ReadFile(stFrameData.hInputYUVFile, yuv[cbcr + 1] + i*stFrameData.dwSurfWidth/2, stFrameData.dwFileWidth/2, &numBytesRead, NULL);
#else
                 numBytesRead = read((LONGLONG)stFrameData.hInputYUVFile, yuv[cbcr + 1] + i*stFrameData.dwSurfWidth/2, stFrameData.dwFileWidth/2);
#endif
             }
        }
    }
    else
    {
#if defined(WIN32) || defined(WIN64)
        // direct file read
        ReadFile(stFrameData.hInputYUVFile, &yuv[0], stFrameData.dwFileWidth * stFrameData.dwFileHeight  , &numBytesRead, NULL);
        ReadFile(stFrameData.hInputYUVFile, &yuv[1], stFrameData.dwFileWidth * stFrameData.dwFileHeight/4, &numBytesRead, NULL);
        ReadFile(stFrameData.hInputYUVFile, &yuv[2], stFrameData.dwFileWidth * stFrameData.dwFileHeight/4, &numBytesRead, NULL);
#else
        numBytesRead = read((LONGLONG)stFrameData.hInputYUVFile, &yuv[0], stFrameData.dwFileWidth * stFrameData.dwFileHeight);
        numBytesRead = read((LONGLONG)stFrameData.hInputYUVFile, &yuv[1], stFrameData.dwFileWidth * stFrameData.dwFileHeight / 4);
        numBytesRead = read((LONGLONG)stFrameData.hInputYUVFile, &yuv[2], stFrameData.dwFileWidth * stFrameData.dwFileHeight / 4);
#endif
    }

    // We assume input is YUV420, and we want to convert to NV12 Tiled
//    convertYUVpitchtoNV12tiled16x16(yuv[0], yuv[1], yuv[2], pInputSurface, pInputSurfaceCh, stFrameData.dwWidth, stFrameData.dwHeight, stFrameData.dwWidth, lockedPitch);
    delete yuv[0];
    delete yuv[1];
    delete yuv[2];
    return hr;
}


HRESULT CNvEncoder::FlushEncoder()
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    HRESULT hr = S_OK;
    memset(&m_stEncodePicParams, 0, sizeof(m_stEncodePicParams));
    SET_VER(m_stEncodePicParams, NV_ENC_PIC_PARAMS);
    // This EOS even signals indicates that all frames in the NVENC input queue have been flushed to the 
    // HW Encoder and it has been processed.  In some cases, the client might not know when the last frame 
    // sent to the encoder is, so it is easier to sometimes insert the EOS and wait for the EOSE event.
    // If used in async mode (windows only), the EOS event must be sent along with the packet to the driver.
    m_stEncodePicParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    m_stEncodePicParams.completionEvent = (m_bAsyncModeEncoding == true) ? m_stEOSOutputBfr.hOutputEvent : NULL;
    nvStatus = m_pEncodeAPI->nvEncEncodePicture(m_hEncoder, &m_stEncodePicParams);
    if(nvStatus != NV_ENC_SUCCESS)
    {
        assert(0);
        hr = E_FAIL;
    }

    // incase of sync modes we might have frames queued for locking
    if ((m_bAsyncModeEncoding == false) && (m_stInitEncParams.enablePTD == 1))
    { 
        EncoderThreadData stThreadData;
        while (m_pEncodeFrameQueue.Remove(stThreadData, 0))
        {
            m_pEncoderThread->QueueSample(stThreadData);
        }
    }

    EncoderThreadData  stThreadData;
    m_stEOSOutputBfr.bWaitOnEvent = m_bAsyncModeEncoding == true ? true : false;
    stThreadData.pOutputBfr = &m_stEOSOutputBfr;
    stThreadData.pInputBfr = NULL;
    if (nvStatus == NV_ENC_SUCCESS)
    {
        // Queue o/p Sample
        if (!m_pEncoderThread->QueueSample(stThreadData))
        {
            assert(0);
        }
    }
    return hr;
}


HRESULT CNvEncoder::ReleaseEncoderResources()
{
    if (m_bEncoderInitialized)
    {
        if (m_pEncoderThread)
        {
            WaitForCompletion();
            m_pEncoderThread->ThreadQuit();
            delete m_pEncoderThread;
            m_pEncoderThread = NULL;
        }
        //m_uRefCount--; // TODO, why we need to track the #references to this object?
    }

    if (m_uRefCount==0) 
    {
        my_printf("CNvEncoder::ReleaseEncoderResources() m_RefCount == 0, releasing resources\n");
        ReleaseIOBuffers();
//        m_pEncodeAPI->nvEncDestroyEncoder(m_hEncoder);

        if (m_spspps.spsppsBuffer)
			delete [] (unsigned char *)m_spspps.spsppsBuffer;
            //delete_array( (unsigned char *)m_spspps.spsppsBuffer );
		m_spspps.spsppsBuffer = NULL;

        delete_array( m_spspps.outSPSPPSPayloadSize );
        delete_array( m_pAvailableSurfaceFmts );

		DestroyEncodeSession();

        if (m_cuContext)
        {
            CUresult cuResult = CUDA_SUCCESS;
            cuResult = cuCtxDestroy(m_cuContext);
            if (cuResult != CUDA_SUCCESS) 
                my_printf("cuCtxDestroy error:0x%x\n", cuResult);
			m_cuContext = NULL;
        }

    #if defined (NV_WINDOWS)
        if (m_pD3D9Device)
        {
            m_pD3D9Device->Release();
            m_pD3D9Device = NULL;
        }

        if (m_pD3D10Device)
        {
            m_pD3D10Device->Release();
            m_pD3D10Device = NULL;
        }

        if (m_pD3D11Device)
        {
            m_pD3D11Device->Release();
            m_pD3D11Device = NULL;
        }

        if (m_pD3D)
        {
            m_pD3D->Release();
            m_pD3D = NULL;
        }
    #endif
    }

	m_bEncoderInitialized = false;
	my_printf("CNvEncoder::ReleaseEncoderResources() checkpoint 7\n");
    return S_OK;
}

HRESULT CNvEncoder::WaitForCompletion()
{
    if (m_pEncoderThread)
    {
        bool bIdle = false;

        do {
            m_pEncoderThread->ThreadLock();
            bIdle = m_pEncoderThread->IsIdle();
            m_pEncoderThread->ThreadUnlock();
            if (!bIdle)
            {
                m_pEncoderThread->ThreadTrigger();
                NvSleep(1);
            }
        } while (!bIdle);
    }

    return S_OK;
}


// Encoder thread
bool CNvEncoderThread::ThreadFunc()
{
    EncoderThreadData stThreadData;
    while (m_pEncoderQueue.Remove(stThreadData, 0))
    {
        m_pOwner->CopyBitstreamData(stThreadData);
    }
    return false;
}


bool CNvEncoderThread::QueueSample(EncoderThreadData &sThreadData)
{
    bool bIsEnqueued = m_pEncoderQueue.Add(sThreadData);
    ThreadTrigger();
    return bIsEnqueued;
}


// GetPresetConfig():
//    Gets the requested NVENC preset and stores it in this.m_stPresetConfig 
//    Note, if requested preset is 'default' (0), then allow any preset to be returned.
//    (This is a workaround for Geforce WHQL driver 314.21, where nvEncGetEncodePresetGUIDs returns
//     DEFAULT_GUID as a supported preset, but nvEncGetEncodePresetConfig returns ERROR for the
//     same DEFAULT_GUID.)
HRESULT CNvEncoder::GetPresetConfig(const int iPresetIdx)
{
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    unsigned int uPresetCount2 = 0;
	
    nvStatus = m_pEncodeAPI->nvEncGetEncodePresetCount(m_hEncoder, m_stEncodeGUID, &m_dwCodecPresetGUIDCount);
    checkNVENCErrors(nvStatus);

    if (nvStatus != NV_ENC_SUCCESS)
		return E_FAIL;

	delete_array(m_stCodecPresetGUIDArray);
    m_stCodecPresetGUIDArray = new GUID[m_dwCodecPresetGUIDCount];
    nvStatus = m_pEncodeAPI->nvEncGetEncodePresetGUIDs(m_hEncoder, m_stEncodeGUID, m_stCodecPresetGUIDArray, m_dwCodecPresetGUIDCount, &uPresetCount2);
    checkNVENCErrors(nvStatus);

    if (nvStatus != NV_ENC_SUCCESS)
		return E_FAIL;

	nvStatus = NV_ENC_ERR_GENERIC;
	for (unsigned int i = 0; i < m_dwCodecPresetGUIDCount; i++)
	{
		// hack: if ( iPresetIdx == 0 "default"), then return the first valid preset.
		if ((int)i == iPresetIdx || (iPresetIdx == 0) )
		{
			nvStatus = m_pEncodeAPI->nvEncGetEncodePresetConfig(m_hEncoder, m_stEncodeGUID, m_stCodecPresetGUIDArray[i], &m_stPresetConfig);
			if (nvStatus == NV_ENC_SUCCESS)
			{
				m_stPresetIdx  = iPresetIdx;
				m_stPresetGUID = m_stCodecPresetGUIDArray[m_stPresetIdx];

				break;
			}
			else if ( iPresetIdx != 0 )
				break;
		}
	}

	checkNVENCErrors(nvStatus);

    if (nvStatus == NV_ENC_SUCCESS)
    {
        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}


void CNvEncoder::Register_fwrite_callback( fwrite_callback_t callback )
{
	m_fwrite_callback = callback;
}

HRESULT CNvEncoder::QueryEncodeCaps(NV_ENC_CAPS caps_type, int *p_nCapsVal)
{
    HRESULT hr = S_OK;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    NV_ENC_CAPS_PARAM stCapsParam = {0};
    SET_VER(stCapsParam, NV_ENC_CAPS_PARAM);
    stCapsParam.capsToQuery = caps_type;

    nvStatus = m_pEncodeAPI->nvEncGetEncodeCaps(m_hEncoder, m_stEncodeGUID, &stCapsParam, p_nCapsVal);
    checkNVENCErrors(nvStatus);
    if (nvStatus == NV_ENC_SUCCESS)
    {
        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}

void CNvEncoder::PrintBufferFormats(string &s) const
{
	ostringstream os;
	
	os << "CNvEncoder::PrintBufferFormat(): supported (framebuffer) SurfaceFmts _NV_ENC_BUFFER_FORMAT(s):\n";

	for(unsigned i = 0; i < m_dwInputFmtCount; ++i ) {
		string sguid;
		os << "\t[" << std::dec << i << "]\t";
		desc_nv_enc_buffer_format_names.value2string( m_pAvailableSurfaceFmts[i], sguid);
		//PrintGUID( m_stCodecProfileGUIDArray[i] );
		os << sguid << endl;
	}

	s = os.str();
}

void CNvEncoder::PrintGUID( const GUID &guid) const
{
	printf("%08X-", guid.Data1 );
	printf("%04X-", guid.Data2 );
	printf("%04X-", guid.Data3 );
	for(unsigned b = 0; b < 8; ++b )
		printf("%02X ", guid.Data4[b] );
}

void CNvEncoder::PrintEncodeFormats(string &s) const
{
	ostringstream os;

	os << "CNvEncoder::PrintEncodeFormats() show supported codecs (MPEG-2, VC1, H264, etc.) \n";
	for(unsigned i = 0; i < m_dwEncodeGUIDCount; ++i ) {
		string sguid;
		os << "\t[" << std::dec << i << "]\t";
		desc_nv_enc_codec_names.guid2string( m_stEncodeGUIDArray[i], sguid);
		//PrintGUID( m_stEncodeGUIDArray[i] );
		os << sguid << endl;
	}

	s = os.str();
}


void CNvEncoder::PrintEncodeProfiles(string &s) const
{
	ostringstream os;

	os << "CNvEncoder::PrintEncodeProfiles() show supported codecs (MPEG-2, VC1, H264, etc.) \n";
	for(unsigned i = 0; i < m_dwCodecProfileGUIDCount; ++i ) {
		string sguid;
		os << "\t[" << std::dec << i << "]\t";
		desc_nv_enc_profile_names.guid2string( m_stCodecProfileGUIDArray[i], sguid);
		//PrintGUID( m_stCodecProfileGUIDArray[i] );
		os << sguid << endl;
	}

	s = os.str();
}

void CNvEncoder::PrintEncodePresets(string &s) const
{
	ostringstream os;

	os << "CNvEncoder::PrintEncodePresets() show supported encoding presets \n";
	for(unsigned i = 0; i < m_dwCodecPresetGUIDCount; ++i ) {
		string sguid;
		os << "\t[" << std::dec << i << "]\t";
		desc_nv_enc_preset_names.guid2string( m_stCodecPresetGUIDArray[i], sguid);
		//PrintGUID( m_stCodecPreseteGUIDArray[i] );
		os << sguid << endl;
	}
	
	s = os.str();
}

void CNvEncoder::UseExternalCudaContext(const CUcontext context, const unsigned int device )
{
	m_cuContext = context;
	m_deviceID  = device;
	assert (context != NULL);
	m_useExternalContext = true;
}

HRESULT CNvEncoder::OpenEncodeSession(const EncodeConfig encodeConfig, const unsigned int deviceID, NVENCSTATUS &nvStatus )
{
    HRESULT hr = S_OK;
    //NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    memcpy(&m_stEncoderInput, &encodeConfig, sizeof(m_stEncoderInput));
    m_fOutput = m_stEncoderInput.fOutput;
    bool bCodecFound = false;
    NV_ENC_CAPS_PARAM stCapsParam = {0};
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS stEncodeSessionParams = {0};
    unsigned int uArraysize = 0;
    SET_VER(stCapsParam, NV_ENC_CAPS_PARAM);
    SET_VER(stEncodeSessionParams, NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS);

	// If another NVENC session is already open, close it (to avoid mem-leak)
	DestroyEncodeSession();

	nvStatus = NV_ENC_SUCCESS;

    switch (m_stEncoderInput.interfaceType)
    {
#if defined (NV_WINDOWS)
        case NV_ENC_DX9:
            InitD3D9(deviceID);
            stEncodeSessionParams.device = reinterpret_cast<void *>(m_pD3D9Device);
            stEncodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        break;

        case NV_ENC_DX10:
            InitD3D10(deviceID);
            stEncodeSessionParams.device = reinterpret_cast<void *>(m_pD3D10Device);
            stEncodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        break;

        case NV_ENC_DX11:
            InitD3D11(deviceID);
            stEncodeSessionParams.device = reinterpret_cast<void *>(m_pD3D11Device);
            stEncodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        break;
#endif
        case NV_ENC_CUDA:
			if ( !m_useExternalContext )
				InitCuda(deviceID);
            stEncodeSessionParams.device = reinterpret_cast<void *>(m_cuContext);
            stEncodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
        break;

        default:
                assert("Encoder interface not supported");
                exit(EXIT_FAILURE);
    } // switch

    nvStatus = m_pEncodeAPI->nvEncOpenEncodeSessionEx( &stEncodeSessionParams, &m_hEncoder);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        my_printf("nvEncOpenEncodeSessionEx() returned with error %d\n", nvStatus);
        printf("Note: GUID key may be invalid or incorrect.  Recommend to upgrade your drivers and obtain a new key\n");
        //checkNVENCErrors(nvStatus);// prevent NVNEC-plugin from exiting prematurely
		return E_FAIL;
    }

	// Enumerate the codec support by the HW Encoder
    nvStatus = m_pEncodeAPI->nvEncGetEncodeGUIDCount(m_hEncoder, &m_dwEncodeGUIDCount);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        my_printf("nvEncGetEncodeGUIDCount() returned with error %d\n", nvStatus);
        checkNVENCErrors(nvStatus);
    } 
    else {
		delete_array( m_stEncodeGUIDArray );
        m_stEncodeGUIDArray       = new GUID[m_dwEncodeGUIDCount];
        memset(m_stEncodeGUIDArray, 0, sizeof(GUID) * m_dwEncodeGUIDCount);
        uArraysize = 0;
        nvStatus = m_pEncodeAPI->nvEncGetEncodeGUIDs(m_hEncoder, m_stEncodeGUIDArray, m_dwEncodeGUIDCount, &uArraysize);
        assert(uArraysize <= m_dwEncodeGUIDCount);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            my_printf("nvEncGetEncodeGUIDs() returned with error %d\n", nvStatus);
            checkNVENCErrors(nvStatus);
        }
        else {
            for (unsigned int i = 0; i < uArraysize; i++)
            {
                // check if HW encoder supports the particular codec -
                if (GetCodecType(m_stEncodeGUIDArray[i]) == (unsigned int)m_stEncoderInput.codec)
                {
					// Found the desired codec GUID - store it as "m_stEncodeGUID"
                    bCodecFound = true;
                    memcpy(&m_stEncodeGUID, &m_stEncodeGUIDArray[i], sizeof(GUID));
                    break;
                }
            }
        }
    } 

    if (bCodecFound == false)
    {
		// Hardware doesn't support our requested codec-type <m_stEncoderInput.codec>
        assert(0);
        return E_FAIL;
    }

    // Enumerate the profile(s) available for selected codec <m_stEncodeGUID>
    nvStatus = m_pEncodeAPI->nvEncGetEncodeProfileGUIDCount(m_hEncoder, m_stEncodeGUID, &m_dwCodecProfileGUIDCount);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        my_printf( "nvEncGetEncodeProfileGUIDCount() returned with error %d\n", nvStatus);
        checkNVENCErrors(nvStatus);
    }
    else {
		delete_array( m_stCodecProfileGUIDArray );
        m_stCodecProfileGUIDArray = new GUID[m_dwCodecProfileGUIDCount];
        memset(m_stCodecProfileGUIDArray, 0, sizeof(GUID) * m_dwCodecProfileGUIDCount);
        uArraysize = 0;
        nvStatus = m_pEncodeAPI->nvEncGetEncodeProfileGUIDs(m_hEncoder,  m_stEncodeGUID, m_stCodecProfileGUIDArray, m_dwCodecProfileGUIDCount, &uArraysize);
        assert(uArraysize <= m_dwCodecProfileGUIDCount);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            my_printf( "nvEncGetEncodeProfileGUIDs() returned with error %d\n", nvStatus);
            checkNVENCErrors(nvStatus);
        }
        else {
            bCodecFound = false;
            for (unsigned int i = 0; i < uArraysize; i++)
            {
                // check if this HW-codec supports the requested profile <m_stEncoderInput.profile>
                if (GetCodecProfile(m_stCodecProfileGUIDArray[i]) == m_stEncoderInput.profile)
                {
					// Found the desired Profile - store it as "m_stCodecProfileGUID"
                    bCodecFound = true;
                    memcpy(&m_stCodecProfileGUID, &m_stCodecProfileGUIDArray[i], sizeof(GUID));
                    break;
                }
            }
        }
    }

    if (bCodecFound == false)
    {
		// Codec doesn't support our requested profile <m_stEncoderInput.profile>
        assert(0);
        return E_FAIL;
    }

    // Enumerate the (framebuffer) InputFormats available for selected codec <m_stEncodeGUID>
    nvStatus =  m_pEncodeAPI->nvEncGetInputFormatCount(m_hEncoder, m_stEncodeGUID, &m_dwInputFmtCount);
    if (nvStatus != NV_ENC_SUCCESS)
    {
        my_printf("nvEncGetInputFormatCount() returned with error %d\n", nvStatus);
        checkNVENCErrors(nvStatus);
    }
    else {
		delete_array( m_pAvailableSurfaceFmts );
        m_pAvailableSurfaceFmts = new NV_ENC_BUFFER_FORMAT[m_dwInputFmtCount];
        memset(m_pAvailableSurfaceFmts, 0, sizeof(NV_ENC_BUFFER_FORMAT) * m_dwInputFmtCount);
        nvStatus = m_pEncodeAPI->nvEncGetInputFormats(m_hEncoder, m_stEncodeGUID, m_pAvailableSurfaceFmts, m_dwInputFmtCount, &uArraysize);
        if (nvStatus != NV_ENC_SUCCESS)
        {
            my_printf("nvEncGetInputFormats() returned with error %d\n", nvStatus);
            checkNVENCErrors(nvStatus);
        }
        else  {
            bool bFmtFound = false;
            unsigned int idx;
            for (idx = 0; idx < m_dwInputFmtCount; idx++)
            {
                // check if this HW-codec supports the requested (framebuffer) InputFormat 
                if (encodeConfig.chromaFormatIDC == cudaVideoChromaFormat_420 &&
						m_pAvailableSurfaceFmts[idx] == NV_ENC_BUFFER_FORMAT_NV12_TILED64x16 ||
						encodeConfig.chromaFormatIDC == cudaVideoChromaFormat_420 &&
						m_pAvailableSurfaceFmts[idx] == NV_ENC_BUFFER_FORMAT_NV12_TILED64x16 ||
						encodeConfig.chromaFormatIDC == cudaVideoChromaFormat_444 &&
                        m_pAvailableSurfaceFmts[idx] == NV_ENC_BUFFER_FORMAT_YUV444_TILED64x16)
                {
		            m_dwInputFormat = m_pAvailableSurfaceFmts[idx];
					bFmtFound = true;
					break;
				}
            }
			if ( !bFmtFound ) {
				my_printf("ERROR, Unable to locate a compatible chromaformatIDC\n");
			}
            assert(bFmtFound == true);
            assert(uArraysize <= m_dwInputFmtCount);
        }
    }

    if (encodeConfig.preset > -1)
    {
        hr = GetPresetConfig(encodeConfig.preset);
        if (hr == S_OK)
        {
            memcpy(&m_stEncodeConfig, &m_stPresetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
        }
    }

		// check if this HW-codec supports the requested (framebuffer) InputFormat 

	if (nvStatus != NV_ENC_SUCCESS)
    {
        checkNVENCErrors(nvStatus);
    }

    return hr;
}


//
// QueryEncodeSession():  Gathers the NVENC-hardware's reported capabilities by
//                        (1) creating NVENC session
//                        (2) calling several query APIs (and storing the query-result in this class)
//                        (3) destroying the session
NVENCSTATUS CNvEncoder::QueryEncodeSession(const unsigned int deviceID, nv_enc_caps_s &nv_enc_caps, const bool destroy_on_exit)
{
	HRESULT hr = S_OK;
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	NV_ENC_CAPS_PARAM stCapsParam = { 0 };
	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS stEncodeSessionParams = { 0 };
	unsigned int uArraysize = 0;
	SET_VER(stCapsParam, NV_ENC_CAPS_PARAM);
	SET_VER(stEncodeSessionParams, NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS);

	stEncodeSessionParams.apiVersion = NVENCAPI_VERSION;

	// always use CUDA interface
	InitCuda(deviceID);
	stEncodeSessionParams.device = reinterpret_cast<void *>(m_cuContext);
	stEncodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;

	// If another NVENC session is already open, close it (to avoid mem-leak)
	DestroyEncodeSession();

	// Get the capabilities of this NVENC-hardware, and store it to object's m_* variables
	//  (1) Start an encoder session (nvEncOpenEncodeSessionEx)
	//  (2) Call query routines in the NVENC API to get the encoder's feature-set:
	//      supported input (framebuffer) pixel format
	//      supported encoding modes, profiles, encoder features (B-frames, cabac, etc.)

	nvStatus = m_pEncodeAPI->nvEncOpenEncodeSessionEx(&stEncodeSessionParams, &m_hEncoder);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		my_printf("nvEncOpenEncodeSessionEx() returned with error %d\n", nvStatus);
		printf("Note: GUID key may be invalid or incorrect.  Recommend to upgrade your drivers and obtain a new key\n");
		//checkNVENCErrors(nvStatus); // don't call exit(), otherwise Premiere plugin will quit without saying why
		return nvStatus;
	}

	// Enumerate the codec support by the HW Encoder
	nvStatus = m_pEncodeAPI->nvEncGetEncodeGUIDCount(m_hEncoder, &m_dwEncodeGUIDCount);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		my_printf("nvEncGetEncodeGUIDCount() returned with error %d\n", nvStatus);
		checkNVENCErrors(nvStatus);
		DestroyEncodeSession();
		return nvStatus;
	}

	delete_array(m_stEncodeGUIDArray);
	m_stEncodeGUIDArray = new GUID[m_dwEncodeGUIDCount];
	memset(m_stEncodeGUIDArray, 0, sizeof(GUID) * m_dwEncodeGUIDCount);
	uArraysize = 0;
	nvStatus = m_pEncodeAPI->nvEncGetEncodeGUIDs(m_hEncoder, m_stEncodeGUIDArray, m_dwEncodeGUIDCount, &uArraysize);
	assert(uArraysize <= m_dwEncodeGUIDCount);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		my_printf("nvEncGetEncodeGUIDs() returned with error %d\n", nvStatus);
		checkNVENCErrors(nvStatus);
		DestroyEncodeSession();
		return nvStatus;
	}

	if ( destroy_on_exit )
		DestroyEncodeSession();

	return nvStatus;
}

NVENCSTATUS CNvEncoder::QueryEncodeSessionCodec(const unsigned int deviceID, const NvEncodeCompressionStd codec, nv_enc_caps_s &nv_enc_caps)
{
    HRESULT hr = S_OK;
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
    NV_ENC_CAPS_PARAM stCapsParam = {0};
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS stEncodeSessionParams = {0};
    unsigned int uArraysize = 0;
    SET_VER(stCapsParam, NV_ENC_CAPS_PARAM);
    SET_VER(stEncodeSessionParams, NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS);
	const GUID codecGUID = GetCodecGUID(codec);

	nvStatus = QueryEncodeSession( deviceID, nv_enc_caps, false );
	if (nvStatus != NV_ENC_SUCCESS) {
		DestroyEncodeSession();
		return nvStatus;
	}

	stEncodeSessionParams.apiVersion = NVENCAPI_VERSION;
	stEncodeSessionParams.device = reinterpret_cast<void *>(m_cuContext);
	stEncodeSessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;

	// Enumerate the profile(s) available for selected <codec>
	nvStatus = m_pEncodeAPI->nvEncGetEncodeProfileGUIDCount(m_hEncoder, codecGUID, &m_dwCodecProfileGUIDCount);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		my_printf( "nvEncGetEncodeProfileGUIDCount() returned with error %d\n", nvStatus);
		checkNVENCErrors(nvStatus);
		DestroyEncodeSession();
		return nvStatus;
	}

	delete_array( m_stCodecProfileGUIDArray );
	m_stCodecProfileGUIDArray = new GUID[m_dwCodecProfileGUIDCount];
	memset(m_stCodecProfileGUIDArray, 0, sizeof(GUID) * m_dwCodecProfileGUIDCount);
	uArraysize = 0;
	nvStatus = m_pEncodeAPI->nvEncGetEncodeProfileGUIDs(m_hEncoder, codecGUID, m_stCodecProfileGUIDArray, m_dwCodecProfileGUIDCount, &uArraysize);
	assert(uArraysize <= m_dwCodecProfileGUIDCount);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		my_printf( "nvEncGetEncodeProfileGUIDs() returned with error %d\n", nvStatus);
		checkNVENCErrors(nvStatus);
		DestroyEncodeSession();
		return nvStatus;
	}

	// Enumerate the (framebuffer) InputFormats available for selected <codec>
	nvStatus = m_pEncodeAPI->nvEncGetInputFormatCount(m_hEncoder, codecGUID, &m_dwInputFmtCount);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		my_printf("nvEncGetInputFormatCount() returned with error %d\n", nvStatus);
		checkNVENCErrors(nvStatus);
		DestroyEncodeSession();
		return nvStatus;
	}

	delete_array( m_pAvailableSurfaceFmts );
	m_pAvailableSurfaceFmts = new NV_ENC_BUFFER_FORMAT[m_dwInputFmtCount];
	memset(m_pAvailableSurfaceFmts, 0, sizeof(NV_ENC_BUFFER_FORMAT) * m_dwInputFmtCount);
	nvStatus = m_pEncodeAPI->nvEncGetInputFormats(m_hEncoder, codecGUID, m_pAvailableSurfaceFmts, m_dwInputFmtCount, &uArraysize);
	if (nvStatus != NV_ENC_SUCCESS)
	{
		my_printf("nvEncGetInputFormats() returned with error %d\n", nvStatus);
		checkNVENCErrors(nvStatus);
		DestroyEncodeSession();
		return nvStatus;
	}

	// Populate the preset-table
	hr = GetPresetConfig(0);

	if (nvStatus != NV_ENC_SUCCESS)
    {
        checkNVENCErrors(nvStatus);
		DestroyEncodeSession();
		return nvStatus;
    }

	hr = _QueryEncoderCaps( codecGUID, nv_enc_caps );
	DestroyEncodeSession();
    return nvStatus;
}

void
CNvEncoder::initEncoderConfig(EncodeConfig *p_nvEncoderConfig) // used by Adobe Premiere Plugin
{
    if (p_nvEncoderConfig) 
    {
		memset( (void *)p_nvEncoderConfig, 0, sizeof(*p_nvEncoderConfig));

        // Parameters that are send to NVENC hardware
        p_nvEncoderConfig->codec            = NV_ENC_H264;
        p_nvEncoderConfig->profile          = NV_ENC_CODEC_PROFILE_AUTOSELECT;
        p_nvEncoderConfig->width            = 720; // this should be init to agree with AME default
        p_nvEncoderConfig->height           = 480; // this should be init to agree with AME default
		p_nvEncoderConfig->frameRateNum     = 24000; // fps = 23.976
        p_nvEncoderConfig->frameRateDen     = 1001;
        p_nvEncoderConfig->darRatioX        = 16;
        p_nvEncoderConfig->darRatioY        = 9;
        p_nvEncoderConfig->avgBitRate       = 25*1000*1000;   // VBR: average, CBR: target
		p_nvEncoderConfig->peakBitRate      = 39*1000*1000;   // VBR: peak
        p_nvEncoderConfig->gopLength        = 23;// Bluray compliancy: gop-length must be less than 1 second?
        p_nvEncoderConfig->monoChromeEncoding = 0;
		p_nvEncoderConfig->numBFrames       = 1;
        p_nvEncoderConfig->FieldEncoding    = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME; // 0=off (progressive), either set to 0 or 2
		p_nvEncoderConfig->rateControl      = NV_ENC_PARAMS_RC_CBR; // Constant Bitrate

		p_nvEncoderConfig->enableLTR        = 0;    // long term reference frames (only supported in HEVC)
		p_nvEncoderConfig->ltrNumFrames     = 0;
		p_nvEncoderConfig->ltrTrustMode     = 0;

		// For ConstQP rate-control only
        //p_nvEncoderConfig->qp               = 24;// not used?
        p_nvEncoderConfig->qpI              = 21;
        p_nvEncoderConfig->qpP              = 23;
        p_nvEncoderConfig->qpB              = 24;

		// for VariableQP rate-control only:  MAX indicates highest QP-value NVENC will use
		//                                    (coarsest or lowest-qual quantizer)
        p_nvEncoderConfig->max_qp_ena       = 0;// flag: enable max_qp?
        p_nvEncoderConfig->max_qpI          = 51;// (for H264: maximum QP = 51) 
        p_nvEncoderConfig->max_qpP          = 51;// (   ... lowest quality)
        p_nvEncoderConfig->max_qpB          = 51;

		// for VariableQP rate-control only:  MIN indicates lowest-values (corresponds to h quality)
		//                                    (finest or highest-qual quantizer)
		p_nvEncoderConfig->min_qp_ena       = 0;// flag: enable min_qp?
        p_nvEncoderConfig->min_qpI          = 0;// (for H264: minimum QP = 0)
        p_nvEncoderConfig->min_qpP          = 0;// (   ... highest quality)
        p_nvEncoderConfig->min_qpB          = 0;

		// for VariableQP rate-control only:  initial indicates starting-values (for frame#0)
        p_nvEncoderConfig->initial_qp_ena   = 0;// flag: enable initial_qp?
        p_nvEncoderConfig->initial_qpI      = 22;
        p_nvEncoderConfig->initial_qpP      = 23;
        p_nvEncoderConfig->initial_qpB      = 24;

		p_nvEncoderConfig->enableFMO        = NV_ENC_H264_FMO_AUTOSELECT;

        p_nvEncoderConfig->hierarchicalP    = 0;
        p_nvEncoderConfig->hierarchicalB    = 0;
        p_nvEncoderConfig->svcTemporal      = 0;
        p_nvEncoderConfig->numlayers        = 0;
        p_nvEncoderConfig->outBandSPSPPS    = 0;
        p_nvEncoderConfig->viewId           = 0;

		p_nvEncoderConfig->stereo3dMode     = 0;
        p_nvEncoderConfig->stereo3dEnable   = 0;
		p_nvEncoderConfig->sliceMode        = 3;
        p_nvEncoderConfig->sliceModeData    = 1;// 1 slice per frame
        p_nvEncoderConfig->level            = NV_ENC_LEVEL_AUTOSELECT;
		p_nvEncoderConfig->idr_period       = p_nvEncoderConfig->gopLength;
        p_nvEncoderConfig->vle_entropy_mode = NV_ENC_H264_ENTROPY_CODING_MODE_CABAC; // 0==cavlc, 1==cabac
//		p_nvEncoderConfig->chromaFormatIDC = NV_ENC_BUFFER_FORMAT_NV12_PL;  // 1 = YUV4:2:0, 3 = YUV4:4:4
		p_nvEncoderConfig->chromaFormatIDC = cudaVideoChromaFormat_420;  // 1 = YUV4:2:0, 3 = YUV4:4:4
		p_nvEncoderConfig->separateColourPlaneFlag = 0;
        p_nvEncoderConfig->output_sei_BufferPeriod = 0;
        p_nvEncoderConfig->mvPrecision      = NV_ENC_MV_PRECISION_QUARTER_PEL;
        p_nvEncoderConfig->output_sei_PictureTime  = 0;

        p_nvEncoderConfig->aud_enable              = 0;
        p_nvEncoderConfig->report_slice_offsets    = 0; // Default dont report slice offsets for nvEncodeAPP.
        p_nvEncoderConfig->enableSubFrameWrite     = 0; // Default do not flust to memory at slice end
        p_nvEncoderConfig->disableDeblock          = 0;
        p_nvEncoderConfig->disable_ptd             = 0;
        p_nvEncoderConfig->adaptive_transform_mode = NV_ENC_H264_ADAPTIVE_TRANSFORM_AUTOSELECT;
        p_nvEncoderConfig->bdirectMode             = NV_ENC_H264_BDIRECT_MODE_SPATIAL;
        p_nvEncoderConfig->preset           = NV_ENC_PRESET_BD; // set to BluRay disc preset

        p_nvEncoderConfig->syncMode                = 1; // 0==synchronous mode, 1==async mode
        p_nvEncoderConfig->interfaceType           = NV_ENC_CUDA;  // Windows R304 (DX9 only), Windows R310 (DX10/DX11/CUDA), Linux R310 (CUDA only)
        p_nvEncoderConfig->disableCodecCfg  = 0;
        p_nvEncoderConfig->useMappedResources      = 0;
		p_nvEncoderConfig->max_ref_frames   = 2; // (for High-profile: up to 4 @ 1080p, or 7 @ 720p)
		p_nvEncoderConfig->vbvBufferSize    = 0;
		p_nvEncoderConfig->vbvInitialDelay  = 0;

		// NVENC API 3.0
		p_nvEncoderConfig->enableVFR        = 0; // Variable Frame Rate (default=off)

		// NVENC API 4.0
		p_nvEncoderConfig->qpPrimeYZeroTransformBypassFlag = 0; //  enable lossless encode set this to 1
		p_nvEncoderConfig->enableAQ         = 0; // adaptive quantization (default=off)

		// NVENC API 5.0
		p_nvEncoderConfig->tier             = NV_ENC_TIER_HEVC_MAIN;   // (HEVC-only)
		p_nvEncoderConfig->minCUsize        = NV_ENC_HEVC_CUSIZE_AUTOSELECT;// (HEVC-only)
		p_nvEncoderConfig->maxCUsize        = NV_ENC_HEVC_CUSIZE_AUTOSELECT;// (HEVC-only)
    }
}

HRESULT
CNvEncoder::QueryEncoderCaps(nv_enc_caps_s &nv_enc_caps)
{
	return _QueryEncoderCaps( m_stEncodeGUID, nv_enc_caps );
}

HRESULT
CNvEncoder::_QueryEncoderCaps( const GUID &codecGUID, nv_enc_caps_s &nv_enc_caps )
{
	NV_ENC_CAPS_PARAM stCapsParam = {0};
    NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
	int         result;

	SET_VER(stCapsParam, NV_ENC_CAPS_PARAM);
#define QUERY_CAPS(CAPS) \
	result = 0; \
    stCapsParam.capsToQuery = CAPS; \
	nvStatus = m_pEncodeAPI->nvEncGetEncodeCaps(m_hEncoder, codecGUID, &stCapsParam, &result); \
	if ( nvStatus == NV_ENC_SUCCESS ) \
		nv_enc_caps.value_ ## CAPS = result; \
	else \
		printf("CNvEncoder::QueryEncodeCapsAll: ERROR occurred while querying property %0s!\n", #CAPS );
	
    if (!m_pEncodeAPI || !m_hEncoder ) {
		printf("CNvEncoder::QueryEncodeCapsAll: ERROR, m_pEncodeAPI or m_hEncoder is NULL!\n");
		return E_FAIL;
	}

	memset( (void *)&nv_enc_caps, 0, sizeof(nv_enc_caps) );// first, clear the struct

    QUERY_CAPS(NV_ENC_CAPS_NUM_MAX_BFRAMES);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_FIELD_ENCODING);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_MONOCHROME);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_FMO);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_QPELMV);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_BDIRECT_MODE);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_CABAC);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM);
//  QUERY_CAPS(NV_ENC_CAPS_SUPPORT_RESERVED);
    QUERY_CAPS(NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES);
    QUERY_CAPS(NV_ENC_CAPS_LEVEL_MAX);
    QUERY_CAPS(NV_ENC_CAPS_LEVEL_MIN);
    QUERY_CAPS(NV_ENC_CAPS_SEPARATE_COLOUR_PLANE);
    QUERY_CAPS(NV_ENC_CAPS_WIDTH_MAX);
    QUERY_CAPS(NV_ENC_CAPS_HEIGHT_MAX);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_INTRA_REFRESH);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE);
    QUERY_CAPS(NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION);
    QUERY_CAPS(NV_ENC_CAPS_PREPROC_SUPPORT);
    QUERY_CAPS(NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT);
	QUERY_CAPS(NV_ENC_CAPS_MB_NUM_MAX);
	QUERY_CAPS(NV_ENC_CAPS_MB_PER_SEC_MAX );
	QUERY_CAPS(NV_ENC_CAPS_SUPPORT_YUV444_ENCODE );
	QUERY_CAPS(NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE );

	return S_OK;
}

void EncodeConfig::print( string &stringout ) const {
	string s;
	ostringstream os;

	const bool codec_is_h264 = (codec == NV_ENC_H264);
	const bool codec_is_hevc = (codec == NV_ENC_H265);

#define PRINT_DEC(var) os << "" #var ": " << std::dec << var;
#define PRINT_HEX(var) os << "" #var ": 0x" << std::hex << var;

	desc_nv_enc_codec_names.value2string(codec, s);
	PRINT_DEC(codec)
	os << " (" << s << "_GUID)";
	os << endl;

	desc_nv_enc_profile_names.value2string(profile, s);
	PRINT_DEC(profile)
	os << " (" << s << ")";
	os << endl;

	if (codec_is_h264)
		desc_nv_enc_level_h264_names.value2string(level, s);
	else if (codec_is_hevc)
		desc_nv_enc_level_hevc_names.value2string(level, s);
	else
		desc_nv_enc_level_hevc_names.value2string(level, s); // assume H265
	PRINT_DEC(level)
	os << " (" << s << ")" << endl;

	if (codec_is_hevc) {
		desc_nv_enc_tier_hevc_names.value2string(tier, s);
		PRINT_DEC(tier)
		os << " (" << s << ")" << endl;

		desc_nv_enc_hevc_cusize_names.value2string(minCUsize, s);
		PRINT_DEC(minCUsize)
		os << " (" << s << ")" << endl;

		desc_nv_enc_hevc_cusize_names.value2string(maxCUsize, s);
		PRINT_DEC(maxCUsize)
		os << " (" << s << ")" << endl;
	}

	desc_nv_enc_preset_names.value2string(preset, s);
	PRINT_DEC(preset)
	os << " (" << s << ")";
	os << endl;

	PRINT_DEC(width)
	os << ",    ";

	PRINT_DEC(height)
	os << endl;

	PRINT_DEC(frameRateNum)
	os << ", ";

	PRINT_DEC(frameRateDen)
	os << " (" << std::defaultfloat
	   << (static_cast<float>(frameRateNum) / frameRateDen) << " fps), ";

	PRINT_DEC(enableVFR)
	os << endl;

	PRINT_DEC(darRatioX)
	os << ",    ";

	PRINT_DEC(darRatioY)
	os << endl;

	if ( rateControl == NV_ENC_PARAMS_RC_CBR ||
		rateControl == NV_ENC_PARAMS_RC_2_PASS_VBR ||
		rateControl == NV_ENC_PARAMS_RC_VBR ||
		rateControl ==  NV_ENC_PARAMS_RC_VBR_MINQP)
	{

		PRINT_DEC(avgBitRate)
		os << endl;
	}

	if ( rateControl == NV_ENC_PARAMS_RC_VBR ||
		rateControl == NV_ENC_PARAMS_RC_VBR_MINQP ||
		rateControl == NV_ENC_PARAMS_RC_2_PASS_VBR)
	{
		PRINT_DEC(peakBitRate)
		os << endl;
	}

	if ( gopLength == NVENC_INFINITE_GOPLENGTH ) {
		PRINT_HEX(gopLength) // infinite GOP
		os << " (NVENC_INFINITE_GOPLENGTH)";
	}
	else {
		PRINT_DEC(gopLength) // non-infinite GOP (value = #frames)
	}
	os << endl;

	PRINT_DEC(monoChromeEncoding)
	os << endl;

	PRINT_DEC(enableLTR)
	if (enableLTR) {
		os << ", ";
		PRINT_DEC(ltrNumFrames);
		if (codec == NV_ENC_H264) {
			os << ", ";
			PRINT_DEC(ltrTrustMode);
		}
	}
	os << endl;

	PRINT_DEC(numBFrames)
	os << endl;

	desc_nv_enc_params_frame_mode_names.value2string(FieldEncoding,s);
	PRINT_DEC(FieldEncoding)
	os << " (" << s << ")";
	os << endl;

	desc_nv_enc_ratecontrol_names.value2string(rateControl,s);
	PRINT_DEC(rateControl)
	os << " (" << s << ")";

	os << ",   ";
	PRINT_DEC(enableAQ)
	os << endl;

	PRINT_DEC(vbvBufferSize)
	os << ",    ";

	PRINT_DEC(vbvInitialDelay)
	os << endl;

	// Constant-QP rate-control parameters
	// These parameters are only used in ConstQP rate-control mode
	if ( rateControl == NV_ENC_PARAMS_RC_CONSTQP ) {

		PRINT_DEC(qpI)
		os << ", ";

		PRINT_DEC(qpP)
		os << ", ";

		PRINT_DEC(qpB)
		os << endl;
	}

	// Min-QP rate-control parameters
	PRINT_DEC(min_qp_ena)
	if ( min_qp_ena ) {
		os << ", ";

		PRINT_DEC(min_qpI)
		os << ", ";

		PRINT_DEC(min_qpP)
		os << ", ";

		PRINT_DEC(min_qpB)
		os << endl;
	}

	PRINT_DEC(max_qp_ena)
	if ( max_qp_ena ) {
		os << ", ";

		PRINT_DEC(max_qpI)
		os << ", ";

		PRINT_DEC(max_qpP)
		os << ", ";

		PRINT_DEC(max_qpB)
		os << endl;
	}

	if (codec_is_h264) {
		desc_nv_enc_h264_fmo_names.value2string(enableFMO, s);
		PRINT_DEC(enableFMO)
			os << " (" << s << ")";
		os << endl;
	}

	PRINT_DEC(hierarchicalP)
		os << ", ";

	PRINT_DEC(hierarchicalB)
	os << endl;

//	PRINT_DEC(svcTemporal)
//	os << endl;

	PRINT_DEC(outBandSPSPPS)
	os << endl;

//	PRINT_DEC(viewId)
//	os << endl;

//	PRINT_DEC(stereo3dMode)
//	os << ",    ";
//
//	PRINT_DEC(stereo3dEnable)
//	os << endl;

	PRINT_DEC(sliceMode)
	os << ", ";

	PRINT_DEC(sliceModeData)
	os << endl;

	//PRINT_DEC(idr_period)
	//os << endl;

	if (codec_is_h264) {
		desc_nv_enc_h264_entropy_coding_mode_names.value2string(vle_entropy_mode, s);
		PRINT_DEC(vle_entropy_mode)
			os << " (" << s << ")";
		os << endl;

		desc_cudaVideoChromaFormat_names.value2string(chromaFormatIDC, s);
		PRINT_DEC(chromaFormatIDC)
			os << " (" << s << ")";
		os << endl;
	} // codec_is_h264

	PRINT_DEC(separateColourPlaneFlag)
	os << endl;

	PRINT_DEC(output_sei_BufferPeriod)
	os << ",   ";

	PRINT_DEC(output_sei_PictureTime)
	os << endl;
	desc_nv_enc_mv_precision_names.value2string(mvPrecision, s);
	PRINT_DEC(mvPrecision)
	os << " (" << s << ")";
	os << endl;

	PRINT_DEC(aud_enable)
	os << endl;

	PRINT_DEC(report_slice_offsets)
	os << endl;

	PRINT_DEC(enableSubFrameWrite)
	os << endl;

	PRINT_DEC(disableDeblock)
	os << endl;

	PRINT_DEC(disable_ptd)
	os << endl;

	if (codec_is_h264) {
		desc_nv_enc_adaptive_transform_names.value2string(adaptive_transform_mode, s);
		PRINT_DEC(adaptive_transform_mode)
			os << " (" << s << ")";
		os << endl;

		desc_nv_enc_h264_bdirect_mode_names.value2string(bdirectMode, s);
		PRINT_DEC(bdirectMode)
			os << " (" << s << ")";
		os << endl;
	}

	PRINT_DEC(maxWidth)
	os << ", ";

	PRINT_DEC(maxHeight)
	os << endl;

	PRINT_DEC(curWidth)
	os << ", ";

	PRINT_DEC(curHeight)
	os << endl;

	PRINT_DEC(syncMode)
	os << endl;

	PRINT_DEC(interfaceType)
	os << endl;

	PRINT_DEC(disableCodecCfg)
	os << endl;

	PRINT_DEC(useMappedResources)
	os << endl;

	PRINT_DEC(max_ref_frames)
	os << ", ";

	PRINT_DEC(enableLTR)  // long term reference frames
	if (enableLTR) {
		os << ", ";
		PRINT_DEC(ltrNumFrames)

		if (codec == NV_ENC_H264) {
			os << ", ";
			PRINT_DEC(ltrTrustMode)
		}
	}
	os << endl;

	if (codec_is_h264) {
		PRINT_DEC(qpPrimeYZeroTransformBypassFlag)
			os << endl;
	}

	stringout = os.str();
}

void CNvEncoder::DestroyEncodeSession() {
	NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

	if ( (m_hEncoder != NULL) && (m_pEncodeAPI != NULL) )
		nvStatus = m_pEncodeAPI->nvEncDestroyEncoder( m_hEncoder );

	// since session is destroyed, clear the encoder-caps struct
	memset( (void *)&m_nv_enc_caps, 0, sizeof(m_nv_enc_caps) );
	m_hEncoder = NULL;
}