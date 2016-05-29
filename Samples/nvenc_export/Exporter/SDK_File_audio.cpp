#include "SDK_File_audio.h"
#include "SDK_Exporter.h" // nvenc_make_output_dirname()
#include "SDK_Exporter_Params.h"

#include <Windows.h> // SetFilePointer(), WriteFile()
#include <sstream>  // ostringstream
#include <cstdio>

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
NVENC_WriteSDK_WAVHeader(exportStdParms *stdParms, exDoExportRec *exportInfoP, PrTime exportDuration)
{
	microsoft_stereo_wav_header_t  w;
	microsoft_wavex_header_t  w2;
	prMALError				result = malNoError;
	//
	// TODO, get rid of the rest of this stuff
	//
	csSDK_uint32			bytesToWriteLu = 0;
	csSDK_uint32			exID = exportInfoP->exporterPluginID;
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

	memset((void *)&w, 0, sizeof(w)); // clear w
	memset((void *)&w2, 0, sizeof(w2)); // clear w2

	// Update the private data with the parameter settings
	mySettings = reinterpret_cast<ExportSettings *>(exportInfoP->privateData);
	if (exportInfoP->exportAudio)
	{
		mySettings->SDKFileRec.hasAudio = kPrTrue;
		mySettings->SDKFileRec.audioSubtype = 'RAW_'; //exportInfoP->outputRec.audCompression.subtype;

		// Calculate audio samples
		mySettings->exportParamSuite->GetParamValue(exID, 0, ADBEAudioRatePerSecond, &sampleRate);
		mySettings->SDKFileRec.sampleRate = sampleRate.value.floatValue;
		mySettings->timeSuite->GetTicksPerAudioSample((float)sampleRate.value.floatValue, &ticksPerSample);
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
	w2.SampleRate = w.SampleRate;
	w.ByteRate = w.SampleRate * w.NumChannels * ((w.BitsPerSample + 7) >> 3);//28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
	w2.ByteRate = w.ByteRate;
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
	if (NumSamples >> 32)
		result = malUnknownError;
	else if (!wfrc)
		result = malUnknownError; // WriteFile() failed
	else if (bytes_written != bytesToWriteLu)
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
	__m128i audio51_mask[3] = { 0 };
	__m128i fixup_mask[4] = { 0 };
	__m128i read_temp[3] = { 0 };
	__m128i write_temp[3] = { 0 };
	__m128i write_fixup[4] = { 0 };

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

	memset(reinterpret_cast<void *>(fmask0_from1), 0x80, sizeof(m128_u));
	memset(reinterpret_cast<void *>(fmask1_from0), 0x80, sizeof(m128_u));
	memset(reinterpret_cast<void *>(fmask1_from2), 0x80, sizeof(m128_u));
	memset(reinterpret_cast<void *>(fmask2_from1), 0x80, sizeof(m128_u));

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
	mask0->w[0] = _m128u_word(0, 0);// FrontLeft
	mask0->w[1] = _m128u_word(1, 0);// FrontRight
	mask0->w[2] = _m128u_word(4, 0);// FrontCenter
	mask0->w[3] = _m128u_word(5, 0);// LFE
	mask0->w[4] = _m128u_word(2, 0);// BackLeft
	mask0->w[5] = _m128u_word(3, 0);// BackRight
	//
	/////

	//////
	// Sample n+1 (second sample)
	mask0->w[0 + 6] = _m128u_word(0 + 6, 0);// FrontLeft
	mask0->w[1 + 6] = _m128u_word(1 + 6, 0);// FrontRight
	mask1->w[2 - 2] = _m128u_word(4 - 2, 0);// FrontCenter
	mask1->w[3 - 2] = _m128u_word(5 - 2, 0);// LFE
	mask1->w[4 - 2] = _m128u_word(2 - 2, 0);// BackLeft
	mask1->w[5 - 2] = _m128u_word(3 - 2, 0);// BackRight

	// Sample n+1
	//////

	/////
	// Sample n+2 (third sample)
	mask1->w[0 + 4] = _m128u_word(0 + 4, 0); // channel[0]
	mask1->w[1 + 4] = _m128u_word(1 + 4, 0); // channel[1]
	mask1->w[2 + 4] = _m128u_word(4 + 4, 1); // ILLEGAL (need fixup)
	mask1->w[3 + 4] = _m128u_word(5 + 4, 1); // ILLEGAL (need fixup)
	fmask1_from2->w[2 + 4] = _m128u_word(4 + 4 - 8, 0); // data from previous <mm128>
	fmask1_from2->w[3 + 4] = _m128u_word(5 + 4 - 8, 0); // data from previous <mm128>

	mask2->w[4 - 4] = _m128u_word(2 - 4, 1); // ILLEGAL (need fixup)
	mask2->w[5 - 4] = _m128u_word(3 - 4, 1); // ILLEGAL (need fixup)

	fmask2_from1->w[4 - 4] = _m128u_word(2 - 4 + 8, 0); // data from previous <mm128>
	fmask2_from1->w[5 - 4] = _m128u_word(3 - 4 + 8, 0); // data from previous <mm128>

	// Sample n+2
	/////

	/////
	// Sample n+3 (fourth sample)
	mask2->w[0 + 2] = _m128u_word(0 + 2, 0);
	mask2->w[1 + 2] = _m128u_word(1 + 2, 0);
	mask2->w[2 + 2] = _m128u_word(4 + 2, 0);
	mask2->w[3 + 2] = _m128u_word(5 + 2, 0);
	mask2->w[4 + 2] = _m128u_word(2 + 2, 0);
	mask2->w[5 + 2] = _m128u_word(3 + 2, 0);
	// Sample n+3
	/////

	// During each loop-iteration,we process four 5.1-audioSamples.  Each 5.1-audioSample is six 16-bit words,
	// for a total of 48 bytes. This is the smallest unit of work using 128-bit SSE2 data-operations.

	for (uint32_t n = 0, aptr = 0; n < numSamples; n += 4, aptr += 3) {
		read_temp[0] = audioBuffer[aptr];
		read_temp[1] = audioBuffer[aptr + 1];
		read_temp[2] = audioBuffer[aptr + 2];

		// Perform the first-round of word-to-word shuffling.
		write_temp[0] = _mm_shuffle_epi8(read_temp[0], mask0->xmm);
		write_temp[1] = _mm_shuffle_epi8(read_temp[1], mask1->xmm);
		write_temp[2] = _mm_shuffle_epi8(read_temp[2], mask2->xmm);

		// Perform the second-round of word-to-word shuffling (i.e. "fixups")

		// No fixup needed for output[0]

		// fixup for output[1]: need to pull in some sampledata from read_temp[2]
		write_fixup[1] = _mm_shuffle_epi8(read_temp[2], fmask1_from2->xmm);

		// fixup for output[2]: need to pull in some sampledata from read_temp[1]
		write_fixup[2] = _mm_shuffle_epi8(read_temp[1], fmask2_from1->xmm);

		audioBuffer[aptr] = write_temp[0];
		audioBuffer[aptr + 1] = _mm_or_si128(write_temp[1], write_fixup[1]);// w1 | wf1
		audioBuffer[aptr + 2] = _mm_or_si128(write_temp[2], write_fixup[2]);// w2 | wf2
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
	SHELLEXECUTEINFOW ShExecInfo = { 0 };
	csSDK_int32					mgroupIndex = 0;
	exParamValues exParamValue_aacpath, exParamValue_temp;
	int32_t kbitrate; // audio bitrate (Kbps)

	mySettings->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_AudioFormat_NEROAAC_Path, &exParamValue_aacpath);
	mySettings->exportParamSuite->GetParamValue(exID, mgroupIndex, ADBEAudioBitrate, &exParamValue_temp);
	kbitrate = exParamValue_temp.value.intValue; // audio bitrate (Kbps)
	nvenc_make_output_dirname(out_aacfilename, tempdirname);

	//
	// build the command-line to execute neroAAC.
	// It will look something like this:
	//
	//    neroaacenc.exe -br 12340000 -if "in_wavfilename.wav" -of "out_aacfilename.aac"

	os << " -br " << std::dec << (kbitrate * 1024);
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
	DeleteFileW(out_aacfilename);

	BOOL rc = ShellExecuteExW(&ShExecInfo);

	// If shellexec was successful, then 
	//		wait for neroAacEnc.exe to finish (could take a while...)
	if (rc) {
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);

		// now verify the output file really exists
		FILE *fp = _wfopen(out_aacfilename, L"rb");
		if (fp == NULL)
			rc = false; // can't find the file, something went wrong!
		else
			fclose(fp);
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
	csSDK_int32					mgroupIndex = 0;
	exParamValues exParamValue_aacpath, exParamValue_temp;
	int32_t kbitrate; // audio bitrate (Kbps)

	mySettings->exportParamSuite->GetParamValue(exID, mgroupIndex, ParamID_AudioFormat_NEROAAC_Path, &exParamValue_aacpath);
	mySettings->exportParamSuite->GetParamValue(exID, mgroupIndex, ADBEAudioBitrate, &exParamValue_temp);
	kbitrate = exParamValue_temp.value.intValue; // audio bitrate (Kbps)
	nvenc_make_output_dirname(out_aacfilename, tempdirname);

	//
	// build the command-line to execute neroAAC.
	// It will look something like this:
	//
	//    neroaacenc.exe -br 12340000 -if - -of "out_aacfilename.aac"
	os << exParamValue_aacpath.paramString; // the execution-path to neroAacEnc.exe
	os << " -br " << std::dec << (kbitrate * 1024);
	os << " -if - "; // input-file: stdin
	os << " -of \"" << out_aacfilename << "\"";
	pwstring = new wchar_t[os.str().size()];
	lstrcpyW(pwstring, os.str().c_str()); // harden the arguments

	// Just in case the output-file already exists, delete it
	DeleteFileW(out_aacfilename);

	// Set up members of the PROCESS_INFORMATION structure. 

	ZeroMemory(&(mySettings->SDKFileRec.child_piProcInfo), sizeof(PROCESS_INFORMATION));

	// Set up members of the STARTUPINFO structure. 
	// This structure specifies the STDIN and STDOUT handles for redirection.

	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
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

	CloseHandle(mySettings->SDKFileRec.H_pipe_aacin);
	CloseHandle(mySettings->SDKFileRec.H_pipe_wavout);
	CloseHandle(mySettings->SDKFileRec.FileRecord_AAClog.hfp);
	mySettings->SDKFileRec.H_pipe_aacin = NULL;
	mySettings->SDKFileRec.H_pipe_wavout = NULL;

	DWORD dw;
	GetExitCodeProcess(mySettings->SDKFileRec.child_piProcInfo.hProcess, &dw);

	if (dw == 0) {
		// neroAacEnc succeeded: no need to keep the logfile so delete it
		DeleteFileW(mySettings->SDKFileRec.FileRecord_AAClog.filename.c_str());
	}
	else {
		// neroAacEnc failed, append an error message
		bSuccess = false;
		FILE *fp = _wfopen(
			mySettings->SDKFileRec.FileRecord_AAClog.filename.c_str(),
			L"r+"
			);

		fprintf(fp, "nvenc_export ERROR: process exited with %0u (0x%0X)\n",
			dw, dw);
		fclose(fp);
	}

	// now verify the output file really exists
	FILE *fp = _wfopen(
		out_aacfilename,
		L"rb"
		);
	if (fp == NULL)
		bSuccess = false; // can't find the file, something went wrong!
	else
		fclose(fp);

	return bSuccess;
}


BOOL
NVENC_create_neroaac_pipe(
ExportSettings *lRec
)
{
	bool success = true;
	SECURITY_ATTRIBUTES saAttr;

	printf("\n->Start of parent execution.\n");

	// Set the bInheritHandle flag so pipe handles are inherited. 

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 
	lRec->SDKFileRec.H_pipe_aacin = 0;
	lRec->SDKFileRec.H_pipe_wavout = 0;

	success = CreatePipe(
		&(lRec->SDKFileRec.H_pipe_aacin),  // PIPE-output (read by neroAacEnc process)
		&(lRec->SDKFileRec.H_pipe_wavout), // PIPE-input (written by nvenc_export WAVwriter)
		&saAttr,
		0
		);

	if (!success) {
		if (lRec->SDKFileRec.H_pipe_aacin)
			CloseHandle(lRec->SDKFileRec.H_pipe_aacin);
		if (lRec->SDKFileRec.H_pipe_wavout)
			CloseHandle(lRec->SDKFileRec.H_pipe_wavout);

		return success;
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(lRec->SDKFileRec.H_pipe_wavout, HANDLE_FLAG_INHERIT, 0))
		return false;

	if (!SetHandleInformation(lRec->SDKFileRec.H_pipe_aacin, HANDLE_FLAG_INHERIT, 0))
		return false;

	return true;
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
	csSDK_uint32		totalVideoFramesLu = 0;
	csSDK_int64			bytesPerFrameLL = 0,
		videoOffsetLL = 0;
	PrAudioSample		audioOffset = 0;

#ifdef PRWIN_ENV
	csSDK_int32			tempErrorS = 0;
	LARGE_INTEGER		distanceToMoveLI;
#else
	SInt64				distanceToMove;
#endif

	totalVideoFramesLu = (*ldataH)->theFile.numFrames;
	if ((*ldataH)->theFile.hasVideo && totalVideoFramesLu > 0)
	{
		bytesPerFrameLL = (*ldataH)->theFile.width * (*ldataH)->theFile.height *
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
		GetLastError();
	}
#else
	distanceToMove = sizeof(SDK_File) + videoOffsetLL + audioOffset;
	FSSetForkPosition(reinterpret_cast<intptr_t>(SDKfileRef),
		fsFromStart,
		distanceToMove);
#endif
}


prMALError readAudioToBuffer(const PrAudioSample	numAudioFrames,
	const PrAudioSample	totalSampleFrames,
	const csSDK_int32	numAudioChannels,
	imFileRef			SDKfileRef,
	float **			audioBuffer)
{
	prMALError			result = malNoError;
	csSDK_uint32		bytesReadLu = 0;

#ifdef PRWIN_ENV
	csSDK_int32			didReadL = 0;
	csSDK_int32			tempErrorS = 0;
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
			static_cast<csSDK_int32>(numAudioFrames)* AUDIO_SAMPLE_SIZE,
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
		tempErrorS = SetFilePointerEx(SDKfileRef,
			distanceToMoveLI,
			NULL,
			FILE_CURRENT);

		if (tempErrorS == INVALID_SET_FILE_POINTER)
		{
			GetLastError();
		}
#else
		distanceToMove = (totalSampleFrames - numAudioFrames) * AUDIO_SAMPLE_SIZE;
		result = FSSetForkPosition(reinterpret_cast<intptr_t>(SDKfileRef),
			fsFromMark,
			distanceToMove);
#endif
	}

	return result;
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

// Returns malNoError if successful
prMALError RenderAndWriteAllAudio(
	exDoExportRec				*exportInfoP,
	PrTime						exportDuration)
{
	csSDK_int32					resultS = malNoError;
	csSDK_uint32				exID = exportInfoP->exporterPluginID;
	ExportSettings				*mySettings = reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	exParamValues				ticksPerFrame,
		sampleRate,
		channelType;
	csSDK_int64					distanceToMove;
	csSDK_int64					filePtrLocation;
	PrAudioSample				totalAudioSamples = 0,
		samplesRemaining = 0;
	csSDK_uint32				audioRenderID = 0;
	csSDK_int32					maxBlip = 0;
	csSDK_int32					audioBufferSizeL = 0,
		samplesRequestedL = 0,
		audioChannelsL = 0;
	float *						audioBufferFloat[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	short *						audioBuffer16bit = NULL;
	__m128i *					audioBuffer16bit_aligned_sse2 = NULL;
	csSDK_uint32				bytesToWriteLu = 0;

	PrSDKMemoryManagerSuite	*memorySuite = mySettings->memorySuite;
	PrSDKTimeSuite			*timeSuite = mySettings->timeSuite;
	PrSDKExportInfoSuite	*exportInfoSuite = mySettings->exportInfoSuite;
	PrTime					ticksPerSample = 0;
	PrParam					srcChannelType;

	PrSDKExportParamSuite	*paramSuite = mySettings->exportParamSuite;
	paramSuite->GetParamValue(exID, 0, ADBEVideoFPS, &ticksPerFrame);
	paramSuite->GetParamValue(exID, 0, ADBEAudioRatePerSecond, &sampleRate);
	paramSuite->GetParamValue(exID, 0, ADBEAudioNumChannels, &channelType);
	audioChannelsL = GetNumberOfAudioChannels(channelType.value.intValue);

	// get the #audio-channels from the export-source
	exportInfoSuite->GetExportSourceInfo(exID,
		kExportInfo_AudioChannelsType,
		&srcChannelType);
	// kludge - TODO we don't support 16-channel audio,
	//          so switch 16-channel to 5.1 audio
	if (srcChannelType.mInt32 >= kPrAudioChannelType_16Channel)
		srcChannelType.mInt32 = kPrAudioChannelType_51;

	//	bool audioformat_incompatible = 
	//	 ( srcChannelType.mInt32 == kPrAudioChannelType_51 && (audioChannelsL > 6)) ||
	//	 ( srcChannelType.mInt32 == kPrAudioChannelType_Stereo && (audioChannelsL > 2)) ||
	//	 ( srcChannelType.mInt32 == kPrAudioChannelType_Mono && (audioChannelsL > 1));

	timeSuite->GetTicksPerAudioSample((float)sampleRate.value.floatValue, &ticksPerSample);

	prSuiteError serr = mySettings->sequenceAudioSuite->MakeAudioRenderer(exID,
		exportInfoP->startTime,
		(PrAudioChannelType)channelType.value.intValue,
		kPrAudioSampleType_32BitFloat,
		(float)sampleRate.value.floatValue,
		&audioRenderID);

	bool audioformat_incompatible = (serr == suiteError_NoError) ? false : true;

	totalAudioSamples = exportDuration / ticksPerSample;
	samplesRemaining = totalAudioSamples;

	// Find size of blip to ask for
	// The lesser of the value returned from GetMaxBlip and number of samples remaining
	if (!audioformat_incompatible)
		serr = mySettings->sequenceAudioSuite->GetMaxBlip(audioRenderID, ticksPerFrame.value.timeValue, &maxBlip);

	if (maxBlip < samplesRemaining)
	{
		samplesRequestedL = maxBlip;
	}
	else
	{
		samplesRequestedL = (csSDK_int32)samplesRemaining;
	}

	//   Premiere Elements 12: crashes if the output-audioformat has more channels 
	//   than the source-audioformat.
	//
	//   Workaround:
	//   Compare the source-audioformat with the selected output-audioformat,
	//   Abort on incompatible output-format.
	if (maxBlip == 0 || audioformat_incompatible) {
		wostringstream oss; // text scratchpad for messagebox and errormsg 
		prUTF16Char title[256];
		prUTF16Char desc[256];
		HWND mainWnd = mySettings->windowSuite->GetMainWindow();

		copyConvertStringLiteralIntoUTF16(L"NVENC-export error: AUDIO", title);
		oss << "There was a problem with exporting Audio:" << endl << endl;
		oss << "The source and output audio channels are not compatible or a conversion does not exist. " << endl;
		oss << "In the nvenc_export tab 'Audio', please reduce the #Channels and try again." << endl;

		copyConvertStringLiteralIntoUTF16(oss.str().c_str(), desc);
		mySettings->errorSuite->SetEventStringUnicode(PrSDKErrorSuite::kEventTypeError, title, desc);

		MessageBoxW(GetLastActivePopup(mainWnd),
			oss.str().c_str(),
			EXPORTER_NAME_W,
			MB_OK | MB_ICONERROR);

		mySettings->sequenceAudioSuite->ReleaseAudioRenderer(exID,
			audioRenderID);
		return exportReturn_IncompatibleAudioChannelType;
	}

	// Set temporary audio buffer size (measured in samples)
	// to be size of first blip requested
	audioBufferSizeL = samplesRequestedL;

	// Allocate audio buffers
	audioBuffer16bit = (short *)memorySuite->NewPtr(4 * sizeof(__m128i) + audioChannelsL * audioBufferSizeL * sizeof(short));

	// Align the buffer to a 16-byte boundary (for SSE2/3)
	audioBuffer16bit_aligned_sse2 = reinterpret_cast<__m128i *>(
		reinterpret_cast<uint64_t>(audioBuffer16bit + 7) - (reinterpret_cast<uint64_t>(audioBuffer16bit + 7) & 15)
		);

	for (csSDK_int32 bufferIndexL = 0; bufferIndexL < audioChannelsL; bufferIndexL++)
	{
		audioBufferFloat[bufferIndexL] = (float *)memorySuite->NewPtr(audioBufferSizeL * AUDIO_SAMPLE_SIZE);
	}

	uint64_t samples_since_update = 0;
	while (samplesRemaining && (resultS == malNoError))
	{
		// Fill the buffer with audio
		resultS = mySettings->sequenceAudioSuite->GetAudio(audioRenderID,
			(csSDK_uint32)samplesRequestedL,
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
			if (audioChannelsL > 2)
				adobe2wav_audio51_swap_ssse3(samplesRequestedL, audioBuffer16bit_aligned_sse2);
			bytesToWriteLu = audioChannelsL * samplesRequestedL * 2;

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
			if (!wfrc) {
				resultS = exportReturn_InternalError;
				continue; // skip to top-of-loop, then loop exits immediately
			}

			// Calculate remaining audio
			samplesRemaining -= samplesRequestedL;

			// count the #samples we've processed since our previous ProgressReport
			samples_since_update += samplesRequestedL;

			// Find size of next blip to ask for
			mySettings->sequenceAudioSuite->GetMaxBlip(audioRenderID, ticksPerFrame.value.timeValue, &maxBlip);
			if (maxBlip < samplesRemaining)
			{
				samplesRequestedL = maxBlip;
			}
			else
			{
				samplesRequestedL = (csSDK_int32)samplesRemaining;
			}
			if (audioBufferSizeL < samplesRequestedL)
			{
				samplesRequestedL = audioBufferSizeL;
			}
		}

		if (samples_since_update >= 500000) {
			samples_since_update -= 500000;
			mySettings->exportProgressSuite->UpdateProgressPercent(exID,
				static_cast<float>(totalAudioSamples - samplesRemaining) / totalAudioSamples
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
	memorySuite->PrDisposePtr((char *)audioBuffer16bit);
	for (csSDK_int32 bufferIndexL = 0; bufferIndexL < audioChannelsL; bufferIndexL++)
		memorySuite->PrDisposePtr((char *)audioBufferFloat[bufferIndexL]);

	mySettings->sequenceAudioSuite->ReleaseAudioRenderer(exID,
		audioRenderID);

	return resultS;
}
