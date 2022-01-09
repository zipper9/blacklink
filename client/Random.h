#ifndef RANDOM_H_
#define RANDOM_H_

#include <stdint.h>
#include "debug.h"

namespace Util
{
	void initRand();
	uint32_t rand();
	inline uint32_t rand(uint32_t high)
	{
		dcassert(high > 0);
		return rand() % high;
	}
	inline uint32_t rand(uint32_t low, uint32_t high)
	{
		return rand(high - low) + low;
	}
}

#endif // RANDOM_H_
