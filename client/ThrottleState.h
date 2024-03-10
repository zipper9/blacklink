#ifndef THROTTLE_STATE_H_
#define THROTTLE_STATE_H_

#include <stdint.h>

class ThrottleState
{
	private:
		static const int N = 100;
		int samples[N];
		int sum;
		int startIndex;
		int endIndex;
		int64_t startTime;

	public:
		ThrottleState();
		void setCurrentTick(int64_t tick);
		void addSize(int size);
		int getAvailSize(int maxSize) const;

	private:
		void clearSum();
};

#endif // THROTTLE_STATE_H_
