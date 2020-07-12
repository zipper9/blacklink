#include "stdafx.h"
#include "SpeedStats.h"

SpeedStats speedStats;

SpeedStats::SpeedStats() : first(0), last(0), size(0), avgUpload(0), avgDownload(0)
{
	samples[0].tick = 0;
}

void SpeedStats::addSample(uint64_t tick, uint64_t upload, uint64_t download)
{
	if (size)
		last = (last + 1) % MAX_SIZE;
	if (size == MAX_SIZE)
		first = (first + 1) % MAX_SIZE;
	else
		++size;
	samples[last].tick = tick;
	samples[last].upload = upload;
	samples[last].download = download;
	uint64_t resUpload = 0, resDownload = 0;
	if (size >= 5)
	{
		uint64_t oldTick = samples[first].tick;
		if (oldTick < tick)
		{
			resUpload = (upload - samples[first].upload) * 1000 / (tick - oldTick);
			resDownload = (download - samples[first].download) * 1000 / (tick - oldTick);
		}
	}
	avgUpload = resUpload;
	avgDownload = resDownload;
}
