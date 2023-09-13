#ifndef FORMAT_UTIL_H_
#define FORMAT_UTIL_H_

#include "typedefs.h"
#include <time.h>

#ifdef _UNICODE
#define formatBytesT formatBytesW
#define formatExactSizeT formatExactSizeW
#define formatSecondsT formatSecondsW
#else
#define formatBytesT formatBytes
#define formatExactSizeT formatExactSize
#define formatSecondsT formatSeconds
#endif

namespace Util
{
	void initFormatParams();
#ifdef _UNICODE
	wstring formatSecondsW(int64_t sec, bool supressHours = false) noexcept;
#endif
	string formatSeconds(int64_t sec, bool supressHours = false) noexcept;
	string formatTime(uint64_t rest, const bool withSeconds = true) noexcept;
	string formatDateTime(const string &format, time_t t, bool useGMT = false) noexcept;
	string formatDateTime(time_t t, bool useGMT = false) noexcept;
	string formatCurrentDate() noexcept;

	string formatBytes(int64_t bytes);
	string formatBytes(double bytes);
	inline string formatBytes(int32_t bytes)  { return formatBytes(int64_t(bytes)); }
	inline string formatBytes(uint32_t bytes) { return formatBytes(int64_t(bytes)); }
	inline string formatBytes(uint64_t bytes) { return formatBytes(int64_t(bytes)); }
	string formatExactSize(int64_t bytes);

#ifdef _UNICODE
	wstring formatBytesW(int64_t bytes);
	wstring formatBytesW(double bytes);
	inline wstring formatBytesW(int32_t bytes)  { return formatBytesW(int64_t(bytes)); }
	inline wstring formatBytesW(uint32_t bytes) { return formatBytesW(int64_t(bytes)); }
	inline wstring formatBytesW(uint64_t bytes) { return formatBytesW(int64_t(bytes)); }
	wstring formatExactSizeW(int64_t bytes);
#endif
}

#endif // FORMAT_UTIL_H_
