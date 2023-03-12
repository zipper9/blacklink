#include "stdinc.h"
#include "TimeUtil.h"
#include "debug.h"

#ifndef _WIN32
#include <sys/time.h>
#endif

#ifdef _WIN32
static const uint64_t frequency = Util::getHighResFrequency();
#endif
static const uint64_t startup = Util::getHighResTimestamp();

#ifdef _WIN32
uint64_t Util::getHighResFrequency() noexcept
{
	LARGE_INTEGER x;
	if (!QueryPerformanceFrequency(&x))
	{
		dcassert(0);
		return 1000;
	}
	return x.QuadPart;
}

uint64_t Util::getHighResTimestamp() noexcept
{
	LARGE_INTEGER x;
	if (!QueryPerformanceCounter(&x)) return 0;
	return x.QuadPart;
}
#else
uint64_t Util::getHighResTimestamp() noexcept
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
#endif

uint64_t Util::getTick() noexcept
{
	auto now = getHighResTimestamp();
	if (now <= startup) return 0; // this should never happen
#ifdef _WIN32
	return (now - startup) * 1000 / frequency;
#else
	return (now - startup) / 1000000;
#endif
}

#ifdef _WIN32
uint64_t Util::getFileTime() noexcept
{
	uint64_t currentTime;
	GetSystemTimeAsFileTime(reinterpret_cast<FILETIME*>(&currentTime));
	return currentTime;
}
#else
uint64_t Util::getFileTime() noexcept
{
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts)) return 0;
	return (uint64_t) 1000000000 * ts.tv_sec + ts.tv_nsec;
}
#endif

static inline bool isLeapYear(int year) noexcept
{
	if (year % 400 == 0) return true;
	if (year % 100 == 0) return false;
	return year % 4 == 0;
}

int64_t Util::gmtToUnixTime(const tm* t) noexcept
{
	static const int16_t daysFromYear[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
	int month = t->tm_mon;
	int year = t->tm_year;
	if (month > 11)
	{
		year += month / 12;
		month %= 12;
	}
	else if (month < 0)
	{
		int dy = (-month + 11) / 12;
		year -= dy;
		month += 12 * dy;
	}
	int days = 365 * (year - 70);
	days += (year - 1 - 68) / 4 - (year - 1) / 100 + (year - 1 + 300) / 400;
	days += daysFromYear[month] + t->tm_mday - 1;
	if (isLeapYear(year + 1900) && month >= 2) days++;
	return 86400 * (int64_t) days + 60 * (60 * t->tm_hour + t->tm_min) + t->tm_sec;
}
