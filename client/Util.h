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
#include "MerkleTree.h"
#include <atomic>

#define URI_SEPARATOR '/'
#define URI_SEPARATOR_STR "/"

#ifdef _UNICODE
#define formatBytesT formatBytesW
#define formatSecondsT formatSecondsW
#else
#define formatBytesT formatBytes
#define formatSecondsT formatSeconds
#endif

const string& getAppName();
const tstring& getAppNameT();

const string& getAppNameVer();
const tstring& getAppNameVerT();

const string& getAppVersion();
const tstring& getAppVersionT();

const string& getHttpUserAgent();

class File;

namespace Util
{
	enum Paths
	{
		/** Global configuration */
		PATH_GLOBAL_CONFIG,
		/** Per-user configuration (queue, favorites, ...) */
		PATH_USER_CONFIG,
		/** Per-user local data (cache, temp files, ...) */
		PATH_USER_LOCAL,
		/** Translations */
		PATH_WEB_SERVER,
		/** Default download location */
		PATH_DOWNLOADS,
		/** Default file list location */
		PATH_FILE_LISTS,
		/** Default hub list cache */
		PATH_HUB_LISTS,
		/** Where the notepad file is stored */
		PATH_NOTEPAD,
		/** Folder with emoticons packs*/
		PATH_EMOPACKS,
		/** Languages files location*/
		PATH_LANGUAGES,
		/** Themes and resources */
		PATH_THEMES,
		/** Executable path **/
		PATH_EXE,
		/** Sounds path **/
		PATH_SOUNDS,
		PATH_LAST
	};

	enum SysPaths
	{
#ifdef _WIN32
		WINDOWS,
		PROGRAM_FILESX86,
		PROGRAM_FILES,
		APPDATA,
		LOCAL_APPDATA,
		COMMON_APPDATA,
		PERSONAL,
#endif
		SYS_PATH_LAST
	};
	
	extern string paths[PATH_LAST];
	extern string sysPaths[SYS_PATH_LAST];
	extern bool away;
	extern string awayMsg;
	extern time_t awayTime;
	extern const time_t startTime;

	/** Path of program configuration files */
	inline const string& getPath(Paths path)
	{
		dcassert(!paths[path].empty());
		return paths[path];
	}

	/** Path of system folder */
	inline const string& getSysPath(SysPaths path)
	{
		dcassert(!sysPaths[path].empty());
		return sysPaths[path];
	}

	extern NUMBERFMT g_nf;

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
	bool isTorrentLink(const tstring& url);
	bool isHttpLink(const string& url);
	bool isHttpLink(const wstring& url);
		
	template<typename string_type>
	inline bool checkFileExt(const string_type& filename, const string_type& ext)
	{
		if (filename.length() <= ext.length()) return false;
		return Text::isAsciiSuffix2<string_type>(filename, ext);
	}

	bool isTorrentFile(const tstring& file);
	bool isDclstFile(const tstring& file);
	bool isDclstFile(const string& file);
		
	template <class T>
	inline void appendPathSeparator(T& path)
	{
		if (!path.empty() && path.back() != PATH_SEPARATOR && path.back() != URI_SEPARATOR)
			path += T::value_type(PATH_SEPARATOR);
	}

	template <class T>
	inline void appendUriSeparator(T& path)
	{
		if (!path.empty() && path.back() != URI_SEPARATOR && path.back() != PATH_SEPARATOR)
			path += T::value_type(URI_SEPARATOR);
	}

	template<typename T>
	inline void uriSeparatorsToPathSeparators(T& str)
	{
		std::replace(str.begin(), str.end(), URI_SEPARATOR, PATH_SEPARATOR);
	}

	template<typename T>
	inline void removePathSeparator(T& path)
	{
#ifdef _WIN32
		if (path.length() == 3 && path[1] == ':') return; // Drive letter, like "C:\"
#endif
		if (!path.empty() && path.back() == PATH_SEPARATOR)
			path.erase(path.length()-1);
	}

	template<typename T>
	inline bool isReservedDirName(const T& name)
	{
		if (name.empty() || name.length() > 2) return false;
		if (name[0] != '.') return false;
		return name.length() == 1 || name[1] == '.';
	}
	
	/** Path of temporary storage */
	string getTempPath();
		
	/** Migrate from pre-localmode config location */
	void migrate(const string& file) noexcept;
		
	inline const string& getListPath() { return getPath(PATH_FILE_LISTS); }
	inline const string& getHubListsPath() { return getPath(PATH_HUB_LISTS); }
	inline const string& getNotepadFile() { return getPath(PATH_NOTEPAD); }
	inline const string& getConfigPath() { return getPath(PATH_USER_CONFIG); }
	inline const string& getDataPath() { return getPath(PATH_GLOBAL_CONFIG); }	
	inline const string& getLocalisationPath() { return getPath(PATH_LANGUAGES); }
	inline const string& getLocalPath() { return getPath(PATH_USER_LOCAL); }
	inline const string& getWebServerPath() { return getPath(PATH_WEB_SERVER); }
	inline const string& getDownloadsPath() { return getPath(PATH_DOWNLOADS); }
	inline const string& getEmoPacksPath() { return getPath(PATH_EMOPACKS); }
	inline const string& getThemesPath() { return getPath(PATH_THEMES); }
	inline const string& getExePath() { return getPath(PATH_EXE); }
	inline const string& getSoundPath() { return getPath(PATH_SOUNDS); }
		
	string getIETFLang();
		
	inline time_t getStartTime() { return startTime; }
	inline time_t getUpTime() { return time(nullptr) - getStartTime(); }
		
	template<typename string_t>
	static string_t getFilePath(const string_t& path)
	{
		const auto i = path.rfind(PATH_SEPARATOR);
		return i != string_t::npos ? path.substr(0, i + 1) : path;
	}

	template<typename string_t>
	inline string_t getFileName(const string_t& path)
	{
		const auto i = path.rfind(PATH_SEPARATOR);
		return i != string_t::npos ? path.substr(i + 1) : path;
	}

	template<typename string_t>
	inline string_t getFileExtWithoutDot(const string_t& path)
	{
		const auto i = path.rfind('.');
		if (i == string_t::npos) return string_t();
		const auto j = path.rfind(PATH_SEPARATOR);
		if (j != string_t::npos && j > i) return string_t();
		return path.substr(i + 1);
	}

	template<typename string_t>
	inline string_t getFileExt(const string_t& path)
	{
		const auto i = path.rfind('.');
		if (i == string_t::npos) return string_t();
		const auto j = path.rfind(PATH_SEPARATOR);
		if (j != string_t::npos && j > i) return string_t();
		return path.substr(i);
	}

	template<typename string_t>
	inline string_t getLastDir(const string_t& path)
	{
		const auto i = path.rfind(PATH_SEPARATOR);
		if (i == string_t::npos) return string_t();	
		const auto j = path.rfind(PATH_SEPARATOR, i - 1);
		return j != string_t::npos ? path.substr(j + 1, i - j - 1) : path;
	}
		
	template<typename string_t>
	void replace(const string_t& search, const string_t& replacement, string_t& str)
	{
		string_t::size_type i = 0;
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
		
	void parseIpPort(const string& ipPort, string& ip, uint16_t& port);

	void decodeUrl(const string& url, string& protocol, string& host, uint16_t& port, string& path, bool& isSecure, string& query, string& fragment);
	inline void decodeUrl(const string& url, string& protocol, string& host, uint16_t& port, string& path, string& query, string& fragment)
	{
		bool isSecure;
		decodeUrl(url, protocol, host, port, path, isSecure, query, fragment);
	}

	enum
	{
		HUB_PROTOCOL_NMDC = 1,
		HUB_PROTOCOL_NMDCS,
		HUB_PROTOCOL_ADC,
		HUB_PROTOCOL_ADCS
	};

	int getHubProtocol(const string& scheme); // 'scheme' must be in lowercase

	std::map<string, string> decodeQuery(const string& query);
	string getQueryParam(const string& query, const string& key);
		
	string validateFileName(string aFile);
	string cleanPathChars(string aNick);
	string ellipsizePath(const string& path);
		
	string getShortTimeString(time_t t = time(nullptr));
		
	string toAdcFile(const string& file);
	string toNmdcFile(const string& file);
		
	wstring formatSecondsW(int64_t aSec, bool supressHours = false) noexcept;
	string formatSeconds(int64_t aSec, bool supressHours = false) noexcept;
	string formatTime(uint64_t rest, const bool withSeconds = true) noexcept;
	string formatDateTime(const string &format, time_t t, bool useGMT = false) noexcept;
	string formatDateTime(time_t t, bool useGMT = false) noexcept;
	string formatCurrentDate() noexcept;
		
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

	inline string toStringPercent(int val)
	{
		char buf[16];
		_snprintf(buf, sizeof(buf), "%d%%", val);
		return buf;
	}

	string toString(const char* sep, const StringList& lst);
	string toString(char sep, const StringList& lst);
	string toString(const StringList& lst);

	template<typename T, class NameOperator>
	string listToStringT(const T& lst, bool forceBrackets, bool squareBrackets)
	{
		string tmp;
		if (lst.empty()) //[+]FlylinkDC++
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
			dcassert(!u.empty()) // FlylinkDC++
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
		
	string formatBytes(int64_t bytes);
	string formatBytes(double bytes);
	inline string formatBytes(int32_t bytes)  { return formatBytes(int64_t(bytes)); }
	inline string formatBytes(uint32_t bytes) { return formatBytes(int64_t(bytes)); }
	inline string formatBytes(uint64_t bytes) { return formatBytes(int64_t(bytes)); }
	
	wstring formatBytesW(int64_t bytes);
	wstring formatBytesW(double bytes);
	inline wstring formatBytesW(int32_t bytes)  { return formatBytesW(int64_t(bytes)); }
	inline wstring formatBytesW(uint32_t bytes) { return formatBytesW(int64_t(bytes)); }
	inline wstring formatBytesW(uint64_t bytes) { return formatBytesW(int64_t(bytes)); }

	wstring formatExactSize(int64_t aBytes);

	string encodeURI(const string& /*aString*/, bool reverse = false);

	int defaultSort(const wchar_t* a, const wchar_t* b, bool noCase = true);
	int defaultSort(const wstring& a, const wstring& b, bool noCase = true);

	inline bool getAway() { return away; }
	void setAway(bool away, bool notUpdateInfo = false);
	string getAwayMessage(const string& customMsg, StringMap& params);
	inline void setAwayMessage(const string& msg) { awayMsg = msg; }
		
	bool validatePath(const string &sPath);
	string getFilenameForRenaming(const string& filename);
		
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
	string getRandomNick(size_t maxLength = 20);
	
	struct AdapterInfo
	{
		AdapterInfo(const tstring& name, const string& ip, int prefix) : adapterName(name), ip(ip), prefix(prefix) { }
		tstring adapterName;
		string ip;
		int prefix;
	};

	void getNetworkAdapters(bool v6, vector<AdapterInfo>& adapterInfos) noexcept;
	string getDefaultGateway();
	string getLocalIp();
	bool isPrivateIp(const string& ip);
	inline bool isPrivateIp(uint32_t ip)
	{
		return ((ip & 0xff000000) == 0x0a000000 || // 10.0.0.0/8
		        (ip & 0xff000000) == 0x7f000000 || // 127.0.0.0/8
		        (ip & 0xffff0000) == 0xa9fe0000 || // 169.254.0.0/16
		        (ip & 0xfff00000) == 0xac100000 || // 172.16.0.0/12
		        (ip & 0xffff0000) == 0xc0a80000);  // 192.168.0.0/16
	}		

	// static string formatMessage(const string& message);[-] IRainman fix
	void setLimiter(bool aLimiter);
		
	bool getTTH(const string& filename, bool isAbsPath, size_t bufSize, std::atomic_bool& stopFlag, TigerTree& tree, unsigned maxLevels = 0);
	void backupSettings();
	string formatDchubUrl(const string& url);
	string formatDchubUrl(const string& proto, const string& host, uint16_t port); // proto must be in lowercase
		
	string getMagnet(const TTHValue& hash, const string& file, int64_t size);
	string getWebMagnet(const TTHValue& hash, const string& file, int64_t size);
		
	tstring getModuleFileName();
		
	void loadBootConfig();
	bool locatedInSysPath(SysPaths sysPath, const string& path);
	bool locatedInSysPath(const string& path);
	void initProfileConfig();
	void moveSettings();
#ifdef _WIN32
	string getDownloadPath(const string& def);		
#endif

	StringList splitSettingAndLower(const string& patternList, bool trimSpace = false);
	
	string getLang();
	
	bool parseIpAddress(uint32_t& result, const string& s, string::size_type start, string::size_type end);
	bool parseIpAddress(uint32_t& result, const wstring& s, wstring::size_type start, wstring::size_type end);
	inline bool parseIpAddress(uint32_t& result, const string& s)
	{
		return parseIpAddress(result, s, 0, s.length());
	}
	inline bool parseIpAddress(uint32_t& result, const wstring& s)
	{
		return parseIpAddress(result, s, 0, s.length());
	}

	uint32_t getNumericIp4(const tstring& s);
	bool isValidIp4(const string& ip);
	bool isValidIp4(const wstring& ip);
	bool isSameNetwork(const string& addr1, const string& addr2, unsigned prefix, bool v6);

	void readTextFile(File& file, std::function<bool(const string&)> func);
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
		const char* end = s.data() + s.size();
		for (const char* str = s.data(); str < end;)
		{
			wchar_t c = 0;
			int n = Text::utf8ToWc(str, c);
			if (n < 0)
			{
				x = x * 32 - x + '_'; //-V112
				str += static_cast<size_t>(abs(n));
			}
			else
			{
				x = x * 32 - x + (size_t)Text::toLower(c); //-V112
				str += static_cast<size_t>(n);
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

// parent class for objects with a lot of empty columns in list
// FIXME: remove it
template<int C> class ColumnBase
#ifdef _DEBUG
	: private boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		virtual ~ColumnBase() {}
		const tstring& getText(const int col) const
		{
			dcassert(col >= 0 && col < C);
			return m_info[col];
		}
		void setText(const int col, const tstring& val)
		{
			dcassert(col >= 0 && col < C);
			m_info[col] = val;
		}
	protected:
		tstring m_info[C];
};

#endif // !defined(UTIL_H)
