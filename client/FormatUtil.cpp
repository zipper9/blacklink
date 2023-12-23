/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "FormatUtil.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "TimeUtil.h"
#include "Text.h"
#include "ParamExpander.h"
#include "ResourceManager.h"

#ifdef _WIN32
static NUMBERFMT nf;
#define swprintf _snwprintf
#endif

void Util::initFormatParams()
{
#ifdef _WIN32
	static TCHAR sep[2] = _T(",");
	static wchar_t dummy[16] = { 0 };
	nf.lpDecimalSep = sep;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SGROUPING, dummy, 16);
	nf.Grouping = _wtoi(dummy);
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, dummy, 16);
	nf.lpThousandSep = dummy;
#endif
}

string Util::formatTime(uint64_t rest, const bool withSeconds /*= true*/) noexcept
{
	string result;
	uint64_t n;
	int i = 0;
	n = rest / (24 * 3600 * 7);
	rest %= (24 * 3600 * 7);
	if (n)
	{
		result += toString(n);
		result += ' ';
		result += (n >= 2) ? STRING(DATETIME_WEEKS) : STRING(DATETIME_WEEK);
		i++;
	}
	n = rest / (24 * 3600);
	rest %= (24 * 3600);
	if (n)
	{
		if (i) result += ' ';
		result += toString(n);
		result += ' ';
		result += (n >= 2) ? STRING(DATETIME_DAYS) : STRING(DATETIME_DAY);
		i++;
	}
	n = rest / (3600);
	rest %= (3600);
	if (n)
	{
		if (i) result += ' ';
		result += toString(n);
		result += ' ';
		result += (n >= 2) ? STRING(DATETIME_HOURS) : STRING(DATETIME_HOUR);
		i++;
	}
	n = rest / (60);
	rest %= (60);
	if (n)
	{
		if (i) result += ' ';
		result += toString(n);
		result += ' ';
		result += (n >= 2) ? STRING(DATETIME_MINUTES) : STRING(DATETIME_MINUTE);
		i++;
	}
	if (withSeconds && i <= 2 && (rest || !i))
	{
		if (i) result += ' ';
		result += toString(rest);
		result += ' ';
		result += STRING(DATETIME_SECONDS);
	}
	return result;
}

string Util::formatDateTime(const string &format, time_t t, bool useGMT) noexcept
{
	if (format.empty()) return Util::emptyString;
	TimeParamExpander ex(t, useGMT);
	return formatParams(format, &ex, false);
}

string Util::formatDateTime(time_t t, bool useGMT) noexcept
{
	static const string defaultTimeFormat("%Y-%m-%d %H:%M:%S");
	return formatDateTime(defaultTimeFormat, t, useGMT);
}

string Util::formatCurrentDate() noexcept
{
	static const string defaultDateFormat("%Y-%m-%d");
	return formatDateTime(defaultDateFormat, GET_TIME());
}

#ifdef _UNICODE
wstring Util::formatSecondsW(int64_t sec, bool supressHours /*= false*/) noexcept
{
	wchar_t buf[64];
	if (!supressHours)
		swprintf(buf, _countof(buf), L"%01u:%02u:%02u", unsigned(sec / (60 * 60)), unsigned((sec / 60) % 60), unsigned(sec % 60));
	else
		swprintf(buf, _countof(buf), L"%02u:%02u", unsigned(sec / 60), unsigned(sec % 60));
	return buf;
}
#endif

string Util::formatSeconds(int64_t sec, bool supressHours /*= false*/) noexcept
{
	char buf[64];
	if (!supressHours)
		snprintf(buf, _countof(buf), "%01u:%02u:%02u", unsigned(sec / (60 * 60)), unsigned((sec / 60) % 60), unsigned(sec % 60));
	else
		snprintf(buf, _countof(buf), "%02u:%02u", unsigned(sec / 60), unsigned(sec % 60));
	return buf;
}

template<typename size_type>
inline string formatBytesTemplate(size_type bytes)
{
	char buf[512];
	if (bytes < 1024)
		snprintf(buf, sizeof(buf), "%d %s", (int) bytes, CSTRING(B));
	else if (bytes < 1048576)
		snprintf(buf, sizeof(buf), "%.02f %s", (double) bytes / 1024.0, CSTRING(KB));
	else if (bytes < 1073741824)
		snprintf(buf, sizeof(buf), "%.02f %s", (double) bytes / 1048576.0, CSTRING(MB));
	else if (bytes < (size_type) 1099511627776)
		snprintf(buf, sizeof(buf), "%.02f %s", (double) bytes / 1073741824.0, CSTRING(GB));
	else if (bytes < (size_type) 1125899906842624)
		snprintf(buf, sizeof(buf), "%.03f %s", (double) bytes / 1099511627776.0, CSTRING(TB));
	else if (bytes < (size_type) 1152921504606846976)
		snprintf(buf, sizeof(buf), "%.03f %s", (double) bytes / 1125899906842624.0, CSTRING(PB));
	else
		snprintf(buf, sizeof(buf), "%.03f %s", (double) bytes / 1152921504606846976.0, CSTRING(EB));
	return buf;
}

string Util::formatBytes(int64_t bytes)
{
	return formatBytesTemplate<int64_t>(bytes);
}

string Util::formatBytes(double bytes)
{
	return formatBytesTemplate<double>(bytes);
}

string Util::formatExactSize(int64_t bytes)
{
#ifdef _WIN32
	return Text::wideToUtf8(formatExactSizeW(bytes));
#else
	char buf[64];
	snprintf(buf, sizeof(buf), I64_FMT, bytes);
	return string(buf) + STRING(B);
#endif
}

#ifdef _UNICODE
template<typename size_type>
inline wstring formatBytesWTemplate(size_type bytes)
{
	wchar_t buf[512];
	if (bytes < 1024)
		swprintf(buf, _countof(buf), L"%d %s", (int) bytes, CWSTRING(B));
	else if (bytes < 1048576)
		swprintf(buf, _countof(buf), L"%.02f %s", (double) bytes / 1024.0, CWSTRING(KB));
	else if (bytes < 1073741824)
		swprintf(buf, _countof(buf), L"%.02f %s", (double) bytes / 1048576.0, CWSTRING(MB));
	else if (bytes < (size_type) 1099511627776)
		swprintf(buf, _countof(buf), L"%.02f %s", (double) bytes / 1073741824.0, CWSTRING(GB));
	else if (bytes < (size_type) 1125899906842624)
		swprintf(buf, _countof(buf), L"%.03f %s", (double) bytes / 1099511627776.0, CWSTRING(TB));
	else if (bytes < (size_type) 1152921504606846976)
		swprintf(buf, _countof(buf), L"%.03f %s", (double) bytes / 1125899906842624.0, CWSTRING(PB));
	else
		swprintf(buf, _countof(buf), L"%.03f %s", (double) bytes / 1152921504606846976.0, CWSTRING(EB));
	return buf;
}

wstring Util::formatBytesW(int64_t bytes)
{
	return formatBytesWTemplate<int64_t>(bytes);
}

wstring Util::formatBytesW(double bytes)
{
	return formatBytesWTemplate<double>(bytes);
}

wstring Util::formatExactSizeW(int64_t bytes)
{
#ifdef _WIN32
	wchar_t strNum[64];
	_snwprintf(strNum, _countof(strNum), _T(I64_FMT), bytes);
	wchar_t formatted[128];
	GetNumberFormat(LOCALE_USER_DEFAULT, 0, strNum, &nf, formatted, _countof(formatted));
	wstring result = formatted;
	result += L' ';
	result += WSTRING(B);
	return result;
#else
	wchar_t buf[64];
	swprintf(buf, _countof(buf), _T(I64_FMT), bytes);
	return wstring(buf) + WSTRING(B);
#endif
}
#endif
