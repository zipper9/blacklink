#ifndef REQUEST_COUNTER_H_
#define REQUEST_COUNTER_H_

#include "compiler.h"
#include "debug.h"

template<int N>
class RequestCounter
{
		uint8_t data[N];
		int first;
		int count;
		int64_t startTick;
		int64_t endTick;
		uint64_t reqCount;

		void reset()
		{
			first = count = 0;
			memset(data, 0, sizeof(data));
		}

	public:
		RequestCounter()
		{
			clear();
		}

		void clear()
		{
			first = 0;
			count = 0;
			startTick = endTick = 0;
			reqCount = 0;
			memset(data, 0, sizeof(data));
		}

		void add(int64_t tickMsec)
		{
			int64_t tick = tickMsec / 1000;
			if (tick < startTick) // error
			{
				reset();
				startTick = tick;
				reqCount = 0;
			}
			++reqCount;
			int pos;
			int64_t offset = tick - startTick;
			if (offset >= N)
			{
				int64_t lshift = offset - (N - 1);
				if (lshift >= N)
				{
					reset();
					startTick = tick;
					pos = 0;
				}
				else
				{
					int shift = (int) lshift;
					startTick += shift;
					pos = ((int) offset + first) % N;
					while (shift)
					{
						dcassert(data[first] <= count);
						count -= data[first];
						data[first] = 0;
						first = (first + 1) % N;
						--shift;
					}

				}
			}
			else
				pos = ((int) offset + first) % N;
			if (data[pos] < 255)
			{
				++data[pos];
				++count;
			}
			endTick = tick;
		}

		int getCount() const { return count; }
		int64_t getReqCount() const { return reqCount; }
		double getAvgPerSecond() const
		{
			if (startTick >= endTick) return 0;
			return (double) count / (endTick - startTick);
		}
		unsigned getAvgPerMinute() const
		{
			if (startTick >= endTick) return 0;
			return (60 * count) / unsigned(endTick - startTick);
		}
};

#endif // REQUEST_COUNTER_H_
