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
#include "LogManager.h"
#include "Util.h"
#include "StrUtil.h"
#include "TimeUtil.h"
#include "SettingsManager.h"
#include "ParamExpander.h"
#include "ConfCore.h"

#ifndef NO_RESOURCE_MANAGER
#include "ResourceManager.h"
#endif

#ifdef _WIN32
#include "ClientManager.h"
#endif

static const int FILE_TIMEOUT     = 240*1000; // 4 min
static const int CLOSE_FILES_TIME = 300*1000; // 5 min

bool LogManager::g_isInit = false;
int  LogManager::g_LogMessageID = 0;
bool LogManager::g_isLogSpeakerEnabled = false;
int64_t LogManager::nextCloseTime = 0;
std::atomic_int LogManager::options(0);

#ifdef _WIN32
HWND LogManager::g_mainWnd = nullptr;
#endif

LogManager::LogArea LogManager::types[LogManager::LAST];

void LogManager::init()
{
	types[UPLOAD].fileOption            = Conf::LOG_FILE_UPLOAD;
	types[UPLOAD].formatOption          = Conf::LOG_FORMAT_UPLOAD;
	types[DOWNLOAD].fileOption          = Conf::LOG_FILE_DOWNLOAD;
	types[DOWNLOAD].formatOption        = Conf::LOG_FORMAT_DOWNLOAD;
	types[CHAT].fileOption              = Conf::LOG_FILE_MAIN_CHAT;
	types[CHAT].formatOption            = Conf::LOG_FORMAT_MAIN_CHAT;
	types[PM].fileOption                = Conf::LOG_FILE_PRIVATE_CHAT;
	types[PM].formatOption              = Conf::LOG_FORMAT_PRIVATE_CHAT;
	types[SYSTEM].fileOption            = Conf::LOG_FILE_SYSTEM;
	types[SYSTEM].formatOption          = Conf::LOG_FORMAT_SYSTEM;
	types[STATUS].fileOption            = Conf::LOG_FILE_STATUS;
	types[STATUS].formatOption          = Conf::LOG_FORMAT_STATUS;
	types[WEBSERVER].fileOption         = Conf::LOG_FILE_WEBSERVER;
	types[WEBSERVER].formatOption       = Conf::LOG_FORMAT_WEBSERVER;

	types[SQLITE_TRACE].fileOption      = Conf::LOG_FILE_SQLITE_TRACE;
	types[SQLITE_TRACE].formatOption    = Conf::LOG_FORMAT_SQLITE_TRACE;
	types[SEARCH_TRACE].fileOption      = Conf::LOG_FILE_SEARCH_TRACE;
	types[SEARCH_TRACE].formatOption    = Conf::LOG_FORMAT_SEARCH_TRACE;
	types[DHT_TRACE].fileOption         = Conf::LOG_FILE_DHT_TRACE;
	types[DHT_TRACE].formatOption       = Conf::LOG_FORMAT_DHT_TRACE;
	types[PSR_TRACE].fileOption         = Conf::LOG_FILE_PSR_TRACE;
	types[PSR_TRACE].formatOption       = Conf::LOG_FORMAT_PSR_TRACE;
	types[FLOOD_TRACE].fileOption       = Conf::LOG_FILE_FLOOD_TRACE;
	types[FLOOD_TRACE].formatOption     = Conf::LOG_FORMAT_FLOOD_TRACE;
#ifdef FLYLINKDC_USE_TORRENT
	types[TORRENT_TRACE].fileOption     = Conf::LOG_FILE_TORRENT_TRACE;
	types[TORRENT_TRACE].formatOption   = Conf::LOG_FORMAT_TORRENT_TRACE;
#endif

	types[TCP_MESSAGES].fileOption      = Conf::LOG_FILE_TCP_MESSAGES;
	types[TCP_MESSAGES].formatOption    = Conf::LOG_FORMAT_TCP_MESSAGES;
	types[UDP_PACKETS].fileOption       = Conf::LOG_FILE_UDP_PACKETS;
	types[UDP_PACKETS].formatOption     = Conf::LOG_FORMAT_UDP_PACKETS;

	g_isInit = true;
}

LogManager::LogManager()
{
}

string LogManager::getLogFileName(int area, const StringMap& params) noexcept
{
	dcassert(area >= 0 && area < LAST);
	//string path = SETTING(LOG_DIRECTORY);
	LogArea& la = types[area];
	la.cs.lock();
	//string filenameTemplate = Conf::get((Conf::StrSetting) types[area].fileOption);
	string path = la.logDirectory;
	path += Util::validateFileName(Util::formatParams(la.filenameTemplate, params, true));
	la.cs.unlock();
	return path;
}

void LogManager::updateSettings() noexcept
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string logDirectory = ss->getString(Conf::LOG_DIRECTORY);
	int newOptions = 0;
	if (ss->getBool(Conf::LOG_SYSTEM))
	{
		newOptions |= OPT_LOG_SYSTEM;
		if (ss->getBool(Conf::LOG_SOCKET_INFO))
			newOptions |= OPT_LOG_SOCKET_INFO;
	}
	if (ss->getBool(Conf::LOG_STATUS_MESSAGES))
		newOptions |= OPT_LOG_STATUS;
	if (ss->getBool(Conf::LOG_TCP_MESSAGES))
		newOptions |= OPT_LOG_TCP_MESSAGES;
	if (ss->getBool(Conf::LOG_UDP_PACKETS))
		newOptions |= OPT_LOG_UDP_PACKETS;
	if (ss->getBool(Conf::LOG_TLS_CERTIFICATES))
		newOptions |= OPT_LOG_CERTIFICATES;
	if (ss->getBool(Conf::LOG_PSR_TRACE))
		newOptions |= OPT_LOG_PSR;
	if (ss->getBool(Conf::LOG_DHT_TRACE))
		newOptions |= OPT_LOG_DHT;
	if (ss->getBool(Conf::LOG_SEARCH_TRACE))
		newOptions |= OPT_LOG_SEARCH;
	if (ss->getBool(Conf::LOG_SQLITE_TRACE))
		newOptions |= OPT_LOG_SQLITE;
	if (ss->getBool(Conf::LOG_WEBSERVER))
		newOptions |= OPT_LOG_WEB_SERVER;
	ss->unlockRead();
	options.store(newOptions);

	for (int area = 0; area < LAST; ++area)
	{
		LogArea& la = types[area];
		ss->lockRead();
		string filenameTemplate = ss->getString(la.fileOption);
		ss->unlockRead();
		la.cs.lock();
		la.logDirectory = logDirectory;
		la.filenameTemplate = std::move(filenameTemplate);
		la.cs.unlock();
	}
}

void LogManager::logRaw(int area, const string& msg, Util::ParamExpander* ex) noexcept
{
	dcassert(area >= 0 && area < LAST);
	LogArea& la = types[area];
	la.cs.lock();
	string path = la.logDirectory;
	path += Util::validateFileName(Util::formatParams(la.filenameTemplate, ex, true));
	if (path.empty())
	{
		dcdebug("Empty log path for %d\n", area);
		la.cs.unlock();
		return;
	}
	try
	{
		auto& lf = la.files[path];
		if (!lf.file.isOpen())
		{
			File::ensureDirectory(path);
			lf.file.init(Text::toT(path), File::WRITE, File::OPEN | File::CREATE, true);
			// move to the end of file
			if (lf.file.setEndPos(0) == 0 && area != TCP_MESSAGES && area != UDP_PACKETS)
				lf.file.write("\xef\xbb\xbf");
		}
		lf.file.write(msg);
		lf.timeout = GET_TICK() + FILE_TIMEOUT;
	}
	catch (...)
	{
	}
	la.cs.unlock();
}

void LogManager::log(int area, Util::ParamExpander* ex) noexcept
{
	dcassert(area >= 0 && area < LAST);
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string msg = Util::formatParams(ss->getString(types[area].formatOption), ex, false);
	ss->unlockRead();
	size_t len = msg.length();
	while (len && (msg[len-1] == '\n' || msg[len-1] == '\r')) len--;
	msg.erase(len);
	// Multiline messages will start with a newline
	if (msg.find('\n') != string::npos) msg.insert(0, 1, '\n');
#ifdef _WIN32
	msg += "\r\n";
#else
	msg += '\n';
#endif
	logRaw(area, msg, ex);
}

#ifndef NO_RESOURCE_MANAGER
class MapAndResourceParamExpander : public Util::MapParamExpander
{
	public:
		MapAndResourceParamExpander(const StringMap& m, time_t t) : MapParamExpander(m, t)
		{
		}

		virtual const string& expandBracket(const string& str, string::size_type pos, string::size_type endPos) noexcept override
		{
			if (endPos - pos > 1 && str[pos] == '@')
			{
				string param = str.substr(pos + 1, endPos - (pos + 1));
				int id = ResourceManager::getStringByName(param);
				if (id != -1)
					return ResourceManager::getString((ResourceManager::Strings) id);
				return Util::emptyString;
			}
			return MapParamExpander::expandBracket(str, pos, endPos);
		}
};
#endif

class TraceMessageExpander : public Util::TimeParamExpander
{
		const string& msg;
		const string& ipPort;
		const string& ip;

	public:
		TraceMessageExpander(const string& msg, const string& ipPort, const string& ip, time_t t) :
			Util::TimeParamExpander(t), msg(msg), ipPort(ipPort), ip(ip) {}
		virtual const string& expandBracket(const string& str, string::size_type pos, string::size_type endPos) noexcept override
		{
			string param = str.substr(pos, endPos - pos);
			if (param == "message") return msg;
			if (param == "ipPort") return ipPort;
			if (param == "IP" || param == "ip") return ip;
#ifndef NO_RESOURCE_MANAGER
			if (param.length() > 1 && param[0] == '@')
			{
				param.erase(0, 1);
				int id = ResourceManager::getStringByName(param);
				if (id != -1)
					return ResourceManager::getString((ResourceManager::Strings) id);
			}
#endif
			return Util::emptyString;
		}
};

void LogManager::log(int area, const StringMap& params) noexcept
{
#ifndef NO_RESOURCE_MANAGER
	MapAndResourceParamExpander ex(params, time(nullptr));
#else
	Util::MapParamExpander ex(params, time(nullptr));
#endif
	log(area, &ex);
}

void LogManager::log(int area, const string& msg) noexcept
{
	TraceMessageExpander ex(msg, Util::emptyString, Util::emptyString, time(nullptr));
	log(area, &ex);
}

void LogManager::closeOldFiles(int64_t now) noexcept
{
	if (now < nextCloseTime) return;
	nextCloseTime = now + CLOSE_FILES_TIME;
	for (int i = 0; i < LAST; ++i)
	{
		LogArea& la = types[i];
		la.cs.lock();
		auto j = la.files.cbegin();
		while (j != la.files.cend())
		{
			if (j->second.timeout < now)
				la.files.erase(j++);
			else
				j++;
		}
		la.cs.unlock();
	}
}

void LogManager::getOptions(int area, TStringPair& p) noexcept
{
	dcassert(area >= 0 && area < LAST);
	const LogArea& la = types[area];
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	p.first = Text::toT(ss->getString(la.fileOption));
	p.second = Text::toT(ss->getString(la.formatOption));
	ss->unlockRead();
}

void LogManager::setOptions(int area, const TStringPair& p) noexcept
{
	dcassert(area >= 0 && area < LAST);
	const LogArea& la = types[area];
	string filename = Text::fromT(p.first);
	string format = Text::fromT(p.second);
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	ss->setString(la.fileOption, filename);
	ss->setString(la.formatOption, format);
	ss->unlockWrite();
}

void LogManager::commandTrace(const char* msg, size_t msgLen, int flags, const string& ip, int port) noexcept
{
	if (flags & FLAG_UDP)
	{
		if (!(getLogOptions() & OPT_LOG_UDP_PACKETS)) return;
	}
	else
	{
		if (!(getLogOptions() & OPT_LOG_TCP_MESSAGES)) return;
	}
	string msgFull = (flags & FLAG_IN)? "Recv from " : "Sent to   ";
	string ipPort = ip + ':' + Util::toString(port);
	msgFull += ipPort;
	msgFull += ": ";
	msgFull.append(msg, msgLen);
	TraceMessageExpander ex(msgFull, ipPort, ip, time(nullptr));
	log((flags & FLAG_UDP) ? UDP_PACKETS : TCP_MESSAGES, &ex);
}

void LogManager::speakStatusMessage(const string& message) noexcept
{
#ifdef _WIN32
	if (LogManager::g_isLogSpeakerEnabled && LogManager::g_mainWnd && !ClientManager::isBeforeShutdown())
	{
		size_t len = std::min<size_t>(message.length(), 255);
		char* data = new char[len + 1];
		memcpy(data, message.c_str(), len);
		data[len] = 0;
		if (!::PostMessage(LogManager::g_mainWnd, WM_SPEAKER, g_LogMessageID, reinterpret_cast<LPARAM>(data)))
		{
			// TODO - LOG dcassert(0);
			dcdebug("[LogManager::g_mainWnd] PostMessage error %d\n", GetLastError());
			delete[] data;
			LogManager::g_isLogSpeakerEnabled = false; // Fix error 1816
		}
	}
#endif
}

void LogManager::message(const string& message, bool useStatus) noexcept
{
	if (getLogOptions() & OPT_LOG_SYSTEM)
		log(SYSTEM, message);
	if (useStatus)
		speakStatusMessage(message);
}

string LogManager::getLogDirectory() noexcept
{
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string result = ss->getString(Conf::LOG_DIRECTORY);
	ss->unlockRead();
	return result;
}

StepLogger::StepLogger(const string& message, bool skipStart, bool skipStop) : message(message), skipStop(skipStop)
{
	startTime = stepTime = GET_TICK();
	if (!skipStart && (LogManager::getLogOptions() & LogManager::OPT_LOG_SYSTEM))
		LogManager::log(LogManager::SYSTEM, "[Start] " + message);
}

StepLogger::~StepLogger()
{
	if (!skipStop && (LogManager::getLogOptions() & LogManager::OPT_LOG_SYSTEM))
	{
		uint64_t now = GET_TICK();
		LogManager::log(LogManager::SYSTEM,
			message + " [" + Util::toString(now - stepTime) + " ms, Total: " + Util::toString(now - startTime) + " ms]");
	}
}

void StepLogger::step(const string& what)
{
	uint64_t now = GET_TICK();
	if (LogManager::getLogOptions() & LogManager::OPT_LOG_SYSTEM)
		LogManager::log(LogManager::SYSTEM,
			message + ' ' + what + " [" + Util::toString(now - stepTime) + " ms]");
	stepTime = now;
}
