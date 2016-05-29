#ifndef SDK_FILE_MUX_H
#define SDK_FILE_MUX_H

#include "SDK_File.h"


// NVENC_mux_m2t() - multiplex the Audio/Video file(s) into a MPEG-2 
//                   transport bitstream by calling the third-party
//                   program "TSMUXER.EXE"
BOOL
NVENC_mux_m2t( 
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to TSMUXER.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings,
	const csSDK_int32 audioCodec
);

// NVENC_mux_mp4() - multiplex the Audio/Video file(s) into a MPEG-4 stream by 
//                   calling an the third-party program "MP4BOX.EXE"
BOOL
NVENC_mux_mp4(
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to MP4BOX.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings
);

// NVENC_mux_mkv() - multiplex the Audio/Video file(s) into a Matroska (MKV) file by 
//                   calling an the third-party program "MKVMERGE.EXE"
BOOL
NVENC_mux_mkv(
const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
const prUTF16Char muxpath[], // filepath to MP4BOX.EXE
const prUTF16Char outpath[], // output file path
ExportSettings * const mySettings
);

#endif // SDK_FILE_MUX_H