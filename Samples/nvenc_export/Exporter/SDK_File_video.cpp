#include "SDK_File.h"
#include "SDK_File_video.h"
#include "SDK_Exporter_Params.h"

#include <Windows.h> // SetFilePointer(), WriteFile()
#include <sstream>  // ostringstream
#include <cstdio>

#include "SDK_Exporter.h" // fwrite_callback()
#include "CNVEncoderH264.h"
#include "CNVEncoderH265.h"

//////////////////////////////////////////////////////////////////////////////
//
// local-use data-structures (for use in this file only)
//

// These pixelformats are used for NVENC chromaformatIDC = NV12.
//    If the Adobe-app prefers BGRA instead of YUV420, then
//    switch to the 422 formats below.
const PrPixelFormat SupportedPixelFormats420[] = {
	PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709, // highest priority
	PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange,
	PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601,
	PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange,
	PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709,
	PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange,
	PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601,
	PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange
};

// These pixelformats are used for NVENC chromatformatIDC = NV12
//   (These are only used if YUV420 planar was attempted and failed.)
const PrPixelFormat SupportedPixelFormats422[] = {
	PrPixelFormat_YUYV_422_8u_709, // highest priority
	PrPixelFormat_UYVY_422_8u_709,
	PrPixelFormat_YUYV_422_8u_601,
	PrPixelFormat_UYVY_422_8u_601
};


// These pixelformats are used for NVENC chromaformatIDC = YUV444
//  (requires NV_ENC_CAPS_SUPPORT_YUV444_ENCODE == 1)
const PrPixelFormat SupportedPixelFormats444[] = {
	PrPixelFormat_VUYX_4444_8u_709, // highest priority
	PrPixelFormat_VUYA_4444_8u_709,
	PrPixelFormat_VUYX_4444_8u,
	PrPixelFormat_VUYA_4444_8u
};

// These pixelformats are used for NVENC chromaformatIDC = RGB
//  nvenc_export must convert this RGB to YUV444
//  (requires NV_ENC_CAPS_SUPPORT_YUV444_ENCODE == 1)
const PrPixelFormat SupportedPixelFormatsRGB[] = {
	PrPixelFormat_BGRX_4444_32f, // highest priority
	PrPixelFormat_BGRA_4444_32f
};

//////////////////////////////////////////////////////////////////////////////
//
// local functions (for use in this file only)
//

prSuiteError
NVENC_initialize_h264_session(
	const PrPixelFormat		PixelFormat0, // pixelformat used on 1st frame of video
	exDoExportRec * const	exportInfoP
);

prMALError RenderAndWriteVideoFrame(
	const bool				isFrame0,  // Is this the 1st frame of the render?
	const bool				dont_encode, // if true, don't submit frame to CNvEncoderH264
	const PrTime			videoTime,
	exDoExportRec			*exportInfoP
	);


prSuiteError
NVENC_export_FrameCompletionFunction(
	const csSDK_uint32		inWhichPass,
	const csSDK_uint32		inFrameNumber,
	const csSDK_uint32		inFrameRepeatCount,
	PPixHand				inRenderedFrame,
	void*					inCallbackData
);

//////////////////////////////////////////////////////////////////////////////

prSuiteError
NVENC_initialize_h264_session(const PrPixelFormat PixelFormat0, exDoExportRec * const exportInfoP)
{
	ExportSettings	*mySettings = reinterpret_cast<ExportSettings *>(exportInfoP->privateData);
	NV_ENC_CONFIG_H264_VUI_PARAMETERS vui;   // Encoder's video-usability struct (for color info)
	NV_ENC_CONFIG_HEVC_VUI_PARAMETERS vui265;// Encoder's video-usability struct (for color info)
	CNvEncoder_color_s color_metadata;
	NVENCSTATUS nvencstatus = NV_ENC_SUCCESS; // OpenSession() return code
	HRESULT hr;

	//
	// The video-usability (VUI) struct describes the color-space/format of the encoded H264 video.
	//

	// Note, although Premiere allows on-the-fly changing of the render
	// PrPixelFormat, NVENC only sets the VUI once, at the start of the render.
	// Therefore, the NVENC-plugin MUST ensure the video-render uses the
	// same PrPixelFormat for the entire clip. 

	memset(&vui, 0, sizeof(NV_ENC_CONFIG_H264_VUI_PARAMETERS));
	memset(&vui265, 0, sizeof(NV_ENC_CONFIG_HEVC_VUI_PARAMETERS));

#define COPY_VUI_FIELD(f)  vui265.f = vui.f;

	vui.videoSignalTypePresentFlag = 1; // control: vui.videoFormat is valid
	COPY_VUI_FIELD(videoSignalTypePresentFlag)

	// videoFormat
	// -----------
	//1 PAL
	// 2 NTSC
	//3 SECAM
	//4 MAC
	//5 Unspecified video format
	switch (mySettings->SDKFileRec.tvformat) {
	case 0: // NTSC
		vui.videoFormat = 2;
		break;
	case 1: // PAL
		vui.videoFormat = 1;
		break;
	case 2: // SECAM
		vui.videoFormat = 3;
		break;
	default: // unknown
		vui.videoFormat = 5; // unspecified
	}
	COPY_VUI_FIELD(videoFormat)

	vui.colourDescriptionPresentFlag = 1; // control: colourMatrix, primaries, etc. are valid
	COPY_VUI_FIELD(colourDescriptionPresentFlag)

	// Matrix
	//  0 = GBR
	//  1 = BT-709.5
	//  2 = unspecified
	//  3 = Rserved
	//  4 = US FC
	//  5 = ITU-R Rec. BT.470-6 System B, G (historical), BT.601-6
	//  6 = BT 601.6 525
	//  7 = SMPTE 240M
	//  8 = E-19 to E-33
	switch (PixelFormat0) {
	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange:
	case PrPixelFormat_YUYV_422_8u_709:
	case PrPixelFormat_UYVY_422_8u_709:
	case PrPixelFormat_VUYX_4444_8u_709:
	case PrPixelFormat_VUYA_4444_8u_709:
	case PrPixelFormat_VUYP_4444_8u_709:
	case PrPixelFormat_VUYA_4444_32f_709:
	case PrPixelFormat_VUYX_4444_32f_709:
	case PrPixelFormat_VUYP_4444_32f_709:
	case PrPixelFormat_V210_422_10u_709:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709_FullRange:
		vui.colourMatrix = 1;
		color_metadata.color_known = true;
		color_metadata.color = true; // Bt709
		break;

	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601:
	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange:
	case PrPixelFormat_YUYV_422_8u_601:
	case PrPixelFormat_UYVY_422_8u_601:
	case PrPixelFormat_V210_422_10u_601:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601_FullRange:
		vui.colourMatrix = 6;
		color_metadata.color_known = true;
		color_metadata.color = false; // Bt601
		break;
	default:
		color_metadata.color_known = false;
		vui.colourMatrix = 2; // unspecified
	}
	COPY_VUI_FIELD(colourMatrix)

	// colourPrimaries
	//  0 =reserved
	//  1 = BT 709.5
	//  2 = unspecified
	//  3 = reserved
	//  ...
	vui.colourPrimaries = vui.colourMatrix;
	vui.transferCharacteristics = vui.colourPrimaries;
	
	COPY_VUI_FIELD(colourPrimaries)
	COPY_VUI_FIELD(transferCharacteristics)

	color_metadata.range_known = false;

	// Is the video full-range (0..255)?
	switch (PixelFormat0) {
	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709_FullRange:
	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601_FullRange:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709_FullRange:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709_FullRange:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601_FullRange:
		color_metadata.range_known = true;
		color_metadata.range_full = true;
		vui.videoFullRangeFlag = 1;
		break;
	default:
		vui.videoFullRangeFlag = 0; // off
	}
	COPY_VUI_FIELD(videoFullRangeFlag)

	switch (PixelFormat0) {
	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_601:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG2_FRAME_PICTURE_PLANAR_8u_601:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG4_FIELD_PICTURE_PLANAR_8u_601:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_709:
	case PrPixelFormat_YUV_420_MPEG2_FIELD_PICTURE_PLANAR_8u_601:
		color_metadata.range_known = true;
		color_metadata.range_full = false;
	}

	//////////////////////////////
	//
	// Create H.264 based encoder -
	//
	//  ... actually, this is already done in SDK_Exporter (exSDKBeginInstance())

	//mySettings->p_NvEncoder = new CNvEncoderH264();
	//mySettings->p_NvEncoder->Register_fwrite_callback(fwrite_callback);
	if (mySettings->p_NvEncoder == NULL) {
		printf("\nnvEncoder Error: NVENC H.264 encoder == NULL!\n");
		assert(0); // NVENC H.264 p_NvEncoder is NULL
		return malUnknownError;
	}

	// Store the encoding job's context-info in the p_NvEncoder object,
	//    so that the fwrite_callback() will write to the correct fileHandle.
	mySettings->p_NvEncoder->m_privateData = (void *)exportInfoP;

	// Section 2.1 (Opening an Encode Session on nDeviceID)
	hr = mySettings->p_NvEncoder->OpenEncodeSession(
		mySettings->NvEncodeConfig,
		mySettings->NvGPUInfo.device,
		nvencstatus);

	// Check for a expired NVENC license-key:
	// --------------------------------------
	//  this check is here because there is no error-handling in the plugin, and
	//  this specific error-message will occur if NVidia retires/revokes the
	//  free 'trial license key' which is used by this plugin.
	if (nvencstatus == NV_ENC_ERR_INCOMPATIBLE_CLIENT_KEY)
		NVENC_errormessage_bad_key(mySettings);

	if (hr != S_OK) {

		printf("\nnvEncoder Error: NVENC H.264 encoder OpenEncodeSession failure!\n");
		assert(0); // NVENC H.264 encoder OpenEncodeSession failure
		return malUnknownError;
	}

	void * pvui = NULL; // pointer to VUI-struct

	// Select the correct VUI-struct
	switch (mySettings->NvEncodeConfig.codec) {
		case NV_ENC_H264 : pvui = &vui;
			break;

		case NV_ENC_H265 : pvui = &vui265;
			break;

		default:
			;
	}

	hr = mySettings->p_NvEncoder->InitializeEncoderCodec( pvui );
	if (hr != S_OK) {
		printf("\nnvEncoder Error: NVENC H.264 encoder initialization failure! Check input params!\n");
		assert(0); // NVENC H.264 encoder InitializeEncoderH264 failure
		return malUnknownError;
	}

	mySettings->p_NvEncoder->set_color_metadata(color_metadata);

	return hr;
}

// callback function for DoMultiPassExportLoop
prSuiteError NVENC_export_FrameCompletionFunction(
	const csSDK_uint32 inWhichPass,
	const csSDK_uint32 inFrameNumber,
	const csSDK_uint32 inFrameRepeatCount,
	PPixHand inRenderedFrame,
	void* inCallbackData
	)
{
	exDoExportRec	*exportInfoP = reinterpret_cast<exDoExportRec*>(inCallbackData);
	csSDK_uint32	exID = exportInfoP->exporterPluginID;
	ExportSettings	*mySettings = reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	char			*frameBufferP = NULL;

	csSDK_int32		rowbytes;
	exParamValues	width, height, temp_param;
	PrPixelFormat   rendered_pixelformat; // pixelformat of the current video-frame

	EncodeFrameConfig nvEncodeFrameConfig = { 0 };

	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &width);
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &height);

	nvEncodeFrameConfig.height = height.value.intValue;
	nvEncodeFrameConfig.width = width.value.intValue;

	mySettings->ppixSuite->GetPixelFormat(inRenderedFrame, &rendered_pixelformat);
	const bool		adobe_yuv420 =      // Adobe is sending Planar YUV420 (instead of packed-pixel 422/444)
		PrPixelFormat_is_YUV420(rendered_pixelformat);

	if (rendered_pixelformat != mySettings->rendered_PixelFormat0) {
		// TODO: ERROR!
	}

	// CNvEncoderH264 must know the source-video's pixelformat, in order to
	//    convert it into an NVENC compatible format (NV12 or YUV444)
	nvEncodeFrameConfig.ppro_pixelformat = rendered_pixelformat;
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv420 = PrPixelFormat_is_YUV420(rendered_pixelformat);
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv444 = PrPixelFormat_is_YUV444(rendered_pixelformat);
	nvEncodeFrameConfig.ppro_pixelformat_is_uyvy422 =
		(rendered_pixelformat == PrPixelFormat_UYVY_422_8u_601) ||
		(rendered_pixelformat == PrPixelFormat_UYVY_422_8u_709);
	nvEncodeFrameConfig.ppro_pixelformat_is_yuyv422 =
		(rendered_pixelformat == PrPixelFormat_YUYV_422_8u_601) ||
		(rendered_pixelformat == PrPixelFormat_YUYV_422_8u_709);
	nvEncodeFrameConfig.ppro_pixelformat_is_rgb444f = 
		(rendered_pixelformat == PrPixelFormat_BGRA_4444_32f) ||
		(rendered_pixelformat == PrPixelFormat_BGRX_4444_32f);

	// NVENC picture-type: Interlaced vs Progressive
	//
	// Note that the picture-type must match the selected encoding-mode.
	// In interlaced or MBAFF-mode, NVENC still requires all sourceFrames to be tagged as fieldPics
	// (even if the sourceFrame is truly progressive.)
	mySettings->exportParamSuite->GetParamValue(exID, 0, ParamID_FieldEncoding, &temp_param);
	nvEncodeFrameConfig.fieldPicflag = (temp_param.value.intValue == NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME) ?
		false :
		true;
	nvEncodeFrameConfig.topField = true;
	if (nvEncodeFrameConfig.fieldPicflag) {
		PrSDKExportInfoSuite	*exportInfoSuite = mySettings->exportInfoSuite;
		PrParam	seqFieldOrder;  // video-sequence field order (top_first/bottom_first)
		exportInfoSuite->GetExportSourceInfo(exID,
			kExportInfo_VideoFieldType,
			&seqFieldOrder);
		nvEncodeFrameConfig.topField = (seqFieldOrder.mInt32 == prFieldsLowerFirst) ? false : true;
	}

	if (adobe_yuv420) {
		// 4:2:0 planar (all 3 yuv[] pointers used)
		size_t       rowsize;
		csSDK_uint32 stride[3]; // #bytes per row (for each of Y/U/V planes)

		// Standard ppixSuite doesn't work with Planar frame(s),
		//    must use ppix2Suite to access them
		mySettings->ppix2Suite->GetYUV420PlanarBuffers(
			inRenderedFrame,
			PrPPixBufferAccess_ReadOnly,
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[0]),
			&stride[0],
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[1]),
			&stride[1],
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[2]),
			&stride[2]
			);
		nvEncodeFrameConfig.stride[0] = stride[0];
		nvEncodeFrameConfig.stride[1] = stride[1];
		nvEncodeFrameConfig.stride[2] = stride[2];
		mySettings->ppix2Suite->GetSize(inRenderedFrame, &rowsize);
		rowbytes = rowsize;
	}
	else {
		// Packed pixel framedata
		//   ... Either 16bpp 4:2:2 or 32bpp 4:4:4
		//
		// In packed-pixel format, only pointer[0] is used,(1 & 2 aren't)
		mySettings->ppixSuite->GetPixels(inRenderedFrame,
			PrPPixBufferAccess_ReadOnly,
			&frameBufferP);
		mySettings->ppixSuite->GetRowBytes(inRenderedFrame, &rowbytes);
		nvEncodeFrameConfig.stride[0] = rowbytes; // Y-plane

		nvEncodeFrameConfig.yuv[0] = reinterpret_cast<unsigned char *>(&frameBufferP[0]); // Y-plane
		nvEncodeFrameConfig.stride[1] = 0; // U-plane not used
		nvEncodeFrameConfig.stride[2] = 0; // V-plane not used
		nvEncodeFrameConfig.yuv[1] = NULL;
		nvEncodeFrameConfig.yuv[2] = NULL;
	}

	// Submit the Adobe rendered frame to NVENC:
	//   (1) If NvEncoder is operating in 'async_mode', then the call will return as soon
	//       as the frame is placed in the encodeQueue.
	//   (2) if NvEncoder is operating in 'sync_mode', then call will not return until
	//       NVENC has completed encoding of this frame.
	//HRESULT hr = mySettings->p_NvEncoder->EncodeFrame( &nvEncodeFrameConfig, false );
	HRESULT hr = mySettings->p_NvEncoder->EncodeFramePPro(
		&nvEncodeFrameConfig,
		false // flush
		);

	return (hr == S_OK) ? malNoError : // no error
		malUnknownError;
}

///////////////////////////////////////////////////////////////////////////////

// Returns malNoError if successful, or comp_CompileAbort if user aborted
prMALError RenderAndWriteVideoFrame(  // export a single frame of video to NVENC encoder
	const bool					isFrame0,    // are we rendering the first-frame of the session?
	const bool					dont_encode, // if true, then don't send video to CNvEncoderH264
	const PrTime				videoTime,
	exDoExportRec				*exportInfoP)
{
	string						errstr; // error-string
	csSDK_int32					resultS = malNoError;
	csSDK_uint32				exID = exportInfoP->exporterPluginID;
	ExportSettings				*mySettings = reinterpret_cast<ExportSettings *>(exportInfoP->privateData);
	csSDK_int32					rowbytes = 0;
	//	csSDK_int32					renderedPixelSize		= 0;
	exParamValues				width,
		height,
		pixelAspectRatio,
		fieldType,
		temp_param;
	PrPixelFormat				renderedPixelFormat;
	csSDK_uint32				bytesToWriteLu = 0;
	char						*frameBufferP = NULL,
		*f32BufferP = NULL,
		*frameNoPaddingP = NULL,
		*v410Buffer = NULL;
	HWND mainWnd = mySettings->windowSuite->GetMainWindow();
	SequenceRender_ParamsRec renderParms;
	csSDK_int32 nvenc_pixelformat; // selected NVENC input pixelFormat (user-parameter from plugin)
	mySettings->exportParamSuite->GetParamValue(exID, 0, ParamID_chromaFormatIDC, &temp_param);
	nvenc_pixelformat = temp_param.value.intValue;

	// Frmaebuffer format flags: what chromaformat video is Adobe-app outputting?
	//   Exactly one of the following flags must be true. (Flags will be updated later)
	bool adobe_yuv444 = (nvenc_pixelformat == cudaVideoChromaFormat_444);// a 4:4:4 format is in use (instead of 4:2:0)
	bool adobe_yuv420 = false;// (Adobe is outputing YUV 4:2:0, i.e. 'YV12')
	bool adobe_yuv422 = false;// (Adobe is outputing YUV 4:2:2, i.e. 'YUYV')
	
	if (isFrame0) {
		// First frame of render:
		// ----------------------
		// For frame#0, PrPixelFormat advertisement depends on whether 'autoselect' is enabled:

		// (1) if autoselect_PixelFormat0==true
		//     -----------------------------
		//     Advertise *ALL* supported PrPixelFormats that match the user-selected chromaFormatIDC (420 vs 444)
		//
		// (2) if autoselect_PixelFormat0==false
		//     -----------------------------
		//     Advertise *ONLY* the user-selected PixelFormat
		//
		// Why?
		// NVENC does not allow the color-format (Bt-601 vs 709, etc.) to be changed during an
		// encode-session, whereas Adobe may change color-format on a frame-by-frame basis..  Thus, during 
		// the first frame of the render, the NVENC plugin advertises all YUV PixelFormats compatible with 
		// the user-selected chromaFormatIDC.  
		// 
		// This lets PPro select the 'best' matching PixelFormat (for the first frame.)  For all subsequent
		// video-frames, the plugin forces Adobe to use same format that was used on frame#0 (PixelFormat0),
		// in order to satisfy NVENC's restriction.

		if (mySettings->forced_PixelFormat0) {
			// autoselect-disabled:  only the user-select pixelformat will be advertised.
			renderParms.inRequestedPixelFormatArray = &(mySettings->requested_PixelFormat0);
			renderParms.inRequestedPixelFormatArrayCount = 1;
		}
		else if (adobe_yuv444) {
			// Packed Pixel YUV 4:4:4 (24bpp + 8bit alpha, NVENC doesn't use the alpha-channel)
			//
			// Assume NVENC is configured to encode 4:4:4 video. 
			// (because we never send packed YUV 4:4:4 to NVENC. in 4:2:0 H264 encoding mode)
			renderParms.inRequestedPixelFormatArray = SupportedPixelFormats444;
			renderParms.inRequestedPixelFormatArrayCount = sizeof(SupportedPixelFormats444) / sizeof(SupportedPixelFormats444[0]);
		}
		else {
			// Assume NVENC is configured to encode 4:2:0 video.
			//
			// Planar YUV 4:2:0
			renderParms.inRequestedPixelFormatArray = SupportedPixelFormats420;
			renderParms.inRequestedPixelFormatArrayCount = sizeof(SupportedPixelFormats420) / sizeof(SupportedPixelFormats420[0]);
		}
	}
	else {
		// All later frames:
		// -----------------
		// Re-use the same PrPixelFormat that Adobe used to render the first frame. 
		renderParms.inRequestedPixelFormatArray = (&mySettings->rendered_PixelFormat0);
		renderParms.inRequestedPixelFormatArrayCount = 1;
	}

	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &width);
	renderParms.inWidth = width.value.intValue;
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &height);
	renderParms.inHeight = height.value.intValue;
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoAspect, &pixelAspectRatio);
	renderParms.inPixelAspectRatioNumerator = pixelAspectRatio.value.ratioValue.numerator;
	renderParms.inPixelAspectRatioDenominator = pixelAspectRatio.value.ratioValue.denominator;

	renderParms.inRenderQuality = kPrRenderQuality_High;
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoFieldType, &fieldType);
	renderParms.inFieldType = fieldType.value.intValue;
	// By setting this to false, we basically leave deinterlacing up to the host logic
	// We could set it to true if we wanted to force deinterlacing
	renderParms.inDeinterlace = kPrFalse;
	renderParms.inDeinterlaceQuality = kPrRenderQuality_High;
	renderParms.inCompositeOnBlack = kPrFalse;

	SequenceRender_GetFrameReturnRec renderResult;

	if (isFrame0) {
		// Frame#0 special: we submit a table of requested/acceptable PrPixelFormats,
		//                  and let adobe automatically select one for us.
		resultS = mySettings->sequenceRenderSuite->RenderVideoFrame(
			mySettings->videoRenderID,
			videoTime,
			&renderParms,
			kRenderCacheType_None,	// [TODO] Try different settings
			&renderResult);

		// If YUV420-video failed, make another attempt with YUV422
		if (PrSuiteErrorFailed(resultS) && !adobe_yuv444) {
			// We attempted {chromaFormat: YUV420}, but the videorender failed.
			//    ... so retry the videorender with YUV 4:2:2 packed-pixel instead
			renderParms.inRequestedPixelFormatArray = SupportedPixelFormats422;
			renderParms.inRequestedPixelFormatArrayCount = sizeof(SupportedPixelFormats422) / sizeof(SupportedPixelFormats422[0]);

			resultS = mySettings->sequenceRenderSuite->RenderVideoFrame(
				mySettings->videoRenderID,
				videoTime,
				&renderParms,
				kRenderCacheType_None,	// [TODO] Try different settings
				&renderResult);
		}

		// If YUV422 or YUV444 failed, make a final attempt with RGBf
		if (PrSuiteErrorFailed(resultS)) { // 2nd-attempt (RGBf)
			ostringstream o;
			PrParam		hasVideo, seqWidth, seqHeight;
			// 2nd videorender attempt failed {chromaFormat: YUV422},
			//   ... retry one last time with RGB32f.

			renderParms.inRequestedPixelFormatArray = SupportedPixelFormatsRGB;
			renderParms.inRequestedPixelFormatArrayCount = sizeof(SupportedPixelFormatsRGB) / sizeof(SupportedPixelFormatsRGB[0]);

			resultS = mySettings->sequenceRenderSuite->RenderVideoFrame(
				mySettings->videoRenderID,
				videoTime,
				&renderParms,
				kRenderCacheType_None,	// [TODO] Try different settings
				&renderResult);

			if (PrSuiteErrorFailed(resultS)) { 
				// Give up!

				// 2 problems:
				// -----------
				// (1) Checking the source-video and framesize here doesn't work, because
				//     Adobe presents a exportInfoSuite with modified properties that are
				//     'conformed' to the user-selected output-size. 

				// (2) In Premeire CS6 and CC, if CUDA hardware-acceleration is enabled,
				//     Adobe's video-renderer will refuse to resize Pixelformat YUV.
				//     (It seems that packed-pixel RGB 32bpp is the only format natively
				//      resized by the CUDA-accelerated MPE.)
				mySettings->exportInfoSuite->GetExportSourceInfo(exID,
					kExportInfo_SourceHasVideo,
					&hasVideo);
				mySettings->exportInfoSuite->GetExportSourceInfo(exID,
					kExportInfo_VideoWidth,
					&seqWidth);
				mySettings->exportInfoSuite->GetExportSourceInfo(exID,
					kExportInfo_VideoHeight,
					&seqHeight);/*
								if ( (hasVideo.mBool == kPrTrue) &&
								((seqWidth.mInt32 != renderParms.inWidth ) ||
								(seqHeight.mInt32 != renderParms.inHeight )) )
								{
								*/
				// 
				o << std::endl << std::endl;
				o << "If CUDA hardware-acceleration is enabled, Adobe may fail to resize the video." << std::endl;
				o << "Either disable acceleration, or change the chosen output-size to match " << std::endl;
				o << "the source's size ( " << std::dec << renderParms.inWidth << " x " << std::dec;
				o << renderParms.inHeight << " )" << std::endl;
				/*
				acceleratio Source video_width / height = " << std::dec << seqWidth.mInt32
				<< " / " << std::dec << seqHeight.mInt32 << std::endl;
				o << "Output video_width / height = " << std::dec << renderParms.inWidth
				<< " / " << std::dec << renderParms.inHeight << std::endl;
				o << "NVENC_export cannot resize the source-video.  Please change the output video size to match the source size!";
				*/
				//			} 

				MessageBox(GetLastActivePopup(mainWnd),
					o.str().c_str(),
					EXPORTER_NAME,
					MB_ICONERROR
					);

				return resultS;
			} // PrSuiteErrorFailed (Give up!)
		} // PrSuiteErrorFailed (2nd-attempt RGBf)

		// Record the pixelFormat of the 1st frame of output. 
		// (NVENC-plugin will lock the remainder of the video-render to this format.)
		resultS = mySettings->ppixSuite->GetPixelFormat(renderResult.outFrame, &renderedPixelFormat);
		if (PrSuiteErrorFailed(resultS)) {
			ostringstream o;

			Check_prSuiteError(resultS, errstr);
			o << __FILE__ << "(" << std::dec << __LINE__ << "): ";
			o << "GetPixelFormat failed with error-value: " << errstr;
			o << std::endl;
			MessageBox(GetLastActivePopup(mainWnd),
				o.str().c_str(),
				EXPORTER_NAME,
				MB_ICONERROR
				);

			return resultS;
		}

		mySettings->rendered_PixelFormat0 = renderedPixelFormat;

		// update the chroma-format flags: we're either in 422 or 420 mode
		//
		// 
		if (PrPixelFormat_is_YUV420(renderedPixelFormat))
			adobe_yuv420 = true;
		else if (PrPixelFormat_is_YUV422(renderedPixelFormat))
			adobe_yuv422 = true;
	}
	else {
		// All later frames: force Adobe to conform to the *same* PrPixelFormat
		//                  (for video consistency)
		resultS = mySettings->sequenceRenderSuite->RenderVideoFrameAndConformToPixelFormat(
			mySettings->videoRenderID,
			videoTime,
			&renderParms,
			kRenderCacheType_None,	// [TODO] Try different settings
			mySettings->rendered_PixelFormat0, // *force* all remaining frames to the same PrPixelForamt
			&renderResult);
		Check_prSuiteError(resultS, errstr);
	}

	// If user hit cancel
	if (resultS == suiteError_CompilerCompileAbort)
	{
		// just return here
		return resultS;
	}

	EncodeFrameConfig nvEncodeFrameConfig = { 0 };
	nvEncodeFrameConfig.height = height.value.intValue;
	nvEncodeFrameConfig.width = width.value.intValue;

	// Update the PrPixelformat flags (what is the Adobe-app actually sending us?)
	//bool adobe_rgb32 = PrPixelFormat_is_RGB32f(mySettings->rendered_PixelFormat0);
	adobe_yuv444 = PrPixelFormat_is_YUV444(mySettings->rendered_PixelFormat0) &&
		(nvenc_pixelformat == cudaVideoChromaFormat_444);
	adobe_yuv420 = PrPixelFormat_is_YUV420(mySettings->rendered_PixelFormat0);
	adobe_yuv422 = PrPixelFormat_is_YUV422(mySettings->rendered_PixelFormat0);

	// CNvEncoderH264 must know the source-video's pixelformat, in order to
	//    convert it into an NVENC compatible format (NV12 or YUV444)
	nvEncodeFrameConfig.ppro_pixelformat = mySettings->rendered_PixelFormat0;
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv420 = PrPixelFormat_is_YUV420(mySettings->rendered_PixelFormat0);
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv444 = PrPixelFormat_is_YUV444(mySettings->rendered_PixelFormat0);
	nvEncodeFrameConfig.ppro_pixelformat_is_uyvy422 =
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_UYVY_422_8u_601) ||
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_UYVY_422_8u_709);
	nvEncodeFrameConfig.ppro_pixelformat_is_yuyv422 =
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_YUYV_422_8u_601) ||
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_YUYV_422_8u_709);
	nvEncodeFrameConfig.ppro_pixelformat_is_rgb444f = PrPixelFormat_is_RGB32f(mySettings->rendered_PixelFormat0);

	// NVENC picture-type: Interlaced vs Progressive
	//
	// Note that the picture-type must match the selected encoding-mode.
	// In interlaced or MBAFF-mode, NVENC still requires all sourceFrames to be tagged as fieldPics
	// (even if the sourceFrame is truly progressive.)
	mySettings->exportParamSuite->GetParamValue(exID, 0, ParamID_FieldEncoding, &temp_param);
	nvEncodeFrameConfig.fieldPicflag = (temp_param.value.intValue == NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME) ?
		false :
		true;
	nvEncodeFrameConfig.topField = true; // default
	if (nvEncodeFrameConfig.fieldPicflag) {
		PrSDKExportInfoSuite	*exportInfoSuite = mySettings->exportInfoSuite;
		PrParam	seqFieldOrder;  // video-sequence field order (top_first/bottom_first)
		exportInfoSuite->GetExportSourceInfo(exID,
			kExportInfo_VideoFieldType,
			&seqFieldOrder);
		nvEncodeFrameConfig.topField = (seqFieldOrder.mInt32 == prFieldsLowerFirst) ? false : true;
	}
	
	//
	// Get critical properties of the Adobe rendered videoframe:
	//
	//   Stride  (#bytes per scanline of video)
	//   Pointer (start-address of the videoframe's pixeldata)

	if ( adobe_yuv420 ) {
		// Adobe's "Planar420" surface format requires special handling (compared
		// to the packed-pixel formats)
		// 
		// In particular, there are 3 different surface pointers (and 3 stride values)
		// for Planar-4:2:0.  These are queried through the ppix2Suite.

		size_t       rowsize;
		csSDK_uint32 stride[3]; // #bytes per row (for each of Y/U/V planes)

		mySettings->ppix2Suite->GetYUV420PlanarBuffers(
			renderResult.outFrame,
			PrPPixBufferAccess_ReadOnly,
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[0]),
			&stride[0],
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[1]),
			&stride[1],
			reinterpret_cast<char **>(&nvEncodeFrameConfig.yuv[2]),
			&stride[2]
			);
		nvEncodeFrameConfig.stride[0] = stride[0];
		nvEncodeFrameConfig.stride[1] = stride[1];
		nvEncodeFrameConfig.stride[2] = stride[2];
		mySettings->ppix2Suite->GetSize(renderResult.outFrame, &rowsize);
		rowbytes = rowsize;
	}
	else {
		// The packed-pixel formats that nvenc_export supports (YUV444, YUV422, RGB32)
		// are queried pretty much the same way (through ppixSuite).
		//
		// ...only pointer[0] and stride0 is used, (pointer[1] & [2] aren't used)

		mySettings->ppixSuite->GetPixels(renderResult.outFrame,
			PrPPixBufferAccess_ReadOnly,
			&frameBufferP);
		mySettings->ppixSuite->GetRowBytes(renderResult.outFrame, &rowbytes);
		nvEncodeFrameConfig.stride[0] = rowbytes; // Y-plane

		nvEncodeFrameConfig.yuv[0] = reinterpret_cast<unsigned char *>(&frameBufferP[0]); // Y-plane
		nvEncodeFrameConfig.stride[1] = 0; // U-plane not used
		nvEncodeFrameConfig.stride[2] = 0; // V-plane not used
		nvEncodeFrameConfig.yuv[1] = NULL;
		nvEncodeFrameConfig.yuv[2] = NULL;
	}

	// Submit the Adobe rendered frame to NVENC:
	//   (1) If NvEncoder is operating in 'async_mode', then the call will return as soon
	//       as the frame is placed in the encodeQueue.
	//   (2) if NvEncoder is operating in 'sync_mode', then call will not return until
	//       NVENC has completed encoding of this frame.
	HRESULT hr = S_OK;
	if (!dont_encode)
		hr = mySettings->p_NvEncoder->EncodeFramePPro(
		&nvEncodeFrameConfig,
		false  // flush?
		);

	// Now that buffer is written to disk, we can dispose of memory
	mySettings->ppixSuite->Dispose(renderResult.outFrame);

	return (hr == S_OK) ? resultS : resultS;
}

///////////////////////////////////////////////////////////////////////////////

void
NVENC_switch_codec(ExportSettings *lRec)
{
	// temporary kludge:
	// -----------------
	//  When the exporter is initialized, it always constructs an
	// CNvEncoderH264 object.  Rather than dynamically destroying/ewing this
	// object during user-interface operation, the switchover is done when
	// the export-operation begins.

	const int GPUIndex = lRec->NvGPUInfo.device;
	nv_enc_caps_s nv_enc_caps;

	if (lRec->p_NvEncoder) {
		lRec->p_NvEncoder->DestroyEncoder();
		delete lRec->p_NvEncoder;
	}

	switch (lRec->NvEncodeConfig.codec) {
	case NV_ENC_H265:
		lRec->p_NvEncoder = new CNvEncoderH265();
		break;

	default: // NV_ENC_H264
		lRec->p_NvEncoder = new CNvEncoderH264();
		break;
	}

	// Initialize mySettings->NvEncodeConfig
	lRec->p_NvEncoder->initEncoderConfig(&lRec->NvEncodeConfig);
	lRec->p_NvEncoder->Register_fwrite_callback(fwrite_callback);

	// Fill p_NvEncoder's capability-tables with GPU-reported info.
	lRec->p_NvEncoder->QueryEncodeSessionCodec(GPUIndex, lRec->NvEncodeConfig.codec, nv_enc_caps);
}

///////////////////////////////////////////////////////////////////////////////

prMALError RenderAndWriteAllVideo(
	exDoExportRec	*exportInfoP,
	float			progress,
	float			videoProgress,
	PrTime			*exportDuration)
{
	prMALError		result = malNoError;
	csSDK_uint32	exID = exportInfoP->exporterPluginID;
	ExportSettings	*mySettings = reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	exParamValues	ticksPerFrame,
		width,
		height,
		pixelAspectRatio;
	PrTime			segmentEnd;
	prtPlaycode		playcode;
	//PrClipID		clipID;
	//ClipFrameFormat	frameFormat;
	//PPixHand		tempFrame;

	// 'PushMode' rendering is new for Adobe CS6 API
	bool			UsePushMode = true; // Assume we can use 'Push mode' (until we know otherwise)

	// logmessage generation
	std::wostringstream os;
	prUTF16Char eventTitle[256];
	prUTF16Char eventDesc[512];

	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoFPS, &ticksPerFrame);
	mySettings->sequenceRenderSuite->MakeVideoRenderer(exID,
		&mySettings->videoRenderID,
		ticksPerFrame.value.timeValue);

	// The following code is in progress to test the new custom pixel format support
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoWidth, &width);
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoHeight, &height);
	mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEVideoAspect, &pixelAspectRatio);
	mySettings->videoSequenceParser = new VideoSequenceParser(mySettings->spBasic,
		width.value.intValue,
		height.value.intValue,
		pixelAspectRatio.value.ratioValue.numerator,
		pixelAspectRatio.value.ratioValue.denominator);
	mySettings->videoSequenceParser->ParseSequence(exportInfoP->timelineData);

	copyConvertStringLiteralIntoUTF16(L"Note from RenderAndWriteAllVideo()", eventTitle);

	/*	// NVENC doesn't use this
	frameFormat.inPixelFormat = MAKE_THIRD_PARTY_CUSTOM_PIXEL_FORMAT_FOURCC('s', 'd', 'k');
	frameFormat.inWidth = width.value.intValue;
	frameFormat.inHeight = height.value.intValue;
	*/
	// The render-loop operation
	// -------------------------
	// The render-loop supports 2 modes of operation:
	//
	//	(1) legacy PULL-mode processing (CS5.5 and earlier)
	//	(2) PUSH-mode processning (new, CS6 and above)
	// 
	// Common to both modes, the first frame (Frame0) is rendered using a call
	// to RenderAndWriteVideoFrame() [which in turn, calls 
	// sequenceRenderSuite->RenderVideoFrame()].  For this first-frame only,
	// the NVENC plugin advertises all compatible PixelFormat(s) ...YUV420
	// After the call returns, the render-loop checks the rendered frame's
	// PrPixelformat:
	//
	//		* If it is one of our requested YUV-format(s), then we assume PUSH-mode
	//        is supported.  (This is true for CS6)
	//        The video will be rendered using push-mode.
	//
	//		* Otherwise, if it is an RGB32-format (which is something we did not request),
	//		  then we conclude the host Adobe-app doesn't support PUSH-mode.
	//        The video will be rendered using pull-mode.
	//
	// ...All subsequent video-rendering uses the selected-mode (PUSH or PULL.)

	// PUSH-mode works as follows: 
	// ---------------------------
	//  (1) For the first frame only (frame0), call RenderAndWriteVideoFrame().
	//      This plugin advertises all compatible PixelFormat(s), then the Adobe
	//      app chooses 1 for us.
	//      (The plugin doesn't actually submit this frame to NVENC,
	//       it just wants Adobe to choose a PixelFormat.)
	//
	//  (2) After the PixelFormat selection (above), then 
	//      call DoMultiPassExportLoop() to actually encode the entire video-sequence
	//      (including Frame#0 again, since it wasn't rendered the first time around.)
	//
	//  (3) break out of for-loop()


	// PULL-mode works as follows: 
	// ---------------------------
	//  (1) For the first frame only (frame0), same as PUSH-mode (i.e. advertise
	//		yuv PixelFormats for Adobe to choose.)  Adobe will return with an 
	//		RGB PixelFormat.
	//
	//		* the loop detects the (unrequested) RGB-format, then disables PUSH-mode.
	//		  And it sets mySettings->rendered_PrPixelFormat0 to the user-requested
	//		  format, in order to manually override all future RenderVideoFrame() ops.
	//
	//  (2) After the PixelFormat override (above), the for-loop proceeds normally.
	//		RenderAndWriteVideoFrame() is called once For each video-frame (including
	//		frame#0 again.) 

	// Since the endTime can fall in between frames, make sure to not include any fractional trailing frames
	bool is_frame0 = true; // flag, is this the first frame being rendered?
	bool encoded_at_least_1 = false;// status, we successfully encoded at least 1 frame
	bool pre11_suppress_messages = false; // workaround for Premiere Elements 11, don't call ReportEvent

	// kludge for Premiere Elements 11 - don't call ReportEventA() when running on PRE11
#define _SafeReportEvent( id, eventtype, title, desc) \
	if ( !pre11_suppress_messages ) \
		mySettings->exporterUtilitySuite->ReportEventA( \
			id, eventtype, title, desc \
		);

	////////////////////////////////////////////////////////////////////////////
	// Video render loop (start)
	//

	for (PrTime videoTime = exportInfoP->startTime;
		videoTime <= (exportInfoP->endTime - ticksPerFrame.value.timeValue);
		videoTime += ticksPerFrame.value.timeValue)
	{
		mySettings->videoSequenceParser->GetRTStatus(videoTime, segmentEnd, playcode);
		/* NVENC plugin doesn't use this
		if (playcode == PRT_PLAYCODE_REALTIME)
		{
		clipID = mySettings->videoSequenceParser->FindClipIDAtTime(videoTime);
		mySettings->clipRenderSuite->GetNumCustomPixelFormats(
		clipID,
		&numPixelFormats);
		if (numPixelFormats > 0)
		{
		// (Really lame asynchronous rendering)
		result = mySettings->clipRenderSuite->InitiateAsyncRead(
		clipID,
		&videoTime,
		&frameFormat);
		result = mySettings->clipRenderSuite->FindFrame(
		clipID,
		&videoTime,
		&frameFormat,
		1,
		kPrTrue,
		&tempFrame);
		mySettings->ppixSuite->Dispose(tempFrame);
		}
		else
		{
		result = RenderAndWriteVideoFrame(is_frame0, false, videoTime, exportInfoP);
		}
		}
		else
		{
		*/
		// Get 1 video-frame of the sequence:
		//    * If this is frame#0, then render the frame without encoding it,
		//      so that the plugin can analyze the returned PrPixelFormat
		//
		//    * For all other frames, submit the rendered frame to NVENC
		//      for H264-encoding.
		result = RenderAndWriteVideoFrame(
			is_frame0,	// flag, is this frame#0? 
			is_frame0,	// Don't Submit the rendered frame to NVENC for encoding?
			videoTime,	// timeStamp
			exportInfoP	// plugin internal data-struct (p_nvEncoder object)
			);

		// If an error occurred during video-rendering, halt the render.
		if (result != malNoError) {
			mySettings->video_encode_fatalerr = true;// video-encode fatal failure
			break; // halt the render (abort the for-loop)
		}

		// for Frame#0 only: analyze the rendered frame's PrPixelFormat,
		//                   
		//
		if (is_frame0) {
			PrPixelFormat adobe_selected_prpixelformat = mySettings->rendered_PixelFormat0;
			// Write informational message to Adobe's log:
			//    which pixelformat did Adobe choose for us?
			os.clear();
			os.flush();
			os << "Video Frame#0 info: Adobe rendered_PixelFormat0 = 0x"
				<< std::hex << mySettings->rendered_PixelFormat0 << " '";
			for (unsigned i = 0; i < 4; ++i)
				os << (static_cast<char>((adobe_selected_prpixelformat >> (i << 3)) & 0xFFU));
			os << "'" << std::endl;
			copyConvertStringLiteralIntoUTF16(os.str().c_str(), eventDesc);
			// Did Adobe-app accept or reject our requested YUV PrPixelFormat?

			if (adobe_selected_prpixelformat == PrPixelFormat_BGRA_4444_8u ||
				adobe_selected_prpixelformat == PrPixelFormat_BGRX_4444_8u)
			{
				// Adobe rejected the YUV-pixelformat because we never requested RGB32.
				// * Assume we're running an Adobe-app that uses an older API-version 
				//   that does NOT support PushMode (such as Adobe Premiere Elements 11.)

				// Actions:
				//  *  disable PushMode, and
				//  *  manually *force* Adobe-app to render all future frames in user-chosen YUV-format.
				UsePushMode = false;// assume this Adobe-app doesn't support push-mode
				pre11_suppress_messages = true; // assume we can't even call ReportEvent() safely

				if (mySettings->forced_PixelFormat0)
					mySettings->rendered_PixelFormat0 = mySettings->requested_PixelFormat0;// user-selection
				else
					mySettings->rendered_PixelFormat0 = PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709;

				os.flush();
				os.clear();
				os << "*** Adobe-app rendered first video-frame using RGB instead of YUV." << std::endl;
				os << "*** NVENC plugin will force Adobe to render all video using PrPixelFormat = 0x"
					<< std::hex << mySettings->rendered_PixelFormat0 << " '";
				for (unsigned i = 0; i < 4; ++i)
					os << static_cast<unsigned char>((mySettings->rendered_PixelFormat0 >> (i << 3)) & 0xFFU);
				os << "'" << std::endl;

				copyConvertStringLiteralIntoUTF16(os.str().c_str(), eventDesc);
				_SafeReportEvent(
					exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
					);

			} // not YUV
			else {
				// Adobe rendered 1st-frame with a compatible YUV-format, print it to a log message.
				_SafeReportEvent(
					exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
					);
			}

			// attempt to initialize the NVENC-hardware.  Failure could be due to an
			// invalid/expired license-key.
			result = NVENC_initialize_h264_session(mySettings->rendered_PixelFormat0, exportInfoP);
			if (result != malNoError) {
				break; // halt the render (abort the for-loop)
			}
		} // if ( is_frame0 )

		if (is_frame0 && !UsePushMode) {
			//
			// PULL-mode (for frame#0 only)
			//

			copyConvertStringLiteralIntoUTF16(L"Using PULL-mode to render video", eventDesc);
			_SafeReportEvent(
				exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
				);

			// For Frame#0 only, call the renderer a second time, to 
			// render frame#0 for *real* this time (this time it will be submitted to NVENC)
			result = RenderAndWriteVideoFrame(
				false,	// flag, is this frame#0? (lie because we want to render for real this time)
				false,	// Don't Submit the rendered frame to NVENC for encoding?
				videoTime,	// timeStamp
				exportInfoP	// plugin internal data-struct (p_nvEncoder object)
				);

			// If the first frame failed to render, then this is a fatal error condition.
			// Set the fatal-flag (to signal the remainder of nvenc_export to quit),
			// and halt render now.
			if (PrSuiteErrorFailed(result)) {
				mySettings->video_encode_fatalerr = true;// video-encode fatal failure
				break; // halt the render (abort the for-loop)
			}
			else
				encoded_at_least_1 = true;
		} // if ( is_frame0 && !UsePushMode)

		if (is_frame0 && UsePushMode) {
			//
			// PUSH-mode (for all frames)
			//
			ExportLoopRenderParams ep;

			// (CS6 and above) PUSH-mode processing
			// ------------------------------------
			//
			// In push-mode: 
			//  (1) For the first frame only (frame0), call RenderAndWriteVideoFrame().
			//      This plugin advertises all compatible PixelFormat(s), then the Adobe
			//      app chooses 1 for us.
			//      (The plugin doesn't actually submit this frame to NVENC,
			//       it just wants Adobe to choose a PixelFormat.)
			//
			//  (2) After the PixelFormat selection (above), then 
			//      call DoMultiPassExportLoop() to actually encode the entire video-sequence
			//      (starting from Frame#0 again)
			//
			//  (3) break out of for-loop()

			memset((void *)&ep, 0, sizeof(ep));
			ep.inEndTime = exportInfoP->endTime;

			// kludge: with the above ep.inEndTime, push-mode renders +1 extra-frame 
			//         compared to pull-mode.
			//    workaround: match the #frames encoded by shortening the endtime
			//    by 1 timetick.
			//
			//  On second-thought, Adobe Premiere Pro/Media-Encoder end up rendering +1 extra-frame
			//  compared to Premiere Elements 11, so maybe the extra-frame is correct behavior?
			//ep.inEndTime -= ticksPerFrame.value.timeValue; // adjustment -1 frame

			// render the entire video using the PrPixelFormat from frame#0
			ep.inFinalPixelFormat = mySettings->rendered_PixelFormat0;

			ep.inRenderParamsSize = sizeof(ep);
			ep.inRenderParamsVersion = 1; // ?!? TODO
			ep.inReservedProgressPostRender = 0;
			ep.inReservedProgressPostRender = 0;
			ep.inStartTime = exportInfoP->startTime;

			copyConvertStringLiteralIntoUTF16(L"Using PUSH-mode to render video", eventDesc);
			_SafeReportEvent(
				exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
				);

			result = mySettings->exporterUtilitySuite->DoMultiPassExportLoop(
				exID,
				&ep,
				1,
				NVENC_export_FrameCompletionFunction, // callback to plugin's completion-Fn
				(void *)exportInfoP
				);

			// done with encoding the entire video-sequence!  Now break out of the for-loop()
			encoded_at_least_1 = true;
			is_frame0 = false;
			break;
		} // if ( is_frame0 && UsePushMode )

		//
		// If using legacy PULL-mode (i.e. not using PUSH-mode),
		//  then each frame of the video-sequence executes the code below
		//
		progress = static_cast<float>(videoTime - exportInfoP->startTime) / static_cast<float>(*exportDuration) * videoProgress;
		result = mySettings->exportProgressSuite->UpdateProgressPercent(exID, progress);
		if (result == suiteError_ExporterSuspended)
		{
			mySettings->exportProgressSuite->WaitForResume(exID);
		}
		else if (result == exportReturn_Abort)
		{
			// Pass back the actual length exported so far
			// Since the endTime can fall in between frames, we go with the lower of the two values
			*exportDuration = videoTime + ticksPerFrame.value.timeValue - exportInfoP->startTime < *exportDuration ?
				videoTime + ticksPerFrame.value.timeValue - exportInfoP->startTime : *exportDuration;
			break; // abort further video-processing (abort the for-loop)
		}

		// clear the 'frame#0 flag' after rendering first frame
		is_frame0 = false;
	} // for

	//
	// Video render loop (end)
	////////////////////////////////////////////////////////////////////////////

	// If we successfully encoded 1 or more frame(s), then
	// notify NVENC to close out the encoded bitstream,
	if (encoded_at_least_1)
		mySettings->p_NvEncoder->EncodeFramePPro(NULL, true);

	// Free up GPU-resources allocated by NVENC
	mySettings->p_NvEncoder->DestroyEncoder();

	mySettings->sequenceRenderSuite->ReleaseVideoRenderer(exID, mySettings->videoRenderID);
	return result;
}
