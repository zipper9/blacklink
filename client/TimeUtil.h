#ifndef TIME_UTIL_H_
#define TIME_UTIL_H_

#include <stdint.h>
#include <time.h>

namespace Util
{
	inline time_t getTime() noexcept { return time(nullptr); }
	uint64_t getTick() noexcept;
	uint64_t getFileTime() noexcept;
#ifdef _WIN32
	uint64_t getHighResFrequency() noexcept;
#else
	inline constexpr uint64_t getHighResFrequency() noexcept { return 1000000000; }
#endif
	uint64_t getHighResTimestamp() noexcept;
	int64_t gmtToUnixTime(const tm* t) noexcept;

#ifdef _WIN32
	static const uint64_t FILETIME_UNITS_PER_SEC = 10000000; // 100ns FILETIME units
#else
	static const uint64_t FILETIME_UNITS_PER_SEC = 1000000000; // nanoseconds
#endif
}

#define GET_TICK() Util::getTick()
#define GET_TIME() Util::getTime()

#endif // TIME_UTIL_H_
