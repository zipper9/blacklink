#include "stdinc.h"
#include "ThrottleState.h"
#include "debug.h"

ThrottleState::ThrottleState()
{
	clearSum();
	startIndex = endIndex = 0;
	startTime = 0;
}

void ThrottleState::setCurrentTick(int64_t tick)
{
	tick /= 10;
	if (tick < startTime)
	{
		dcassert(0);
		clearSum();
		startIndex = endIndex = 0;
		startTime = tick;
		return;
	}
	if (tick >= startTime + 2*N-1)
	{
		clearSum();
		startIndex = 0;
		endIndex = N - 1;
		startTime = tick - (N - 1);
		return;
	}
	int diff = (int) (tick - startTime);
	int offset = 0;
	while (diff >= N)
	{
		sum -= samples[startIndex];
		samples[startIndex] = 0;
		startIndex = (startIndex + 1) % N;
		offset++;
		diff--;
	}
	dcassert(sum >= 0);
	startTime += offset;
	endIndex = (startIndex + diff) % N;
}

void ThrottleState::addSize(int size)
{
	dcassert(size > 0);
	samples[endIndex] += size;
	sum += size;
}

int ThrottleState::getAvailSize(int maxSize) const
{
	if (maxSize <= 0) return INT_MAX;
	return maxSize <= sum ? 0 : maxSize - sum;
}

void ThrottleState::clearSum()
{
	memset(samples, 0, sizeof(samples));
	sum = 0;
}
