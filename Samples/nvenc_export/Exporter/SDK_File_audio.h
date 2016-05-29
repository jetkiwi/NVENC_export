#ifndef SDK_FILE_AUDIO_H
#define SDK_FILE_AUDIO_H

#include "SDK_File.h"

///////////////////////////////////////////////////////////////////////////////
// Miscellaneous helper funcs (audio-export related)
prMALError
NVENC_WriteSDK_WAVHeader(
	exportStdParms *stdParms,
	exDoExportRec *exportInfoP,
	PrTime exportDuration
);

// standard (non-pipe) mode
bool NVENC_run_neroaacenc(
	const csSDK_uint32 exID,
	const ExportSettings *mySettings,
	const wchar_t in_wavfilename[],
	const wchar_t out_aacfilename[]
);

// for "PIPE-mode": not currently used
// NVENC_spawn_neroaacenc() - create's a background process that
//     executes NeroAacEnc.exe with <stdin> input (from pipe)
bool NVENC_spawn_neroaacenc(
	const csSDK_uint32 exID,
	ExportSettings *mySettings,
	const wchar_t out_aacfilename[]  // output *.AAC filename
);

// for "PIPE-mode": not currently used
// NVENC_wait_neroaacenc() wait for completion of the process
// spawned by NVENC_spawn_neroaacenc()
bool NVENC_wait_neroaacenc(
	ExportSettings *mySettings,
	const wchar_t out_aacfilename[]
);

// for "PIPE-mode": not currently used
//    Create the link between the WAV-audio writer (output) and 
//    stdin of the neroaacenc shell-session. 
BOOL NVENC_create_neroaac_pipe(ExportSettings *lRec);

csSDK_int32 GetNumberOfAudioChannels(csSDK_int32 audioChannelType);

///////////////////////////////////////////////////////////////////////////////
// Audio import-related calls

void calculateAudioRequest(
	imImportAudioRec7	*audioRec7,
	const PrAudioSample	totalSampleFrames,
	PrAudioSample		*savedAudioPosition,
	PrAudioSample		*startAudioPosition,
	PrAudioSample		*numAudioFrames);

void setPointerToAudioStart(
	ImporterLocalRec8H		ldataH,
	const PrAudioSample		startAudioPosition,
	imFileRef				SDKfileRef);

prMALError readAudioToBuffer(const PrAudioSample	numAudioFrames,
	const PrAudioSample	totalSampleFrames,
	const csSDK_int32	numAudioChannels,
	imFileRef			SDKfileRef,
	float **			audioBuffer);

///////////////////////////////////////////////////////////////////////////////
// Audio export-related calls

prMALError RenderAndWriteAllAudio(
	exDoExportRec				*exportInfoP,
	PrTime						exportDuration);

#endif // SDK_FILE_AUDIO_H