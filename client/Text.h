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

#ifndef DCPLUSPLUS_DCPP_TEXT_H
#define DCPLUSPLUS_DCPP_TEXT_H

#include "typedefs.h"

/**
 * Text handling routines for DC++. DC++ internally uses UTF-8 for
 * (almost) all string:s, hence all foreign text must be converted
 * appropriately...
 * acp - ANSI code page used by the system
 * wide - wide unicode string
 * utf8 - UTF-8 representation of the string
 * t - current GUI text format
 * string - UTF-8 string (most of the time)
 * wstring - Wide string
 * tstring - GUI type string (acp string or wide string depending on build type)
 */

namespace Text
{

extern const string g_utf8;

enum
{
	CHARSET_SYSTEM_DEFAULT = 0,
	CHARSET_UTF8 = 8
};

static const int NUM_SUPPORTED_CHARSETS = 11;
extern const int supportedCharsets[NUM_SUPPORTED_CHARSETS];

int charsetFromString(const string& charset);
string charsetToString(int charset);

int getDefaultCharset();

const string& acpToUtf8(const string& str, string& tmp, int fromCharset = CHARSET_SYSTEM_DEFAULT) noexcept;
inline string acpToUtf8(const string& str, int fromCharset = CHARSET_SYSTEM_DEFAULT) noexcept
{
	string tmp;
	return acpToUtf8(str, tmp, fromCharset);
}

const wstring& acpToWide(const string& str, wstring& tmp, int fromCharset = CHARSET_SYSTEM_DEFAULT) noexcept;
inline wstring acpToWide(const string& str, int fromCharset = CHARSET_SYSTEM_DEFAULT) noexcept
{
	wstring tmp;
	return acpToWide(str, tmp, fromCharset);
}

const string& utf8ToAcp(const string& str, string& tmp, int toCharset = CHARSET_SYSTEM_DEFAULT) noexcept;
inline string utf8ToAcp(const string& str, int toCharset = CHARSET_SYSTEM_DEFAULT) noexcept
{
	string tmp;
	return utf8ToAcp(str, tmp, toCharset);
}

const wstring& utf8ToWide(const string& str, wstring& tmp) noexcept;
inline wstring utf8ToWide(const string& str) noexcept
{
	wstring tmp;
	return utf8ToWide(str, tmp);
}

const string& wideToAcp(const wstring& str, string& tmp, int toCharset = CHARSET_SYSTEM_DEFAULT) noexcept;
inline string wideToAcp(const wstring& str, int toCharset = CHARSET_SYSTEM_DEFAULT) noexcept
{
	string tmp;
	return wideToAcp(str, tmp, toCharset);
}

const string& wideToUtf8(const wchar_t* str, size_t len, string& tgt) noexcept;

inline const string& wideToUtf8(const wstring& str, string& tmp) noexcept
{
	return wideToUtf8(str.c_str(), str.length(), tmp);
}

inline string wideToUtf8(const wstring& str) noexcept
{
	string tmp;
	return wideToUtf8(str, tmp);
}

// Returns the number of bytes used or a negative value in case of error.
// The error value indicates how many bytes should be skipped.
int utf8ToWc(const char* s, size_t pos, size_t len, uint32_t& wc) noexcept;

#ifdef _UNICODE
inline const tstring& toT(const string& str, tstring& tmp) noexcept
{
	return utf8ToWide(str, tmp);
}

inline tstring toT(const string& str) noexcept
{
	return utf8ToWide(str);
}

inline const string& fromT(const tstring& str, string& tmp) noexcept
{
	return wideToUtf8(str, tmp);
}

inline string fromT(const tstring& str) noexcept
{
	return wideToUtf8(str);
}

inline string fromT(const TCHAR* str) noexcept
{
	return fromT(tstring(str));
}
#else

inline const tstring& toT(const string& str, tstring&) noexcept { return str; }
inline const tstring& toT(const string& str) noexcept { return str; }
inline const string& fromT(const tstring& str, string&) noexcept { return str; }
inline const string& fromT(const tstring& str) noexcept { return str; }

#endif

bool isAscii(const string& str) noexcept;
bool isAscii(const char* str) noexcept;
bool validateUtf8(const string& str, size_t pos = 0) noexcept;

static inline int asciiToLower(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
	return c;
}

void asciiMakeLower(string& str) noexcept;

template<typename string_type>
static bool isAsciiSuffix(const string_type& str, const string_type& suffix)
{
	if (str.length() < suffix.length()) return false;
	typename string_type::size_type offset = str.length() - suffix.length(); 
	for (typename string_type::size_type i = 0; i < suffix.length(); i++)
		if (Text::asciiToLower(str[offset + i]) != Text::asciiToLower(suffix[i])) return false;
	return true;
}

// same as isAsciiSuffix but suffix is already in lowercase
template<typename string_type>
static bool isAsciiSuffix2(const string_type& str, const string_type& suffix)
{
	if (str.length() < suffix.length()) return false;
	typename string_type::size_type offset = str.length() - suffix.length(); 
	for (typename string_type::size_type i = 0; i < suffix.length(); i++)
		if (Text::asciiToLower(str[offset + i]) != suffix[i]) return false;
	return true;
}

template<typename string_type>
static bool isAsciiPrefix(const string_type& str, const string_type& prefix)
{
	if (str.length() < prefix.length()) return false;
	for (typename string_type::size_type i = 0; i < prefix.length(); i++)
		if (Text::asciiToLower(str[i]) != Text::asciiToLower(prefix[i])) return false;
	return true;
}

// same as isAsciiPrefix but prefix is already in lowercase
template<typename string_type>
static bool isAsciiPrefix2(const string_type& str, const string_type& prefix)
{
	if (str.length() < prefix.length()) return false;
	for (typename string_type::size_type i = 0; i < prefix.length(); i++)
		if (Text::asciiToLower(str[i]) != prefix[i]) return false;
	return true;
}

template<typename char_type>
static bool isAsciiPrefix2(const char_type* str, const char_type* prefix, size_t prefixLen)
{
	for (size_t i = 0; i < prefixLen; i++)
		if (Text::asciiToLower(str[i]) != prefix[i]) return false;
	return true;
}

template<typename string_type>
static bool asciiEqual(const string_type& s1, const string_type& s2)
{
	if (s1.length() != s2.length()) return false;
	for (size_t i = 0; i < s1.length(); i++)
		if (Text::asciiToLower(s1[i]) != Text::asciiToLower(s2[i])) return false;
	return true;
}

inline wchar_t toLower(wchar_t c) noexcept
{
#ifdef _WIN32
	return static_cast<wchar_t>(reinterpret_cast<ptrdiff_t>(CharLowerW((LPWSTR)c)));
#else
	return (wchar_t) towlower(c);
#endif
}

void asciiMakeLower(string& str) noexcept;
void makeLower(wstring& str) noexcept;
wstring toLower(const wstring& str) noexcept;
wstring toLower(const wstring& str, wstring& tmp) noexcept;

void makeLower(string& str) noexcept;
string toLower(const string& str) noexcept;
string toLower(const string& str, string& tmp) noexcept;

const string& toUtf8(const string& str, int fromCharset, string& tmp) noexcept;
inline string toUtf8(const string& str, int fromCharset = CHARSET_SYSTEM_DEFAULT)
{
	string tmp;
	return toUtf8(str, fromCharset, tmp);
}

const string& fromUtf8(const string& str, int toCharset, string& tmp) noexcept;
inline string fromUtf8(const string& str, int toCharset = CHARSET_SYSTEM_DEFAULT) noexcept
{
	string tmp;
	return fromUtf8(str, toCharset, tmp);
}

string toDOS(string tmp);
wstring toDOS(wstring tmp);

} // namespace Text

#endif // DCPLUSPLUS_DCPP_TEXT_H
