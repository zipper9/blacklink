#include "stdinc.h"
#include "ParamExpander.h"
#include "BaseUtil.h"

#ifdef _WIN32
#include "Text.h"
#endif

static void appendFiltered(string& out, const string& s) noexcept
{	
#ifdef _WIN32
	static const char badChars[] = "\\/:*?|<>";
#else
	static const char badChars[] = "\\/";
#endif
	if (s.length() == 2 && s[0] == '.' && s[1] == '.')
	{
		out += "__";
		return;
	}
	string::size_type prev = 0;
	while (prev < s.length())
	{
		auto pos = s.find_first_of(badChars, prev);
		if (pos == string::npos)
		{
			out.append(s, prev, s.length() - prev);
			break;
		}
		out.append(s, prev, pos - prev);
		out += '_';
		prev = pos + 1;
	}
}

string Util::formatParams(const string& s, ParamExpander* ex, bool filter) noexcept
{
	string out;
	string::size_type prev = 0;
	while (prev < s.length())
	{
		auto pos = s.find('%', prev);
		if (pos == string::npos || pos == s.length()-1)
		{
			out.append(s, prev, s.length() - prev);
			break;
		}
		pos++;
		if (s[pos] == '%')
		{
			out.append(s, prev, pos - prev);
			prev = pos + 1;
			continue;
		}
		if (s[pos] == '[')
		{
			out.append(s, prev, pos - prev - 1);
			pos++;
			auto endPos = s.find(']', pos);
			if (endPos == string::npos) break;
			const string& value = ex->expandBracket(s.substr(pos, endPos - pos));
			if (filter)
				appendFiltered(out, value);
			else
				out += value;
			prev = endPos + 1;
			continue;
		}
		string::size_type usedChars;
		const string& value = ex->expandCharSequence(s, pos, usedChars);
		if (usedChars)
		{
			out.append(s, prev, pos - prev - 1);
			if (filter)
				appendFiltered(out, value);
			else
				out += value;
		}
		else
			out.append(s, prev, pos - prev);
		prev = pos + usedChars;
	}
	return out;
}

bool Util::TimeParamExpander::initialize() noexcept
{
	if (initialized) return true;
	if (!t) return false;
	const tm* plt = useGMT ? gmtime(&t) : localtime(&t);
	if (!plt) return false;
	lt = *plt;
	initialized = true;
	return true;
}

bool Util::TimeParamExpander::strftime(char c) noexcept
{
#ifdef _WIN32
	wchar_t format[3];
	format[0] = '%';
	format[1] = c;
	format[2] = 0;
	size_t size = wcsftime(buf, BUF_SIZE, format, &lt);
	if (!size) return false;
	Text::wideToUtf8(buf, size, result);
#else
	char format[3];
	format[0] = '%';
	format[1] = c;
	format[2] = 0;
	result.resize(BUF_SIZE);
	size_t size = ::strftime(&result[0], BUF_SIZE, format, &lt);
	if (!size) return false;
	result.resize(size);
#endif
	return true;
}

#define FORMAT_VALUE(format, ...) \
	result.resize(BUF_SIZE); \
	len = snprintf(&result[0], BUF_SIZE, format, __VA_ARGS__); \
	if (len <= 0) return Util::emptyString; \
	result.resize(len);

const string& Util::TimeParamExpander::expandCharSequence(const string& str, string::size_type pos, string::size_type& usedChars) noexcept
{
	int len, tmp2;
	unsigned tmp;
	usedChars = 0;
	switch (str[pos])
	{
		case 'a':
		case 'A':
		case 'b':
		case 'B':
		case 'c':
		case 'p':
		case 'x':
		case 'X':
		case 'Z':
			if (!initialize() || !strftime(str[pos])) return Util::emptyString;
			usedChars = 1;
			return result;

		case 'd':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%02u", lt.tm_mday);
			usedChars = 1;
			return result;
			
		case 'H':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%02u", lt.tm_hour);
			usedChars = 1;
			return result;

		case 'I':
			if (!initialize()) return Util::emptyString;
			tmp = lt.tm_hour;
			if (!tmp) tmp = 12; else
			if (tmp > 12) tmp -= 12;
			FORMAT_VALUE("%02u", tmp);
			usedChars = 1;
			return result;

		case 'j':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%03u", lt.tm_yday + 1);
			usedChars = 1;
			return result;

		case 'm':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%02u", lt.tm_mon + 1);
			usedChars = 1;
			return result;

		case 'M':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%02u", lt.tm_min);
			usedChars = 1;
			return result;
	
		case 'S':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%02u", lt.tm_sec);
			usedChars = 1;
			return result;

		case 'U':
			if (!initialize()) return Util::emptyString;
			if (lt.tm_yday < lt.tm_wday)
				tmp = 0;
			else
			{
				tmp = lt.tm_yday / 7;
				if (lt.tm_yday % 7 >= lt.tm_wday)
					tmp++;
			}
			FORMAT_VALUE("%02u", tmp);
			usedChars = 1;
			return result;

		case 'w':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%u", lt.tm_wday);
			usedChars = 1;
			return result;

		case 'W':
			if (!initialize()) return Util::emptyString;
			if (!lt.tm_wday)
				tmp2 = 6;
			else
				tmp2 = lt.tm_wday - 1;
			if (lt.tm_yday < tmp2)
				tmp = 0;
			else
			{
				tmp = lt.tm_yday / 7;
				if (lt.tm_yday % 7 >= tmp2)
					tmp++;
			}
			FORMAT_VALUE("%02u", tmp);
			usedChars = 1;
			return result;

		case 'y':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%02u", lt.tm_year % 100);
			usedChars = 1;
			return result;

		case 'Y':
			if (!initialize()) return Util::emptyString;
			FORMAT_VALUE("%u", lt.tm_year + 1900);
			usedChars = 1;
			return result;
	}
	return Util::emptyString;
}

#undef FORMAT_VALUE

const string& Util::TimeParamExpander::expandBracket(const string& param) noexcept
{
	return Util::emptyString;
}

const string& Util::MapParamExpander::expandBracket(const string& param) noexcept
{
	if (param.empty()) return Util::emptyString;
	auto i = m.find(param);
	if (i == m.end()) return Util::emptyString;
	return i->second;
}

string Util::formatParams(const string& s, const StringMap& params, bool filter, time_t t) noexcept
{
	MapParamExpander ex(params, t);
	return formatParams(s, &ex, filter);
}
