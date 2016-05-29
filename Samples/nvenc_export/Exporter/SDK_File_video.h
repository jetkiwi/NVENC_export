#ifndef SDK_FILE_VIDEO_H
#define SDK_FILE_VIDEO_H

#include "SDK_File.h"

void
NVENC_switch_codec(ExportSettings *lRec);

prMALError
RenderAndWriteAllVideo(
	exDoExportRec	*exportInfoP,
	float			progress,
	float			videoProgress,
	PrTime			*exportDuration
);

#endif // SDK_FILE_VIDEO_H