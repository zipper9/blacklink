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
#include <boost/algorithm/string/trim.hpp>

#include "Util.h"
#include "StrUtil.h"
#include "AppPaths.h"
#include "PathUtil.h"
#include "Random.h"
#include "File.h"
#include "StringTokenizer.h"
#include "ParamExpander.h"
#include "TimeUtil.h"
#include "FormatUtil.h"
#include "SettingsManager.h"
#include "ClientManager.h"
#include "LogManager.h"
#include "CompatibilityManager.h"
#include "version.h"

const time_t Util::startTime = time(nullptr);

bool Util::away = false;
string Util::awayMsg;
time_t Util::awayTime;

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

void Util::initialize()
{
	initRand();

#if defined _MSC_VER && _MSC_VER >= 1400
	_set_invalid_parameter_handler(reinterpret_cast<_invalid_parameter_handler>(invalidParameterHandler));
#endif
	setlocale(LC_ALL, "");

	Util::initAppPaths();
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
string Util::validateFileName(string tmp, bool badCharsOnly)
{
	string::size_type i = 0;
	
	// First, eliminate forbidden chars
	while ((i = tmp.find_first_of(badChars, i)) != string::npos)
	{
		tmp[i] = '_';
		i++;
	}

	if (badCharsOnly) return tmp;

#ifdef _WIN32
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
#endif
	
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

int Util::getCurrentHour()
{
	time_t currentTime = time(nullptr);
#ifdef HAVE_TIME_R
	tm local;
	localtime_r(&currentTime, &local);
	return local.tm_hour;
#else
	return localtime(&currentTime)->tm_hour;
#endif
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
		if (checkHour(getCurrentHour(), SETTING(SECONDARY_AWAY_START), SETTING(SECONDARY_AWAY_END)))
			message = SettingsManager::SECONDARY_AWAY_MESSAGE;
	}
	return formatParams(SettingsManager::get(message), params, false, awayTime);
}

string Util::getDownloadDir(const UserPtr& user)
{
	string s = SETTING(DOWNLOAD_DIRECTORY);
	if (s.find('%') == string::npos) return s;
	return expandDownloadDir(s, user);
}

class UserParamExpander : public Util::TimeParamExpander
{
		const UserPtr& user;
		string value;

	public:
		UserParamExpander(const UserPtr& user, time_t t) : TimeParamExpander(t), user(user) {}
		virtual const string& expandBracket(const string& str, string::size_type pos, string::size_type endPos) noexcept override
		{
			if (user)
			{
				string param = str.substr(pos, endPos - pos);
				if (param == "userNI")
				{
					value = user->getLastNick();
					return value;
				}
				if (param == "userI4")
				{
					value = Util::printIpAddress(user->getIP4());
					return value;
				}
				if (param == "userI6")
				{
					value = Util::printIpAddress(user->getIP6());
					return value;
				}
			}
			return Util::emptyString;
		}
};

string Util::expandDownloadDir(const string& dir, const UserPtr& user)
{
	if (!user)
	{
		auto pos = dir.find('%');
		if (pos == string::npos) return dir;
		pos = dir.rfind(PATH_SEPARATOR, pos);
		return pos == string::npos ? dir : dir.substr(0, pos + 1);
	}
	UserParamExpander upe(user, GET_TIME());
	return Util::formatParams(dir, &upe, true);	
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

struct GetDirSizeControl
{
	std::atomic_bool* abortFlag;
	void* ctx;
	void (*progressFunc)(void*, int64_t);
};

static uint64_t getDirSizeInternal(string& path, GetDirSizeControl& info)
{
	uint64_t size = 0;	
	size_t pathLen = path.length();
	path += "*";
	for (FileFindIter i(path); i != FileFindIter::end; ++i)
	{
		if (*info.abortFlag) break;
		const string& fileName = i->getFileName();
		if (Util::isReservedDirName(fileName) || fileName.empty())
			continue;
		if (i->isTemporary())
			continue;
		if (i->isDirectory())
		{
			path.erase(pathLen);
			path += fileName;
			path += PATH_SEPARATOR;
			size += getDirSizeInternal(path, info);
		}
		else
		{
			int64_t fileSize = i->getSize();
			if (info.progressFunc) info.progressFunc(info.ctx, fileSize);
			size += fileSize;
		}
	}
	return size;
}

uint64_t Util::getDirSize(const string& path, std::atomic_bool& abortFlag, void (*progressFunc)(void* ctx, int64_t size), void* ctx)
{
	if (path.empty())
		return (uint64_t) -1;
	string tmp = path;
	if (tmp.back() != PATH_SEPARATOR)
		tmp += PATH_SEPARATOR;
	GetDirSizeControl gds;
	gds.abortFlag = &abortFlag;
	gds.ctx = ctx;
	gds.progressFunc = progressFunc;
	return getDirSizeInternal(tmp, gds);
}

string Util::getNewFileName(const string& filename)
{
	string outFilename;
	const string fname = getFileName(filename);
	const string ext = getFileExt(fname);
	int i = 0;
	do
	{
		if (i == INT_MAX) return Util::emptyString;
		outFilename = filename.substr(0, filename.length() - fname.length());
		outFilename += fname.substr(0, fname.length() - ext.length());
		outFilename += '(' + Util::toString(++i) + ')';
		outFilename += ext;
	}
	while (File::isExist(outFilename));
	
	return outFilename;
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

string Util::getRandomPassword()
{
	uint8_t data[10];
	for (size_t i = 0; i < sizeof(data); ++i)
		data[i] = (uint8_t) Util::rand();
	return toBase32(data, sizeof(data));
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

#ifdef _WIN32
int Util::defaultSort(const wchar_t *a, const wchar_t *b, bool noCase /*=  true*/)
{
#ifdef OSVER_WIN_XP
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
#ifdef OSVER_WIN_XP
	return CompareStringW(LOCALE_INVARIANT, 
		(noCase? NORM_IGNORECASE : 0) | CompatibilityManager::compareFlags,
		a.c_str(), a.length(), b.c_str(), b.length()) - 2;
#else
	return CompareStringEx(LOCALE_NAME_INVARIANT, 
		(noCase? LINGUISTIC_IGNORECASE : 0) | CompatibilityManager::compareFlags,
		a.c_str(), a.length(), b.c_str(), b.length(), nullptr, nullptr, 0) - 2;
#endif
}
#endif

void Util::setLimiter(bool enable)
{
	SET_SETTING(THROTTLE_ENABLE, enable);
	ClientManager::infoUpdated();
}

string Util::formatDchubUrl(const string& url)
{
	if (url.empty() || url == "DHT") return url;
	ParsedUrl p;
	decodeUrl(url, p);
	return formatDchubUrl(p);
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

string Util::formatDchubUrl(Util::ParsedUrl& p)
{
	p.clearUser();
	p.clearPath();
	bool isNmdc = false;
	if (p.protocol == "nmdc" || p.protocol.empty())
	{
		p.protocol = "dchub";
		isNmdc = true;
	}
	else if (p.protocol == "dchub")
		isNmdc = true;
	if (p.port == 411 && isNmdc)
		p.port = 0;
	return formatUrl(p, false);
}

string Util::getMagnet(const string& hash, const string& file, int64_t size)
{
	return "magnet:?xt=urn:tree:tiger:" + hash + "&xl=" + toString(size) + "&dn=" + encodeUriQuery(file);
}

string Util::getMagnet(const TTHValue& hash, const string& file, int64_t size)
{
	return getMagnet(hash.toBase32(), file, size);
}

string Util::getWebMagnet(const TTHValue& hash, const string& file, int64_t size)
{
	string tthStr = hash.toBase32();
	StringMap params;
	params["magnet"] = getMagnet(tthStr, file, size);
	params["size"] = formatBytes(size);
	params["TTH"] = tthStr;
	params["name"] = file;
	return formatParams(SETTING(WMLINK_TEMPLATE), params, false);
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

bool Util::isTorrentFile(const tstring& file)
{
	static const tstring ext = _T(".torrent");
	return checkFileExt(file, ext);
}

bool Util::isDclstFile(const string& file)
{
	static const string ext1 = ".dcls";
	static const string ext2 = ".dclst";
	return checkFileExt(file, ext1) || checkFileExt(file, ext2);
}

bool Util::isAdc(const string& hubUrl)
{
	return strnicmp("adc://", hubUrl.c_str(), 6) == 0;
}

bool Util::isAdcS(const string& hubUrl)
{
	return strnicmp("adcs://", hubUrl.c_str(), 7) == 0;
}

bool Util::isMagnetLink(const char* url)
{
	return strnicmp(url, "magnet:?", 8) == 0;
}

bool Util::isMagnetLink(const string& url)
{
	return strnicmp(url.c_str(), "magnet:?", 8) == 0;
}

bool Util::isMagnetLink(const wchar_t* url)
{
	return strnicmp(url, L"magnet:?", 8) == 0;
}

bool Util::isMagnetLink(const wstring& url)
{
	return strnicmp(url.c_str(), L"magnet:?", 8) == 0;
}

bool Util::isHttpLink(const string& url)
{
	return strnicmp(url.c_str(), "http://", 7) == 0 ||
	       strnicmp(url.c_str(), "https://", 8) == 0;
}

bool Util::isHttpLink(const wstring& url)
{
	return strnicmp(url.c_str(), L"http://", 7) == 0 ||
	       strnicmp(url.c_str(), L"https://", 8) == 0;
}

void Util::readTextFile(File& file, std::function<bool(const string&)> func)
{
	static const size_t BUF_SIZE = 256 * 1024;
	unique_ptr<char[]> buf(new char[BUF_SIZE]);
	size_t writePtr = 0;
	bool eof = false;
	bool firstLine = true;
	while (!eof)
	{
		size_t size = BUF_SIZE - writePtr;
		file.read(buf.get() + writePtr, size);
		writePtr += size;
		if (!size) eof = true;

		size_t readPtr = 0;
		if (firstLine)
		{
			const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.get());
			if (writePtr >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) readPtr = 3;
			firstLine = false;
		}
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

#ifdef _DEBUG
static uintptr_t mainThreadId;

void ASSERT_MAIN_THREAD()
{
	dcassert(BaseThread::getCurrentThreadId() == mainThreadId);
}

void ASSERT_MAIN_THREAD_INIT()
{
	mainThreadId = BaseThread::getCurrentThreadId();
}
#endif
