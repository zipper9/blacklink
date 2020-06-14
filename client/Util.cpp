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

#ifdef _WIN32
#include <iphlpapi.h>
#endif

const time_t Util::g_startTime = time(nullptr);

bool Util::g_away = false;
string Util::g_awayMsg;
time_t Util::g_awayTime;

string Util::g_paths[Util::PATH_LAST];
string Util::g_sysPaths[Util::SYS_PATH_LAST];
NUMBERFMT Util::g_nf = { 0 };
bool Util::g_localMode = true;

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
	const string& path = g_sysPaths[sysPath];
	return !path.empty() && strnicmp(currentPath, path, path.size()) == 0;
}

void Util::initProfileConfig()
{
	g_paths[PATH_USER_CONFIG] = getSysPath(APPDATA) + APPNAME PATH_SEPARATOR_STR;
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
	g_paths[PATH_ALL_USER_CONFIG] = getSysPath(COMMON_APPDATA) + APPNAME PATH_SEPARATOR_STR;
# endif
}

static const string g_configFileLists[] =
{
	"ADLSearch.xml",
	"DCPlusPlus.xml",
	"Favorites.xml",
	"IPTrust.ini",
#ifdef SSA_IPGRANT_FEATURE
	"IPGrant.ini",
#endif
	"IPGuard.ini",
	"Queue.xml"
};

static void copySettings(const string& sourcePath, const string& destPath)
{
	File::ensureDirectory(destPath);
	for (size_t i = 0; i < _countof(g_configFileLists); ++i)
	{
		string sourceFile = sourcePath + g_configFileLists[i];
		string destFile = destPath + g_configFileLists[i];
		if (!File::isExist(destFile) && File::isExist(sourceFile))
		{
			if (!File::copyFile(sourceFile, destFile))
				LogManager::message("Error copying " + sourceFile + " to " + destFile + ": " + Util::translateError());
		}
	}
}

void Util::moveSettings()
{
	copySettings(g_paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR, g_paths[PATH_USER_CONFIG]);
}

void Util::backupSettings()
{
	copySettings(getConfigPath(),
		formatDateTime(getConfigPath() + "Backup" PATH_SEPARATOR_STR "%Y-%m-%d" PATH_SEPARATOR_STR, time(nullptr)));
}

string Util::getModuleCustomFileName(const string& fileName)
{
	string path = Util::getFilePath(Text::fromT(Util::getModuleFileName()));
	path += fileName;
	return path;
}

tstring Util::getModuleFileName()
{
	static tstring g_module_file_name;
	if (g_module_file_name.empty())
	{
		LocalArray<TCHAR, MAX_PATH> buf;
		const DWORD x = GetModuleFileName(NULL, buf.data(), MAX_PATH);
		g_module_file_name = tstring(buf.data(), x);
	}
	return g_module_file_name;
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
	
	g_paths[PATH_EXE] = Util::getModuleCustomFileName("");
	TCHAR buf[MAX_PATH];
#define SYS_WIN_PATH_INIT(path) \
	if(::SHGetFolderPath(NULL, CSIDL_##path, NULL, SHGFP_TYPE_CURRENT, buf) == S_OK) \
	{ \
		g_sysPaths[path] = Text::fromT(buf) + PATH_SEPARATOR; \
	}
	
	//LogManager::message("Sysytem Path: " + g_sysPaths[path]);
	//LogManager::message("Error SHGetFolderPath: GetLastError() = " + Util::toString(GetLastError()));
	
	SYS_WIN_PATH_INIT(WINDOWS);
	SYS_WIN_PATH_INIT(PROGRAM_FILESX86);
	SYS_WIN_PATH_INIT(PROGRAM_FILES);
	if (CompatibilityManager::runningIsWow64())
	{
		// [!] Correct PF path on 64 bit system with run 32 bit programm.
		const char* l_PFW6432 = getenv("ProgramW6432");
		if (l_PFW6432)
		{
			g_sysPaths[PROGRAM_FILES] = string(l_PFW6432) + PATH_SEPARATOR;
		}
	}
	SYS_WIN_PATH_INIT(APPDATA);
	SYS_WIN_PATH_INIT(LOCAL_APPDATA);
	SYS_WIN_PATH_INIT(COMMON_APPDATA);
	SYS_WIN_PATH_INIT(PERSONAL);
	
#undef SYS_WIN_PATH_INIT
	
	// Global config path is FlylinkDC++ executable path...
	g_paths[PATH_GLOBAL_CONFIG] = g_paths[PATH_EXE];
#ifdef USE_APPDATA
	if (File::isExist(g_paths[PATH_EXE] + "Settings" PATH_SEPARATOR_STR "DCPlusPlus.xml") ||
	        !(locatedInSysPath(PROGRAM_FILES, g_paths[PATH_EXE]) || locatedInSysPath(PROGRAM_FILESX86, g_paths[PATH_EXE]))
	   )
	{
		// Check if settings directory is writable
		g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
		const auto tempFile = g_paths[PATH_USER_CONFIG] + ".test-writable-" + Util::toString(Util::rand()) + ".tmp";
		try
		{
			{
				File f(tempFile, File::WRITE, File::CREATE | File::TRUNCATE);
			}
			File::deleteFile(tempFile);
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
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		g_paths[PATH_ALL_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
# endif
	}
	else
	{
		initProfileConfig();
	}
#else // USE_APPDATA
	g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + "Settings" PATH_SEPARATOR_STR;
#endif //USE_APPDATA    
	g_paths[PATH_LANGUAGES] = g_paths[PATH_GLOBAL_CONFIG] + "Lang" PATH_SEPARATOR_STR;
	g_paths[PATH_THEMES] = g_paths[PATH_GLOBAL_CONFIG] + "Themes" PATH_SEPARATOR_STR;
	g_paths[PATH_SOUNDS] = g_paths[PATH_GLOBAL_CONFIG] + "Sounds" PATH_SEPARATOR_STR;
	
	loadBootConfig();
	
	if (!File::isAbsolute(g_paths[PATH_USER_CONFIG]))
	{
		g_paths[PATH_USER_CONFIG] = g_paths[PATH_GLOBAL_CONFIG] + g_paths[PATH_USER_CONFIG];
	}
	
	g_paths[PATH_USER_CONFIG] = validateFileName(g_paths[PATH_USER_CONFIG]);
	
	if (g_localMode)
	{
		g_paths[PATH_USER_LOCAL] = g_paths[PATH_USER_CONFIG];
	}
	else
	{
		if (!getSysPath(PERSONAL).empty())
		{
			g_paths[PATH_USER_CONFIG] = getSysPath(PERSONAL) + APPNAME PATH_SEPARATOR_STR;
		}
	
		g_paths[PATH_USER_LOCAL] = !getSysPath(PERSONAL).empty() ? getSysPath(PERSONAL) + APPNAME PATH_SEPARATOR_STR : g_paths[PATH_USER_CONFIG];
	}
	
	g_paths[PATH_DOWNLOADS] = getDownloadPath(CompatibilityManager::getDefaultPath());
//	g_paths[PATH_RESOURCES] = exePath;
	g_paths[PATH_WEB_SERVER] = g_paths[PATH_EXE] + "WEBserver" PATH_SEPARATOR_STR;
	
	g_paths[PATH_FILE_LISTS] = g_paths[PATH_USER_LOCAL] + "FileLists" PATH_SEPARATOR_STR;
	g_paths[PATH_HUB_LISTS] = g_paths[PATH_USER_LOCAL] + "HubLists" PATH_SEPARATOR_STR;
	g_paths[PATH_NOTEPAD] = g_paths[PATH_USER_CONFIG] + "Notepad.txt";
	g_paths[PATH_EMOPACKS] = g_paths[PATH_GLOBAL_CONFIG] + "EmoPacks" PATH_SEPARATOR_STR;
	
	for (int i = 0; i < PATH_LAST; ++i)
		g_paths[i].shrink_to_fit();
	
	for (int i = 0; i < SYS_PATH_LAST; ++i)
		g_sysPaths[i].shrink_to_fit();
	
	File::ensureDirectory(g_paths[PATH_USER_CONFIG]);
	File::ensureDirectory(g_paths[PATH_USER_LOCAL]);
	File::ensureDirectory(getTempPath()); // airdc++
}

void Util::migrate(const string& file) noexcept
{
	if (g_localMode)
		return;
	if (File::getSize(file) != -1)
		return;
	string fname = getFileName(file);
	string old = g_paths[PATH_GLOBAL_CONFIG] + "Settings\\" + fname;
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
			g_localMode = boot.getChildData() != "0";
		}
		boot.resetCurrentChild();
		StringMap params;
#ifdef _WIN32
		// @todo load environment variables instead? would make it more useful on *nix
	
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
	
		if (boot.findChild("ConfigPath"))
		{
	
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA //[+] NightOrion
			g_paths[PATH_ALL_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			appendPathSeparator(g_paths[PATH_ALL_USER_CONFIG]);
#endif
			g_paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			appendPathSeparator(g_paths[PATH_USER_CONFIG]);
		}
#ifdef USE_APPDATA //[+] NightOrion
# ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		boot.resetCurrentChild();
	
		if (boot.findChild("SharedConfigPath"))
		{
			g_paths[PATH_ALL_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			appendPathSeparator(g_paths[PATH_ALL_USER_CONFIG]);
		}
# endif
		boot.resetCurrentChild();
	
		if (boot.findChild("UserConfigPath"))
		{
			g_paths[PATH_USER_CONFIG] = formatParams(boot.getChildData(), params, false);
			appendPathSeparator(g_paths[PATH_USER_CONFIG]);
		}
#endif
	}
	catch (const Exception&)
	{
		//-V565
		// Unable to load boot settings...
	}
}
	
#ifdef _WIN32
static const char g_badChars[] =
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, '<', '>', '/', '"', '|', '?', '*', 0
};
#else
	
static const char g_badChars[] =
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
	while ((i = tmp.find_first_of(g_badChars, i)) != string::npos)
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
		dcdebug("f");
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
		dcdebug("q");
		fileEnd = queryStart;
		queryStart++;
	}
	
	size_t protoStart = 0;
	auto protoEnd = url.find("://", protoStart);
	
	auto authorityStart = protoEnd == string::npos ? protoStart : protoEnd + 3;
	auto authorityEnd = url.find_first_of("/#?", authorityStart);
	
	size_t fileStart;
	if (authorityEnd == string::npos)
	{
		authorityEnd = fileStart = fileEnd;
	}
	else
	{
		dcdebug("a");
		fileStart = authorityEnd;
	}
	
	protocol = (protoEnd == string::npos ? Util::emptyString : Text::toLower(url.substr(protoStart, protoEnd - protoStart))); // [!] IRainman rfc fix lower string to proto and servername
	if (protocol.empty())
		protocol = "dchub";
	
	if (authorityEnd > authorityStart)
	{
		dcdebug("x");
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
	
			dcdebug("h");
			host = Text::toLower(url.substr(authorityStart, hostEnd - authorityStart)); // [!] IRainman rfc fix lower string to proto and servername
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
			dcdebug("p");
			port = static_cast<uint16_t>(Util::toInt(url.substr(portStart, authorityEnd - portStart)));
		}
	}
	
	dcdebug("\n");
	path = url.substr(fileStart, fileEnd - fileStart);
	query = url.substr(queryStart, queryEnd - queryStart);
	fragment = url.substr(fragmentStart, fragmentEnd - fragmentStart);  //http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2606
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
	
void Util::setAway(bool away, bool notUpdateInfo /*= false*/)
{
	g_away = away;
	
	SET_SETTING(AWAY, away);
	
	if (away)
		g_awayTime = time(nullptr);
	
	if (!notUpdateInfo)
		ClientManager::infoUpdated();
}

static inline bool checkHour(int hour, int start, int end)
{
	if (start < end) return hour >= start && hour < end;
	return false;
}

string Util::getAwayMessage(StringMap& params)
{
	time_t currentTime = time(nullptr);
	params["idleTI"] = formatSeconds(currentTime - g_awayTime);
	
	SettingsManager::StrSetting message = SettingsManager::DEFAULT_AWAY_MESSAGE;
	if (BOOLSETTING(ENABLE_SECONDARY_AWAY))
	{
		int currentHour = localtime(&currentTime)->tm_hour;
		int start = SETTING(SECONDARY_AWAY_START);
		int end = SETTING(SECONDARY_AWAY_END);
		if (start < end && currentHour >= start && currentHour < end)
			message = SettingsManager::SECONDARY_AWAY_MESSAGE;
	}
	const string& msg = SettingsManager::get(message);
	return formatParams(g_awayMsg.empty() ? msg : g_awayMsg, params, false, g_awayTime);
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

static string findBindIP(const string& tmp, const string& p_gateway_mask, const bool p_check_bind_address, sockaddr_in& dest, const hostent* he)
{
	for (int i = 1; he->h_addr_list[i]; ++i)
	{
		memcpy(&dest.sin_addr, he->h_addr_list[i], he->h_length);
		const string tmp2 = inet_ntoa(dest.sin_addr);
		if (p_check_bind_address && tmp2 == SETTING(BIND_ADDRESS))
			return tmp2;
		if (tmp2.find(p_gateway_mask) != string::npos)
		{
			if (Util::isPrivateIp(tmp2)) // ѕроблема с Hamachi
			{
				return tmp2;
			}
		}
		if (tmp2 == "192.168.56.1") // Virtual Box ?
		{
			continue;
		}
	}
	return tmp;
}

string Util::getLocalOrBindIp(const bool p_check_bind_address)
{
	string tmp;
	char buf[256];
	if (!gethostname(buf, 255)) // двойной вызов
	{
		string l_gateway_ip = Socket::getDefaultGateway();
		const auto l_dot = l_gateway_ip.rfind('.');
		if (l_dot != string::npos)
		{
			l_gateway_ip = l_gateway_ip.substr(0, l_dot + 1);
		}
		else
		{
			l_gateway_ip.clear();
		}
		const hostent* he = gethostbyname(buf);
		if (he == nullptr || he->h_addr_list[0] == 0)
			return Util::emptyString;
		sockaddr_in dest  = { { 0 } };
		// We take the first ip as default, but if we can find a better one, use it instead...
		memcpy(&dest.sin_addr, he->h_addr_list[0], he->h_length);
		tmp = inet_ntoa(dest.sin_addr);
		if (p_check_bind_address && tmp == SETTING(BIND_ADDRESS))
		{
			return tmp;
		}
		if (Util::isPrivateIp(tmp) || strncmp(tmp.c_str(), "169", 3) == 0)
		{
			const auto l_bind_address = findBindIP(tmp, l_gateway_ip, p_check_bind_address, dest, he);
			if (!l_bind_address.empty())
			{
				return l_bind_address;
			}
		}
	}
	return tmp;
}
	
bool Util::isPrivateIp(const string& ip)
{
	dcassert(!ip.empty());
	struct in_addr addr = {0};
	addr.s_addr = inet_addr(ip.c_str());
	if (addr.s_addr != INADDR_NONE)
	{
		const uint32_t haddr = ntohl(addr.s_addr);
		return isPrivateIp(haddr);
	}
	return false;
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
	
string Util::encodeURI(const string& aString, bool reverse)
{
	// reference: rfc2396
	string tmp = aString;
	if (reverse)
	{
		// TODO idna: convert host name from punycode
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp.length() > idx + 2 && tmp[idx] == '%' && isxdigit(tmp[idx + 1]) && isxdigit(tmp[idx + 2]))
			{
				tmp[idx] = fromHexEscape(tmp.substr(idx + 1, 2));
				tmp.erase(idx + 1, 2);
			}
			else   // reference: rfc1630, magnet-uri draft
			{
				if (tmp[idx] == '+')
					tmp[idx] = ' ';
			}
		}
	}
	else
	{
		static const string disallowed = ";/?:@&=+$," // reserved
		                                 "<>#%\" "    // delimiters
		                                 "{}|\\^[]`"; // unwise
		string::size_type idx;
		for (idx = 0; idx < tmp.length(); ++idx)
		{
			if (tmp[idx] == ' ')
			{
				tmp[idx] = '+';
			}
			else
			{
				if (tmp[idx] <= 0x1F || tmp[idx] >= 0x7f || (disallowed.find_first_of(tmp[idx])) != string::npos)
				{
					tmp.replace(idx, 1, toHexEscape(tmp[idx]));
					idx += 2;
				}
			}
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
	                               CompatibilityManager::g_find_file_level,
	                               &fData,
	                               FindExSearchNameMatch,
	                               nullptr,
	                               CompatibilityManager::g_find_file_flags);
	
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

string Util::getFilenameForRenaming(const string& p_filename)
{
	string outFilename;
	const string ext = getFileExt(p_filename);
	const string fname = getFileName(p_filename);
	int i = 0;
	do
	{
		i++;
		outFilename = p_filename.substr(0, p_filename.length() - fname.length());
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
	if (withSeconds && i <= 2 && rest)
	{
		if (i) result += ' ';
		result += toString(n);
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
		"Jocker",
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
		name += samples[Util::rand(_countof(samples))];
	
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
	
TCHAR* Util::strstr(const TCHAR *str1, const TCHAR *str2, int *pnIdxFound)
{
	TCHAR *s1, *s2;
	TCHAR *cp = const_cast<TCHAR*>(str1);
	if (!*str2)
		return const_cast<TCHAR*>(str1);
	int nIdx = 0;
	while (*cp)
	{
		s1 = cp;
		s2 = (TCHAR *) str2;
		while (*s1 && *s2 && !(*s1 - *s2))
			s1++, s2++;
		if (!*s2)
		{
			if (pnIdxFound != NULL)
				*pnIdxFound = nIdx;
			return cp;
		}
		cp++;
		nIdx++;
	}
	if (pnIdxFound != NULL)
		*pnIdxFound = -1;
	return nullptr;
}
	
/* natural sorting */
	
int Util::defaultSort(const wchar_t *a, const wchar_t *b, bool noCase /*=  true*/)
{
	return CompareStringEx(LOCALE_NAME_INVARIANT, 
		(noCase? LINGUISTIC_IGNORECASE : 0) | SORT_DIGITSASNUMBERS,
		a, -1, b, -1, NULL, NULL, 0) - 2;
}

int Util::defaultSort(const wstring& a, const wstring& b, bool noCase /*=  true*/)
{
	return CompareStringEx(LOCALE_NAME_INVARIANT, 
		(noCase? LINGUISTIC_IGNORECASE : 0) | SORT_DIGITSASNUMBERS,
		a.c_str(), a.length(), b.c_str(), b.length(), NULL, NULL, 0) - 2;
}

void Util::setLimiter(bool aLimiter)
{
	SET_SETTING(THROTTLE_ENABLE, aLimiter);
	ClientManager::infoUpdated();
}

void Util::getNetworkAdapters(bool v6, vector<AdapterInfo>& adapterInfos) noexcept
{
	adapterInfos.clear();
#ifdef _WIN32
	ULONG len = 15360;
	for (int i = 0; i < 3; ++i)
	{
		uint8_t* infoBuf = new uint8_t[len];
		PIP_ADAPTER_ADDRESSES adapterInfo = (PIP_ADAPTER_ADDRESSES) infoBuf;
		ULONG ret = GetAdaptersAddresses(v6 ? AF_INET6 : AF_INET, GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST, NULL, adapterInfo, &len);

		if (ret == ERROR_SUCCESS)
		{
			for (PIP_ADAPTER_ADDRESSES pAdapterInfo = adapterInfo; pAdapterInfo != NULL; pAdapterInfo = pAdapterInfo->Next)
			{
				// we want only enabled ethernet interfaces
				if (pAdapterInfo->OperStatus == IfOperStatusUp && (pAdapterInfo->IfType == IF_TYPE_ETHERNET_CSMACD || pAdapterInfo->IfType == IF_TYPE_IEEE80211))
				{
					PIP_ADAPTER_UNICAST_ADDRESS ua;
					for (ua = pAdapterInfo->FirstUnicastAddress; ua != NULL; ua = ua->Next)
					{
						//get the name of the adapter
						char buf[512];
						memset(buf, 0, sizeof(buf));
						if (!getnameinfo(ua->Address.lpSockaddr, ua->Address.iSockaddrLength, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST))
							adapterInfos.emplace_back(pAdapterInfo->FriendlyName, buf, ua->OnLinkPrefixLength);
					}
				}
			}
			delete[] infoBuf;
			return;
		}

		delete[] infoBuf;
		if (ret != ERROR_BUFFER_OVERFLOW)
			break;
	}
#endif
}
	
string Util::getWANIP(const string& p_url, LONG p_timeOut /* = 500 */)
{
#if 0 // FIXME FIXME FIXME
	CFlyLog l_log("[GetIP]");
	string l_downBuf;
	getDataFromInet(false, p_url, l_downBuf, p_timeOut);
	if (!l_downBuf.empty())
	{
		SimpleXML xml;
		try
		{
			xml.fromXML(l_downBuf);
			if (xml.findChild("html"))
			{
				xml.stepIn();
				if (xml.findChild("body"))
				{
					const string l_IP = xml.getChildData().substr(20);
					l_log.step("Download : " + p_url + " IP = " + l_IP);
					if (isValidIp4(l_IP))
					{
						return l_IP;
					}
					else
					{
						dcassert(0);
					}
				}
			}
		}
		catch (SimpleXMLException & e)
		{
			l_log.step(string("Error parse XML: ") + e.what());
		}
		catch (std::exception& e)
		{
			l_log.step(string("std::exception: ") + e.what()); // fix https://drdump.com/Problem.aspx?ProblemID=300455
		}
	}
	else
		l_log.step("Error download : " + Util::translateError());
#endif
	return Util::emptyString;
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
	if (!(port == 411 && isNmdc))
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
	
StringList Util::splitSettingAndReplaceSpace(string patternList)
{
	patternList.erase(std::remove(patternList.begin(), patternList.end(), ' '), patternList.end());
	return splitSettingAndLower(patternList);
}
	
string Util::toSettingString(const StringList& patternList)
{
	string ret;
	for (auto i = patternList.cbegin(), iend = patternList.cend(); i != iend; ++i)
	{
		ret += *i + ';';
	}
	if (!ret.empty())
	{
		ret.resize(ret.size() - 1);
	}
	return ret;
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
