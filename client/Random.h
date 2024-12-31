#ifndef RANDOM_H_
#define RANDOM_H_

#include <stdint.h>
#include <stddef.h>
#include "debug.h"

namespace Util
{
	void initRand();
	uint32_t rand();
	void randBytes(void* buf, size_t size, bool secure);

	inline uint32_t rand(uint32_t high)
	{
		dcassert(high > 0);
		return rand() % high;
	}

	inline uint32_t rand(uint32_t low, uint32_t high)
	{
		return rand(high - low) + low;
	}

	struct MT19937
	{
		static const int N = 624;

		uint32_t mt[N + 1];
		int mti = N + 1;

		void randomize();
		void setSeed(uint32_t seed);
		uint32_t getValue();
	};
}

#endif // RANDOM_H_
