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
#include "Text.h"
#include "StrUtil.h"
#include "BaseUtil.h"
#include "debug.h"

#ifndef _WIN32
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#include <boost/predef/other/endian.h>
#endif

namespace Text
{

const string g_utf8 = "utf-8";
static const string g_utf8NoHyp = "utf8";
uint16_t g_errorChar = 0xFFFD;

#ifndef _WIN32
#if BOOST_ENDIAN_BIG_BYTE
#define WCHAR_BYTE_ORDER "BE"
#else
#define WCHAR_BYTE_ORDER "LE"
#endif
#if SIZEOF_WCHAR == 2
static const string g_wideCharset = "UTF-16" WCHAR_BYTE_ORDER;
#else
static const string g_wideCharset = "UTF-32" WCHAR_BYTE_ORDER;
#endif
#endif

const int supportedCharsets[NUM_SUPPORTED_CHARSETS] =
{
	1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 936, 950
};

#ifdef _WIN32
static inline int getWindowsCodePage(int charset)
{
	if (charset == CHARSET_SYSTEM_DEFAULT)
		return CP_ACP;
	if (charset == CHARSET_UTF8)
		return CP_UTF8;
	return charset;
}

int getDefaultCharset()
{
	return GetACP();
}
#else
int getDefaultCharset()
{
	return CHARSET_UTF8;
}
#endif

static inline bool checkPrefix(const string& s, const string& prefix, size_t& pos)
{
	if (!isAsciiPrefix2(s, prefix)) return false;
	pos = prefix.length();
	if (pos < s.length() && s[pos] == '-') pos++;
	return pos + 1 < s.length();
}

int charsetFromString(const string& charset)
{
	if (charset.empty())
		return CHARSET_SYSTEM_DEFAULT;
	if (charset.length() == g_utf8.length() && isAsciiPrefix2(charset, g_utf8))
		return CHARSET_UTF8;
	if (charset.length() == g_utf8NoHyp.length() && isAsciiPrefix2(charset, g_utf8NoHyp))
		return CHARSET_UTF8;
	string::size_type pos;
	if (!checkPrefix(charset, "windows", pos) && !checkPrefix(charset, "cp", pos))
	{
		pos = charset.rfind('.');
		if (pos == string::npos) pos = 0; else pos++;
	}
	int value = Util::toInt(charset.c_str() + pos);
	for (int i = 0; i < NUM_SUPPORTED_CHARSETS; ++i)
		if (supportedCharsets[i] == value) return value;
	return CHARSET_SYSTEM_DEFAULT; // fallback
}

string charsetToString(int charset)
{
	if (charset == CHARSET_SYSTEM_DEFAULT)
		return string();
	if (charset == CHARSET_UTF8)
		return g_utf8;
	return Util::toString(charset);
}

bool isAscii(const char* str) noexcept
{
	for (; *str; str++)
		if (*str & 0x80) return false;
	return true;
}

bool isAscii(const string& str) noexcept
{
	for (auto p = str.cbegin(); p != str.cend(); ++p)
		if (*p & 0x80) return false;
	return true;
}

string& acpToUtf8(const string& str, string& tmp, int fromCharset) noexcept
{
	wstring wtmp;
	return wideToUtf8(acpToWide(str, wtmp, fromCharset), tmp);
}

#ifndef _WIN32
void getIconvCharset(char out[], int charset)
{
	if (charset == CHARSET_UTF8 || charset == CHARSET_SYSTEM_DEFAULT)
		strcpy(out, g_utf8.c_str());
	else
		sprintf(out, "windows-%d", charset);
}

template<typename C>
void putErrorChar(char* &out, size_t& outSize)
{
	if (outSize >= sizeof(C))
	{
		*reinterpret_cast<C*>(out) = '?';
		outSize -= sizeof(C);
		out += sizeof(C);
	}
}

template<typename string_type>
void convert(char* inBuf, size_t inCharSize, size_t inSize, string_type& tgt, const char* fromCharset, const char* toCharset)
{
	iconv_t ic = iconv_open(toCharset, fromCharset);
	if (ic == (iconv_t) -1)
	{
		assert(0);
		tgt.clear();
		return;
	}
#ifdef ICONV_SET_ILSEQ_INVALID
	int flag = 1;
	iconvctl(ic, ICONV_SET_ILSEQ_INVALID, &flag);
#endif
	char* outBuf = (char *) tgt.data();
	size_t outBytesLeft = tgt.length() * sizeof(typename string_type::value_type);
	while (inSize)
	{
		size_t result = iconv(ic, &inBuf, &inSize, &outBuf, &outBytesLeft);
		if (result == (size_t) -1)
		{
			int error = errno;
			if (error == E2BIG)
			{
				size_t size = outBuf - (char *) tgt.data();
				tgt.resize(tgt.length() * 2);
				outBuf = (char *) tgt.data() + size;
				outBytesLeft = tgt.length() * sizeof(typename string_type::value_type) - size;
				continue;
			}
			if (error == EILSEQ || error == EINVAL)
			{
				putErrorChar<typename string_type::value_type>(outBuf, outBytesLeft);
				if (error == EINVAL || inSize <= inCharSize) break;
				inSize -= inCharSize;
				inBuf += inCharSize;
				continue;
			}
			dcassert(0);
			break;
		}
	}
	size_t size = outBuf - (char *) tgt.data();
	iconv_close(ic);
	tgt.resize(size / sizeof(typename string_type::value_type));
}
#endif

wstring& acpToWide(const string& str, wstring& tgt, int fromCharset) noexcept
{
	if (str.empty())
	{
		tgt.clear();
		return tgt;
	}
	if (fromCharset == CHARSET_UTF8)
		return utf8ToWide(str, tgt);
#ifdef _WIN32
	string::size_type size = 0;
	tgt.resize(str.length() + 1);
	const int cp = getWindowsCodePage(fromCharset);
	while ((size = MultiByteToWideChar(cp, MB_PRECOMPOSED, str.c_str(), str.length(), &tgt[0], tgt.length())) == 0)
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			dcassert(0);
			tgt.resize(tgt.size() * 2);
		}
		else
		{
			dcassert(0);
			break;
		}
	}
	tgt.resize(size);
#else
	if (fromCharset == CHARSET_SYSTEM_DEFAULT)
		return utf8ToWide(str, tgt);
	char charset[64];
	getIconvCharset(charset, fromCharset);
	tgt.resize(str.length());
	convert<wstring>((char *) str.data(), 1, str.length(), tgt, charset, g_wideCharset.c_str());
#endif
	return tgt;
}

int wcToUtf8(uint32_t value, char out[]) noexcept
{
	if (value < 0x80)
	{
		out[0] = value;
		return 1;
	}
	if (value < 0x800)
	{
		out[0] = 0xC0 | value>>6;
		out[1] = 0x80 | (value & 0x3F);
		return 2;
	}
	if (value < 0x10000)
	{
		out[0] = 0xE0 | value>>12;
		out[1] = 0x80 | ((value>>6) & 0x3F);
		out[2] = 0x80 | (value & 0x3F);
		return 3;
	}
	dcassert(value < 0x110000);
	out[0] = 0xF0 | value>>18;
	out[1] = 0x80 | ((value>>12) & 0x3F);
	out[2] = 0x80 | ((value>>6) & 0x3F);
	out[3] = 0x80 | (value & 0x3F);
	return 4;
}

string& wideToUtf8(const wchar_t* str, size_t len, string& tgt) noexcept
{
	char ubuf[4];
	size_t outSize = 0;
#if SIZEOF_WCHAR == 2
	uint16_t highSurrogate = 0;
	for (size_t i = 0; i < len; i++)
	{
		uint32_t value = str[i];
		if (value >= 0xD800 && value < 0xDC00)
		{
			if (highSurrogate) outSize += wcToUtf8(g_errorChar, ubuf);
			highSurrogate = value;
			continue;
		}
		if (value >= 0xDC00 && value < 0xE000)
		{
			if (!highSurrogate)
			{
				outSize += wcToUtf8(g_errorChar, ubuf);
				continue;
			}
			value = ((value - 0xDC00) | (highSurrogate - 0xD800) << 10) + 0x10000;
			highSurrogate = 0;
		}
		else if (highSurrogate)
		{
			outSize += wcToUtf8(g_errorChar, ubuf);
			highSurrogate = 0;
		}
		outSize += wcToUtf8(value, ubuf);
	}
	if (highSurrogate) outSize += wcToUtf8(g_errorChar, ubuf);

	if (!outSize)
	{
		tgt.clear();
		return tgt;
	}

	tgt.resize(outSize);
	char* out = &tgt[0];
	highSurrogate = 0;
	for (size_t i = 0; i < len; i++)
	{
		uint32_t value = str[i];
		if (value >= 0xD800 && value < 0xDC00)
		{
			if (highSurrogate) out += wcToUtf8(g_errorChar, out);
			highSurrogate = value;
			continue;
		}
		if (value >= 0xDC00 && value < 0xE000)
		{
			if (!highSurrogate)
			{
				out += wcToUtf8(g_errorChar, out);
				continue;
			}
			value = ((value - 0xDC00) | (highSurrogate - 0xD800) << 10) + 0x10000;
			highSurrogate = 0;
		}
		else if (highSurrogate)
		{
			out += wcToUtf8(g_errorChar, out);
			highSurrogate = 0;
		}
		out += wcToUtf8(value, out);
	}
	if (highSurrogate) wcToUtf8(g_errorChar, out);
#else
	for (size_t i = 0; i < len; i++)
	{
		uint32_t value = str[i];
		outSize += wcToUtf8(value < 0x110000 ? value : g_errorChar, ubuf);
	}

	if (!outSize)
	{
		tgt.clear();
		return tgt;
	}

	tgt.resize(outSize);
	char* out = &tgt[0];
	for (size_t i = 0; i < len; i++)
	{
		uint32_t value = str[i];
		out += wcToUtf8(value < 0x110000 ? value : g_errorChar, out);
	}
#endif
	return tgt;
}

string& wideToAcp(const wstring& str, string& tgt, int toCharset) noexcept
{
	if (str.empty())
	{
		tgt.clear();
		return tgt;
	}
	if (toCharset == CHARSET_UTF8)
		return wideToUtf8(str, tgt);
#ifdef _WIN32
	const int cp = getWindowsCodePage(toCharset);
	tgt.resize(str.length() * 2 + 1);
	int size = 0;
	while ((size = WideCharToMultiByte(cp, 0, str.c_str(), str.length(), &tgt[0], tgt.length(), nullptr, nullptr)) == 0)
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			dcassert(0);
			tgt.resize(tgt.size() * 2);
		}
		else
		{
			dcassert(0);
			break;
		}
	}
	tgt.resize(size);
#else
	if (toCharset == CHARSET_SYSTEM_DEFAULT)
		return wideToUtf8(str, tgt);
	char charset[64];
	getIconvCharset(charset, toCharset);
	tgt.resize(str.length() * 2);
	convert<string>((char *) str.data(), sizeof(wchar_t), str.length() * sizeof(wchar_t), tgt, g_wideCharset.c_str(), charset);
#endif
	return tgt;
}

int utf8ToWc(const char* s, size_t pos, size_t len, uint32_t& wc) noexcept
{
	if (!(s[pos] & 0x80)) // 1 byte
	{
		wc = s[pos];
		return 1;
	}
	if ((s[pos] & 0xC0) == 0x80) return -1;
	if ((s[pos] & 0xE0) == 0xC0) // 2 bytes
	{
		if (len - pos < 2 || (s[pos + 1] & 0xC0) != 0x80) return -1;
		wc = (s[pos] & 0x1F)<<6 | (s[pos + 1] & 0x3F);
		if (wc < 0x80) return -1;
		return 2;
	}
	if ((s[pos] & 0xF0) == 0xE0) // 3 bytes
	{
		if (len - pos < 2 || (s[pos + 1] & 0xC0) != 0x80) return -1;
		if (len - pos < 3 || (s[pos + 2] & 0xC0) != 0x80) return -2;
		wc = (s[pos] & 0xF)<<12 | (s[pos + 1] & 0x3F)<<6 | (s[pos + 2] & 0x3F);
		if (wc < 0x800) return -2;
		if (wc >= 0xD800 && wc <= 0xDFFF) return -2;
		return 3;
	}
	if ((s[pos] & 0xF8) == 0xF0) // 4 bytes
	{
		if (len - pos < 2 || (s[pos + 1] & 0xC0) != 0x80) return -1;
		wc = (s[pos] & 0x7)<<18;
		if (wc > 0x10FFFF) return -1;
		if (len - pos < 3 || (s[pos + 2] & 0xC0) != 0x80) return -2;
		wc |= (s[pos + 1] & 0x3F)<<12;
		if (wc > 0x10FFFF || wc < 0x10000) return -2;
		if (len - pos < 4 || (s[pos + 3] & 0xC0) != 0x80) return -3;
		wc |= (s[pos + 2] & 0x3F)<<6 | (s[pos + 3] & 0x3F);
		return 4;
	}
	return -1;
}

bool validateUtf8(const char* data, size_t len, size_t pos /* = 0 */) noexcept
{
	while (pos < len)
	{
		uint32_t unused;
		int b = utf8ToWc(data, pos, len, unused);
		if (b < 0) return false;
		pos += b;
	}
	return true;
}

wstring& utf8ToWide(const string& str, wstring& tgt) noexcept
{
	const char* data = str.data();
	size_t len = str.length();
	uint32_t wc;
	size_t outSize = 0;
	size_t i = 0;
#if SIZEOF_WCHAR == 2
	while (i < len)
	{
		int result = utf8ToWc(data, i, len, wc);
		if (result < 0)
		{
			i -= result;
			outSize++;
		}
		else
		{
			i += result;
			outSize += wc >= 0x10000 ? 2 : 1;
		}
	}

	if (!outSize)
	{
		tgt.clear();
		return tgt;
	}

	tgt.resize(outSize);
	wchar_t* out = &tgt[0];
	i = 0;
	while (i < len)
	{
		int result = utf8ToWc(data, i, len, wc);
		if (result < 0)
		{
			i -= result;
			*out++ = g_errorChar;
		}
		else
		{
			i += result;
			if (wc >= 0x10000)
			{
				wc -= 0x10000;
				out[0] = (wchar_t) (0xD800 | (wc >> 10));
				out[1] = (wchar_t) (0xDC00 | (wc & 0x3FF));
				out += 2;
			}
			else
				*out++ = (wchar_t) wc;
		}
	}
#else
	while (i < len)
	{
		i += abs(utf8ToWc(data, i, len, wc));
		outSize++;
	}

	if (!outSize)
	{
		tgt.clear();
		return tgt;
	}

	tgt.resize(outSize);
	wchar_t* out = &tgt[0];
	i = 0;
	while (i < len)
	{
		int result = utf8ToWc(data, i, len, wc);
		if (result < 0)
		{
			i -= result;
			*out++ = (wchar_t) g_errorChar;
		}
		else
		{
			i += result;
			*out++ = (wchar_t) wc;
		}
	}
#endif
	return tgt;
}

string& utf8ToAcp(const string& str, string& tmp, int toCharset) noexcept
{
	wstring wtmp;
	return wideToAcp(utf8ToWide(str, wtmp), tmp, toCharset);
}

void makeLower(wstring& str) noexcept
{
#ifdef _WIN32
	_wcslwr_s(&str[0], str.length() + 1);
#else
	for (wchar_t& w : str)
		w = towlower(w);
#endif
}

wstring toLower(const wstring& str) noexcept
{
	wstring tmp = str;
	makeLower(tmp);
	return tmp;
}

wstring toLower(const wstring& str, wstring& tmp) noexcept
{
	tmp = str;
	makeLower(tmp);
	return tmp;
}

void makeLower(string& str) noexcept
{
	wstring ws = utf8ToWide(str);
	makeLower(ws);
	wideToUtf8(ws, str);
}

string toLower(const string& str) noexcept
{
	wstring ws = utf8ToWide(str);
	makeLower(ws);
	return wideToUtf8(ws);
}

string toLower(const string& str, string& tmp) noexcept
{
	wstring ws = utf8ToWide(str);
	makeLower(ws);
	return wideToUtf8(ws, tmp);
}

const string& toUtf8(const string& str, int fromCharset, string& tmp) noexcept
{
	if (str.empty())
		return str;
	if (fromCharset == CHARSET_UTF8)
		return str;
	return acpToUtf8(str, tmp, fromCharset);
}

const string& fromUtf8(const string& str, int toCharset, string& tmp) noexcept
{
	if (str.empty())
		return str;
	if (toCharset == CHARSET_UTF8)
		return str;
	return utf8ToAcp(str, tmp, toCharset);
}

}
