/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 1999-2008 Adobe Systems Incorporated                  */
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
//
//		SDK_File.cpp - Used to read/write the SDK file format.
//		
//		Description: SDK Files can contain audio and video
//		In addition they can be RLE compressed
//
//		Purpose: This "artificial" file format is used as
//		a part of the SDK "Media Abstraction Layer".  By writing
//		an import and compile module, and optionally a
//		real-time playback module and assigning a Filetype, 
//		Subtype, and a unique ClassID, your plug-ins can 
//		work cooperatively with one another.
//
//		Created by Adobe Developer Technologies for 
//		Adobe Systems, Inc.
//		Part of the Adobe Premiere Pro SDK
//		_______________________________________
//		Version 1.0 - eks
//				1.1	- bbb -	(.c -> .cpp)
//				1.2 - zal - Fixed file write, work area export
//				1.3 - zal - Added support for arbitrary audio sample rates, multi-channel audio,
//							pixel aspect ratio, and fields
//				1.4 - zal - Support for 24-bit video (no alpha channel), versioning
//				2.0 - zal - Generic routines for rendering and writing video, audio, and markers
//				2.5 - zal - High-bit video support (v410)

#include <nvEncodeAPI.h>                // the NVENC common API header
#include "CNVEncoderH264.h"             // class definition for the H.264 encoding class
#include "xcodeutil.h"                  // class helper functions for video encoding
#include <platform/NvTypes.h>           // type definitions

#include <cuda.h>                       // include CUDA header for CUDA/NVENC interop
#include "SDK_File.h"
#include <MMReg.h> // for GUID KSDATAFORMAT_SUBTYPE_PCM
#include "SDK_Exporter.h" // nvenc_make_output_dirname()
#include "SDK_Exporter_Params.h"
#include <Windows.h> // SetFilePointer(), WriteFile()
#include <sstream>  // ostringstream
#include <cstdio>

bool Check_prSuiteError( const prSuiteError errval, string &str );

typedef union {
	uint32_t  dword[1];
	uint16_t  word[2];
	uint8_t   byte[4];
} dword_word_byte_u; // a 32-bit glob that is addressable as byte/word/dword

// These pixelformats are used for NVENC chromaformatIDC = NV12.
//    If the Adobe-app prefers BGRA instead of YUV420, then
//    switch to the 422 formats below.
const PrPixelFormat SupportedPixelFormats420[] ={
	PrPixelFormat_YUV_420_MPEG4_FRAME_PICTURE_PLANAR_8u_709,
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
const PrPixelFormat SupportedPixelFormats422[] ={
	PrPixelFormat_YUYV_422_8u_709,
	PrPixelFormat_UYVY_422_8u_709,
	PrPixelFormat_YUYV_422_8u_601,
	PrPixelFormat_UYVY_422_8u_601
};


// These pixelformats are used for NVENC chromaformatIDC = YUV444
//  (requires NV_ENC_CAPS_SUPPORT_YUV444_ENCODE == 1)
const PrPixelFormat SupportedPixelFormats444[] ={
	PrPixelFormat_VUYX_4444_8u_709,
	PrPixelFormat_VUYA_4444_8u_709,
	PrPixelFormat_VUYX_4444_8u,
	PrPixelFormat_VUYA_4444_8u
};



typedef struct {
dword_word_byte_u ChunkID;		// 0         4   ChunkID          Contains the letters "RIFF" in ASCII form
						// (0x52494646 big-endian form).
uint32_t ChunkSize; // 4         4   ChunkSize// 36 + SubChunk2Size, or more precisely:
//                               4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
//                               This is the size of the rest of the chunk 
//                               following this number.  This is the size of the 
//                               entire file in bytes minus 8 bytes for the
//                               two fields not included in this count:
//                               ChunkID and ChunkSize.
dword_word_byte_u Format; // 8         4   Format           Contains the letters "WAVE"
//                               (0x57415645 big-endian form).
//
//The "WAVE" format consists of two subchunks: "fmt " and "data":
//The "fmt " subchunk describes the sound data's format:
//
dword_word_byte_u Subchunk1ID;//12        4   Subchunk1ID      Contains the letters "fmt "
//                               (0x666d7420 big-endian form).
uint32_t Subchunk1Size;//16        4   Subchunk1Size    16 for PCM.  This is the size of the
//                               rest of the Subchunk which follows this number.
uint16_t AudioFormat;//20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
//                               Values other than 1 indicate some 
//                               form of compression.
uint16_t NumChannels;//22        2   NumChannels      Mono = 1, Stereo = 2, etc.
uint32_t SampleRate;//24        4   SampleRate       8000, 44100, etc.
uint32_t ByteRate;//28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
uint16_t BlockAlign;//32        2   BlockAlign       == NumChannels * BitsPerSample/8
//                               The number of bytes for one sample including
//                               all channels. I wonder what happens when
//                               this number isn't an integer?
uint16_t BitsPerSample;//34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.
//uint16_t ExtraParamSize;//          2   ExtraParamSize   if PCM, then doesn't exist
//          X   ExtraParams      space for extra parameters
//
//The "data" subchunk contains the size of the data and the actual sound:
//
dword_word_byte_u Subchunk2ID;//36        4   Subchunk2ID      Contains the letters "data"
//                               (0x64617461 big-endian form).
uint32_t Subchunk2Size;//40        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
//                               This is the number of bytes in the data.
//                               You can also think of this as the size
//                               of the read of the subchunk following this 
//                               number.
//44        *   Data           
} microsoft_stereo_wav_header_t; // simplest RIFF WAV file header (up to stereo channel)

//////////////////////////////////////////////////////////////////////////////

typedef struct {
dword_word_byte_u ChunkID;		// 0         4   ChunkID          Contains the letters "RIFF" in ASCII form
						// (0x52494646 big-endian form).
uint32_t ChunkSize; // 4         4   ChunkSize// 60 + SubChunk2Size, or more precisely:
//                               4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
//                               This is the size of the rest of the chunk 
//                               following this number.  This is the size of the 
//                               entire file in bytes minus 8 bytes for the
//                               two fields not included in this count:
//                               ChunkID and ChunkSize.
dword_word_byte_u Format; // 8         4   Format           Contains the letters "WAVE"
//                               (0x57415645 big-endian form).
//
//The "WAVE" format consists of two subchunks: "fmt " and "data":
//The "fmt " subchunk describes the sound data's format:
//
dword_word_byte_u Subchunk1ID;//12        4   Subchunk1ID      Contains the letters "fmt "
//                               (0x666d7420 big-endian form).
uint32_t Subchunk1Size;//16        4   Subchunk1Size    16+2+22 (40) for multichannel-PCM.  This is the size of the
//                               rest of the Subchunk which follows this number.
uint16_t AudioFormat;//20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
//                               Values other than 1 indicate some 
//                               form of compression.
uint16_t NumChannels;//22        2   NumChannels      Mono = 1, Stereo = 2, etc.
uint32_t SampleRate;//24        4   SampleRate       8000, 44100, etc.
uint32_t ByteRate;//28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
uint16_t BlockAlign;//32        2   BlockAlign       == NumChannels * BitsPerSample/8
//                               The number of bytes for one sample including
//                               all channels. I wonder what happens when
//                               this number isn't an integer?
uint16_t BitsPerSample;//34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.
uint16_t ExtraParamSize;//36         2   ExtraParamSize   0 or 22 (bytes)
uint16_t ValidBitsPerSample;//38       2   # valid bits per sample
uint32_t ChannelMask;    //40       4   speaker position mask
union {
	uint8_t  b[16];  // 44      16 bytes (GUID including format code)
	GUID     guid;
} SubFormat;

//
//The "data" subchunk contains the size of the data and the actual sound:
//
dword_word_byte_u Subchunk2ID;//60        4   Subchunk2ID      Contains the letters "data"
//                               (0x64617461 big-endian form).
uint32_t Subchunk2Size;//64        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
//                               This is the number of bytes in the data.
//                               You can also think of this as the size
//                               of the read of the subchunk following this 
//                               number.
//68        *   Data           
} microsoft_wavex_header_t; // RIFF WAV file header (up to 8.1 channel PCM)


prMALError
WriteSDK_WAVHeader(exportStdParms *stdParms, exDoExportRec *exportInfoP, PrTime exportDuration)
{
	microsoft_stereo_wav_header_t  w;
	microsoft_wavex_header_t  w2;
	prMALError				result				= malNoError;
	//
	// TODO, get rid of the rest of this stuff
	//
	csSDK_uint32			bytesToWriteLu		= 0;
	csSDK_uint32			exID				= exportInfoP->exporterPluginID;
	exParamValues			codecSubType,
							height,
							width,
							ticksPerFrame,
							fieldType,
							pixelAspectRatio,
							sampleRate,
							channelType;
	PrTime					ticksPerSample;
	ExportSettings			*mySettings;
	uint64_t				NumSamples;

	memset( (void *)&w, 0, sizeof(w) ); // clear w
	memset( (void *)&w2, 0, sizeof(w2) ); // clear w2

	// Update the private data with the parameter settings
	mySettings = reinterpret_cast<ExportSettings *>(exportInfoP->privateData);
	if(exportInfoP->exportAudio)
	{
		mySettings->SDKFileRec.hasAudio = kPrTrue;
		mySettings->SDKFileRec.audioSubtype = 'RAW_'; //exportInfoP->outputRec.audCompression.subtype;

		// Calculate audio samples
		mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEAudioRatePerSecond, &sampleRate);
		mySettings->SDKFileRec.sampleRate = sampleRate.value.floatValue;
		mySettings->timeSuite->GetTicksPerAudioSample ((float)sampleRate.value.floatValue, &ticksPerSample);
		mySettings->SDKFileRec.numSampleFrames = exportDuration / ticksPerSample;
		mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEAudioNumChannels, &channelType);
		mySettings->SDKFileRec.channelType = channelType.value.intValue;
	}
	else
	{
		mySettings->SDKFileRec.hasAudio = kPrFalse;				// No audio in file
	}
	mySettings->SDKFileRec.version = SDK_FILE_CURRENT_VERSION;

	NumSamples = mySettings->SDKFileRec.numSampleFrames; // exportDuration * sampleRate.value.floatValue / ticksPerSample;

	//w.ChunkID.dword[0] = 'RIFF'; // opps, this needs to be little-endian on x86_64 machines
	w.ChunkID.byte[0] = 'R'; w2.ChunkID.byte[0] = 'R'; 
	w.ChunkID.byte[1] = 'I'; w2.ChunkID.byte[1] = 'I'; 
	w.ChunkID.byte[2] = 'F'; w2.ChunkID.byte[2] = 'F'; 
	w.ChunkID.byte[3] = 'F'; w2.ChunkID.byte[3] = 'F'; 
	w.ChunkSize = 0; w2.ChunkSize = 0;// 4         4   ChunkSize// 36 + SubChunk2Size, or more precisely:
//                               4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
//                               This is the size of the rest of the chunk 
//                               following this number.  This is the size of the 
//                               entire file in bytes minus 8 bytes for the
//                               two fields not included in this count:
//                               ChunkID and ChunkSize.
	//w.Format.dword[0] = 'WAVE'; // 8         4   Format           Contains the letters "WAVE"
//                               (0x57415645 big-endian form).
	w.Format.byte[0] = 'W'; w2.Format.byte[0] = 'W';
	w.Format.byte[1] = 'A'; w2.Format.byte[1] = 'A';
	w.Format.byte[2] = 'V'; w2.Format.byte[2] = 'V';
	w.Format.byte[3] = 'E'; w2.Format.byte[3] = 'E';
//
//The "WAVE" format consists of two subchunks: "fmt " and "data":
//The "fmt " subchunk describes the sound data's format:
//
	//w.Subchunk1ID.dword[0] = 'fmt ';//12        4   Subchunk1ID      Contains the letters "fmt "
	w.Subchunk1ID.byte[0] = 'f'; w2.Subchunk1ID.byte[0] = 'f';
	w.Subchunk1ID.byte[1] = 'm'; w2.Subchunk1ID.byte[1] = 'm';
	w.Subchunk1ID.byte[2] = 't'; w2.Subchunk1ID.byte[2] = 't';
	w.Subchunk1ID.byte[3] = ' '; w2.Subchunk1ID.byte[3] = ' ';
//                               (0x666d7420 big-endian form).
	w.Subchunk1Size = 16;//16        4   Subchunk1Size    16 for PCM.  This is the size of the
//                               rest of the Subchunk which follows this number.
	w2.Subchunk1Size = w.Subchunk1Size + 24;
	w.AudioFormat = WAVE_FORMAT_PCM;
	w2.AudioFormat = WAVE_FORMAT_EXTENSIBLE;//20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
//                               Values other than 1 indicate some 
//                               form of compression.

	w.BitsPerSample = 16; w2.BitsPerSample = 16;//34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.

	w.NumChannels = GetNumberOfAudioChannels(channelType.value.intValue);//22        2   NumChannels      Mono = 1, Stereo = 2, etc.
	w2.NumChannels = w.NumChannels;
	w.SampleRate = sampleRate.value.floatValue;//24        4   SampleRate       8000, 44100, etc.
	w2.SampleRate = w.SampleRate ;
	w.ByteRate = w.SampleRate * w.NumChannels * ((w.BitsPerSample+7) >> 3);//28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
	w2.ByteRate = w.ByteRate ;
	w.BlockAlign = w.NumChannels * (w.BitsPerSample >> 3);//32        2   BlockAlign       == NumChannels * BitsPerSample/8
	w2.BlockAlign = w.BlockAlign;
//                               The number of bytes for one sample including
//                               all channels. I wonder what happens when
//                               this number isn't an integer?
//	w.ExtraParamSize = 0;//          2   ExtraParamSize   if PCM, then doesn't exist
//          X   ExtraParams      space for extra parameters
//
	w2.ExtraParamSize = 22;
	w2.ValidBitsPerSample = 16;//       2   # valid bits per sample
	w2.ChannelMask = 0x3F;    //       4   speaker position mask
	w2.SubFormat.guid = KSDATAFORMAT_SUBTYPE_PCM;

//The "data" subchunk contains the size of the data and the actual sound:
//
	//w.Subchunk2ID.dword[0] = 'data';//36        4   Subchunk2ID      Contains the letters "data"
	w.Subchunk2ID.byte[0] = 'd'; w2.Subchunk2ID.byte[0] = 'd';
	w.Subchunk2ID.byte[1] = 'a'; w2.Subchunk2ID.byte[1] = 'a';
	w.Subchunk2ID.byte[2] = 't'; w2.Subchunk2ID.byte[2] = 't';
	w.Subchunk2ID.byte[3] = 'a'; w2.Subchunk2ID.byte[3] = 'a';
//                               (0x64617461 big-endian form).
	w.Subchunk2Size = NumSamples * w.BlockAlign;//40        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
	w2.Subchunk2Size = w.Subchunk2Size;
//                               This is the number of bytes in the data.
//                               You can also think of this as the size
//                               of the read of the subchunk following this 
//                               number.

	//w.ChunkSize = w.Subchunk2Size + sizeof(w) - 8;
	w.ChunkSize = w.Subchunk2Size + 36;
	w2.ChunkSize = w.ChunkSize + 24;

	#ifdef PRWIN_ENV
	strcpy_s(mySettings->SDKFileRec.name, sizeof(SDK_NAME), SDK_NAME);
	#else
	strcpy(mySettings->SDKFileRec.name, SDK_NAME);
	#endif

	// Write out the header
	bytesToWriteLu = (w.NumChannels > 2) ? sizeof(w2) : sizeof(w);

	exportInfoP->privateData = reinterpret_cast<void*>(mySettings);
		
/*	// Adobe's way
	// Seek to beginning of file
	unsigned sizeof_w = sizeof(w); // just for debug
	prInt64 newPosition;

	mySettings->exportFileSuite->Seek(exportInfoP->fileObject,
										0,
										newPosition,
										fileSeekMode_Begin);
	mySettings->exportFileSuite->Write(	exportInfoP->fileObject,
										reinterpret_cast<void*>(&w),
										bytesToWriteLu);
*/
	void *pHeader;

	if (w.NumChannels > 2)
		pHeader = reinterpret_cast<void*>(&w2); // 5.1 audio only
	else
		pHeader = reinterpret_cast<void*>(&w);  // mono, stereo


	// NVENC plugin doesn't use Adobe's provided exportFileSuite to create & manage file-I/O.
	// Instead, it uses good old-fashioned <cstdio> fopen/fseek/fwrite/fclose
/*
	size_t bytes_written = 0;
	_fseeki64( 
		mySettings->SDKFileRec.FileRecord_Audio.fp,
		0LL, 
		SEEK_SET
	); // goto start of file

	bytes_written = fwrite(
		pHeader,
		sizeof(uint8_t),
		bytesToWriteLu,
		mySettings->SDKFileRec.FileRecord_Audio.fp
	);
*/
/*
	SetFilePointer(
		mySettings->SDKFileRec.FileRecord_Audio.hfp,
		0,
		NULL, // upper 32-bits (pointer)
		FILE_BEGIN
	);
*/
	// ^^^ Note, don't need to seek to the start_of_file, because
	// the wave-header is only written *ONCE*, and its only written at the start of the 
	// AudioRendering process.

	DWORD bytes_written = 0;
	BOOL wfrc = WriteFile(
		mySettings->SDKFileRec.FileRecord_Audio.hfp,
		pHeader,
		bytesToWriteLu,
		&bytes_written,
		NULL // not overlapped
	);

	// if the NumSamples 
	if ( NumSamples>>32 )
		result = malUnknownError;
	else if ( !wfrc )
		result = malUnknownError; // WriteFile() failed
	else if ( bytes_written != bytesToWriteLu )
		result = malUnknownError;

	return result;
}

#include <emmintrin.h> // Visual Studio 2005 MMX/SSE/SSE2 compiler intrinsics
#include <tmmintrin.h> // Visual Studio 2008 SSSE3 (Tejas New Instr) compiler intrinsics (_mm_shuffle_epi8 or 'PSHUFB')

void
adobe2wav_audio51_swap_ssse3( 
	const uint32_t numSamples, // number of 5.1-audio samples (each 5.1-audioSample is 6+2=12 bytes)
	__m128i audioBuffer[]      // audioBuffer (requirement: 128-bit aligned)
)
{
	__m128i audio51_mask[3] = {0};
	__m128i fixup_mask[4] = {0};
	__m128i read_temp[3] = {0};
	__m128i write_temp[3] = {0};
	__m128i write_fixup[4] = {0};

	// m128_u : abstract data-strut to represent a 128-bit SSE2 (XMM) register

	typedef __declspec(align(16)) union {
	    __m128i  xmm;   // the SSE2 register
	    uint8_t  b[16]; // byte arrangement
	    uint16_t w[8];  // word arrangement
	    uint32_t d[4];  // dword arrangement
	    uint64_t q[2];  // qword arrangement
	} m128_u;

	// _m128u_word(): generate a 16-bit mask-value for 2nd-operand of 'PSHUFB',
	//                for selecting a 16-bit word from the 128-bit source reg
	//             bit#127                                 bit#0
	//                :____ ____ ____ ____ ____ ____ ____ ____:
	//                | w7 | w6 | w5 | w4 | w3 | w2 | w1 | w0 |   source_reg
	//
	//                Each word (w0..w7) is 16-bits wide
	//
	//                Let src_word_index = {0..7}  (selects 1 word from #{0..7})
	//                mask_bit: 0 = copy (copy word[i] from source to dest)
	//                          1 = zero (zero out the dest field)

#define _m128u_word(src_word_index, mask_bit) ( \
	(  (mask_bit) ? 0x80 : ( (src_word_index)<<1 ) ) & 0xFF | \
	( ((mask_bit) ? 0x80 : ( ((src_word_index)<<1)+1)) << 8 ) & 0xFF00 \
	)

	m128_u * const mask0 = reinterpret_cast<m128_u *>(&audio51_mask[0]);
	m128_u * const mask1 = reinterpret_cast<m128_u *>(&audio51_mask[1]);
	m128_u * const mask2 = reinterpret_cast<m128_u *>(&audio51_mask[2]);
	m128_u * const fmask0_from1 = reinterpret_cast<m128_u *>(&fixup_mask[0]);
	m128_u * const fmask1_from0 = reinterpret_cast<m128_u *>(&fixup_mask[1]);
	m128_u * const fmask1_from2 = reinterpret_cast<m128_u *>(&fixup_mask[2]);
	m128_u * const fmask2_from1 = reinterpret_cast<m128_u *>(&fixup_mask[3]);

	memset( reinterpret_cast<void *>(fmask0_from1), 0x80, sizeof(m128_u) );
	memset( reinterpret_cast<void *>(fmask1_from0), 0x80, sizeof(m128_u) );
	memset( reinterpret_cast<void *>(fmask1_from2), 0x80, sizeof(m128_u) );
	memset( reinterpret_cast<void *>(fmask2_from1), 0x80, sizeof(m128_u) );

	// WAVEFORMATEXTENSIBLE									   ADOBE								VST 3
	// ----------------------------------------------	   -----------------------------	---------------------------------------		------------------
	//kPrAudioChannelLabel_FrontLeft				= 100,	// SPEAKER_FRONT_LEFT				kAudioChannelLabel_Left						kSpeakerL
	//kPrAudioChannelLabel_FrontRight				= 101,	// SPEAKER_FRONT_RIGHT				kAudioChannelLabel_Right					kSpeakerR
	//kPrAudioChannelLabel_FrontCenter			= 102,	// SPEAKER_FRONT_CENTER				kAudioChannelLabel_Center					kSpeakerC
	//kPrAudioChannelLabel_LowFrequency			= 103,	// SPEAKER_LOW_FREQUENCY			kAudioChannelLabel_LFEScreen				kSpeakerLfe
	//kPrAudioChannelLabel_BackLeft				= 104,	// SPEAKER_BACK_LEFT				kAudioChannelLabel_LeftSurround				kSpeakerLs
	//kPrAudioChannelLabel_BackRight				= 105,	// SPEAKER_BACK_RIGHT				kAudioChannelLabel_RightSurround			kSpeakerRs	

	// For 5.1 surround audio, Adobe and RIFF(*.WAV) use different channel-order.
	// Adobe                    RIFF WAV
	// -------------		    ---------
	// [1] FrontLeft            (1) FrontLeft           
	// [2] FrontRight           (2) FrontRight          
	// [3] BackLeft             (3) FrontCenter         
	// [4] BackRight            (4) LFE                 
	// [5] FrontCenter          (5) BackLeft            
	// [6] LFE                  (6) BackRight           

	// To convert Adobe -> RIFF, the channels are re-ordered as follows:
	//
	// Source                   Destination
	// -------------            ------------
	// [1] FrontLeft            [1] FrontLeft           
	// [2] FrontRight           [2] FrontRight          
	// [3] BackLeft             [5] FrontCenter         
	// [4] BackRight            [6] LFE                 
	// [5] FrontCenter          [3] BackLeft            
	// [6] LFE                  [4] BackRight             

	// SSE2 (128-bit) performance optimization
	// (10 operate on 4 blockSamples at a time (i.e. four 5.1-audio samples, or 4 x 12 bytes.)
	//
	//    16-bit
	//    word#		first_output					second_output					third_output
	//		|<------- 128 bits --------->|	|<-------- 128 bits -------->|	|<------ 128 bits ---------->|
	// 		w0	w1	w2	w3	w4	w5	w6	w7	x8	x9	x10	x11	x12	x13	x14	x15	y16	y17	y18	y19	y20	y21	y22	y23
	//	------------------+---------------+---------------+---------------+---------------+---------------+--> time
	//	src	1	2	3	4	5	6	1	2	3	4	5	6	1	2	3	4	5	6	1	2	3	4	5	6
	//	dst	1   2   5   6   3   4   1   2   5   6   3   4   1   2   5   6   3   4   1   2   5   6   3   4
	//	  |		Sample n		  |		Sample n+1		  |		Sample n+2		  |		Sample n+3		 |
	//
	//  re-indexed for (0)..(5) instead of (1)..(6)
	//
	//	src	a0	a1	a2	a3	a4	a5	b0	b1	b2	b3	b4	b5	c0	c1	c2	c3	c4	c5	d0	d1	d2	d3	d4	d5
	//	dst	a0	a1	a4	a5	a2	a3	b0	b1	b4	b5	b2	b3	c0	c1	c4	c4	c2	c3	d0	d1	d4	d5	d2	d3	
	//	  |		Sample n		  |		Sample n+1		  |		Sample n+2		  |		Sample n+3		 |
	// 		w0	w1	w2	w3	w4	w5	w6	w7	x8	x9	x10	x11	x12	x13	x14	x15	y16	y17	y18	y19	y20	y21	y22	y23
	//		|<------- 128 bits --------->|	|<-------- 128 bits -------->|	|<------ 128 bits ---------->|
	//    word#		first_output					second_output					third_output

	///// 
	// Sample n+0 (first sample)
	mask0->w[0] = _m128u_word( 0, 0 );// FrontLeft
	mask0->w[1] = _m128u_word( 1, 0 );// FrontRight
	mask0->w[2] = _m128u_word( 4, 0 );// FrontCenter
	mask0->w[3] = _m128u_word( 5, 0 );// LFE
	mask0->w[4] = _m128u_word( 2, 0 );// BackLeft
	mask0->w[5] = _m128u_word( 3, 0 );// BackRight
	//
	/////

	//////
	// Sample n+1 (second sample)
	mask0->w[ 0 + 6 ] = _m128u_word( 0 + 6, 0 );// FrontLeft
	mask0->w[ 1 + 6 ] = _m128u_word( 1 + 6, 0 );// FrontRight
	mask1->w[ 2 - 2 ] = _m128u_word( 4 - 2, 0 );// FrontCenter
	mask1->w[ 3 - 2 ] = _m128u_word( 5 - 2, 0 );// LFE
	mask1->w[ 4 - 2 ] = _m128u_word( 2 - 2, 0 );// BackLeft
	mask1->w[ 5 - 2 ] = _m128u_word( 3 - 2, 0 );// BackRight

	// Sample n+1
	//////

	/////
	// Sample n+2 (third sample)
	mask1->w[ 0 + 4 ] = _m128u_word( 0 + 4, 0 ); // channel[0]
	mask1->w[ 1 + 4 ] = _m128u_word( 1 + 4, 0 ); // channel[1]
	mask1->w[ 2 + 4 ] = _m128u_word( 4 + 4, 1 ); // ILLEGAL (need fixup)
	mask1->w[ 3 + 4 ] = _m128u_word( 5 + 4, 1 ); // ILLEGAL (need fixup)
	fmask1_from2->w[ 2 + 4 ] = _m128u_word( 4 + 4 - 8 , 0 ); // data from previous <mm128>
	fmask1_from2->w[ 3 + 4 ] = _m128u_word( 5 + 4 - 8 , 0 ); // data from previous <mm128>

	mask2->w[ 4 - 4 ] = _m128u_word( 2 - 4, 1 ); // ILLEGAL (need fixup)
	mask2->w[ 5 - 4 ] = _m128u_word( 3 - 4, 1 ); // ILLEGAL (need fixup)

	fmask2_from1->w[ 4 - 4 ] = _m128u_word( 2 - 4 + 8 , 0 ); // data from previous <mm128>
	fmask2_from1->w[ 5 - 4 ] = _m128u_word( 3 - 4 + 8 , 0 ); // data from previous <mm128>

	// Sample n+2
	/////

	/////
	// Sample n+3 (fourth sample)
	mask2->w[ 0 + 2 ] = _m128u_word( 0 + 2, 0 );
	mask2->w[ 1 + 2 ] = _m128u_word( 1 + 2, 0 );
	mask2->w[ 2 + 2 ] = _m128u_word( 4 + 2, 0 );
	mask2->w[ 3 + 2 ] = _m128u_word( 5 + 2, 0 );
	mask2->w[ 4 + 2 ] = _m128u_word( 2 + 2, 0 );
	mask2->w[ 5 + 2 ] = _m128u_word( 3 + 2, 0 );
	// Sample n+3
	/////

	// During each loop-iteration,we process four 5.1-audioSamples.  Each 5.1-audioSample is six 16-bit words,
	// for a total of 48 bytes. This is the smallest unit of work using 128-bit SSE2 data-operations.

	for (uint32_t n = 0, aptr = 0; n < numSamples; n += 4, aptr += 3 ) {
		read_temp[0] = audioBuffer[aptr];
		read_temp[1] = audioBuffer[aptr+1];
		read_temp[2] = audioBuffer[aptr+2];

		// Perform the first-round of word-to-word shuffling.
		write_temp[0] = _mm_shuffle_epi8( read_temp[0], mask0->xmm );
		write_temp[1] = _mm_shuffle_epi8( read_temp[1], mask1->xmm );
		write_temp[2] = _mm_shuffle_epi8( read_temp[2], mask2->xmm );

		// Perform the second-round of word-to-word shuffling (i.e. "fixups")

		// No fixup needed for output[0]

		// fixup for output[1]: need to pull in some sampledata from read_temp[2]
		write_fixup[1] = _mm_shuffle_epi8( read_temp[2], fmask1_from2->xmm );

		// fixup for output[2]: need to pull in some sampledata from read_temp[1]
		write_fixup[2] = _mm_shuffle_epi8( read_temp[1], fmask2_from1->xmm );

		audioBuffer[aptr]   = write_temp[0];
		audioBuffer[aptr+1] = _mm_or_si128(write_temp[1], write_fixup[1] );// w1 | wf1
		audioBuffer[aptr+2] = _mm_or_si128(write_temp[2], write_fixup[2] );// w2 | wf2
	}
}

//
// NVENC_run_neroaacenc() - convert NVENC-generated *.WAV file into *.AAC file
//     by directly executing neroAacEnc.exe from the command-shell.
//     Returns: true if successful
//              false otherwise

bool
NVENC_run_neroaacenc(
	const csSDK_uint32 exID,
	const ExportSettings *mySettings,
	const wchar_t in_wavfilename[],  // input *.WAV filename
	const wchar_t out_aacfilename[]  // output *.AAC filename
)
{
	std::wostringstream os;
	wstring tempdirname;
	SHELLEXECUTEINFOW ShExecInfo = {0};
	csSDK_int32					mgroupIndex		= 0;
	exParamValues exParamValue_aacpath, exParamValue_temp;
	int32_t kbitrate; // audio bitrate (Kbps)
	
	mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ParamID_AudioFormat_NEROAAC_Path, &exParamValue_aacpath);
	mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ADBEAudioBitrate, &exParamValue_temp);
	kbitrate = exParamValue_temp.value.intValue; // audio bitrate (Kbps)
	nvenc_make_output_dirname( out_aacfilename, tempdirname );
	
	//
	// build the command-line to execute neroAAC.
	// It will look something like this:
	//
	//    neroaacenc.exe -br 12340000 -if "in_wavfilename.wav" -of "out_aacfilename.aac"

	os << " -br " << std::dec << (kbitrate*1024);
	os << " -if \"" << in_wavfilename << "\" -of \"" << out_aacfilename << "\"";
	wstring shellargs = os.str();// harden the arguments

	ShExecInfo.cbSize = sizeof(ShExecInfo);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = exParamValue_aacpath.paramString;// TargetFile to execute/open
	ShExecInfo.lpParameters = shellargs.c_str();
	ShExecInfo.lpDirectory = tempdirname.c_str();
	ShExecInfo.nShow = SW_SHOWNORMAL; // SW_SHOW;
	ShExecInfo.hInstApp = NULL;	

	// Just in case the output-file already exists, delete it
	DeleteFileW( out_aacfilename );

	BOOL rc = ShellExecuteExW(&ShExecInfo);

	// If shellexec was successful, then 
	//		wait for neroAacEnc.exe to finish (could take a while...)
	if ( rc ) {
		WaitForSingleObject(ShExecInfo.hProcess,INFINITE);

		// now verify the output file really exists
		FILE *fp = _wfopen( out_aacfilename, L"rb" );
		if ( fp == NULL ) 
			rc = false; // can't find the file, something went wrong!
		else
			fclose( fp );
	}

	return rc;
}

//
// NVENC_spawn_neroaacenc() - convert NVENC-generated *.WAV file into *.AAC file
//     Spawn neroAacEnc.exe as a process which reads from STDIN.
//     nvenc_export's wav-writer will pass audio-data to STDIN of the neroAacEnc process.
//    
//     After calling the spawn process, nvenc_export must write audiodata into the pipe.
//     (and finally, call NVENC_wait_neroaacenc to wait for neroAacEnc to indicate completion.)
//
//     Returns: true if successful
//              false otherwise

bool
NVENC_spawn_neroaacenc(
	const csSDK_uint32 exID,
	ExportSettings *mySettings,
	const wchar_t out_aacfilename[]  // output *.AAC filename
)
{
	std::wostringstream os;
	wstring tempdirname;
	STARTUPINFOW siStartInfo;
	wchar_t *pwstring = NULL; // pointer to a wchar_t string
	csSDK_int32					mgroupIndex		= 0;
	exParamValues exParamValue_aacpath, exParamValue_temp;
	int32_t kbitrate; // audio bitrate (Kbps)
	
	mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ParamID_AudioFormat_NEROAAC_Path, &exParamValue_aacpath);
	mySettings->exportParamSuite->GetParamValue( exID, mgroupIndex, ADBEAudioBitrate, &exParamValue_temp);
	kbitrate = exParamValue_temp.value.intValue; // audio bitrate (Kbps)
	nvenc_make_output_dirname( out_aacfilename, tempdirname );
	
	//
	// build the command-line to execute neroAAC.
	// It will look something like this:
	//
	//    neroaacenc.exe -br 12340000 -if - -of "out_aacfilename.aac"
	os << exParamValue_aacpath.paramString; // the execution-path to neroAacEnc.exe
	os << " -br " << std::dec << (kbitrate*1024);
	os << " -if - "; // input-file: stdin
	os << " -of \"" << out_aacfilename << "\"";
	pwstring = new wchar_t[ os.str().size() ];
	lstrcpyW( pwstring, os.str().c_str() ); // harden the arguments

	// Just in case the output-file already exists, delete it
	DeleteFileW( out_aacfilename );
 
// Set up members of the PROCESS_INFORMATION structure. 
 
   ZeroMemory( &(mySettings->SDKFileRec.child_piProcInfo), sizeof(PROCESS_INFORMATION) );
 
// Set up members of the STARTUPINFO structure. 
// This structure specifies the STDIN and STDOUT handles for redirection.
 
   ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
   siStartInfo.cb = sizeof(STARTUPINFO); 
   siStartInfo.hStdError = mySettings->SDKFileRec.FileRecord_AAClog.hfp;
   siStartInfo.hStdOutput = mySettings->SDKFileRec.FileRecord_AAClog.hfp;
   siStartInfo.hStdInput = mySettings->SDKFileRec.H_pipe_aacin; // WAVout
   siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
 
// Create the child process. 
   BOOL bSuccess = CreateProcessW(NULL, 
		pwstring,		// command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		tempdirname.c_str(), // use parent's current directory 
		&siStartInfo,  // STARTUPINFO pointer 
		&(mySettings->SDKFileRec.child_piProcInfo)
	);  // receives PROCESS_INFORMATION 

	//delete pwstring;// don't forget to free this manually allocated buffer

	return bSuccess;
}

bool
NVENC_wait_neroaacenc(
	ExportSettings *mySettings,
	const wchar_t out_aacfilename[]  // output *.AAC filename
)
{
	bool bSuccess = true;

	WaitForSingleObject(
		mySettings->SDKFileRec.child_piProcInfo.hProcess,
		INFINITE
	);

	CloseHandle( mySettings->SDKFileRec.H_pipe_aacin );
	CloseHandle( mySettings->SDKFileRec.H_pipe_wavout );
	CloseHandle( mySettings->SDKFileRec.FileRecord_AAClog.hfp );
	mySettings->SDKFileRec.H_pipe_aacin = NULL;
	mySettings->SDKFileRec.H_pipe_wavout= NULL;

	DWORD dw;
	GetExitCodeProcess( mySettings->SDKFileRec.child_piProcInfo.hProcess, &dw );

	if ( dw == 0 ) {
		// neroAacEnc succeeded: no need to keep the logfile so delete it
		DeleteFileW( mySettings->SDKFileRec.FileRecord_AAClog.filename.c_str() );
	}
	else {
		// neroAacEnc failed, append an error message
		bSuccess = false;
		FILE *fp = _wfopen( 
			mySettings->SDKFileRec.FileRecord_AAClog.filename.c_str(),
			L"r+"
		);

		fprintf( fp, "nvenc_export ERROR: process exited with %0u (0x%0X)\n",
			dw, dw );
		fclose( fp );
	}

	// now verify the output file really exists
	FILE *fp = _wfopen(
		out_aacfilename,
		L"rb"
	);
	if ( fp == NULL ) 
		bSuccess = false; // can't find the file, something went wrong!
	else
		fclose( fp );

	return bSuccess;
}


//////////////////////////////////////////////////////////////////
//
//	prWriteFile - File write.
//

int 
prWriteFile (
	imFileRef		refNum, 
	const void 		*data, 
	csSDK_uint32	*bytes)
{
	csSDK_uint32		orig_bytes = *bytes;
	csSDK_int32		err;
	#ifdef PRWIN_ENV
	err = !WriteFile(refNum, data, (DWORD)*bytes, (LPDWORD)bytes, NULL);
	if (err)
	{
		err = GetLastError ();
	}
	#else
	err = FSWriteFork(	reinterpret_cast<intptr_t>(refNum),
						fsAtMark,
						0,
						orig_bytes,
						data,
						reinterpret_cast<ByteCount*>(bytes));
	#endif
	
	if (!err && (*bytes != orig_bytes)) {
		err = exportReturn_OutOfDiskSpace;
	}
	return err;
}

//////////////////////////////////////////////////////////////////
//
//	WriteRLE - Simple RLE function, frame by frame.
//
//	RLE Compress the data in the frame, then write to file
//
/*
void WriteRLE(long *src, compFileRef ref, long totalPix)
{
	long			totalNodes 	= 1;
	unsigned long	in_bytes = 0;		
	Node *  nodes = (Node*)malloc((sizeof(Node) * totalPix));	// Create Nodes
	register Node * N = nodes;
	register long * s1 = src;
	register long * s2 = src;
	
	++s2;				

	if (!N)				// Make sure I've got the memory...
		return;
	
	N->count = 1;		// initialize the first element in our node
	N->pixel = *src;	// get the value of the first pixel from src

	while (--totalPix)
	{	
		// Look at the source buffer, compare to next. pixel

		if (*s1++ == *s2++)
		{
			// If the current and next pixel values match,
			// increment the count of the already stored
			// pixel.
			++(N->count);
		}
		else
		{
			//	If the current and next pixel don't match
			//	Increment the node position, start a new node count
			//	assign the value of the current pixel and increment
			//	the total number of nodes created.
			++N;
			N->count = 1;
			N->pixel = *s1;
			++totalNodes;
		}
	}
	in_bytes = totalNodes * sizeof(Node);
	prWriteFile(ref,
				nodes,
				&in_bytes);

	free(nodes);
	
}
*/

//////////////////////////////////////////////////////////////////
//
//	ReadSDKFileAsync - Read theFrame from the indicated SDK file,
//	Returns a populated inFrameBuffer. 
//	Only for uncompressed SDK files
//

#ifdef PRWIN_ENV
unsigned char ReadSDKFileAsync(
	imFileRef	SDKfileRef, 
	csSDK_int32	frameBytes, 
	csSDK_int32	theFrame, 
	char		*inFrameBuffer,
	OVERLAPPED	*overlapped)
{
	prBool			didRead		= 0;
	DWORD			lastError	= 0;
	csSDK_int32		offset		= sizeof(SDK_File);
	csSDK_uint32	bytesRead	= 0;

	offset += theFrame * frameBytes;
	if( theFrame > 0)
	{	
		// take the extra 4 bytes (\n\n\n\n) at the end of each frame into account
		offset += (4 * (theFrame));
	}

	overlapped->Offset = offset;
	overlapped->OffsetHigh = 0;
	didRead = ReadFile(	SDKfileRef,
						inFrameBuffer,
						frameBytes,
						NULL,
						overlapped);

	return imNoErr;
}
#endif


//////////////////////////////////////////////////////////////////
//
//	ReadSDK_File - Read theFrame from the indicated SDK file,
//	Returns a populated inFrameBuffer. 
//	Only for uncompressed SDK files
//

unsigned char ReadSDK_File(	imFileRef		SDKfileRef, 
							csSDK_int32		frameBytes, 
							csSDK_int32		theFrame, 
							char			*inFrameBuffer)
{
	csSDK_int32		offset		= sizeof(SDK_File);
	csSDK_uint32	bytesRead	= 0;

	#ifdef PRWIN_ENV
	char			didRead		= 0;
	#else
	OSErr			returnValue = 0;
	#endif

	offset += theFrame * frameBytes;
	if( theFrame > 0)
	{	
		// Take the extra bytes at the end of each frame into account
		offset += (PLUS_LINE_LENGTH * (theFrame));
	}
	
	#ifdef PRWIN_ENV
	SetFilePointer(SDKfileRef,offset,NULL,FILE_BEGIN);
	didRead = ReadFile(	SDKfileRef,
						inFrameBuffer,
						frameBytes,
						reinterpret_cast<LPDWORD>(&bytesRead),
						NULL);
	if(!didRead)
	{
		return imBadFile;
	}						
	#else
	returnValue = FSReadFork(	reinterpret_cast<intptr_t>(SDKfileRef),
								fsFromStart,
								offset,
								frameBytes,
								inFrameBuffer,
								reinterpret_cast<ByteCount*>(&bytesRead));
	if(returnValue)
	{
		return imBadFile;
	}							
	#endif

	return imNoErr;
}


//////////////////////////////////////////////////////////////////
//
//	ScaleAndBltFrame - Scaling Function
//		
//	Designed to work with SDK format files, modify for your own importer needs
//		

void ScaleAndBltFrame(imStdParms		*stdParms,
					  SDK_File			fileHeader,
					  csSDK_uint32		frameBytes,
					  char				*inFrameBuffer, 
					  imImportImageRec	*imageRec)
{
	// original source and dest in pixels
	csSDK_int32	srcWidth, srcHeight, dstWidth, dstHeight, dstCoorW,dstCoorH;
	float		ratioW, ratioH;
	char		paddingBytes = 0;
	
	char		*tempPix;
	csSDK_int32	*dstPix = (csSDK_int32*)imageRec->pix; 
	
	paddingBytes = (imageRec->rowbytes) - (imageRec->dstWidth * 4); 

	srcWidth	= fileHeader.width;;
	srcHeight	= fileHeader.height;
	dstWidth	= imageRec->dstWidth;
	dstHeight	= imageRec->dstHeight;

	// coordinate numbers, ratios
	
	ratioW		= (float)srcWidth / (float)dstWidth;
	ratioH		= (float)srcHeight / (float)dstHeight;
	
	// loop through the destination coordinate grid, find the "virtual" pixel in source grid

	for(dstCoorH = 0; dstCoorH < dstHeight; ++dstCoorH)
	{
		for(dstCoorW = 0; dstCoorW < dstWidth; ++dstCoorW)
		{
			*dstPix = GetSrcPix(inFrameBuffer, fileHeader,dstCoorW,dstCoorH,ratioW,ratioH);
			++dstPix;
		}
		// add the padding bytes to the dst after it's scaled
		tempPix = (char*)dstPix;
		tempPix += paddingBytes;
		dstPix = (csSDK_int32*)tempPix;
	}
	return;
}

//////////////////////////////////////////////////////////////////
//
//	GetSrcPix - Utility function used by the scaling functions
//	

csSDK_int32 GetSrcPix(char			*inFrameBuffer, 
			   SDK_File		fileHeader,
			   csSDK_uint32	dstCoorW, 
			   csSDK_uint32	dstCoorH, 
			   float		ratioW, 
			   float		ratioH)
{
	csSDK_uint32	w, h;
	csSDK_uint32	*thePixel = reinterpret_cast<csSDK_uint32*>(inFrameBuffer);

	// The translated coordinates

	w = static_cast<csSDK_uint32>(dstCoorW * ratioW);
	h = static_cast<csSDK_uint32>(dstCoorH * ratioH);
	
	thePixel += (h * fileHeader.width) + w; 

	return *thePixel;
}


// Source and destination frames may be the same
void RemoveRowPadding(	char		*srcFrame,
						char		*dstFrame, 
						csSDK_int32 rowBytes, 
						csSDK_int32 pixelSize,
						csSDK_int32 widthL, 
						csSDK_int32 heightL)
{
	csSDK_int32 widthBytes = widthL * pixelSize;

	if (widthBytes < rowBytes)
	{
		for(csSDK_int32 hL = 0; hL < heightL; ++hL)
		{
			memcpy (&dstFrame[hL * widthBytes], &srcFrame[hL * rowBytes], widthBytes);
		}
	}

	return;
}


// Source and destination frames may be the same
void AddRowPadding(	char			*srcFrame,
					char			*dstFrame, 
					csSDK_uint32	rowBytesL, 
					csSDK_uint32	pixelSize,
					csSDK_uint32	widthL, 
					csSDK_uint32	heightL)
{
	csSDK_uint32 widthBytes = widthL * pixelSize;

	if (widthBytes < rowBytesL)
	{
		// Expand rows starting from last row, so that we can handle an in-place operation
		for(csSDK_int32 hL = heightL - 1; hL >= 0; --hL)
		{
			memcpy (&dstFrame[hL * rowBytesL], &srcFrame[hL * widthBytes], widthBytes);
		}
	}

	return;
}

#define _SafeReportEvent( id, eventtype, title, desc) \
	if ( !pre11_suppress_messages ) \
		mySettings->exporterUtilitySuite->ReportEventA( \
			id, eventtype, title, desc \
		);

// Returns malNoError if successful, or comp_CompileAbort if user aborted
prMALError RenderAndWriteVideoFrame(
	const bool					isFrame0,    // are we rendering the first-frame of the session?
	const bool					dont_encode, // if true, then don't send video to CNvEncoderH264
	const PrTime				videoTime,
	exDoExportRec				*exportInfoP)
{
	string						errstr; // error-string
	csSDK_int32					resultS					= malNoError;
	csSDK_uint32				exID					= exportInfoP->exporterPluginID;
	ExportSettings				*mySettings = reinterpret_cast<ExportSettings *>(exportInfoP->privateData);
	csSDK_int32					rowbytes				= 0;
//	csSDK_int32					renderedPixelSize		= 0;
	exParamValues				width,
								height,
								pixelAspectRatio,
								fieldType,
								codecSubType,
								temp_param;
	PrPixelFormat				renderedPixelFormat;
	csSDK_uint32				bytesToWriteLu			= 0;
	char						*frameBufferP			= NULL,
								*f32BufferP				= NULL,
								*frameNoPaddingP		= NULL,
								*v410Buffer				= NULL;
	HWND mainWnd                        = mySettings->windowSuite->GetMainWindow();
	SequenceRender_ParamsRec renderParms;

	// logmessage generation
	std::wostringstream os;
	prUTF16Char eventTitle[256];
	prUTF16Char eventDesc[512];
	bool pre11_suppress_messages = false; // workaround for Premiere Elements 11, don't call ReportEvent

	PrPixelFormat *pixelFormats = NULL;
	csSDK_int32 nvenc_pixelformat; // selected NVENC input pixelFormat (user-parameter from plugin)
	mySettings->exportParamSuite->GetParamValue(exID, 0, ParamID_chromaFormatIDC, &temp_param);
	nvenc_pixelformat = temp_param.value.intValue;
	
	// Frmaebuffer format flags: what chromaformat video is Adobe-app outputting?
	//   Exactly one of the following flags must be true.
	// 
	const bool adobe_yuv444 = (nvenc_pixelformat >= NV_ENC_BUFFER_FORMAT_YUV444_PL);// a 4:4:4 format is in use (instead of 4:2:0)
	bool adobe_yuv420 = false;// (Adobe is outputing YUV420, i.e. 'YV12')
	bool adobe_yuv422 = false;// (Adobe is outputing YUV420, i.e. 'YV12')


	if ( isFrame0 ) {
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

		if ( mySettings->forced_PixelFormat0 ) {
			// autoselect-disabled:  only the user-select pixelformat will be advertised.
			renderParms.inRequestedPixelFormatArray = &(mySettings->requested_PixelFormat0);
			renderParms.inRequestedPixelFormatArrayCount = 1;
		}
		else if ( adobe_yuv444 ) {
			// Packed Pixel YUV 4:4:4 (24bpp + 8bit alpha, NVECN doesn't use the alpha-channel)
			//
			// Assume NVENC is configured to encode 4:4:4 video. 
			// (because we never send packed YUV 4:4:4 to NVENC. in 4:2:0 H264 encoding mode)
			renderParms.inRequestedPixelFormatArray = SupportedPixelFormats444;
			renderParms.inRequestedPixelFormatArrayCount = sizeof(SupportedPixelFormats444)/sizeof(SupportedPixelFormats444[0]);
		}
		else {
			// Assume NVENC is configured to encode 4:2:0 video.
			//
			// Planar YUV 4:2:0
			renderParms.inRequestedPixelFormatArray = SupportedPixelFormats420;
			renderParms.inRequestedPixelFormatArrayCount = sizeof(SupportedPixelFormats420)/sizeof(SupportedPixelFormats420[0]);
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

	if ( isFrame0 ) {
		// Frame#0 special: we submit a table of requested/acceptable PrPixelFormats,
		//                  and let adobe automatically select one for us.
		resultS = mySettings->sequenceRenderSuite->RenderVideoFrame(
					mySettings->videoRenderID,
					videoTime,
					&renderParms,
					kRenderCacheType_None,	// [TODO] Try different settings
					&renderResult);

		// Kludge for Adobe YUV_420_Planar format incompatiblity with most MPE hardware effects
		//
		// Problem:
		// --------
		// When the MPE (mercury playback engine) is configured to use CPU-only (software),
		// the Adobe videorender will accept any requested PrPixelFormat (including our preferred 420_Planar
		// formats.)
		//
		// When MPE is configured to use hardware-acceleration (CUDA), *and* an MPE-accelerated effect
		// is present & enabled anywhere in the exported video timeline, then RenderVideoFrame() will
		// return an error code if the <renderParms.inRequestedPixelFormatArray> contains any entries of
		// the Planar_420 type.
		//
		// Workaround:
		// -----------
		// The above problem is fixed by making another request with <renderParms.inRequestedPixelFormatArray> 
		// containing nothing but packed-pixel YUV entries (in our case, UYVY422/YUYV422.)  For some reason,
		// combining both Planar & packed-pixel422 in the same Array, still causes an error.  So we
		// let the first render attempt (with YUV420planar) fail, and then make a second attempt with the
		// packed422 formats.

		if ( PrSuiteErrorFailed(resultS) && !adobe_yuv444) {
			// chromaFormat: YUV420

			// The videorender failed with our requested Planar YUV 4:2:0 formats.
			//
			// Inform the user: Write informational message to Adobe's log:
			//    which pixelformat did Adobe choose for us?
			os.clear();
			os.flush();
			os << "Video Frame#0 info: Adobe failed to render output to requested YUV_420_Planar pixelformat(s)";
			os << "'" << std::endl;
			copyConvertStringLiteralIntoUTF16( os.str().c_str(), eventDesc);
			copyConvertStringLiteralIntoUTF16(L"Note from RenderAndWriteVideoFrame()", eventTitle);
			_SafeReportEvent( 
				exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
			);


			//    ... so retry the videorender with YUV 4:2:2 packed-pixel instead
			renderParms.inRequestedPixelFormatArray = SupportedPixelFormats422;
			renderParms.inRequestedPixelFormatArrayCount = sizeof(SupportedPixelFormats422)/sizeof(SupportedPixelFormats422[0]);

			resultS = mySettings->sequenceRenderSuite->RenderVideoFrame(
					mySettings->videoRenderID,
					videoTime,
					&renderParms,
					kRenderCacheType_None,	// [TODO] Try different settings
					&renderResult);
		}

		if ( PrSuiteErrorFailed(resultS) ) {
			ostringstream o;
			PrParam		hasVideo, seqWidth, seqHeight;

			Check_prSuiteError(resultS, errstr );
			o << __FILE__ << "(" << std::dec << __LINE__ << "): ";
			o << "There was a problem with exporting video." << std::endl;
			o << "On frame#0, RenderVideoFrame() failed with error-value: " << errstr;

			// 2 problems:
			// -----------
			// (1) Checking the source-video and framesize here doesn't work, because
			//     Adobe presents a exportInfoSuite with modified properties that are
			//     'conformed' to the user-selected output-size. 

			// (2) In Premeire CS6 and CC, if CUDA hardware-acceleration is enabled,
			//     Adobe's video-renderer will refuse to resize Pixelformat YUV.
			//     (It seems that packed-pixel RGB 32bpp is the only format natively
			//      resized by the CUDA-accelerated MPE.)
			mySettings->exportInfoSuite->GetExportSourceInfo( exID,
											kExportInfo_SourceHasVideo,
											&hasVideo);
			mySettings->exportInfoSuite->GetExportSourceInfo( exID,
											kExportInfo_VideoWidth,
											&seqWidth);
			mySettings->exportInfoSuite->GetExportSourceInfo( exID,
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

			MessageBox(	GetLastActivePopup(mainWnd),
				o.str().c_str(),
				EXPORTER_NAME,
				MB_ICONERROR
			);

			return resultS;
		} // PrSuiteErrorFailed

		// Record the pixelFormat of the 1st frame of output. 
		// (NVENC-plugin will lock the remainder of the video-render to this format.)
		resultS = mySettings->ppixSuite->GetPixelFormat(renderResult.outFrame, &renderedPixelFormat);
		if ( PrSuiteErrorFailed(resultS) ) {
			ostringstream o;

			Check_prSuiteError(resultS, errstr );
			o << __FILE__ << "(" << std::dec << __LINE__ << "): ";
			o << "GetPixelFormat failed with error-value: " << errstr;
			o << std::endl;
			MessageBox(	GetLastActivePopup(mainWnd),
				o.str().c_str(),
				EXPORTER_NAME,
				MB_ICONERROR
			);

			return resultS;
		}

		mySettings->rendered_PixelFormat0 = renderedPixelFormat;

		// tell the user which PrPixelFormat Adobe will use to render the whole video
		os.clear();
		os.flush();
		os << "Video Frame#0 info: Adobe chose '";
		for( unsigned i = 0; i < 4; ++i )
			os <<  (static_cast<char>((renderedPixelFormat >> (i<<3)) & 0xFFU));

		os << "'" << std::endl;
		copyConvertStringLiteralIntoUTF16( os.str().c_str(), eventDesc);
		copyConvertStringLiteralIntoUTF16(L"Note from RenderAndWriteVideoFrame()", eventTitle);
		_SafeReportEvent( 
			exID, PrSDKErrorSuite3::kEventTypeWarning, eventTitle, eventDesc
		);

		// update the chroma-format flags: we're either in 422 or 420 mode
		//
		// 
		if ( PrPixelFormat_is_YUV420(renderedPixelFormat) )
			adobe_yuv420 = true;
		else if ( PrPixelFormat_is_YUV422(renderedPixelFormat) )
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
		Check_prSuiteError(resultS, errstr );
	}

	// If user hit cancel
	if (resultS == suiteError_CompilerCompileAbort)
	{
		// just return here
		return resultS;
	}

	EncodeFrameConfig nvEncodeFrameConfig = {0};
	nvEncodeFrameConfig.height = height.value.intValue;
	nvEncodeFrameConfig.width  = width.value.intValue;

	// CNvEncoderH264 must know the source-video's pixelformat, in order to
	//    convert it into an NVENC compatible format (NV12 or YUV444)
	nvEncodeFrameConfig.ppro_pixelformat = mySettings->rendered_PixelFormat0;
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv420 = PrPixelFormat_is_YUV420( mySettings->rendered_PixelFormat0 );
	nvEncodeFrameConfig.ppro_pixelformat_is_yuv444 = PrPixelFormat_is_YUV444( mySettings->rendered_PixelFormat0 );
	nvEncodeFrameConfig.ppro_pixelformat_is_uyvy422 = 
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_UYVY_422_8u_601) ||
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_UYVY_422_8u_709);
	nvEncodeFrameConfig.ppro_pixelformat_is_yuyv422 = 
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_YUYV_422_8u_601) ||
		(mySettings->rendered_PixelFormat0 == PrPixelFormat_YUYV_422_8u_709);

	// NVENC picture-type: Interlaced vs Progressive
	//
	// Note that the picture-type must match the selected encoding-mode.
	// In interlaced or MBAFF-mode, NVENC still requires all sourceFrames to be tagged as fieldPics
	// (even if the sourceFrame is truly progressive.)
	mySettings->exportParamSuite->GetParamValue(exID, 0, ParamID_FieldEncoding, &temp_param);
	nvEncodeFrameConfig.fieldPicflag = ( temp_param.value.intValue == NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME ) ?
		false :
		true;
	nvEncodeFrameConfig.topField = true; // default
	if ( nvEncodeFrameConfig.fieldPicflag ) {
		PrSDKExportInfoSuite	*exportInfoSuite	= mySettings->exportInfoSuite;
		PrParam	seqFieldOrder;  // video-sequence field order (top_first/bottom_first)
		exportInfoSuite->GetExportSourceInfo( exID,
										kExportInfo_VideoFieldType,
										&seqFieldOrder);
		nvEncodeFrameConfig.topField = (seqFieldOrder.mInt32 == prFieldsLowerFirst) ? false : true;
	}

	if ( adobe_yuv444 || adobe_yuv422 ) {
		// 4:4:4 packed pixel - only pointer[0] is used, (1 & 2 aren't)
		mySettings->ppixSuite->GetPixels(	renderResult.outFrame,
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
	else {
		size_t       rowsize;
		csSDK_uint32 stride[3]; // #bytes per row (for each of Y/U/V planes)

		// 4:2:0 planar (all 3 pointers used)
		// must use ppix2Suite to access Planar frame(s)
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

	// Submit the Adobe rendered frame to NVENC:
	//   (1) If NvEncoder is operating in 'async_mode', then the call will return as soon
	//       as the frame is placed in the encodeQueue.
	//   (2) if NvEncoder is operating in 'sync_mode', then call will not return until
	//       NVENC has completed encoding of this frame.
	HRESULT hr = S_OK;
	if ( !dont_encode )
		hr = mySettings->p_NvEncoder->EncodeFramePPro( 
			&nvEncodeFrameConfig, 
			false  // flush?
		);
				
	// Now that buffer is written to disk, we can dispose of memory
	mySettings->ppixSuite->Dispose(renderResult.outFrame);

	return (hr == S_OK) ? resultS : resultS;
}


void calculateAudioRequest(
	imImportAudioRec7	*audioRec7,
	const PrAudioSample	totalSampleFrames,
	PrAudioSample		*savedAudioPosition,
	PrAudioSample		*startAudioPosition,
	PrAudioSample		*numAudioFrames)
{
	// If audioRec7->position is less than zero, this means Premiere Pro is requesting
	// contiguous samples from the last call
	if (audioRec7->position < 0)
	{
		*startAudioPosition = *savedAudioPosition;
	}
	else
	{
		*startAudioPosition = audioRec7->position;
	}

	// If amount requested is more than amount left in file
	if (*startAudioPosition + audioRec7->size > totalSampleFrames)
	{
		// Update number of actual audio frames to read
		*numAudioFrames = totalSampleFrames - *startAudioPosition;

        // Save off next audio position, in case another sequential call is made
		*savedAudioPosition = totalSampleFrames;
	}
	else
	{
		// Update number of actual audio frames to read
		*numAudioFrames = audioRec7->size;

        // Save off next audio position, in case another sequential call is made
		*savedAudioPosition = *startAudioPosition + audioRec7->size;
	}

	return;
}


void setPointerToAudioStart(
	ImporterLocalRec8H		ldataH,
	const PrAudioSample		startAudioPosition,
	imFileRef				SDKfileRef)
{
	csSDK_uint32		totalVideoFramesLu	= 0;
	csSDK_int64			bytesPerFrameLL		= 0,
						videoOffsetLL		= 0;
	PrAudioSample		audioOffset			= 0;

	#ifdef PRWIN_ENV
	csSDK_int32			tempErrorS			= 0;
	LARGE_INTEGER		distanceToMoveLI;
	#else
	SInt64				distanceToMove;
	#endif

	totalVideoFramesLu	= (*ldataH)->theFile.numFrames;
	if ((*ldataH)->theFile.hasVideo && totalVideoFramesLu > 0)
	{	
		bytesPerFrameLL		= (*ldataH)->theFile.width * (*ldataH)->theFile.height *
			GetPixelFormatSize((*ldataH)->theFile.videoSubtype);

		videoOffsetLL += totalVideoFramesLu * bytesPerFrameLL;

		// Take the extra bytes at the end of each frame into account
		videoOffsetLL += (PLUS_LINE_LENGTH * (totalVideoFramesLu));
	}

	audioOffset = startAudioPosition * AUDIO_SAMPLE_SIZE;

	#ifdef PRWIN_ENV
	distanceToMoveLI.QuadPart = sizeof(SDK_File) + videoOffsetLL + audioOffset;
	tempErrorS = SetFilePointerEx(SDKfileRef,
								distanceToMoveLI,
								NULL,
								FILE_BEGIN);

	if (tempErrorS == INVALID_SET_FILE_POINTER)
	{
		GetLastError ();
	}
	#else
	distanceToMove = sizeof(SDK_File) + videoOffsetLL + audioOffset;
	FSSetForkPosition (	reinterpret_cast<intptr_t>(SDKfileRef),
						fsFromStart,
						distanceToMove);
	#endif
}


prMALError readAudioToBuffer (	const PrAudioSample	numAudioFrames,
								const PrAudioSample	totalSampleFrames,
								const csSDK_int32	numAudioChannels,
								imFileRef			SDKfileRef,
								float **			audioBuffer)
{
	prMALError			result				= malNoError;
	csSDK_uint32		bytesReadLu			= 0;

	#ifdef PRWIN_ENV
	csSDK_int32			didReadL			= 0;
	csSDK_int32			tempErrorS			= 0;
	LARGE_INTEGER		distanceToMoveLI;
	#else
	SInt64				distanceToMove;
	#endif

	// Read all channels into their respective buffers
	for (csSDK_int32 bufferIndexL = 0; bufferIndexL < numAudioChannels; bufferIndexL++)
	{
		#ifdef PRWIN_ENV
		didReadL = ReadFile(SDKfileRef,
							audioBuffer[bufferIndexL],
							static_cast<csSDK_int32>(numAudioFrames) * AUDIO_SAMPLE_SIZE,
							reinterpret_cast<LPDWORD>(&bytesReadLu),
							NULL);
		if (!didReadL)
		{
			return imBadFile;
		}

		#else
		result = FSReadFork(reinterpret_cast<intptr_t>(SDKfileRef),
							fsAtMark,
							0,
							numAudioFrames * AUDIO_SAMPLE_SIZE,
							audioBuffer[bufferIndexL],
							reinterpret_cast<ByteCount*>(&bytesReadLu));
		if (result)
		{
			return imBadFile;
		}
		#endif

		// Move file pointer to next audio channel
		#ifdef PRWIN_ENV
		distanceToMoveLI.QuadPart = (totalSampleFrames - numAudioFrames) * AUDIO_SAMPLE_SIZE;
		tempErrorS = SetFilePointerEx(	SDKfileRef,
										distanceToMoveLI,
										NULL,
										FILE_CURRENT);

		if (tempErrorS == INVALID_SET_FILE_POINTER)
		{
			GetLastError ();
		}
		#else
		distanceToMove = (totalSampleFrames - numAudioFrames) * AUDIO_SAMPLE_SIZE;
		result = FSSetForkPosition(	reinterpret_cast<intptr_t>(SDKfileRef),
									fsFromMark,
									distanceToMove);
		#endif
	}

	return result;
}


// Returns malNoError if successful
prMALError RenderAndWriteAllAudio(
	exDoExportRec				*exportInfoP,
	PrTime						exportDuration)
{
	csSDK_int32					resultS					= malNoError;
	csSDK_uint32				exID					= exportInfoP->exporterPluginID;
	ExportSettings				*mySettings				= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	exParamValues				ticksPerFrame,
								sampleRate,
								channelType;
	csSDK_int64					distanceToMove;
	csSDK_int64					filePtrLocation;
	PrAudioSample				totalAudioSamples		= 0,
								samplesRemaining		= 0;
	csSDK_uint32				audioRenderID			= 0;
	csSDK_int32					maxBlip					= 0;
	csSDK_int32					audioBufferSizeL		= 0,
								samplesRequestedL		= 0,
								audioChannelsL			= 0;
	float *						audioBufferFloat[6]		= {NULL, NULL, NULL, NULL, NULL, NULL};
	short *						audioBuffer16bit		= NULL;
	__m128i *					audioBuffer16bit_aligned_sse2 = NULL;
	csSDK_uint32				bytesToWriteLu			= 0;

	PrSDKMemoryManagerSuite	*memorySuite	= mySettings->memorySuite;
	PrSDKTimeSuite			*timeSuite		= mySettings->timeSuite;
	PrSDKExportInfoSuite	*exportInfoSuite= mySettings->exportInfoSuite;
	PrTime					ticksPerSample	= 0;
	PrParam					srcChannelType;

	PrSDKExportParamSuite	*paramSuite	= mySettings->exportParamSuite;
	paramSuite->GetParamValue(exID, 0, ADBEVideoFPS, &ticksPerFrame);
	paramSuite->GetParamValue(exID, 0, ADBEAudioRatePerSecond, &sampleRate);
	paramSuite->GetParamValue(exID, 0, ADBEAudioNumChannels, &channelType);
	audioChannelsL = GetNumberOfAudioChannels (channelType.value.intValue);

	// get the #audio-channels from the export-source
	exportInfoSuite->GetExportSourceInfo( exID,
											kExportInfo_AudioChannelsType,
											&srcChannelType);
	// kludge - TODO we don't support 16-channel audio,
	//          so switch 16-channel to 5.1 audio
	if ( srcChannelType.mInt32 >= kPrAudioChannelType_16Channel )
		srcChannelType.mInt32 = kPrAudioChannelType_51;

//	bool audioformat_incompatible = 
//	 ( srcChannelType.mInt32 == kPrAudioChannelType_51 && (audioChannelsL > 6)) ||
//	 ( srcChannelType.mInt32 == kPrAudioChannelType_Stereo && (audioChannelsL > 2)) ||
//	 ( srcChannelType.mInt32 == kPrAudioChannelType_Mono && (audioChannelsL > 1));

	timeSuite->GetTicksPerAudioSample ((float)sampleRate.value.floatValue, &ticksPerSample);

	prSuiteError serr = mySettings->sequenceAudioSuite->MakeAudioRenderer(	exID,
														exportInfoP->startTime,
														(PrAudioChannelType)channelType.value.intValue,
														kPrAudioSampleType_32BitFloat,
														(float)sampleRate.value.floatValue,
														&audioRenderID);

	bool audioformat_incompatible = ( serr == suiteError_NoError ) ? false : true;

	totalAudioSamples = exportDuration / ticksPerSample;
	samplesRemaining = totalAudioSamples;

	// Find size of blip to ask for
	// The lesser of the value returned from GetMaxBlip and number of samples remaining
	if ( !audioformat_incompatible ) 
		serr = mySettings->sequenceAudioSuite->GetMaxBlip (audioRenderID, ticksPerFrame.value.timeValue, &maxBlip);

	if (maxBlip < samplesRemaining)
	{
		samplesRequestedL = maxBlip;
	}
	else
	{
		samplesRequestedL = (csSDK_int32) samplesRemaining;
	}

	//   Premiere Elements 12: crashes if the output-audioformat has more channels 
	//   than the source-audioformat.
	//
	//   Workaround:
	//   Compare the source-audioformat with the selected output-audioformat,
	//   Abort on incompatible output-format.
	if ( maxBlip == 0 || audioformat_incompatible ) { 
		wostringstream oss; // text scratchpad for messagebox and errormsg 
		prUTF16Char title[256];
		prUTF16Char desc[256];
		HWND mainWnd = mySettings->windowSuite->GetMainWindow();

		copyConvertStringLiteralIntoUTF16( L"NVENC-export error: AUDIO", title );
		oss << "There was a problem with exporting Audio:" << endl << endl;
		oss << "The source and output audio channels are not compatible or a conversion does not exist. " << endl;
		oss << "In the nvenc_export tab 'Audio', please reduce the #Channels and try again." << endl;

		copyConvertStringLiteralIntoUTF16( oss.str().c_str(), desc );
		mySettings->errorSuite->SetEventStringUnicode( PrSDKErrorSuite::kEventTypeError, title, desc );

		MessageBoxW( GetLastActivePopup(mainWnd),
								oss.str().c_str(),
								EXPORTER_NAME_W,
								MB_OK | MB_ICONERROR );

		mySettings->sequenceAudioSuite->ReleaseAudioRenderer(	exID,
															audioRenderID);
		return exportReturn_IncompatibleAudioChannelType;
	}

	// Set temporary audio buffer size (measured in samples)
	// to be size of first blip requested
	audioBufferSizeL = samplesRequestedL;

	// Allocate audio buffers
	audioBuffer16bit = (short *) memorySuite->NewPtr (4*sizeof(__m128i) + audioChannelsL * audioBufferSizeL * sizeof(short));

	// Align the buffer to a 16-byte boundary (for SSE2/3)
	audioBuffer16bit_aligned_sse2 = reinterpret_cast<__m128i *>(
		reinterpret_cast<uint64_t>(audioBuffer16bit+7) - (reinterpret_cast<uint64_t>(audioBuffer16bit+7) & 15)
	);

	for (csSDK_int32 bufferIndexL = 0; bufferIndexL < audioChannelsL; bufferIndexL++)
	{
		audioBufferFloat[bufferIndexL] = (float *) memorySuite->NewPtr (audioBufferSizeL * AUDIO_SAMPLE_SIZE);
	}

	uint64_t samples_since_update = 0;
	while(samplesRemaining && (resultS == malNoError))
	{
		// Fill the buffer with audio
		resultS = mySettings->sequenceAudioSuite->GetAudio(	audioRenderID,
															(csSDK_uint32) samplesRequestedL,
															audioBufferFloat,
															kPrFalse);

		if (resultS == malNoError)
		{
			// convert the 32-bit float audio -> 16-bit int audio
			mySettings->audioSuite->ConvertAndInterleaveTo16BitInteger( 
				audioBufferFloat,
				reinterpret_cast<short *>(audioBuffer16bit_aligned_sse2),
				audioChannelsL,
				samplesRequestedL
			);

			// remap the channel-order: Adobe -> RIFF-WAV
			// (Note, this routine requires processor-support for SSSE3 instructions)
			if ( audioChannelsL > 2 )
				adobe2wav_audio51_swap_ssse3( samplesRequestedL, audioBuffer16bit_aligned_sse2 );
			bytesToWriteLu = audioChannelsL * samplesRequestedL * 2 ;

			// Write out the buffer of audio retrieved
			//resultS = mySettings->exportFileSuite->Write(	exportInfoP->fileObject,
			//												reinterpret_cast<void*>(audioBuffer16bit),
			//												(csSDK_int32) bytesToWriteLu);
			/*
			size_t bytes_written = fwrite( 
				reinterpret_cast<void*>(audioBuffer16bit_aligned_sse2),
				sizeof(uint8_t), 
				bytesToWriteLu, 
				mySettings->SDKFileRec.FileRecord_Audio.fp
			);
			*/
			DWORD bytes_written = 0;
			BOOL wfrc = WriteFile(
				mySettings->SDKFileRec.FileRecord_Audio.hfp,
				reinterpret_cast<void*>(audioBuffer16bit_aligned_sse2),
				bytesToWriteLu,
				&bytes_written,
				NULL // not overlapped
			);

			// If write-operation failed, give Adobe-app a generic error 
//			if ( bytes_written != bytesToWriteLu ) {
			if ( !wfrc ) {
				resultS = exportReturn_InternalError;
				continue; // skip to top-of-loop, then loop exits immediately
			}

			// Calculate remaining audio
			samplesRemaining -= samplesRequestedL;

			// count the #samples we've processed since our previous ProgressReport
			samples_since_update += samplesRequestedL;

			// Find size of next blip to ask for
			mySettings->sequenceAudioSuite->GetMaxBlip (audioRenderID, ticksPerFrame.value.timeValue, &maxBlip);
			if (maxBlip < samplesRemaining)
			{
				samplesRequestedL = maxBlip;
			}
			else
			{
				samplesRequestedL = (csSDK_int32) samplesRemaining;
			}
			if (audioBufferSizeL < samplesRequestedL)
			{
				samplesRequestedL = audioBufferSizeL;
			}
		}

		if ( samples_since_update >= 500000 ) {
			samples_since_update -= 500000;
			mySettings->exportProgressSuite->UpdateProgressPercent(exID,
				static_cast<float>(totalAudioSamples -samplesRemaining) / totalAudioSamples
			);
/*
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
*/
		}
	} // while

	// Free up audioBuffers
	memorySuite->PrDisposePtr ((char *) audioBuffer16bit);
	for (csSDK_int32 bufferIndexL = 0; bufferIndexL < audioChannelsL; bufferIndexL++)
		memorySuite->PrDisposePtr ((char *) audioBufferFloat[bufferIndexL]);
	
	mySettings->sequenceAudioSuite->ReleaseAudioRenderer(	exID,
															audioRenderID);

	return resultS;
}


//	Writes an HTML File that contains the marker info in the same folder as the output 
void WriteMarkerAndProjectDataToFile(
	exportStdParms		*stdParmsP, 
	exDoExportRec		*exportInfoP)
{
	#ifdef PRWIN_ENV
	FILE			*fileP					= NULL;
	prMarkerRef		marker					= 0;
	char			*nameZ					= NULL,
					*commentZ				= NULL,
					*chapterZ				= NULL,
					*hrefZ					= NULL,
					*targetZ				= NULL;
	wchar_t			htmlOutputFilePath[256]	= {'\0'};
	char			settingsA[256]			= {'\0'};
	prBool			firstMarker				= kPrTrue;
	ExportSettings	*mySettings				= reinterpret_cast<ExportSettings*>(exportInfoP->privateData);

	char			HTML_begin[]			= "<html>\n<head>\n<title>SDK Exporter - Sequence Marker Data Output\n</title>\n</head>\n",
					HTML_body[]				= "<body>",
					HTML_end[]				= "</body>\n</html>",
					noMarkers[]				= "<center>There were no markers found in the Adobe Premiere Pro Sequence";
	csSDK_int32		filepathLen				= 255;
	csSDK_uint32	markerType				= 0,
					DVDmarkerType			= 0,
					numMarkers				= 0;
	PrTime			ticksPerSecond			= 0,
					markerTime				= 0,
					markerDuration			= 0;
	float			markerTimeFloat			= 0.0,
					markerDurationFloat		= 0.0;

	mySettings->exportFileSuite->GetPlatformPath(exportInfoP->fileObject, &filepathLen, htmlOutputFilePath);
	mySettings->timeSuite->GetTicksPerSecond (&ticksPerSecond);

	#ifdef PRWIN_ENV
	wcscat_s(htmlOutputFilePath, sizeof (htmlOutputFilePath) / sizeof (wchar_t), L".html");
	_wfopen_s(&fileP, htmlOutputFilePath, L"w");
	#else
	wcscat(htmlOutputFilePath, L".html");
	fileP = _wfopen(htmlOutputFilePath, L"w");
	#endif

	mySettings->markerSuite->GetMarkerCount(exportInfoP->timelineData, &numMarkers);
	marker = mySettings->markerSuite->GetFirstMarker(exportInfoP->timelineData);
	
	// If no markers in the timeline, create default "no markers" 
	if (numMarkers == 0)
	{
		fprintf(fileP, HTML_begin);
		fprintf(fileP, HTML_body);
		fprintf(fileP, settingsA);
		fprintf(fileP, "%s", &noMarkers);
		fprintf(fileP, HTML_end);
		fclose(fileP);
		
		// Exit the function, nothing else to do
		return;
	}
	
	while (marker != kInvalidMarkerRef)
	{
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_VALUE64, &markerTime);
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_DURATION64, &markerDuration);
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_NAME, reinterpret_cast<void*>(&nameZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_COMMENT, reinterpret_cast<void*>(&commentZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_CHAPTER, reinterpret_cast<void*>(&chapterZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_HREF,	reinterpret_cast<void*>(&hrefZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_TARGET, reinterpret_cast<void*>(&targetZ));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_MARKER_TYPE, reinterpret_cast<void*>(&markerType));
		mySettings->markerSuite->GetMarkerData(exportInfoP->timelineData, marker, PRT_DVD_MARKER_TYPE, reinterpret_cast<void*>(&DVDmarkerType));

		// Create an HTML table of marker information, make links active
		if (firstMarker)
		{
			fprintf(fileP, HTML_begin);
			fprintf(fileP, HTML_body);
			fprintf(fileP, "<center>\nSequence Marker Data Output<p>\n");
			fprintf(fileP, "<table border=\"4\" cellpadding=\"0\" cellspacing=\"2\" width=\"350\">\n");
			firstMarker = false;
		}

		markerTimeFloat = static_cast<float>(markerTime) / static_cast<float>(ticksPerSecond);
		fprintf(fileP, "<tr><td>Time</td><td>%.2f sec</td></tr>", markerTimeFloat);
		markerDurationFloat = static_cast<float>(markerDuration) / static_cast<float>(ticksPerSecond);
		fprintf(fileP, "<tr><td>Duration</td><td>%.2f sec</td></tr>\n", markerDurationFloat);
		fprintf(fileP, "<tr><td>Name</td><td>%s</td></tr>\n", nameZ);
		fprintf(fileP, "<tr><td>Comment</td><td>%s</td></tr>\n", commentZ);
		fprintf(fileP, "<tr><td>Chapter</td><td>%s</td></tr>\n", chapterZ);
		fprintf(fileP, "<tr><td>HREF</td><td><a href=\"%s\">%s</a></td></tr>\n", hrefZ, hrefZ);
		fprintf(fileP, "<tr><td>Frame Target</td><td>%s</td></tr>\n", targetZ);
		if (markerType == kMarkerType_Timeline)
		{
			fprintf(fileP, "<tr><td>Marker Type</td><td>Timeline Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
		}
		else if (markerType == kMarkerType_DVD)
		{
			if (DVDmarkerType == kDVDMarkerType_Main)
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>DVD Chapter Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
			else if (DVDmarkerType == kDVDMarkerType_Scene)
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>DVD Scene Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
			else if (DVDmarkerType == kDVDMarkerType_Stop)
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>DVD Stop Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
			else
			{
				fprintf(fileP, "<tr><td>Marker Type</td><td>Unknown DVD Marker</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
			}
		}
		else
		{
			fprintf(fileP, "<tr><td>Marker Type</td><td>Unknown Marker Type</td></tr>\n<tr><td>----------</td><td>----------</td>\n");
		}

		marker = mySettings->markerSuite->GetNextMarker(exportInfoP->timelineData, marker);
	}

	fprintf(fileP, "</table>\n</center>\n</body>\n</html>");
	fclose(fileP);
	
	#endif

	return;
}


csSDK_int32 GetNumberOfAudioChannels(csSDK_int32 audioChannelType)
{
	csSDK_int32 numberOfChannels = -1;

	if (audioChannelType == kPrAudioChannelType_Mono)
	{
		numberOfChannels = 1;
	}
	else if (audioChannelType == kPrAudioChannelType_Stereo)
	{
		numberOfChannels = 2;
	}
	else if (audioChannelType == kPrAudioChannelType_51)
	{
		numberOfChannels = 6;
	}
	else if (audioChannelType == kPrAudioChannelType_16Channel)
	{
		numberOfChannels = 16;
	}
	return numberOfChannels;
}


csSDK_int32 GetPixelFormatSize(PrFourCC subtype)
{
	csSDK_int32 formatSize = 4; // Default to size of 8-bit pixel formats

	if (subtype == SDK_10_BIT_YUV)
	{
		formatSize = 4;
	}
	return formatSize;
}


csSDK_int32 GetPixelFormatSize(PrPixelFormat pixelFormat) // #bytes per pixel
{
	switch ( pixelFormat ) {
		case PrPixelFormat_VUYX_4444_8u_709:
		case PrPixelFormat_VUYA_4444_8u_709:
		case PrPixelFormat_VUYX_4444_8u:
		case PrPixelFormat_VUYA_4444_8u:
			return 4;
	}

	// assume we're using a YUV420 planar mode, 1 byte per pixel
	return 1;
}


//float max(float a, float b)
//{
//	return (a > b ? a : b);
//}


//float min(float a, float b)
//{
//	return (a < b ? a : b);
//}


void ConvertFrom8uTo32f(
  	char		*buffer8u,
	char		*buffer32f,
	csSDK_int32 width,
	csSDK_int32 height)
{
	csSDK_uint32 *tempSrcBuffer = (csSDK_uint32 *)buffer8u;
	float *tempDestBuffer = (float *)buffer32f;
	csSDK_uint32 X, Y, Z, A;
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			Z = ((*tempSrcBuffer) << 24) >> 24;
			Y = ((*tempSrcBuffer) << 16) >> 24;
			X = ((*tempSrcBuffer) << 8) >> 24;
			A = (*tempSrcBuffer) >> 24;
			tempDestBuffer[0] = (float)Z / 255.0f;
			tempDestBuffer[1] = (float)Y / 255.0f;
			tempDestBuffer[2] = (float)X / 255.0f;
			tempDestBuffer[3] = (float)A / 255.0f;
			tempSrcBuffer++;
			tempDestBuffer += 4;
		}
	}
}


// This uses ITU-R Recommendation BT.601
void ConvertFromBGRA32fToVUYA32f(
  	char		*buffer32f,
	csSDK_int32	width,
	csSDK_int32	height)
{
	float *tempBuffer		= (float *)buffer32f;
	float Y, Cb, Cr;
	// The luma component float range is 0.0 = black to 1.0 = white
	float Y_RGBtoYCbCr[3]	= { 0.299f, 0.587f, 0.114f};
	// The Cb and Cr float range is -0.5 to 0.5
	float Cb_RGBtoYCbCr[3]	= { -0.168736f, -0.331264f, 0.5f}; 
	float Cr_RGBtoYCbCr[3]	= { 0.5f, -0.418688f, -0.081312f};
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			// BGR -> VUY
			Y =		Y_RGBtoYCbCr[0] * tempBuffer[2] +	// Red
					Y_RGBtoYCbCr[1] * tempBuffer[1] +	// Green
					Y_RGBtoYCbCr[2] * tempBuffer[0];	// Blue
			Cb =	Cb_RGBtoYCbCr[0] * tempBuffer[2] +
					Cb_RGBtoYCbCr[1] * tempBuffer[1] +
					Cb_RGBtoYCbCr[2] * tempBuffer[0];
			Cr =	Cr_RGBtoYCbCr[0] * tempBuffer[2] +
					Cr_RGBtoYCbCr[1] * tempBuffer[1] +
					Cr_RGBtoYCbCr[2] * tempBuffer[0];

			tempBuffer[0] = Cr;
			tempBuffer[1] = Cb;
			tempBuffer[2] = Y;

			tempBuffer += 4;
		}
	}
}


// Converts a 32f VUYA buffer to the v410 format described at
// http://developer.apple.com/quicktime/icefloe/dispatch019.html#v410
void ConvertFrom32fToV410(
	char *buffer32f,
	char *bufferV410,
	csSDK_int32 width,
	csSDK_int32 height)
{
	float *tempSrcBuffer = (float *)buffer32f;
	csSDK_int32 *tempDestBuffer = (csSDK_int32 *)bufferV410;
	float fY, fCr, fCb;
	csSDK_uint32 Y, Cr, Cb;
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			fCr = (*(tempSrcBuffer + 1) * 896.0f + 512.5f);
			Cr = (csSDK_uint32)max(64, min(960, fCr));
			fY = (*(tempSrcBuffer + 2) * 876.0f + 64.5f);
			Y = (csSDK_uint32)max(64, min(940, fY));
			fCb = (*tempSrcBuffer * 896.0f + 512.5f);
			Cb = (csSDK_uint32)max(64, min(960, fCb));
			*tempDestBuffer = (Cr << 22) + (Y << 12) + (Cb << 2);
			tempSrcBuffer += 4;
			tempDestBuffer++;
		}
	}
}


// Converts to a 32f VUYA buffer from the v410 format described at
// http://developer.apple.com/quicktime/icefloe/dispatch019.html#v410
void ConvertFromV410To32f(
  	char *bufferV410,
	char *buffer32f,
	csSDK_int32 width,
	csSDK_int32 height)
{
	csSDK_uint32 *tempSrcBuffer = (csSDK_uint32 *)bufferV410;
	float *tempDestBuffer = (float *)buffer32f;
	csSDK_uint32 Y, Cr, Cb; // Y != y
	for (csSDK_int32 row = 0; row < height; row++)
	{
		for (csSDK_int32 col = 0; col < width; col++)
		{
			Cr = (*tempSrcBuffer) >> 22;
			Y = ((*tempSrcBuffer) << 10) >> 22;
			Cb = ((*tempSrcBuffer) << 20) >> 22;
			tempDestBuffer[0] = ((float)Cb - 512.0f) / 896.0f;
			tempDestBuffer[1] = ((float)Cr - 512.0f) / 896.0f;
			tempDestBuffer[2] = ((float)Y - 64.0f) / 876.0f;
			tempDestBuffer[3] = 1.0f;
			tempSrcBuffer++;
			tempDestBuffer += 4;
		}
	}
}


// Assumes that prTime is a framerate < ticksPerSecond
void ConvertPrTimeToScaleSampleSize(
	PrSDKTimeSuite	*timeSuite,
	PrTime			prTime,
	csSDK_int32		*scale,
	csSDK_int32		*sampleSize)
{
	PrTime	ticksPerSecond = 0,
			tempFrameRate = 0;
	timeSuite->GetTicksPerSecond(&ticksPerSecond);
	if (ticksPerSecond % prTime == 0) // a nice round frame rate
	{
		*scale = static_cast<csSDK_int32>(ticksPerSecond / prTime);
		*sampleSize = 1;
	}
	else
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC, &tempFrameRate);
		if (tempFrameRate == prTime)
		{
			*scale = 30000;
			*sampleSize = 1001;
		}
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC_HD, &tempFrameRate);
		if (tempFrameRate == prTime)
		{
			*scale = 60000;
			*sampleSize = 1001;
		}
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_24Drop, &tempFrameRate);
		if (tempFrameRate == prTime)
		{
			*scale = 24000;
			*sampleSize = 1001;
		}
	}
}


void ConvertScaleSampleSizeToPrTime(
	PrSDKTimeSuite	*timeSuite,
	csSDK_int32		*scale,
	csSDK_int32		*sampleSize,
	PrTime			*prTime)
{
	if ((*scale == 24000 && *sampleSize == 1001) ||
			(*scale == 23976 && *sampleSize == 1000) ||
			(*scale == 2397 && *sampleSize == 100))
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_24Drop, prTime);
	}
	else if (*scale == 24)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_24, prTime);
	}
	else if (*scale == 25)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_PAL, prTime);
	}
	else if ((*scale == 30000 && *sampleSize == 1001) ||
		(*scale == 29970 && *sampleSize == 1000) ||
		(*scale == 2997 && *sampleSize == 100))
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC, prTime);
	}
	else if (*scale == 30)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_30, prTime);
	}
	else if (*scale == 50)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_PAL_HD, prTime);
	}
	else if ((*scale == 60000 && *sampleSize == 1001) ||
			(*scale == 5994 && *sampleSize == 100))
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_NTSC_HD, prTime);
	}
	else if (*scale == 60)
	{
		timeSuite->GetTicksPerVideoFrame(kVideoFrameRate_60, prTime);
	}
}


// Function to convert and copy string literals to the format expected by the exporter API.
// On Win: Pass the input directly to the output
// On Mac: All conversion happens through the CFString format
void copyConvertStringLiteralIntoUTF16(const wchar_t* inputString, prUTF16Char* destination)
{
#ifdef PRMAC_ENV
	int length = wcslen(inputString);
	CFRange	range = {0, kPrMaxPath};
	range.length = length;
	CFStringRef inputStringCFSR = CFStringCreateWithBytes(	kCFAllocatorDefault,
															reinterpret_cast<const UInt8 *>(inputString),
															length * sizeof(wchar_t),
															kCFStringEncodingUTF32LE,
															kPrFalse);
	CFStringGetBytes(	inputStringCFSR,
						range,
						kCFStringEncodingUTF16,
						0,
						kPrFalse,
						reinterpret_cast<UInt8 *>(destination),
						length * (sizeof (prUTF16Char)),
						NULL);
	destination[length] = 0; // Set NULL-terminator, since CFString calls don't set it, and MediaCore hosts expect it
	CFRelease(inputStringCFSR);
#elif defined PRWIN_ENV
	size_t length = wcslen(inputString);
	wcscpy_s(destination, length + 1, inputString);
#endif
}


// Utility function to merge strcpy_s on Win and strcpy on Mac into one call
void safeStrCpy (char *destStr, int size, const char *srcStr)
{
#ifdef PRWIN_ENV
	strcpy_s (destStr, size, srcStr);
#elif defined PRMAC_ENV
	strcpy (destStr, srcStr);
#endif
}


// Utility function to merge wcscat_s on Win and wcscat on Mac into one call
void safeWcscat (wchar_t *destStr, int size, const wchar_t *srcStr)
{
#ifdef PRWIN_ENV
	wcscat_s (destStr, size, srcStr);
#elif defined PRMAC_ENV
	wcscat (destStr, srcStr);
#endif
}

bool Check_prSuiteError( const prSuiteError errval, string &str )
{
	ostringstream oss;
	bool   found_error = true;
	str.clear();

	// top of error-check
	if ( PrSuiteErrorSucceeded(errval) )
		return false;

#define case_prSuiteError_VALUE(e) case e : str = #e; break

	switch( errval ) {
		//case_prSuiteError_VALUE(suiteError_NoError);	// Method succeeded
/*
**	General error results.
*/
		case_prSuiteError_VALUE(suiteError_Fail				);	// Method failed
		case_prSuiteError_VALUE(suiteError_InvalidParms		);	// A parameter to this method is invalid
		case_prSuiteError_VALUE(suiteError_OutOfMemory		);	// There is not enough memory to complete this method
		case_prSuiteError_VALUE(suiteError_InvalidCall		);	// Usually this means this method call is not appropriate at this time
		case_prSuiteError_VALUE(suiteError_NotImplemented	);	// The requested action is not implemented
		case_prSuiteError_VALUE(suiteError_IDNotValid		);	// The passed in ID (pluginID, clipID...) is not valid


/*
**	RenderSuite results
*/

/*	<private>
**	RenderSuite ErrorCategory == 1
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_RenderPending				);	// Render is pending
		case_prSuiteError_VALUE(suiteError_RenderedFrameNotFound		);	// A cached frame was not found.
		case_prSuiteError_VALUE(suiteError_RenderedFrameCanceled		);	// A render was canceled

		case_prSuiteError_VALUE(suiteError_RenderInvalidPixelFormat		);	// Render output pixel format list is invalid
		case_prSuiteError_VALUE(suiteError_RenderCompletionProcNotSet	);	// The render completion proc was not set for an async request

/*
**	TimeSuite results
*/

/*	<private>
**	TimeSuite ErrorCategory == 2
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_TimeRoundedAudioRate			);	// Audio rate returned was rounded

/*
**	Compiler{Render,Audio,Settings}Suite results
**
**	NOTE: If this list is changed in any way, you must also
**	update:
**
**	1.) SuiteErrorToCompilerError() and CompilerErrorToSuiteError()
**		in \Plugins\MediaCommon\MediaUtils\Src\Compilers\CompilerErrorUtils.cpp
**	2.)	CompilerErrorToSuiteError() in \MediaLayer\Src\Compilers\CompilerModuleCallbacks.cpp
*/

/*	<private>
**	Compiler{Render,Audio,Settings}Suite ErrorCategory == 3
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_CompilerCompileAbort				);	// User aborted the compile
		case_prSuiteError_VALUE(suiteError_CompilerCompileDone				);	// Compile finished normally
		case_prSuiteError_VALUE(suiteError_CompilerOutputFormatAccept		);	// The output format is valid
		case_prSuiteError_VALUE(suiteError_CompilerOutputFormatDecline		);	// The compile module cannot compile to the output format
		case_prSuiteError_VALUE(suiteError_CompilerRebuildCutList			);	// Return value from compGetFilePrefs used to force Premiere to bebuild its cutlist
		case_prSuiteError_VALUE(suiteError_CompilerIterateCompiler			);	// 6.0 Return value from compInit to request compiler iteration
		case_prSuiteError_VALUE(suiteError_CompilerIterateCompilerDone		);	// 6.0 Return value from compInit to indicate there are no more compilers
		case_prSuiteError_VALUE(suiteError_CompilerInternalErrorSilent		);	// 6.0 Silent error code; Premiere will not display an error message on screen.
																					// Compilers can return this error code from compDoCompile if they wish to
																					// put their own customized error message on screen just before returning 
																					// control to Premiere
		case_prSuiteError_VALUE(suiteError_CompilerIterateCompilerCacheable );	// 7.0 Return value from compInit to request compiler iteration and indicating that this
																					// compiler is cacheable.

		case_prSuiteError_VALUE(suiteError_CompilerBadFormatIndex			);	// Invalid format index - used to stop compGetIndFormat queries
		case_prSuiteError_VALUE(suiteError_CompilerInternalError			);	// 
		case_prSuiteError_VALUE(suiteError_CompilerOutOfDiskSpace			);	// Out of disk space error
		case_prSuiteError_VALUE(suiteError_CompilerBufferFull				);	// The offset into the audio buffer would overflow it
		case_prSuiteError_VALUE(suiteError_CompilerErrOther					);	// Someone set gCompileErr
		case_prSuiteError_VALUE(suiteError_CompilerErrMemory				);	// Ran out of memory
		case_prSuiteError_VALUE(suiteError_CompilerErrFileNotFound			);	// File not found
		case_prSuiteError_VALUE(suiteError_CompilerErrTooManyOpenFiles		);	// Too many open files
		case_prSuiteError_VALUE(suiteError_CompilerErrPermErr				);	// Permission violation
		case_prSuiteError_VALUE(suiteError_CompilerErrOpenErr				);	// Unable to open the file
		case_prSuiteError_VALUE(suiteError_CompilerErrInvalidDrive			);	// Drive isn't valid.
		case_prSuiteError_VALUE(suiteError_CompilerErrDupFile				);	// Duplicate Filename
		case_prSuiteError_VALUE(suiteError_CompilerErrIo					);	// File io error
		case_prSuiteError_VALUE(suiteError_CompilerErrInUse					);	// File is in use
		case_prSuiteError_VALUE(suiteError_CompilerErrCodecBadInput			);	// A video codec refused the input format
		case_prSuiteError_VALUE(suiteError_ExporterSuspended				);	// The host has suspended the export
		case_prSuiteError_VALUE(suiteError_ExporterNoMoreFrames			);	// Halt export early skipping all remaining frames including this one. AE uses

/*
**	FileSuite results
*/

/*	<private>
**	FileSuite ErrorCategory == 4
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_FileBufferTooSmall			);
		case_prSuiteError_VALUE(suiteError_FileNotImportableFileType	);	// Not an importable file type

/*
**	LegacySuite results
*/

/*	<private>
**	LegacySuite ErrorCategory == 5
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_LegacyInvalidVideoRate		);	// Invalid video rate (scale and sample rate don't match a valid rate)

/*
**	PlayModuleAudioSuite results
*/

/*	<private>
**	PlayModuleAudioSuite ErrorCategory == 6
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioInitFailure			);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioIllegalPlaySetting	);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioNotInitialized		);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioNotStarted			);
		case_prSuiteError_VALUE(suiteError_PlayModuleAudioIllegalAction			);

/*
**	PlayModuleDeviceControlSuite
*/

/*	<private>
**	PlayModuleDeviceControlSuite ErrorCategory == 7
**	</private>
*/
		case_prSuiteError_VALUE(suiteError_PlayModuleDeviceControlSuiteIllegalCallSequence	);

/*
**	MediaAcceleratorSuite ErrorCategory == 8
*/
		case_prSuiteError_VALUE(suiteError_MediaAcceleratorSuitePathNotFound	);
		case_prSuiteError_VALUE(suiteError_MediaAcceleratorSuiteRegisterFailure	);


/*
**	Royalty Activation ErrorCategory == 9
*/
		case_prSuiteError_VALUE(suiteError_RepositoryReadFailed					);
		case_prSuiteError_VALUE(suiteError_RepositoryWriteFailed				);
		case_prSuiteError_VALUE(suiteError_NotActivated							);
		case_prSuiteError_VALUE(suiteError_DataNotPresent						);
		case_prSuiteError_VALUE(suiteError_ServerCommunicationFailed			);
		case_prSuiteError_VALUE(suiteError_Internal								);

/*
**	PrSDKStringSuite ErrorCategory == A
*/
		case_prSuiteError_VALUE(suiteError_StringNotFound						);
		case_prSuiteError_VALUE(suiteError_StringBufferTooSmall					);


/*
**	PrSDKVideoSegmentSuite ErrorCategory == B
*/
		case_prSuiteError_VALUE(suiteError_NoKeyframeAfterInTime				);

/*
**	PrSDKCaptioningSuite ErrorCategory == C
*/
		case_prSuiteError_VALUE(suiteError_NoMoreData							);

/*
**	PrSDKThreadedWorkSuite ErrorCategory == D
*/
		case_prSuiteError_VALUE(suiteError_InstanceDestroyed					);

		default:
			oss << "Unknown:" << std::hex << errval;
			str.clear();
			found_error = false;
	} // switch( errval )

	if ( errval == suiteError_NoError )
		found_error = false;

	return found_error;
}
