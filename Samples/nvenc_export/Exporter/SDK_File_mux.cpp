
#include "SDK_File_mux.h"
#include "SDK_Exporter_Params.h"

#include <Windows.h> // SetFilePointer(), WriteFile()
#include <sstream>  // ostringstream
#include <fstream>
#include <cstdio>

//
// NVENC_mux_m2t() - multiplex the Audio/Video file(s) into a MPEG-2 
//                   transport bitstream by calling the third-party
//                   program "TSMUXER.EXE"
//
BOOL
NVENC_mux_m2t(
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to TSMUXER.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings,
	const csSDK_int32 audioCodec
) {
	//ExportSettings *mySettings = reinterpret_cast<ExportSettings*>(exportInfoP->privateData);
	wstring metafilename; // name of the control-file (*.meta) to operate TSMUXER.EXE
	wstring shellargs;    // command-line shell arguments
	wstring tempdirname;  // temporary-directory

	// Set FileRecord_Audio.filename to the *actual* outputfile path: 'XXX.M2T'
	//	nvenc_make_output_filename( outpath, SDK_FILE_EXTENSION_M2T, mySettings->SDKFileRec.FileRecord_AV.filename );
	nvenc_make_output_filename(
		outpath,
		L"", // no postfix
		L"ts", // extension
		mySettings->SDKFileRec.FileRecord_AV.filename
	);

	// create a '.meta' file for controlling TSMUXER
	{
		wostringstream postfix; // filename postfix (unique tempID)
		postfix << "_temp_" << std::dec << exporterPluginID;

		nvenc_make_output_filename(
			outpath,
			postfix.str(),
			L"meta",
			metafilename
			);
		nvenc_make_output_dirname(outpath, tempdirname);
	}

	wofstream metafile;
	metafile.open(metafilename, ios::out | ios::trunc);
	metafile << "MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr --vbv-len=500 ";
	metafile << endl;

	if (mySettings->SDKFileRec.hasVideo) {
		double frameRate = mySettings->NvEncodeConfig.frameRateNum;
		frameRate /= mySettings->NvEncodeConfig.frameRateDen;
		switch (mySettings->NvEncodeConfig.codec) {
		case NV_ENC_H264: metafile << "V_MPEG4/ISO/AVC, "; // H.264
			break;
		case NV_ENC_H265: metafile << "V_MPEGH/ISO/HEVC, ";// H.265
			break;
		default:          metafile << "V_MPEG4/ISO/AVC, "; // assume H.264
			break;
		}
		metafile << "\"" << mySettings->SDKFileRec.FileRecord_Video.filename << "\"";
		metafile << ", fps=";

		// Configure floating-point output to print up to 5-digits after decimal (.1234)
		metafile.setf(std::ios::fixed, std::ios::floatfield);
		metafile.precision(4);
		metafile << frameRate;
		metafile << L", insertSEI, contSPS, ar=As source ";
		metafile << endl;
	}

	if (mySettings->SDKFileRec.hasAudio) {
		if (audioCodec == ADBEAudioCodec_PCM)
			metafile << "A_LPCM, ";
		else if (audioCodec == ADBEAudioCodec_AAC)
			metafile << "A_AAC, ";
		else
			metafile << "A_LPCM, "; // unknown, default
		metafile << "\"" << mySettings->SDKFileRec.FileRecord_Audio.filename << "\"";

		// For AAC-audio, neroAacEnc gave us an M4A file (AAC-bitstream wrapped in MP4 file)
		//    select the correct audiotrack, which is assumed to be track#1
		if (audioCodec == ADBEAudioCodec_AAC)
			metafile << ", track=1"; // audiotrack#1 of the M4A file

		metafile << endl;
	}

	// finalize the metafile
	metafile.close();

	// Just in case the output-file already exists, delete it
	DeleteFileW(mySettings->SDKFileRec.FileRecord_AV.filename.c_str());

	shellargs = L"\"";
	shellargs += metafilename;
	shellargs += L"\"";
	shellargs += L" ";
	shellargs += L"\"";
	shellargs += mySettings->SDKFileRec.FileRecord_AV.filename;
	shellargs += L"\"";

	SHELLEXECUTEINFOW ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(ShExecInfo);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = muxpath;// TargetFile to execute/open
	ShExecInfo.lpParameters = shellargs.c_str();
	ShExecInfo.lpDirectory = tempdirname.c_str();
	ShExecInfo.nShow = SW_SHOWNORMAL; // SW_SHOW;
	ShExecInfo.hInstApp = NULL;

	// Now launch the external-program: TSMUXER.EXE
	BOOL rc = ShellExecuteExW(&ShExecInfo);

	// If shellexec was successful, then 
	//		wait for TSMUXER.exe to finish (could take a while...)
	if (rc) {
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
		DeleteFileW(metafilename.c_str());
	}

	// done with TS-muxing!
	return rc;
}

//
// NVENC_mux_mp4() - multiplex the Audio/Video file(s) into a MPEG-4 stream by 
//                   calling an the third-party program "MP4BOX.EXE"
//

BOOL
NVENC_mux_mp4(
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to MP4BOX.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings
) {
	wstring metafilename; // name of the control-file (*.meta) to operate MP4BOX.EXE
	wstring shellargs;    // command-line shell arguments
	wstring tempdirname;  // temporary dir for MP4BOX

	// Set FileRecord_Audio.filename to the *actual* outputfile path: 'XXX.MP4'
	//	nvenc_make_output_filename( outpath, SDK_FILE_EXTENSION_MP4, mySettings->SDKFileRec.FileRecord_AV.filename );
	nvenc_make_output_filename(
		outpath,
		L"",		// no postfix (since this is the *final* output file)
		L"mp4",		// mpeg-4 extension
		mySettings->SDKFileRec.FileRecord_AV.filename
		);
	nvenc_make_output_dirname(outpath, tempdirname);

	if (mySettings->SDKFileRec.hasVideo) {
		shellargs += L" ";
		shellargs += L"-add \"";
		shellargs += mySettings->SDKFileRec.FileRecord_Video.filename;
		shellargs += L"\"";
		shellargs += L" ";
	}

	if (mySettings->SDKFileRec.hasAudio) {
		shellargs += L" ";
		shellargs += L"-add \"";
		shellargs += mySettings->SDKFileRec.FileRecord_Audio.filename;
		shellargs += L"\"";
		shellargs += L" ";
	}

	// Specify a temporary directory (use the output-file's dir)
	shellargs += L" ";
	shellargs += L"-tmp \"";
	shellargs += tempdirname;
	shellargs += L"\" ";

	// Now add the output filepath
	shellargs += L" ";
	shellargs += L"\"";
	shellargs += mySettings->SDKFileRec.FileRecord_AV.filename;
	shellargs += L"\"";

	// Just in case the output-file already exists, delete it
	DeleteFileW(mySettings->SDKFileRec.FileRecord_AV.filename.c_str());

	SHELLEXECUTEINFOW ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(ShExecInfo);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = muxpath;// TargetFile to execute/open
	ShExecInfo.lpParameters = shellargs.c_str();
	ShExecInfo.lpDirectory = tempdirname.c_str();
	ShExecInfo.nShow = SW_SHOWNORMAL; // SW_SHOW;
	ShExecInfo.hInstApp = NULL;

	// Now launch the external-program: MP4BOX.EXE
	BOOL rc = ShellExecuteExW(&ShExecInfo);

	// If shellexec was successful, then 
	//		wait for MP4BOX.exe to finish (could take a while...)
	if (rc)
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);

	// done with MP4-muxing!
	return rc;
}

// NVENC_mux_mkv() - multiplex the Audio/Video file(s) into a Matroska (MKV) file by 
//                   calling an the third-party program "MKVMERGE.EXE"
BOOL
NVENC_mux_mkv(
	const csSDK_uint32 exporterPluginID, // used to generate a unique tempfilename
	const prUTF16Char muxpath[], // filepath to MKVMERGE.EXE
	const prUTF16Char outpath[], // output file path
	ExportSettings * const mySettings
)
{
	wstring metafilename; // name of the control-file (*.meta) to operate TSMUXER.EXE
	wstring shellargs;    // command-line shell arguments
	wstring tempdirname;  // temporary dir for MKVMERGE

	// Set FileRecord_Audio.filename to the *actual* outputfile path: 'XXX.MP4'
	//	nvenc_make_output_filename( outpath, SDK_FILE_EXTENSION_MP4, mySettings->SDKFileRec.FileRecord_AV.filename );
	nvenc_make_output_filename(
		outpath,
		L"",		// no postfix (since this is the *final* output file)
		L"mkv",		// mpeg-4 extension
		mySettings->SDKFileRec.FileRecord_AV.filename
		);
	nvenc_make_output_dirname(outpath, tempdirname);

	// First, add the output filepath
	shellargs += L" -o ";
	shellargs += L"\"";
	shellargs += mySettings->SDKFileRec.FileRecord_AV.filename;
	shellargs += L"\"";

	// If required, add the video bitstream
	if (mySettings->SDKFileRec.hasVideo) {
		shellargs += L" ";
		shellargs += L"\"";
		shellargs += mySettings->SDKFileRec.FileRecord_Video.filename;
		shellargs += L"\"";
		shellargs += L" ";
	}

	// If required, add the audio bitstream
	if (mySettings->SDKFileRec.hasAudio) {
		shellargs += L" ";
		shellargs += L"\"";
		shellargs += mySettings->SDKFileRec.FileRecord_Audio.filename;
		shellargs += L"\"";
		shellargs += L" ";
	}

	// Just in case the output-file already exists, delete it
	DeleteFileW(mySettings->SDKFileRec.FileRecord_AV.filename.c_str());

	SHELLEXECUTEINFOW ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(ShExecInfo);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = muxpath;// TargetFile to execute/open
	ShExecInfo.lpParameters = shellargs.c_str();
	ShExecInfo.lpDirectory = tempdirname.c_str();
	ShExecInfo.nShow = SW_SHOWNORMAL; // SW_SHOW;
	ShExecInfo.hInstApp = NULL;

	// Now launch the external-program: MP4BOX.EXE
	BOOL rc = ShellExecuteExW(&ShExecInfo);

	// If shellexec was successful, then 
	//		wait for MP4BOX.exe to finish (could take a while...)
	if (rc)
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);

	// done with MP4-muxing!
	return rc;
}
