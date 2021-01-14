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

#include "shlobj.h"
#include "StrUtil.h"
#include "CID.h"
#include "File.h"
#include "StringTokenizer.h"
#include "ParamExpander.h"
#include "SettingsManager.h"
#include "ClientManager.h"
#include "SimpleXML.h"
#include "OnlineUser.h"
#include "Socket.h"
#include "LogManager.h"
#include "CompatibilityManager.h"
#include "idna/idna.h"

#include <boost/algorithm/string.hpp>
#include <openssl/rand.h>

/** In local mode, all config and temp files are kept in the same dir as the executable */
static bool localMode;

const time_t Util::startTime = time(nullptr);

bool Util::away = false;
string Util::awayMsg;
time_t Util::awayTime;

string Util::paths[Util::PATH_LAST];
string Util::sysPaths[Util::SYS_PATH_LAST];
NUMBERFMT Util::g_nf = { 0 };

static const string httpUserAgent = "FlylinkDC++ r504 build 22345";

const string& getAppName()
{
	static const string s(APPNAME);
	return s;
}

const tstring& getAppNameT()
{
	static const tstring s(_T(APPNAME));
	return s;
}

const string& getAppNameVer()
{
	static const string s(APPNAME " " VERSION_STR);
	return s;
}

const tstring& getAppNameVerT()
{
	static const tstring s(_T(APPNAME " " VERSION_STR));
	return s;
}

const string& getAppVersion()
{
	static const string s(VERSION_STR);
	return s;
}

const tstring& getAppVersionT()
{
	static const tstring s(_T(VERSION_STR));
	return s;
}

const string& getHttpUserAgent()
{
	return httpUserAgent;
}

static void initRand();

extern "C" void bz_internal_error(int errcode)
{
	dcdebug("bzip2 internal error: %d\n", errcode);
	// TODO - логирование?
}

#if (_MSC_VER >= 1400 )
void WINAPI invalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	//do nothing, this exist because vs2k5 crt needs it not to crash on errors.
}
#endif

bool Util::locatedInSysPath(const string& path)
{
	// don't share Windows directory
	return Util::locatedInSysPath(Util::WINDOWS, path) ||
	       Util::locatedInSysPath(Util::APPDATA, path) ||
	       Util::locatedInSysPath(Util::LOCAL_APPDATA, path) ||
	       Util::locatedInSysPath(Util::PROGRAM_FILES, path) ||
	       Util::locatedInSysPath(Util::PROGRAM_FILESX86, path);
}

bool Util::locatedInSysPath(Util::SysPaths sysPath, const string& currentPath)
{
	const string& path = sysPaths[sysPath];
	return !path.empty() && currentPath.length() >= path.length() && strnicmp(currentPath, path, path.size()) == 0;
}

void Util::initProfileConfig()
{
	paths[PATH_USER_CONFIG] = getSysPath(APPDATA) + APPNAME PATH_SEPARATOR_STR;
}

static const string configFiles[] =
{
	"ADLSearch.xml",
	"DCPlusPlus.xml",
	"Favorites.xml",
	"IPTrust.ini",
#ifdef SSA_IPGRANT_FEATURE
	"IPGrant.ini",
#endif
	"IPGuard.ini",
	"Queue.xml",
	"DHT.xml"
};

static void copySettings(const string& sourcePath, const string& destPath)
{
	File::ensureDirectory(destPath);
	for (size_t i = 0; i < _countof(configFiles); ++i)
	{
		string sourceFile = sourcePath + configFiles[i];
		string destFile = destPath + configFiles[i];
		if (!File::isExist(destFile) && File::isExist(sourceFile))
		{
			if (!File::copyFile(sourceFile, destFile))
				LogManager::message("Error copying " + sourceFile + " to " + destFile + ": " + Util::translateError());
		}
	}
}

void Util::moveSettings()
{
	copySettings(paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR, paths[PATH_USER_CONFIG]);
}

void Util::backupSettings()
{
	copySettings(getConfigPath(),
		formatDateTime(getConfigPath() + "Backup" PATH_SEPARATOR_STR "%Y-%m-%d" PATH_SEPARATOR_STR, time(nullptr)));
}

tstring Util::getModuleFileName()
{
	static tstring moduleFileName;
	if (moduleFileName.empty())
	{
		TCHAR buf[MAX_PATH];
		DWORD len = GetModuleFileName(NULL, buf, MAX_PATH);
		moduleFileName.assign(buf, len);
	}
	return moduleFileName;
}

void Util::initialize()
{
	initRand();
	
#if (_MSC_VER >= 1400)
	_set_invalid_parameter_handler(reinterpret_cast<_invalid_parameter_handler>(invalidParameterHandler));
#endif
	setlocale(LC_ALL, "");

	static TCHAR g_sep[2] = _T(",");
	static wchar_t g_Dummy[16] = { 0 };
	g_nf.lpDecimalSep = g_sep;
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SGROUPING, g_Dummy, 16);
	g_nf.Grouping = _wtoi(g_Dummy);
	GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, g_Dummy, 16);
	g_nf.lpThousandSep = g_Dummy;
	
	paths[PATH_EXE] = Util::getFilePath(Text::fromT(Util::getModuleFileName()));
	TCHAR buf[MAX_PATH];
#define SYS_WIN_PATH_INIT(path) \
	if(::SHGetFolderPath(NULL, CSIDL_##path, NULL, SHGFP_TYPE_CURRENT, buf) == S_OK) \
	{ \
		sysPaths[path] = Text::fromT(buf) + PATH_SEPARATOR; \
	}
	
	//LogManager::message("Sysytem Path: " + sysPaths[path]);
	//LogManager::message("Error SHGetFolderPath: GetLastError() = " + Util::toString(GetLastError()));
	
	SYS_WIN_PATH_INIT(WINDOWS);
	SYS_WIN_PATH_INIT(PROGRAM_FILESX86);
	SYS_WIN_PATH_INIT(PROGRAM_FILES);
	if (CompatibilityManager::runningIsWow64())
	{
		// Correct PF path on a 64-bit system running 32-bit version.
		const char* PFW6432 = getenv("ProgramW6432");
		if (PFW6432)
			sysPaths[PROGRAM_FILES] = string(PFW6432) + PATH_SEPARATOR;
	}
	SYS_WIN_PATH_INIT(APPDATA);
	SYS_WIN_PATH_INIT(LOCAL_APPDATA);
	SYS_WIN_PATH_INIT(COMMON_APPDATA);
	SYS_WIN_PATH_INIT(PERSONAL);
	
#undef SYS_WIN_PATH_INIT
	
	paths[PATH_GLOBAL_CONFIG] = paths[PATH_EXE];
	loadBootConfig();

	if (localMode && paths[PATH_USER_CONFIG].empty())
		paths[PATH_USER_CONFIG] = paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR;

#ifdef USE_APPDATA
	if (paths[PATH_USER_CONFIG].empty() &&
	    (File::isExist(paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR "DCPlusPlus.xml") ||
	    !(locatedInSysPath(PROGRAM_FILES, paths[PATH_EXE]) || locatedInSysPath(PROGRAM_FILESX86, paths[PATH_EXE]))))
	{
		// Check if settings directory is writable
		paths[PATH_USER_CONFIG] = paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
		const auto tempFile = paths[PATH_USER_CONFIG] + ".test-writable-" + Util::toString(Util::rand()) + ".tmp";
		try
		{
			File f(tempFile, File::WRITE, File::CREATE | File::TRUNCATE);
			f.close();
			File::deleteFile(tempFile);
			localMode = true;
		}
		catch (const FileException&)
		{
			auto error = GetLastError();
			if (error == ERROR_ACCESS_DENIED)
			{
				initProfileConfig();
				moveSettings();
			}
		}
	}
	else
	{
		initProfileConfig();
	}
#else // USE_APPDATA
	paths[PATH_USER_CONFIG] = paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
#endif //USE_APPDATA    
	paths[PATH_LANGUAGES] = paths[PATH_GLOBAL_CONFIG] + "Lang" PATH_SEPARATOR_STR;
	paths[PATH_THEMES] = paths[PATH_GLOBAL_CONFIG] + "Themes" PATH_SEPARATOR_STR;
	paths[PATH_SOUNDS] = paths[PATH_GLOBAL_CONFIG] + "Sounds" PATH_SEPARATOR_STR;
	
	if (!File::isAbsolute(paths[PATH_USER_CONFIG]))
	{
		paths[PATH_USER_CONFIG] = paths[PATH_GLOBAL_CONFIG] + paths[PATH_USER_CONFIG];
	}
	
	paths[PATH_USER_CONFIG] = validateFileName(paths[PATH_USER_CONFIG]);
	paths[PATH_USER_LOCAL] = paths[PATH_USER_CONFIG];
	
	paths[PATH_DOWNLOADS] = localMode ? paths[PATH_USER_CONFIG] + "Downloads" PATH_SEPARATOR_STR : getDownloadPath(CompatibilityManager::getDefaultPath());
	paths[PATH_WEB_SERVER] = paths[PATH_EXE] + "WEBserver" PATH_SEPARATOR_STR;

	paths[PATH_FILE_LISTS] = paths[PATH_USER_LOCAL] + "FileLists" PATH_SEPARATOR_STR;
	paths[PATH_HUB_LISTS] = paths[PATH_USER_LOCAL] + "HubLists" PATH_SEPARATOR_STR;
	paths[PATH_NOTEPAD] = paths[PATH_USER_CONFIG] + "Notepad.txt";
	paths[PATH_EMOPACKS] = paths[PATH_GLOBAL_CONFIG] + "EmoPacks" PATH_SEPARATOR_STR;
	
	for (int i = 0; i < PATH_LAST; ++i)
		paths[i].shrink_to_fit();
	
	for (int i = 0; i < SYS_PATH_LAST; ++i)
		sysPaths[i].shrink_to_fit();
	
	File::ensureDirectory(paths[PATH_USER_CONFIG]);
	File::ensureDirectory(getTempPath());
}

void Util::migrate(const string& file) noexcept
{
	if (localMode)
		return;
	if (File::getSize(file) != -1)
		return;
	string fname = getFileName(file);
	string old = paths[PATH_GLOBAL_CONFIG] + "Settings\\" + fname;
	if (File::getSize(old) == -1)
		return;
	LogManager::message("Util::migrate old = " + old + " new = " + file, false);
	File::renameFile(old, file);
}
	
void Util::loadBootConfig()
{
	// Load boot settings
	try
	{
		SimpleXML boot;
		boot.fromXML(File(getPath(PATH_GLOBAL_CONFIG) + "dcppboot.xml", File::READ, File::OPEN).read());
		boot.stepIn();
	
		if (boot.findChild("LocalMode"))
		{
			localMode = boot.getChildData() != "0";
		}
		boot.resetCurrentChild();
		if (boot.findChild("ConfigPath"))
		{
			StringMap params;
#ifdef _WIN32
			string s = getSysPath(APPDATA);
			removePathSeparator(s);
			params["APPDATA"] = std::move(s);

			s = getSysPath(LOCAL_APPDATA);
			removePathSeparator(s);
			params["LOCAL_APPDATA"] = std::move(s);

			s = getSysPath(COMMON_APPDATA);
			removePathSeparator(s);
			params["COMMON_APPDATA"] = std::move(s);

			s = getSysPath(PERSONAL);
			removePathSeparator(s);
			params["PERSONAL"] = std::move(s);

			s = getSysPath(PROGRAM_FILESX86);
			removePathSeparator(s);
			params["PROGRAM_FILESX86"] = std::move(s);

			s = getSysPath(PROGRAM_FILES);
			removePathSeparator(s);
			params["PROGRAM_FILES"] = std::move(s);
#endif
			paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			appendPathSeparator(paths[PATH_USER_CONFIG]);
		}
	}
	catch (const Exception&)
	{
	}
}
	
#ifdef _WIN32
static const char badChars[] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, '<', '>', '/', '"', '|', '?', '*', 0
};
#else
	
static const char badChars[] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, '<', '>', '\\', '"', '|', '?', '*', 0
};
#endif

/**
 * Replaces all strange characters in a file with '_'
 * @todo Check for invalid names such as nul and aux...
 */
string Util::validateFileName(string tmp)
{
	string::size_type i = 0;
	
	// First, eliminate forbidden chars
	while ((i = tmp.find_first_of(badChars, i)) != string::npos)
	{
		tmp[i] = '_';
		i++;
	}
	
	// Then, eliminate all ':' that are not the second letter ("c:\...")
	i = 0;
	while ((i = tmp.find(':', i)) != string::npos)
	{
		if (i == 1)
		{
			i++;
			continue;
		}
		tmp[i] = '_';
		i++;
	}
	
	// Remove the .\ that doesn't serve any purpose
	i = 0;
	while ((i = tmp.find("\\.\\", i)) != string::npos)
	{
		tmp.erase(i + 1, 2);
	}
	i = 0;
	while ((i = tmp.find("/./", i)) != string::npos)
	{
		tmp.erase(i + 1, 2);
	}
	
	// Remove any double \\ that are not at the beginning of the path...
	i = 1;
	while ((i = tmp.find("\\\\", i)) != string::npos)
	{
		tmp.erase(i + 1, 1);
	}
	i = 1;
	while ((i = tmp.find("//", i)) != string::npos)
	{
		tmp.erase(i + 1, 1);
	}
	
	// And last, but not least, the infamous ..\! ...
	i = 0;
	while (((i = tmp.find("\\..\\", i)) != string::npos))
	{
		tmp[i + 1] = '_';
		tmp[i + 2] = '_';
		tmp[i + 3] = '_';
		i += 2;
	}
	i = 0;
	while (((i = tmp.find("/../", i)) != string::npos))
	{
		tmp[i + 1] = '_';
		tmp[i + 2] = '_';
		tmp[i + 3] = '_';
		i += 2;
	}
	
	// Dots at the end of path names aren't popular
	i = 0;
	while (((i = tmp.find(".\\", i)) != string::npos))
	{
		if (i != 0)
			tmp[i] = '_';
		i += 1;
	}
	i = 0;
	while (((i = tmp.find("./", i)) != string::npos))
	{
		if (i != 0)
			tmp[i] = '_';
		i += 1;
	}
	
	
	return tmp;
}
	
string Util::cleanPathChars(string aNick)
{
	string::size_type i = 0;
	
	while ((i = aNick.find_first_of("/.\\", i)) != string::npos)
	{
		aNick[i] = '_';
	}
	return aNick;
}

string Util::ellipsizePath(const string& path)
{
	static const size_t MAX_LEN = 80;
	if (path.length() < MAX_LEN) return path;
	string::size_type pos = path.rfind(PATH_SEPARATOR);
	if (pos == string::npos || pos == 0) return path;
	string result = path.substr(pos);
	while (pos > 0)
	{
		string::size_type nextPos = path.rfind(PATH_SEPARATOR, pos-1);
		if (nextPos == string::npos || result.length() + pos - nextPos > MAX_LEN - 3) break;
		result.insert(0, path, nextPos, pos - nextPos);
		pos = nextPos;
	}
	result.insert(0, "...");
	return result;
}
	
string Util::getShortTimeString(time_t t)
{
	TimeParamExpander ex(t);
	return formatParams(SETTING(TIME_STAMPS_FORMAT), &ex, false);
}
	
/**
 * Decodes a URL the best it can...
 * Default ports:
 * http:// -> port 80
 * https:// -> port 443
 * dchub:// -> port 411
 */
void Util::decodeUrl(const string& url, string& protocol, string& host, uint16_t& port, string& path, bool& isSecure, string& query, string& fragment)
{
	auto fragmentEnd = url.size();
	auto fragmentStart = url.rfind('#');
	
	size_t queryEnd;
	if (fragmentStart == string::npos)
	{
		queryEnd = fragmentStart = fragmentEnd;
	}
	else
	{
		queryEnd = fragmentStart;
		fragmentStart++;
	}
	
	auto queryStart = url.rfind('?', queryEnd);
	size_t fileEnd;
	
	if (queryStart == string::npos)
	{
		fileEnd = queryStart = queryEnd;
	}
	else
	{
		fileEnd = queryStart;
		queryStart++;
	}
	
	size_t protoStart = 0;
	auto protoEnd = url.find("://", protoStart);
	
	auto authorityStart = protoEnd == string::npos ? protoStart : protoEnd + 3;
	auto authorityEnd = url.find_first_of("/#?", authorityStart);
	
	size_t fileStart;
	if (authorityEnd == string::npos)
		authorityEnd = fileStart = fileEnd;
	else
		fileStart = authorityEnd;
	
	protocol = (protoEnd == string::npos ? Util::emptyString : Text::toLower(url.substr(protoStart, protoEnd - protoStart))); // [!] IRainman rfc fix lower string to proto and servername
	if (protocol.empty())
		protocol = "dchub";
	
	if (authorityEnd > authorityStart)
	{
		size_t portStart = string::npos;
		if (url[authorityStart] == '[')
		{
			// IPv6?
			auto hostEnd = url.find(']');
			if (hostEnd == string::npos)
			{
				return;
			}
	
			host = url.substr(authorityStart + 1, hostEnd - authorityStart - 1);
			if (hostEnd + 1 < url.size() && url[hostEnd + 1] == ':')
			{
				portStart = hostEnd + 2;
			}
		}
		else
		{
			size_t hostEnd;
			portStart = url.find(':', authorityStart);
			if (portStart != string::npos && portStart > authorityEnd)
			{
				portStart = string::npos;
			}
	
			if (portStart == string::npos)
			{
				hostEnd = authorityEnd;
			}
			else
			{
				hostEnd = portStart;
				portStart++;
			}
			host = Text::toLower(url.substr(authorityStart, hostEnd - authorityStart));
		}
	
		if (portStart == string::npos)
		{
			if (protocol == "dchub" || protocol == "nmdc" || protocol == "adc")
			{
				port = 411;
			}
			else if (protocol == "nmdcs")
			{
				isSecure = true;
				port = 411;
			}
			else if (protocol == "adcs")
			{
				isSecure = true;
				port = 412;
			}
			else if (protocol == "http")
			{
				port = 80;
			}
			else if (protocol == "https")
			{
				port = 443;
				isSecure = true;
			}
			else port = 0;
		}
		else
		{
			port = static_cast<uint16_t>(Util::toInt(url.substr(portStart, authorityEnd - portStart)));
		}
	}
	
	path = url.substr(fileStart, fileEnd - fileStart);
	query = url.substr(queryStart, queryEnd - queryStart);
	fragment = url.substr(fragmentStart, fragmentEnd - fragmentStart);
	if (!Text::isAscii(host))
	{
		wstring wstr;
		Text::utf8ToWide(host, wstr);
		Text::toLower(wstr);
		int error;
		string converted;
		if (IDNA_convert_to_ACE(wstr, converted, error))
			host = converted;
	}
}
	
void Util::parseIpPort(const string& ipPort, string& ip, uint16_t& port)
{
	string::size_type i = ipPort.rfind(':');
	if (i == string::npos)
	{
		ip = ipPort;
	}
	else
	{
		ip = ipPort.substr(0, i);
		port = Util::toInt(ipPort.c_str() + i + 1);
	}
}
	
std::map<string, string> Util::decodeQuery(const string& query)
{
	std::map<string, string> ret;
	size_t start = 0;
	while (start < query.size())
	{
		auto eq = query.find('=', start);
		if (eq == string::npos)
			break;
	
		auto param = eq + 1;
		auto end = query.find('&', param);
	
		if (end == string::npos)
			end = query.size();
	
		if (eq > start && end > param)
			ret[query.substr(start, eq - start)] = query.substr(param, end - param);
	
		start = end + 1;
	}
	
	return ret;
}
	
string Util::getQueryParam(const string& query, const string& key)
{
	string value;
	size_t start = 0;
	while (start < query.size())
	{
		auto eq = query.find('=', start);
		if (eq == string::npos)
			break;
	
		auto param = eq + 1;
		auto end = query.find('&', param);
	
		if (end == string::npos)
			end = query.size();
	
		if (eq > start && end > param && query.substr(start, eq - start) == key)
		{
			value = query.substr(param, end - param);
			break;
		}
	
		start = end + 1;
	}
	
	return value;
}
	
void Util::setAway(bool isAway, bool notUpdateInfo /*= false*/)
{
	away = isAway;
	
	SET_SETTING(AWAY, away);
	
	if (away)
		awayTime = time(nullptr);
	else
		awayMsg.clear();
	
	if (!notUpdateInfo)
		ClientManager::infoUpdated();
}

static inline bool checkHour(int hour, int start, int end)
{
	if (start < end) return hour >= start && hour < end;
	if (start > end) return hour >= start || hour < end;
	return false;
}

string Util::getAwayMessage(const string& customMsg, StringMap& params)
{
	time_t currentTime = time(nullptr);
	params["idleTI"] = formatSeconds(currentTime - awayTime);

	if (!awayMsg.empty())
		return formatParams(awayMsg, params, false, awayTime);

	if (!customMsg.empty())
		return formatParams(customMsg, params, false, awayTime);
	
	SettingsManager::StrSetting message = SettingsManager::DEFAULT_AWAY_MESSAGE;
	if (BOOLSETTING(ENABLE_SECONDARY_AWAY))
	{
		if (checkHour(localtime(&currentTime)->tm_hour, SETTING(SECONDARY_AWAY_START), SETTING(SECONDARY_AWAY_END)))
			message = SettingsManager::SECONDARY_AWAY_MESSAGE;
	}
	return formatParams(SettingsManager::get(message), params, false, awayTime);
}
	
wstring Util::formatSecondsW(int64_t aSec, bool supressHours /*= false*/) noexcept
{
	wchar_t buf[64];
	if (!supressHours)
		_snwprintf(buf, _countof(buf), L"%01lu:%02u:%02u", unsigned long(aSec / (60 * 60)), unsigned((aSec / 60) % 60), unsigned(aSec % 60));
	else
		_snwprintf(buf, _countof(buf), L"%02u:%02u", unsigned(aSec / 60), unsigned(aSec % 60));
	return buf;
}
	
string Util::formatSeconds(int64_t aSec, bool supressHours /*= false*/) noexcept
{
	char buf[64];
	if (!supressHours)
		_snprintf(buf, _countof(buf), "%01lu:%02u:%02u", unsigned long(aSec / (60 * 60)), unsigned((aSec / 60) % 60), unsigned(aSec % 60));
	else
		_snprintf(buf, _countof(buf), "%02u:%02u", unsigned(aSec / 60), unsigned(aSec % 60));
	return buf;
}
	
template<typename size_type>
inline string formatBytesTemplate(size_type bytes)
{
	char buf[512];
	if (bytes < 1024)
		_snprintf(buf, sizeof(buf), "%d %s", (int) bytes, CSTRING(B));
	else if (bytes < 1048576)
		_snprintf(buf, sizeof(buf), "%.02f %s", (double) bytes / 1024.0, CSTRING(KB));
	else if (bytes < 1073741824)
		_snprintf(buf, sizeof(buf), "%.02f %s", (double) bytes / 1048576.0, CSTRING(MB));
	else if (bytes < (size_type) 1099511627776)
		_snprintf(buf, sizeof(buf), "%.02f %s", (double) bytes / 1073741824.0, CSTRING(GB));
	else if (bytes < (size_type) 1125899906842624)
		_snprintf(buf, sizeof(buf), "%.03f %s", (double) bytes / 1099511627776.0, CSTRING(TB));
	else if (bytes < (size_type) 1152921504606846976)
		_snprintf(buf, sizeof(buf), "%.03f %s", (double) bytes / 1125899906842624.0, CSTRING(PB));
	else
		_snprintf(buf, sizeof(buf), "%.03f %s", (double) bytes / 1152921504606846976.0, CSTRING(EB));
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

template<typename size_type>
inline wstring formatBytesWTemplate(size_type bytes)
{
	wchar_t buf[512];
	if (bytes < 1024)
		_snwprintf(buf, _countof(buf), L"%d %s", (int) bytes, CWSTRING(B));
	else if (bytes < 1048576)
		_snwprintf(buf, _countof(buf), L"%.02f %s", (double) bytes / 1024.0, CWSTRING(KB));
	else if (bytes < 1073741824)
		_snwprintf(buf, _countof(buf), L"%.02f %s", (double) bytes / 1048576.0, CWSTRING(MB));
	else if (bytes < (size_type) 1099511627776)
		_snwprintf(buf, _countof(buf), L"%.02f %s", (double) bytes / 1073741824.0, CWSTRING(GB));
	else if (bytes < (size_type) 1125899906842624)
		_snwprintf(buf, _countof(buf), L"%.03f %s", (double) bytes / 1099511627776.0, CWSTRING(TB));
	else if (bytes < (size_type) 1152921504606846976)
		_snwprintf(buf, _countof(buf), L"%.03f %s", (double) bytes / 1125899906842624.0, CWSTRING(PB));
	else
		_snwprintf(buf, _countof(buf), L"%.03f %s", (double) bytes / 1152921504606846976.0, CWSTRING(EB));
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
	
wstring Util::formatExactSize(int64_t bytes)
{
#ifdef _WIN32
	wchar_t strNum[64];
	_snwprintf(strNum, _countof(strNum), _T(I64_FMT), bytes);
	wchar_t formatted[128];
	GetNumberFormat(LOCALE_USER_DEFAULT, 0, strNum, &g_nf, formatted, _countof(formatted));
	wstring result = formatted;
	result += L' ';
	result += WSTRING(B);
	return result;
#else
	wchar_t buf[64];
	_snwprintf(buf, _countof(buf), _T(I64_FMT), bytes);
	return wstring(buf) + WSTRING(B);
#endif
}

string Util::toString(const char* sep, const StringList& lst)
{
	string ret;
	for (StringList::size_type i = 0; i != lst.size(); ++i)
	{
		if (i) ret += sep;
		ret += lst[i];
	}
	return ret;
}

string Util::toString(char sep, const StringList& lst)
{
	string ret;
	for (StringList::size_type i = 0; i != lst.size(); ++i)
	{
		if (i) ret += sep;
		ret += lst[i];
	}
	return ret;
}

string Util::toString(const StringList& lst)
{
	if (lst.empty())
		return emptyString;
	if (lst.size() == 1)
		return lst[0];
	string tmp("[");
	for (auto i = lst.cbegin(), iend = lst.cend(); i != iend; ++i)
	{
		tmp += *i + ',';
	}
	if (tmp.length() == 1)
		tmp.push_back(']');
	else
		tmp[tmp.length() - 1] = ']';
	return tmp;
}

static inline int getHex(char c)
{
	if (c >= '0' && c <= '9') return c-'0';
	if (c >= 'a' && c <= 'f') return c-'a'+10;
	if (c >= 'A' && c <= 'F') return c-'A'+10;
	return -1;
}
	
string Util::encodeURI(const string& str, bool reverse)
{
	// reference: rfc2396
	string tmp;
	tmp.reserve(str.length());
	if (reverse)
	{
		// TODO idna: convert host name from punycode
		for (string::size_type i = 0; i < str.length(); ++i)
		{
			if (str[i] == '%' && i + 2 < str.length())
			{
				int h1 = getHex(str[i+1]);
				int h2 = getHex(str[i+2]);
				if (h1 != -1 && h2 != -1)
				{
					tmp += h1 << 4 | h2;
					i += 2;
					continue;
				}
			}
			else if (str[i] == '+')
			{
				tmp += ' ';
				continue;
			}
			tmp += str[i];
		}
	}
	else
	{
		static const string disallowed = ";/?:@&=+$," // reserved
		                                 "<>#%\" "    // delimiters
		                                 "{}|\\^[]`"; // unwise
		static const char* hexDigits = "0123456789ABCDEF";
		char escape[4];
		for (string::size_type i = 0; i < str.length(); ++i)
		{
			if (str[i] == ' ')
			{
				tmp += '+';
			}
			else if (str[i] <= 0x1F || str[i] >= 0x7F || disallowed.find(str[i]) != string::npos)
			{
				escape[0] = '%';
				escape[1] = hexDigits[(str[i] >> 4) & 0xF];
				escape[2] = hexDigits[str[i] & 0xF];
				tmp.append(escape, 3);
			}
			else
				tmp += str[i];
		}
	}
	return tmp;
}
	
#if 0
uint64_t Util::getDirSize(const string &sFullPath)
{
	uint64_t total = 0;
	
	WIN32_FIND_DATA fData;
	HANDLE hFind = FindFirstFileEx(Text::toT(sFullPath + "\\*").c_str(),
	                               CompatibilityManager::findFileLevel,
	                               &fData,
	                               FindExSearchNameMatch,
	                               nullptr,
	                               CompatibilityManager::findFileFlags);
	
	if (hFind != INVALID_HANDLE_VALUE)
	{
		const string l_tmp_path = SETTING(TEMP_DOWNLOAD_DIRECTORY);
		do
		{
			if ((fData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) && !BOOLSETTING(SHARE_HIDDEN))
				continue;
			const string name = Text::fromT(fData.cFileName);
			if (isReservedDirName(name))
				continue;
			if (fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				const string newName = sFullPath + PATH_SEPARATOR + name;
				// TODO TEMP_DOWNLOAD_DIRECTORY может содержать шаблон "[targetdrive]" сравнивать с ним не всегда верно
				if (stricmp(newName + PATH_SEPARATOR, l_tmp_path) != 0)
				{
					total += getDirSize(newName);
				}
			}
			else
			{
				total += (uint64_t)fData.nFileSizeLow | ((uint64_t)fData.nFileSizeHigh) << 32;
			}
		}
		while (FindNextFile(hFind, &fData));
		FindClose(hFind);
	}
	return total;
}
#endif

bool Util::validatePath(const string &sPath)
{
	if (sPath.empty())
		return false;
	
	if ((sPath.substr(1, 2) == ":\\") || (sPath.substr(0, 2) == "\\\\"))
	{
		if (GetFileAttributes(Text::toT(sPath).c_str()) & FILE_ATTRIBUTE_DIRECTORY)
			return true;
	}
	
	return false;
}

string Util::getFilenameForRenaming(const string& filename)
{
	string outFilename;
	const string ext = getFileExt(filename);
	const string fname = getFileName(filename);
	int i = 0;
	do
	{
		i++;
		outFilename = filename.substr(0, filename.length() - fname.length());
		outFilename += fname.substr(0, fname.length() - ext.length());
		outFilename += '(' + Util::toString(i) + ')';
		outFilename += ext;
	}
	while (File::isExist(outFilename));
	
	return outFilename;
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
	
/* Below is a high-speed random number generator with much
   better granularity than the CRT one in msvc...(no, I didn't
   write it...see copyright) */
/* Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.
   Any feedback is very welcome. For any question, comments,
   see http://www.math.keio.ac.jp/matumoto/emt.html or email
   matumoto@math.keio.ac.jp */
/* Period parameters */
	
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */
	
/* Tempering parameters */
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)
	
static std::vector<unsigned long> g_mt(N + 1); /* the array for the state vector  */
static int g_mti = N + 1; /* mti==N+1 means mt[N] is not initialized */
	
/* initializing the array with a NONZERO seed */
static void initRand()
{
	RAND_pseudo_bytes((unsigned char*) &g_mt[0], (N + 1)*sizeof(unsigned long));
	g_mti = N;
}
	
uint32_t Util::rand()
{
	unsigned long y;
	/* mag01[x] = x * MATRIX_A  for x=0,1 */
	
	if (g_mti >= N)   /* generate N words at one time */
	{
		static unsigned long mag01[2] = {0x0, MATRIX_A};
		int kk;
	
		if (g_mti == N + 1)
			initRand();
	
		for (kk = 0; kk < N - M; kk++)
		{
			y = (g_mt[kk] & UPPER_MASK) | (g_mt[kk + 1] & LOWER_MASK);
			g_mt[kk] = g_mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		for (; kk < N - 1; kk++)
		{
			y = (g_mt[kk] & UPPER_MASK) | (g_mt[kk + 1] & LOWER_MASK);
			g_mt[kk] = g_mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		y = (g_mt[N - 1] & UPPER_MASK) | (g_mt[0] & LOWER_MASK);
		g_mt[N - 1] = g_mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1];
	
		g_mti = 0;
	}
	
	y = g_mt[g_mti++];
	y ^= TEMPERING_SHIFT_U(y);
	y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
	y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
	y ^= TEMPERING_SHIFT_L(y);
	
	return y;
}
	
string Util::getRandomNick(size_t maxLength /*= 20*/)
{
	static const char  samples[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	static const char* samples3[] =
	{
		"Bear",
		"Cool", "Cow",
		"Dolly", "DCman",
		"Eagle", "Earth",
		"Fire",
		"Hawk", "Head", "Hulk",
		"Indy",
		"Joker",
		"Man", "Moon", "Monkey",
		"Rabbit",
		"Smile", "Sun",
		"Troll",
		"User",
		"Water"
	};
	
	string name = samples3[Util::rand(_countof(samples3))];
	name += '_';
	
	for (size_t i = Util::rand(3, 7); i; --i)
		name += samples[Util::rand(sizeof(samples)-1)];
	
	if (name.length() > maxLength)
		name.resize(maxLength);
	
	return name;
}

string Util::toAdcFile(const string& file)
{
	if (file == "files.xml.bz2" || file == "files.xml")
		return file;
	
	string ret;
	ret.reserve(file.length() + 1);
	ret += '/';
	ret += file;
	for (string::size_type i = 0; i < ret.length(); ++i)
	{
		if (ret[i] == '\\')
		{
			ret[i] = '/';
		}
	}
	return ret;
}

string Util::toNmdcFile(const string& file)
{
	if (file.empty())
		return Util::emptyString;
	
	string ret(file.substr(1));
	for (string::size_type i = 0; i < ret.length(); ++i)
	{
		if (ret[i] == '/')
		{
			ret[i] = '\\';
		}
	}
	return ret;
}
	
/* natural sorting */

int Util::defaultSort(const wchar_t *a, const wchar_t *b, bool noCase /*=  true*/)
{
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	return CompareStringW(LOCALE_INVARIANT,
		(noCase? NORM_IGNORECASE : 0) | CompatibilityManager::compareFlags,
		a, -1, b, -1) - 2;
#else
	return CompareStringEx(LOCALE_NAME_INVARIANT, 
		(noCase? LINGUISTIC_IGNORECASE : 0) | CompatibilityManager::compareFlags,
		a, -1, b, -1, nullptr, nullptr, 0) - 2;
#endif
}

int Util::defaultSort(const wstring& a, const wstring& b, bool noCase /*=  true*/)
{
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	return CompareStringW(LOCALE_INVARIANT, 
		(noCase? NORM_IGNORECASE : 0) | CompatibilityManager::compareFlags,
		a.c_str(), a.length(), b.c_str(), b.length()) - 2;
#else
	return CompareStringEx(LOCALE_NAME_INVARIANT, 
		(noCase? LINGUISTIC_IGNORECASE : 0) | CompatibilityManager::compareFlags,
		a.c_str(), a.length(), b.c_str(), b.length(), nullptr, nullptr, 0) - 2;
#endif
}

void Util::setLimiter(bool aLimiter)
{
	SET_SETTING(THROTTLE_ENABLE, aLimiter);
	ClientManager::infoUpdated();
}

bool Util::getTTH(const string& filename, bool isAbsPath, size_t bufSize, std::atomic_bool& stopFlag, TigerTree& tree, unsigned maxLevels)
{       	
	AutoArray<uint8_t> buf(bufSize);
	try
	{
		File f(filename, File::READ, File::OPEN, isAbsPath);
		int64_t blockSize = maxLevels ? TigerTree::calcBlockSize(f.getSize(), maxLevels) : TigerTree::getMaxBlockSize(f.getSize());
		tree.setBlockSize(blockSize);
		if (f.getSize() > 0)
		{
			size_t n;
			while ((n = f.read(buf.data(), bufSize)) != 0)
			{
				tree.update(buf.data(), n);
				if (stopFlag.load())
				{
					f.close();
					tree = TigerTree(tree.getFileSize(), tree.getBlockSize(), TTHValue());
					return false;
				}
			}
		}
		f.close();
		tree.finalize();
		return true;
	}
	catch (const FileException&) {}
	return false;
}

string Util::formatDchubUrl(const string& url)
{
	if (url.empty() || url == "DHT") return url;

	uint16_t port = 0;
	string proto, host, file, query, fragment;
	
	decodeUrl(url, proto, host, port, file, query, fragment);
	return formatDchubUrl(proto, host, port);
}

int Util::getHubProtocol(const string& scheme)
{
	if (scheme == "dchub" || scheme == "nmdc")
		return HUB_PROTOCOL_NMDC;
	if (scheme == "nmdcs")
		return HUB_PROTOCOL_NMDCS;
	if (scheme == "adc")
		return HUB_PROTOCOL_ADC;
	if (scheme == "adcs")
		return HUB_PROTOCOL_ADCS;
	return 0;
}

string Util::formatDchubUrl(const string& proto, const string& host, uint16_t port)
{
	string result;
	if (host.empty()) return result;

	bool isNmdc = false;
	if (proto == "nmdc")
	{
		result += "dchub";
		isNmdc = true;
	}
	else
	{
		result += proto;
		if (proto == "dchub") isNmdc = true;
	}
	result += "://";
	result += host;
	if (port && !(port == 411 && isNmdc))
	{
		result += ':';
		result += Util::toString(port);
	}
	return result;
}
	
string Util::getMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	return "magnet:?xt=urn:tree:tiger:" + aHash.toBase32() + "&xl=" + toString(aSize) + "&dn=" + encodeURI(aFile);
}
	
string Util::getWebMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize)
{
	StringMap params;
	params["magnet"] = getMagnet(aHash, aFile, aSize);
	params["size"] = formatBytes(aSize);
	params["TTH"] = aHash.toBase32();
	params["name"] = aFile;
	return formatParams(SETTING(WMLINK_TEMPLATE), params, false);
}
	
string Util::getDownloadPath(const string& def)
{
	typedef HRESULT(WINAPI * _SHGetKnownFolderPath)(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR* ppszPath);
	static HMODULE shell32 = nullptr;
	if (!shell32)
	{
		shell32 = ::LoadLibrary(_T("Shell32.dll"));
		if (shell32)
		{
			_SHGetKnownFolderPath getKnownFolderPath = (_SHGetKnownFolderPath)::GetProcAddress(shell32, "SHGetKnownFolderPath");
			if (getKnownFolderPath)
			{
				PWSTR path = nullptr;
				// Defined in KnownFolders.h.
				static const GUID downloads = {0x374de290, 0x123f, 0x4565, {0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b}};
				if (getKnownFolderPath(downloads, 0, NULL, &path) == S_OK)
				{
					const string ret = Text::wideToUtf8(path) + "\\";
					::CoTaskMemFree(path);
					return ret;
				}
			}
			::FreeLibrary(shell32);
		}
	}
	
	return def + "Downloads\\";
}

StringList Util::splitSettingAndLower(const string& patternList, bool trimSpace)
{
	StringList sl = StringTokenizer<string>(Text::toLower(patternList), ';').getTokens();
	if (trimSpace)
		for (string& s : sl)
			boost::algorithm::trim(s);
	return sl;
}
	
string Util::getLang()
{
	string lang = SETTING(LANGUAGE_FILE);
	if (lang.length() != 9 || !Text::isAsciiSuffix2(lang, string(".xml")))
		return string();
	lang.erase(2);
	return lang;
}
	
string Util::getIETFLang()
{
	string lang = SETTING(LANGUAGE_FILE);
	if (lang.length() != 9 || !Text::isAsciiSuffix2(lang, string(".xml")))
		return string();	
	lang.erase(5);
	return lang;
}
	
string Util::getTempPath()
{
	LocalArray<TCHAR, MAX_PATH> buf;
	DWORD x = GetTempPath(MAX_PATH, buf.data());
	return Text::fromT(tstring(buf.data(), static_cast<size_t>(x))); // [!] PVS V106 Implicit type conversion second argument 'x' of function 'tstring' to memsize type. util.h 558
}

bool Util::isTorrentFile(const tstring& file)
{
	static const tstring ext = _T(".torrent");
	return checkFileExt(file, ext);
}

bool Util::isDclstFile(const tstring& file)
{
	return isDclstFile(Text::fromT(file));
}

bool Util::isDclstFile(const string& file)
{
	static const string ext1 = ".dcls";
	static const string ext2 = ".dclst";
	return checkFileExt(file, ext1) || checkFileExt(file, ext2);
}

bool Util::isAdc(const string& hubUrl)
{
	return _strnicmp("adc://", hubUrl.c_str(), 6) == 0;
}

bool Util::isAdcS(const string& hubUrl)
{
	return _strnicmp("adcs://", hubUrl.c_str(), 7) == 0;
}

bool Util::isMagnetLink(const char* url)
{
	return _strnicmp(url, "magnet:?", 8) == 0;
}

bool Util::isMagnetLink(const string& url)
{
	return _strnicmp(url.c_str(), "magnet:?", 8) == 0;
}

bool Util::isMagnetLink(const wchar_t* url)
{
	return _wcsnicmp(url, L"magnet:?", 8) == 0;
}

bool Util::isMagnetLink(const wstring& url)
{
	return _wcsnicmp(url.c_str(), L"magnet:?", 8) == 0;
}

bool Util::isTorrentLink(const tstring& url)
{
	return url.find(_T("xt=urn:btih:")) != tstring::npos &&
	       url.find(_T("xt=urn:tree:tiger:")) == tstring::npos;
}

bool Util::isHttpLink(const string& url)
{
	return strnicmp(url.c_str(), "http://", 7) == 0 ||
	       strnicmp(url.c_str(), "https://", 8) == 0;
}

bool Util::isHttpLink(const wstring& url)
{
	return _wcsnicmp(url.c_str(), L"http://", 7) == 0 ||
	       _wcsnicmp(url.c_str(), L"https://", 8) == 0;
}

template<typename string_type>
bool parseIpAddress(uint32_t& result, const string_type& s, typename string_type::size_type start, typename string_type::size_type end)
{
	uint32_t byte = 0;
	uint32_t bytes = 0;
	bool digitFound = false;
	int dotCount = 0;
	result = 0;
	while (start < end)
	{
		if (s[start] >= '0' && s[start] <= '9')
		{
			byte = byte * 10 + s[start] - '0';
			if (byte > 255) return false;
			digitFound = true;
		}
		else
		if (s[start] == '.')
		{
			if (!digitFound || ++dotCount == 4) return false;
			bytes = bytes << 8 | byte;
			byte = 0;
			digitFound = false;
		}
		else return false;
		++start;
	}
	if (dotCount != 3 || !digitFound) return false;
	result = bytes << 8 | byte;
	return true;
}

bool Util::parseIpAddress(uint32_t& result, const string& s, string::size_type start, string::size_type end)
{
	return ::parseIpAddress(result, s, start, end);
}

bool Util::parseIpAddress(uint32_t& result, const wstring& s, wstring::size_type start, wstring::size_type end)
{
	return ::parseIpAddress(result, s, start, end);
}

uint32_t Util::getNumericIp4(const tstring& s)
{
	uint32_t result;
	return (parseIpAddress(result, s, 0, s.length()) && result != 0xFFFFFFFF) ? result : 0;
}

bool Util::isValidIp4(const string& ip)
{
	uint32_t result;
	return parseIpAddress(result, ip, 0, ip.length()) && result && result != 0xFFFFFFFF;
}

bool Util::isValidIp4(const wstring& ip)
{
	uint32_t result;
	return parseIpAddress(result, ip, 0, ip.length()) && result && result != 0xFFFFFFFF;
}

void Util::readTextFile(File& file, std::function<bool(const string&)> func)
{
	static const size_t BUF_SIZE = 256 * 1024;
	unique_ptr<char[]> buf(new char[BUF_SIZE]);
	size_t writePtr = 0;
	bool eof = false;
	while (!eof)
	{
		size_t size = BUF_SIZE - writePtr;
		file.read(buf.get() + writePtr, size);
		writePtr += size;
		if (!size) eof = true;

		size_t readPtr = 0;
		while (readPtr < writePtr)
		{
			size_t endPtr;
			char* ptr = static_cast<char*>(memchr(buf.get() + readPtr, '\n', writePtr - readPtr));
			if (!ptr)
			{
				if (!eof) break;
				endPtr = writePtr;
			}
			else
				endPtr = ptr - buf.get();
			size_t nextPtr = endPtr + 1;
			while (endPtr > readPtr && (buf[endPtr-1] == '\r' || buf[endPtr-1] == '\n')) endPtr--;
			string s(buf.get() + readPtr, endPtr - readPtr);
			if (!func(s))
				throw Exception("Bad input line");
			readPtr = nextPtr;
		}
		if (readPtr)
		{
			if (readPtr < writePtr)
			{
				writePtr -= readPtr;
				memmove(buf.get(), buf.get() + readPtr, writePtr);
			}
			else
				writePtr = 0;
		}
		else
		if (writePtr == BUF_SIZE)
			throw Exception("Buffer overflow");
	}
}
