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

#ifndef DCPLUSPLUS_DCPP_UTIL_H
#define DCPLUSPLUS_DCPP_UTIL_H

#include "Text.h"
#include "BaseUtil.h"
#include "HashValue.h"
#include "UriUtil.h"
#include "forward.h"
#include "debug.h"
#include <atomic>

const string& getAppName();
const tstring& getAppNameT();

const string& getAppNameVer();
const tstring& getAppNameVerT();

const string& getAppVersion();
const tstring& getAppVersionT();

class File;

#ifdef _DEBUG
void ASSERT_MAIN_THREAD();
void ASSERT_MAIN_THREAD_INIT();
#else
#define ASSERT_MAIN_THREAD()
#define ASSERT_MAIN_THREAD_INIT()
#endif

namespace Util
{
	extern bool away;
	extern string awayMsg;
	extern time_t awayTime;
	extern const time_t startTime;

	void initialize();

	bool isAdc(const string& url);
	bool isAdcS(const string& url);
	inline bool isAdcHub(const string& url)
	{
		return isAdc(url) || isAdcS(url);
	}

	// Identify magnet links.
	bool isMagnetLink(const char* url);
	bool isMagnetLink(const string& url);
	bool isMagnetLink(const wchar_t* url);
	bool isMagnetLink(const wstring& url);
	bool isHttpLink(const string& url);
	bool isHttpLink(const wstring& url);

	bool isTorrentFile(const tstring& file);
	bool isDclstFile(const string& file);

	string getIETFLang();

	inline time_t getStartTime() { return startTime; }
	inline time_t getUpTime() { return time(nullptr) - getStartTime(); }
	int getCurrentHour();

	template<typename string_t>
	void replace(const string_t& search, const string_t& replacement, string_t& str)
	{
		typename string_t::size_type i = 0;
		while ((i = str.find(search, i)) != string_t::npos)
		{
			str.replace(i, search.size(), replacement);
			i += replacement.size();
		}
	}
	template<typename string_t>
	void replace(const typename string_t::value_type* search, const typename string_t::value_type* replacement, string_t& str)
	{
		replace(string_t(search), string_t(replacement), str);
	}

	enum
	{
		HUB_PROTOCOL_NMDC = 1,
		HUB_PROTOCOL_NMDCS,
		HUB_PROTOCOL_ADC,
		HUB_PROTOCOL_ADCS
	};

	int getHubProtocol(const string& scheme); // 'scheme' must be in lowercase

	string validateFileName(string file, bool badCharsOnly = false);
	string ellipsizePath(const string& path);

	string getShortTimeString(time_t t = time(nullptr));

	string toAdcFile(const string& file);
	string toNmdcFile(const string& file);

	template<typename T>
	T roundDown(T size, T blockSize)
	{
		return ((size + blockSize / 2) / blockSize) * blockSize;
	}
		
	template<typename T>
	T roundUp(T size, T blockSize)
	{
		return ((size + blockSize - 1) / blockSize) * blockSize;
	}

	string toString(const char* sep, const StringList& lst);
	string toString(char sep, const StringList& lst);
	string toString(const StringList& lst);

	template<typename T, class NameOperator>
	string listToStringT(const T& lst, bool forceBrackets, bool squareBrackets)
	{
		string tmp;
		if (lst.empty())
			return tmp;
		if (lst.size() == 1 && !forceBrackets)
			return NameOperator()(*lst.begin());
			
		tmp.push_back(squareBrackets ? '[' : '(');
		for (auto i = lst.begin(), iend = lst.end(); i != iend; ++i)
		{
			tmp += NameOperator()(*i);
			tmp += ", ";
		}
		
		if (tmp.length() == 1)
		{
			tmp.push_back(squareBrackets ? ']' : ')');
		}
		else
		{
			tmp.pop_back();
			tmp[tmp.length() - 1] = squareBrackets ? ']' : ')';
		}
		return tmp;
	}

	struct StrChar
	{
		const char* operator()(const string& u)
		{
			dcassert(!u.empty());
			if (!u.empty())
				return u.c_str();
			else
				return "";
		}
	};

	template<typename ListT>
	static string listToString(const ListT& lst)
	{
		return listToStringT<ListT, StrChar>(lst, false, true);
	}

#ifdef _WIN32
	int defaultSort(const wchar_t* a, const wchar_t* b, bool noCase = true);
	int defaultSort(const wstring& a, const wstring& b, bool noCase = true);
#endif

	inline bool getAway() { return away; }
	void setAway(bool away, bool notUpdateInfo = false);
	string getAwayMessage(const string& customMsg, StringMap& params);
	inline void setAwayMessage(const string& msg) { awayMsg = msg; }

	string getDownloadDir(const UserPtr& user);
	string expandDownloadDir(const string& dir, const UserPtr& user);

	uint64_t getDirSize(const string& path, std::atomic_bool& abortFlag, void (*progressFunc)(void* ctx, int64_t size), void* ctx);
	string getNewFileName(const string& filename);

	string getRandomNick(size_t maxLength = 20);
	string getRandomPassword();

	string formatDchubUrl(const string& url);
	string formatDchubUrl(ParsedUrl& p);

	string getMagnet(const string& hash, const string& file, int64_t size);
	string getMagnet(const TTHValue& hash, const string& file, int64_t size);
	string getWebMagnet(const TTHValue& hash, const string& file, int64_t size);

	StringList splitSettingAndLower(const string& patternList, bool trimSpace = false);

	string getLang();

	void readTextFile(File& file, std::function<bool(const string&)> func);

	static inline bool isTTHBase32(const string& str)
	{
		if (str.length() != 43 || memcmp(str.c_str(), "TTH:", 4)) return false;
		for (size_t i = 4; i < 43; i++)
			if (!((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= '2' && str[i] <= '7'))) return false;
		return true;
	}

	template<typename C> bool isTigerHashString(const std::basic_string<C>& s)
	{
		if (s.length() != 39) return false;
		auto str = s.data();
		for (size_t i = 0; i < 39; i++)
			if (!((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= '2' && str[i] <= '7'))) return false;
		return true;
	}

	template<typename string_type> void convertToDos(string_type& s)
	{
		typename string_type::size_type i = 0;
		while (i < s.length())
		{
			if (s[i] == '\n' && (i == 0 || s[i-1] != '\r'))
			{
				s.insert(i, 1, '\r');
				i += 2;
			}
			else if (s[i] == '\r' && (i + 1 == s.length() || s[i+1] != '\n'))
			{
				s.insert(i + 1, 1, '\n');
				i += 2;
			}
			else
				++i;
		}
	}
}

// FIXME FIXME FIXME
struct noCaseStringHash
{
	size_t operator()(const string* s) const
	{
		return operator()(*s);
	}
	
	size_t operator()(const string& s) const
	{
		size_t x = 0;
		size_t i = 0, len = s.length();
		while (i < len)
		{
			uint32_t c;
			int n = Text::utf8ToWc(s.data(), i, len, c);
			if (n < 0)
			{
				x = x * 31 + (unsigned char) s[i];
				i -= n;
			}
			else
			{
				if (c < 0x10000) c = Text::toLower(c);
				x = x * 31 + (size_t) c;
				i += n;
			}
		}
		return x;
	}
	
	size_t operator()(const wstring* s) const
	{
		return operator()(*s);
	}
	size_t operator()(const wstring& s) const
	{
		size_t x = 0;
		const wchar_t* y = s.data();
		wstring::size_type j = s.size();
		for (wstring::size_type i = 0; i < j; ++i)
		{
			x = x * 31 + (size_t)Text::toLower(y[i]);
		}
		return x;
	}
	
	//bool operator()(const string* a, const string* b) const
	//{
	//  return stricmp(*a, *b) < 0;
	//}
	bool operator()(const string& a, const string& b) const
	{
		return stricmp(a, b) < 0;
	}
	//bool operator()(const wstring* a, const wstring* b) const
	//{
	//  return stricmp(*a, *b) < 0;
	//}
	bool operator()(const wstring& a, const wstring& b) const
	{
		return stricmp(a, b) < 0;
	}
};

// FIXME FIXME FIXME
struct noCaseStringEq
{
	bool operator()(const string* a, const string* b) const
	{
		return a == b || stricmp(*a, *b) == 0;
	}
	bool operator()(const string& a, const string& b) const
	{
		return stricmp(a, b) == 0;
	}
	bool operator()(const wstring* a, const wstring* b) const
	{
		return a == b || stricmp(*a, *b) == 0;
	}
	bool operator()(const wstring& a, const wstring& b) const
	{
		return stricmp(a, b) == 0;
	}
};

// FIXME FIXME FIXME
struct noCaseStringLess
{
	//bool operator()(const string* a, const string* b) const
	//{
	//  return stricmp(*a, *b) < 0;
	//}
	bool operator()(const string& a, const string& b) const
	{
		return stricmp(a, b) < 0;
	}
	//bool operator()(const wstring* a, const wstring* b) const
	//{
	//  return stricmp(*a, *b) < 0;
	//}
	bool operator()(const wstring& a, const wstring& b) const
	{
		return stricmp(a, b) < 0;
	}
};

#endif // !defined(UTIL_H)
