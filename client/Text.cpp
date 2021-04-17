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
#include "StrUtil.h"
#include "BaseUtil.h"

#ifndef _WIN32
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>
#endif

namespace Text
{

const string g_utf8 = "utf-8";
static const string g_utf8NoHyp = "utf8";

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

int charsetFromString(const string& charset)
{
	if (charset.empty())
		return CHARSET_SYSTEM_DEFAULT;
	if (charset.length() == g_utf8.length() && isAsciiPrefix2(charset, g_utf8))
		return CHARSET_UTF8;
	if (charset.length() == g_utf8NoHyp.length() && isAsciiPrefix2(charset, g_utf8NoHyp))
		return CHARSET_UTF8;
	string::size_type pos = charset.rfind('.');
	if (pos == string::npos)
		pos = 0;
	else
		pos++;
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

int utf8ToWc(const char* str, wchar_t& c)
{
	dcassert(c == 0);
	uint8_t c0 = (uint8_t)str[0];
	if (c0 & 0x80)                                  // 1xxx xxxx
	{
		if (c0 & 0x40)                              // 11xx xxxx
		{
			if (c0 & 0x20)                          // 111x xxxx
			{
				if (c0 & 0x10)                      // 1111 xxxx
				{
					int n = -4;
					if (c0 & 0x08)                  // 1111 1xxx
					{
						n = -5;
						if (c0 & 0x04)              // 1111 11xx
						{
							if (c0 & 0x02)          // 1111 111x
							{
								return -1;
							}
							n = -6;
						}
					}
					int i = -1;
					while (i > n && (str[abs(i)] & 0x80) == 0x80)
						--i;
					return i;
				}
				else        // 1110xxxx
				{
					uint8_t c1 = (uint8_t)str[1];
					if ((c1 & (0x80 | 0x40)) != 0x80)
						return -1;
						
					uint8_t c2 = (uint8_t)str[2];
					if ((c2 & (0x80 | 0x40)) != 0x80)
						return -2;
						
					// Ugly utf-16 surrogate catch
					if ((c0 & 0x0f) == 0x0d && (c1 & 0x3c) >= (0x08 << 2))
						return -3;
						
					// Overlong encoding
					if (c0 == (0x80 | 0x40 | 0x20) && (c1 & (0x80 | 0x40 | 0x20)) == 0x80)
						return -3;
						
					c = (((wchar_t)c0 & 0x0f) << 12) |
					    (((wchar_t)c1 & 0x3f) << 6) |
					    ((wchar_t)c2 & 0x3f);
					    
					return 3;
				}
			}
			else                // 110xxxxx
			{
				uint8_t c1 = (uint8_t)str[1];
				if ((c1 & (0x80 | 0x40)) != 0x80)
					return -1;
					
				// Overlong encoding
				if ((c0 & ~1) == (0x80 | 0x40))
					return -2;
					
				c = (((wchar_t)c0 & 0x1f) << 6) |
				    ((wchar_t)c1 & 0x3f);
				return 2;
			}
		}
		else                    // 10xxxxxx
		{
			return -1;
		}
	}
	else                        // 0xxxxxxx
	{
		c = (unsigned char)str[0];
		return 1;
	}
}

const string& acpToUtf8(const string& str, string& tmp, int fromCharset) noexcept
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
	strcat(out, "//IGNORE");
}

template<typename string_type>
void convert(char* inBuf, size_t inSize, string_type& tgt, const char* fromCharset, const char* toCharset)
{
	size_t size = 0;
	iconv_t ic = iconv_open(toCharset, fromCharset);
	if (ic == (iconv_t) -1)
	{
		dcassert(0);
		tgt.clear();
		return;
	}
	char* outBuf = (char *) tgt.data();
	while (inSize)
	{
		size_t outBytesLeft = (tgt.length() - size) * sizeof(typename string_type::value_type);
		size_t result = iconv(ic, &inBuf, &inSize, &outBuf, &outBytesLeft);
		if (result == (size_t) -1)
		{
			if (errno == E2BIG)
			{
				tgt.resize(tgt.length() * 2);
				outBuf = (char *) tgt.data();
				continue;
			}
			//dcassert(0);
			break;
		}
		size = outBuf - (char *) tgt.data();
	}
	iconv_close(ic);
	tgt.resize(size / sizeof(typename string_type::value_type));
}
#endif

const wstring& acpToWide(const string& str, wstring& tgt, int fromCharset) noexcept
{
	if (str.empty())
	{
		tgt.clear();
		return tgt;
	}
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
	char charset[64];
	getIconvCharset(charset, fromCharset);
	tgt.resize(str.length());
	convert<wstring>((char *) str.data(), str.length(), tgt, charset, "wchar_t");
#endif
	return tgt;
}

const string& wideToUtf8(const wchar_t* str, size_t len, string& tgt) noexcept
{
	if (len == 0)
	{
		tgt.clear();
		return tgt;
	}
#ifdef _WIN32
	size_t size = 0;
	tgt.resize(len * 2 + 1);
	while ((size = WideCharToMultiByte(CP_UTF8, 0, str, len, &tgt[0], tgt.length(), nullptr, nullptr)) == 0)
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
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
	tgt.resize(len * 2);
	convert<string>((char *) str, len * sizeof(wchar_t), tgt, "wchar_t", g_utf8.c_str());
#endif
	return tgt;
}

const string& wideToAcp(const wstring& str, string& tgt, int toCharset) noexcept
{
	if (str.empty())
	{
		tgt.clear();
		return tgt;
	}
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
	char charset[64];
	getIconvCharset(charset, toCharset);
	tgt.resize(str.length() * 2);
	convert<string>((char *) str.data(), str.length() * sizeof(wchar_t), tgt, "wchar_t", charset);
#endif
	return tgt;
}

bool validateUtf8(const string& str, size_t pos /* = 0 */) noexcept
{
	while (pos < str.length())
	{
		wchar_t dummy = 0;
		const int j = utf8ToWc(&str[pos], dummy);
		if (j < 0)
			return false;
		pos += j;
	}
	return true;
}

const string& utf8ToAcp(const string& str, string& tmp, int toCharset) noexcept
{
#ifdef _WIN32
	wstring wtmp;
	return wideToAcp(utf8ToWide(str, wtmp), tmp, toCharset);
#else
	char charset[64];
	getIconvCharset(charset, toCharset);
	tmp.resize(str.length());
	convert<string>((char *) str.data(), str.length(), tmp, g_utf8.c_str(), charset);
	return tmp;
#endif
}

const wstring& utf8ToWide(const string& str, wstring& tgt) noexcept
{
	if (str.empty())
	{
		tgt.clear();
		return tgt;
	}
#ifdef _WIN32
	wstring::size_type size = 0;
	tgt.resize(str.length() + 1);
	while ((size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &tgt[0], tgt.length())) == 0)
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
	tgt.resize(str.length());
	convert<wstring>((char *) str.data(), str.length(), tgt, g_utf8.c_str(), "wchar_t");
	return tgt;
#endif
	return tgt;
}

void asciiMakeLower(string& str) noexcept
{
	for (string::size_type i = 0; i < str.length(); i++)
		str[i] = asciiToLower(str[i]);
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

string toDOS(string tmp)
{
	if (tmp.empty())
		return Util::emptyString;
		
	if (tmp[0] == '\r' && (tmp.size() == 1 || tmp[1] != '\n'))
	{
		tmp.insert(1, "\n");
	}
	for (string::size_type i = 1; i < tmp.size() - 1; ++i)
	{
		if (tmp[i] == '\r' && tmp[i + 1] != '\n')
		{
			// Mac ending
			tmp.insert(i + 1, "\n");
			i++;
		}
		else if (tmp[i] == '\n' && tmp[i - 1] != '\r')
		{
			// Unix encoding
			tmp.insert(i, "\r");
			i++;
		}
	}
	return tmp;
}

wstring toDOS(wstring tmp)
{
	if (tmp.empty())
		return Util::emptyStringW;
		
	if (tmp[0] == L'\r' && (tmp.size() == 1 || tmp[1] != L'\n'))
	{
		tmp.insert(1, L"\n");
	}
	for (string::size_type i = 1; i < tmp.size() - 1; ++i)
	{
		if (tmp[i] == L'\r' && tmp[i + 1] != L'\n')
		{
			// Mac ending
			tmp.insert(i + 1, L"\n");
			i++;
		}
		else if (tmp[i] == L'\n' && tmp[i - 1] != L'\r')
		{
			// Unix encoding
			tmp.insert(i, L"\r");
			i++;
		}
	}
	return tmp;
}

}
