#ifndef SPEED_STATS_H_
#define SPEED_STATS_H_

#include "../client/typedefs.h"

class SpeedStats
{
		static const unsigned MAX_SIZE = 30;

		struct Sample
		{
			uint64_t tick;
			uint64_t upload;
			uint64_t download;
		};

		Sample samples[MAX_SIZE];
		unsigned first;
		unsigned last;
		unsigned size;

		uint64_t avgUpload;
		uint64_t avgDownload;

	public:
		SpeedStats();
		void addSample(uint64_t tick, uint64_t upload, uint64_t download);
		uint64_t getUpload() const { return avgUpload; }
		uint64_t getDownload() const { return avgDownload; }
		uint64_t getLastTick() const { return samples[last].tick; }
};

extern SpeedStats speedStats;

#endif // SPEED_STATS_H_
