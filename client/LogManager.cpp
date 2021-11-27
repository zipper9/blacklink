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
#include "SettingsManager.h"
#include "ParamExpander.h"
#include "TimerManager.h"
#include "ClientManager.h"

#ifdef _WIN32
#include "CompatibilityManager.h"
#endif

static const int FILE_TIMEOUT     = 240*1000; // 4 min
static const int CLOSE_FILES_TIME = 300*1000; // 5 min

bool LogManager::g_isInit = false;
int  LogManager::g_LogMessageID = 0;
bool LogManager::g_isLogSpeakerEnabled = false;
int64_t LogManager::nextCloseTime = 0;

#ifdef _WIN32
HWND LogManager::g_mainWnd = nullptr;
#endif

LogManager::LogArea LogManager::types[LogManager::LAST]; 

void LogManager::init()
{
	types[UPLOAD].fileOption            = SettingsManager::LOG_FILE_UPLOAD;
	types[UPLOAD].formatOption          = SettingsManager::LOG_FORMAT_UPLOAD;
	types[DOWNLOAD].fileOption          = SettingsManager::LOG_FILE_DOWNLOAD;
	types[DOWNLOAD].formatOption        = SettingsManager::LOG_FORMAT_DOWNLOAD;
	types[CHAT].fileOption              = SettingsManager::LOG_FILE_MAIN_CHAT;
	types[CHAT].formatOption            = SettingsManager::LOG_FORMAT_MAIN_CHAT;
	types[PM].fileOption                = SettingsManager::LOG_FILE_PRIVATE_CHAT;
	types[PM].formatOption              = SettingsManager::LOG_FORMAT_PRIVATE_CHAT;
	types[SYSTEM].fileOption            = SettingsManager::LOG_FILE_SYSTEM;
	types[SYSTEM].formatOption          = SettingsManager::LOG_FORMAT_SYSTEM;
	types[STATUS].fileOption            = SettingsManager::LOG_FILE_STATUS;
	types[STATUS].formatOption          = SettingsManager::LOG_FORMAT_STATUS;
	types[WEBSERVER].fileOption         = SettingsManager::LOG_FILE_WEBSERVER;
	types[WEBSERVER].formatOption       = SettingsManager::LOG_FORMAT_WEBSERVER;
	
	types[SQLITE_TRACE].fileOption      = SettingsManager::LOG_FILE_SQLITE_TRACE;
	types[SQLITE_TRACE].formatOption    = SettingsManager::LOG_FORMAT_SQLITE_TRACE;
	types[DDOS_TRACE].fileOption        = SettingsManager::LOG_FILE_DDOS_TRACE;
	types[DDOS_TRACE].formatOption      = SettingsManager::LOG_FORMAT_DDOS_TRACE;
	types[SEARCH_TRACE].fileOption      = SettingsManager::LOG_FILE_SEARCH_TRACE;
	types[SEARCH_TRACE].formatOption    = SettingsManager::LOG_FORMAT_SEARCH_TRACE;
	types[DHT_TRACE].fileOption         = SettingsManager::LOG_FILE_DHT_TRACE;
	types[DHT_TRACE].formatOption       = SettingsManager::LOG_FORMAT_DHT_TRACE;
	types[PSR_TRACE].fileOption         = SettingsManager::LOG_FILE_PSR_TRACE;
	types[PSR_TRACE].formatOption       = SettingsManager::LOG_FORMAT_PSR_TRACE;
	types[FLOOD_TRACE].fileOption       = SettingsManager::LOG_FILE_FLOOD_TRACE;
	types[FLOOD_TRACE].formatOption     = SettingsManager::LOG_FORMAT_FLOOD_TRACE;
#ifdef FLYLINKDC_USE_TORRENT
	types[TORRENT_TRACE].fileOption     = SettingsManager::LOG_FILE_TORRENT_TRACE;
	types[TORRENT_TRACE].formatOption   = SettingsManager::LOG_FORMAT_TORRENT_TRACE;
#endif

	types[TCP_MESSAGES].fileOption      = SettingsManager::LOG_FILE_TCP_MESSAGES;
	types[TCP_MESSAGES].formatOption    = SettingsManager::LOG_FORMAT_TCP_MESSAGES;
	types[UDP_PACKETS].fileOption       = SettingsManager::LOG_FILE_UDP_PACKETS;
	types[UDP_PACKETS].formatOption     = SettingsManager::LOG_FORMAT_UDP_PACKETS;
	
	g_isInit = true;
	
#ifdef _WIN32
	if (!CompatibilityManager::getStartupInfo().empty())
	{
		message(CompatibilityManager::getStartupInfo());
	}
	
	if (CompatibilityManager::isIncompatibleSoftwareFound())
	{
		message(CompatibilityManager::getIncompatibleSoftwareMessage());
	}
#endif
}

LogManager::LogManager()
{
}

string LogManager::getLogFileName(int area, const StringMap& params) noexcept
{
	dcassert(area >= 0 && area < LAST);
	string path = SETTING(LOG_DIRECTORY);
	LogArea& la = types[area];
	la.cs.lock();
	string filenameTemplate = SettingsManager::get((SettingsManager::StrSetting) types[area].fileOption);
	path += Util::validateFileName(Util::formatParams(filenameTemplate, params, true));
	la.cs.unlock();
	return path;
}

void LogManager::logRaw(int area, const string& msg, Util::ParamExpander* ex) noexcept
{
	dcassert(area >= 0 && area < LAST);
	string path = SETTING(LOG_DIRECTORY);
	LogArea& la = types[area];
	la.cs.lock();
	string filenameTemplate = SettingsManager::get((SettingsManager::StrSetting) types[area].fileOption);
	path += Util::validateFileName(Util::formatParams(filenameTemplate, ex, true));
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
	string msg = Util::formatParams(SettingsManager::get((SettingsManager::StrSetting) types[area].formatOption, true), ex, false);
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

void LogManager::log(int area, const StringMap& params) noexcept
{
	Util::MapParamExpander ex(params, time(nullptr));
	log(area, &ex);
}

class LogMessageExpander : public Util::TimeParamExpander
{
		const string& msg;
		const string& ipPort;
		const string& ip;

	public:	
		LogMessageExpander(const string& msg, const string& ipPort, const string& ip, time_t t) :
			Util::TimeParamExpander(t), msg(msg), ipPort(ipPort), ip(ip) {}
		virtual const string& expandBracket(const string& param) noexcept override
		{
			if (param == "message") return msg;
			if (param == "ipPort") return ipPort;
			if (param == "IP" || param == "ip") return ip;
			return Util::emptyString;
		}
};

void LogManager::log(int area, const string& msg) noexcept
{
	LogMessageExpander ex(msg, Util::emptyString, Util::emptyString, time(nullptr));
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
	p.first = Text::toT(SettingsManager::get((SettingsManager::StrSetting) la.fileOption, true));
	p.second = Text::toT(SettingsManager::get((SettingsManager::StrSetting) la.formatOption, true));	
}

void LogManager::setOptions(int area, const TStringPair& p) noexcept
{
	dcassert(area >= 0 && area < LAST);
	LogArea& la = types[area];
	string filename = Text::fromT(p.first);
	SettingsManager::set((SettingsManager::StrSetting) la.fileOption, filename);
	SettingsManager::set((SettingsManager::StrSetting) la.formatOption, Text::fromT(p.second));
}

void LogManager::ddos_message(const string& message) noexcept
{
	if (BOOLSETTING(LOG_DDOS_TRACE))
		LOG(DDOS_TRACE, message);
}

void LogManager::flood_message(const string& message) noexcept
{
	if (BOOLSETTING(LOG_FLOOD_TRACE))
		LOG(FLOOD_TRACE, message);
}

#ifdef FLYLINKDC_USE_TORRENT
void LogManager::torrent_message(const string& message, bool addToSystem /*= true*/) noexcept
{
	if (BOOLSETTING(LOG_TORRENT_TRACE))
		LOG(TORRENT_TRACE, message);
	if (addToSystem)
		LOG(SYSTEM, message);
}
#endif

void LogManager::commandTrace(const string& msg, int flags, const string& ip, int port) noexcept
{
	if (flags & FLAG_UDP)
	{
		if (!BOOLSETTING(LOG_UDP_PACKETS)) return;
	}
	else
	{
		if (!BOOLSETTING(LOG_TCP_MESSAGES)) return;
	}
	string msgFull = (flags & FLAG_IN)? "Recv from " : "Sent to   ";
	string ipPort = ip + ':' + Util::toString(port);
	msgFull += ipPort;
	msgFull += ": ";
	msgFull += msg;
	LogMessageExpander ex(msgFull, ipPort, ip, time(nullptr));
	log((flags & FLAG_UDP) ? UDP_PACKETS : TCP_MESSAGES, &ex);
}

void LogManager::speakStatusMessage(const string& message) noexcept
{
#ifdef _WIN32
	if (LogManager::g_isLogSpeakerEnabled && LogManager::g_mainWnd && !ClientManager::isStartup() && !ClientManager::isBeforeShutdown())
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
	if (BOOLSETTING(LOG_SYSTEM))
		log(SYSTEM, message);
	if (useStatus)
		speakStatusMessage(message);
}

StepLogger::StepLogger(const string& message, bool skipStart, bool skipStop) : message(message), skipStop(skipStop)
{
	startTime = stepTime = GET_TICK();
	if (!skipStart && BOOLSETTING(LOG_SYSTEM))
		LogManager::log(LogManager::SYSTEM, "[Start] " + message);
}

StepLogger::~StepLogger()
{
	if (!skipStop && BOOLSETTING(LOG_SYSTEM))
	{
		uint64_t now = GET_TICK();
		LogManager::log(LogManager::SYSTEM,
			message + " [" + Util::toString(now - stepTime) + " ms, Total: " + Util::toString(now - startTime) + " ms]");
	}
}

void StepLogger::step(const string& what)
{
	uint64_t now = GET_TICK();
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::log(LogManager::SYSTEM,
			message + ' ' + what + " [" + Util::toString(now - stepTime) + " ms]");
	stepTime = now;
}
