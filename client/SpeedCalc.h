#ifndef SPEED_CALC_H_
#define SPEED_CALC_H_

#include "compiler.h"
#include "debug.h"

template<int M>
class SpeedCalc
{
		struct Sample
		{
			int64_t size;
			int64_t tick;
		};

		Sample data[M];
		int first;
		int last;
		int count;

	public:
		SpeedCalc()
		{
			dcassert((M & (M-1)) == 0);
			count = 0;
			first = 0;
			last = M - 1;
		}

		void setStartTick(int64_t tick)
		{
			first = last = 0;
			count = 1;
			data[0].size = 0;
			data[0].tick = tick;
		}
		
		bool addSample(int64_t size, int64_t tick)
		{
			if (count && data[last].tick + 1000 > tick) return false;
			last = (last + 1) & (M - 1);
			data[last].size = size;
			data[last].tick = tick;
			if (++count > M)
			{
				first = (first + 1) & (M - 1);
				count = M;
			}
			return true;
		}

		int64_t getAverage(int minTime, int minSize) const
		{
			if (count < 2) return -1;
			int64_t time = data[last].tick - data[first].tick;
			if (time <= 0) return -1;
			int64_t size = data[last].size - data[first].size;
			if (time < minTime && size < minSize) return -1;
			return size * 1000 / time;
		}
};

#endif // SPEED_CALC_H_
