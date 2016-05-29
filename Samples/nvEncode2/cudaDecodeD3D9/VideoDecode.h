#ifndef NV_VIDEODECODE_H
#define NV_VIDEODECODE_H

#include <cstring>
#include <cassert>
#include <string>

#include <cuda.h>
//#include <cudad3d9.h>
#include <builtin_types.h>

#include "FrameQueue.h"
#include "VideoSource.h"
#include "VideoParser.h"
#include "VideoDecoder.h"

//#include "cudaProcessFrame.h"
//#include "cudaModuleMgr.h"

// CUDA utilities and system includes
#include <helper_functions.h>
#include <helper_cuda_drvapi.h>    // helper file for CUDA Driver API calls and error checking

#include "cudaProcessFrame.h"
#include "cudaModuleMgr.h"

// Include files
#include <memory>
#include <cassert>

#include <D3D9.h>
#include <D3D11.h>
#include <DXGI.h>

// my_printf(): annotate all printed msgs with 'file(line#)'
#define my_printf(...) printf("%s(%0u):", __FILE__, __LINE__), printf(__VA_ARGS__)

class videoDecode {
public:
    videoDecode(const int argc, char *argv[] );  // constructor
    ~videoDecode(); // destructor

    // these 2 vars are still accessed externally from main().  These should really be protected.
    bool                m_bQAReadback ;
    bool                m_bWaived     ;
    bool                m_bException  ;

protected: //////////////////////////////////////////
const char *sAppName     ;
const char *sAppFilename ;
const char *sSDKname     ;


#ifdef _DEBUG
#define ENABLE_DEBUG_OUT    1
#else
#define ENABLE_DEBUG_OUT    0
#endif

StopWatchInterface *m_frame_timer ;
StopWatchInterface *m_global_timer ;

int                 m_DeviceID    ;
bool                m_bWindowed   ;
bool                m_bDeviceLost ;
bool                m_bDone       ;
bool                m_bRunning    ;
bool                m_bAutoQuit   ;
bool                m_bUseVsync   ;
bool                m_bFrameRepeat;
bool                m_bFrameStep  ;
//bool                m_bQAReadback ;
bool                m_bFirstFrame ;
bool                m_bLoop       ;
bool                m_bUpdateCSC  ;
bool                m_bUpdateAll  ;
bool                m_bUseDisplay ; // this flag enables/disables video on the window
bool                m_bUseInterop ;
bool                m_bReadback   ; // this flag enables/disables reading back of a video from a window
bool                m_bIsProgressive ; // assume it is progressive, unless otherwise noted
//bool                m_bException  ;
//bool                m_bWaived     ;

int                 m_iRepeatFactor; // 1:1 assumes no frame repeats

int    m_argc ;
char **m_argv;

cudaVideoCreateFlags m_eVideoCreateFlags ;
CUvideoctxlock       m_CtxLock ;

float present_fps, decoded_fps, total_time ;

//D3DDISPLAYMODE        m_d3ddm;
//D3DPRESENT_PARAMETERS m_d3dpp;

// Direct3D9 interop
IDirect3D9        *m_pD3D9; // Used to create the D3D9Device
IDirect3DDevice9  *m_pD3D9Device;

// Direct3D11 interop
ID3D11Device      *m_pD3D11Device;
ID3D11DeviceContext *m_pD3D11DeviceContext;

IDXGIAdapter1     *m_DXGIAdapter;// Direct3D11

// These are CUDA function pointers to the CUDA kernels
CUmoduleManager   *m_pCudaModule;

CUmodule           cuModNV12toARGB  ;
CUfunction         gfpNV12toARGB    ;
CUfunction         gfpPassthru      ;

CUcontext          m_oContext ;
CUdevice           m_oDevice  ;

CUstream           m_ReadbackSID, m_KernelSID;

eColorSpace        m_eColorSpace ;
float              m_nHue        ;

// System Memory surface we want to readback to
BYTE          *m_bFrameData[2] ;
FrameQueue    *m_pFrameQueue   ;
VideoSource   *m_pVideoSource  ;
VideoParser   *m_pVideoParser  ;
VideoDecoder *m_pVideoDecoder ;

//ImageDX       *m_pImageDX      ;
void          *m_pImageDX      ;
CUdeviceptr    m_pInteropFrame[2] ; // if we're using CUDA malloc

std::string    m_sFileName;

char exec_path[256];

unsigned int m_nWindowWidth  ;
unsigned int m_nWindowHeight ;

unsigned int m_nClientAreaWidth  ;
unsigned int m_nClientAreaHeight ;

unsigned int m_nVideoWidth  ;
unsigned int m_nVideoHeight ;

unsigned int m_FrameCount ;
unsigned int m_DecodeFrameCount ;
unsigned int m_fpsCount ;      // FPS count for averaging
unsigned int m_fpsLimit ;

#ifdef WIN32
#ifndef STRCASECMP
#define STRCASECMP  _stricmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP _strnicmp
#endif
#else // Linux
#ifndef STRCASECMP
#define STRCASECMP  strcasecmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP strncasecmp
#endif
#endif


//LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
public:
bool Start();
HRESULT cleanup(bool bDestroyContext);
bool checkHW(char *name, char *gpuType, int dev);
int findGraphicsGPU(char *name);
void printStatistics();
void computeFPS(HWND hWnd);
HRESULT initCudaResources(int bUseInterop, int bTCC, 
    CUdevice *pcudevice = NULL, CUcontext *pcucontext = NULL);
HRESULT reinitCudaResources();
CUcontext GetCudaContext() const;
void displayHelp();
void parseCommandLineArguments();
bool loadVideoSource( CUVIDEOFORMAT *fmt );
void initCudaVideo();
void freeCudaResources(bool bDestroyContext);
unsigned int GetFrameDecodeCount() const {return m_DecodeFrameCount;};
bool copyDecodedFrameToTexture(
    unsigned int &nRepeats,
    CUVIDPICPARAMS *pDecodedPicInfo,   // decoded frame metainfo (I/B/P frame, etc.)
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[],       // handle - decoded framebuffer (or fields)
	unsigned int* pDecodedFrame_pitch  // #bytes per scanline
);
//HRESULT cleanup(bool bDestroyContext);
bool renderVideoFrame(HWND hWnd,
    CUVIDPICPARAMS *pDecodedPicInfo,   // decoded frame metainfo (I/B/P frame, etc.)
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[],       // handle - decoded framebuffer (or fields)
	unsigned int* pDecodedFrame_pitch  // #bytes per scanline
);
bool GetFrame(
    bool *got_frame,
    CUVIDPICPARAMS *pDecodedPicInfo,   // decoded frame metainfo (I/B/P frame, etc.)
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[],       // handle - decoded framebuffer (or fields)
	unsigned int* pDecodedFrame_pitch  // #bytes per scanline
);
bool GetFrameFinish(
    CUVIDPARSERDISPINFO *pDisplayInfo, // decoded frame information (pic-index)
    CUdeviceptr pDecodedFrame[]       // handle - decoded framebuffer (or fields)
);

bool initD3D9(HWND hWnd, const int unsigned width, const int unsigned height, int *pbTCC);

protected:
bool CreateD3D9device(HWND hWnd, const int unsigned adapternum, const int unsigned width, const int unsigned height);

}; // class VideoDecode

#endif // NV_VIDEODECODE_H
