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
#include "CFlylinkDBManager.h"
#include "CompatibilityManager.h"
#include "TimerManager.h"
#include "ClientManager.h"

static const int FILE_TIMEOUT     = 240*1000; // 4 min
static const int CLOSE_FILES_TIME = 300*1000; // 5 min

bool LogManager::g_isInit = false;
HWND LogManager::g_mainWnd = nullptr;
int  LogManager::g_LogMessageID = 0;
bool LogManager::g_isLogSpeakerEnabled = false;
int64_t LogManager::nextCloseTime = 0;

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
	types[CUSTOM_LOCATION].fileOption   = SettingsManager::LOG_FILE_CUSTOM_LOCATION;
	types[CUSTOM_LOCATION].formatOption = SettingsManager::LOG_FORMAT_CUSTOM_LOCATION;
	
	types[SQLITE_TRACE].fileOption      = SettingsManager::LOG_FILE_SQLITE_TRACE;
	types[SQLITE_TRACE].formatOption    = SettingsManager::LOG_FORMAT_SQLITE_TRACE;
	types[DDOS_TRACE].fileOption        = SettingsManager::LOG_FILE_DDOS_TRACE;
	types[DDOS_TRACE].formatOption      = SettingsManager::LOG_FORMAT_DDOS_TRACE;
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
	
	if (!CompatibilityManager::getStartupInfo().empty())
	{
		message(CompatibilityManager::getStartupInfo());
	}
	
	if (CompatibilityManager::isIncompatibleSoftwareFound())
	{
		message(CompatibilityManager::getIncompatibleSoftwareMessage());
	}
}

LogManager::LogManager()
{
}

void LogManager::logRaw(int area, const string& msg, const StringMap& params) noexcept
{
	dcassert(area >= 0 && area < LAST);
	LogArea& la = types[area];
	la.cs.lock();
	string path = SETTING(LOG_DIRECTORY);
	string filenameTemplate = SettingsManager::get((SettingsManager::StrSetting) types[area].fileOption);
	path += Util::validateFileName(Util::formatParams(filenameTemplate, params, true));
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

void LogManager::log(int area, const StringMap& params) noexcept
{
	dcassert(area >= 0 && area < LAST);
	string msg = Util::formatParams(SettingsManager::get((SettingsManager::StrSetting) types[area].formatOption, true), params, false);
	size_t len = msg.length();
	while (len && (msg[len-1] == '\n' || msg[len-1] == '\r')) len--;
	msg.erase(len);
	// Multiline messages will start with a newline
	if (msg.find('\n') != string::npos) msg.insert(0, 1, '\n');
	msg += "\r\n";
	logRaw(area, msg, params);
}

void LogManager::log(int area, const string& msg) noexcept
{
	StringMap params;
	params["message"] = msg;
	log(area, params);
}

void LogManager::closeOldFiles() noexcept
{
	int64_t now = GET_TICK();
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

void LogManager::psr_message(const string& message) noexcept
{
	if (BOOLSETTING(LOG_PSR_TRACE))
		LOG(PSR_TRACE, message);
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

void LogManager::commandTrace(const string& msg, int flags, const string& ipPort) noexcept
{
	if (flags & FLAG_UDP)
	{
		if (!BOOLSETTING(LOG_UDP_PACKETS)) return;
	}
	else
	{
		if (!BOOLSETTING(LOG_TCP_MESSAGES)) return;
	}
	StringMap params;	
	string msgFull = (flags & FLAG_IN)? "Recv from " : "Sent to   ";
	msgFull += ipPort;
	msgFull += ": ";
	msgFull += msg;
	params["message"] = msgFull;
	params["ipPort"] = ipPort;
	log((flags & FLAG_UDP) ? UDP_PACKETS : TCP_MESSAGES, params);
}

void LogManager::speakStatusMessage(const string& message) noexcept
{
	if (LogManager::g_isLogSpeakerEnabled && LogManager::g_mainWnd && !ClientManager::isStartup() && !ClientManager::isBeforeShutdown())
	{
		char* data = new char[message.length() + 1];
		memcpy(data, message.c_str(), message.length() + 1); 
		if (!::PostMessage(LogManager::g_mainWnd, WM_SPEAKER, g_LogMessageID, reinterpret_cast<LPARAM>(data)))
		{
			// TODO - LOG dcassert(0);
			dcdebug("[LogManager::g_mainWnd] PostMessage error %d\n", GetLastError());
			delete[] data;
			LogManager::g_isLogSpeakerEnabled = false; // Fix error 1816
		}
	}
}

void LogManager::message(const string& message, bool useStatus) noexcept
{
	if (BOOLSETTING(LOG_SYSTEM))
		log(SYSTEM, message);
	if (useStatus)
		speakStatusMessage(message);
}

CFlyLog::CFlyLog(const string& p_message, bool p_skip_start /* = true */) :
	m_start(GET_TICK()),
	m_message(p_message),
	m_tc(m_start),
	m_skip_start(p_skip_start), // TODO - может оно не нужно?
	m_skip_stop(false)
{
	if (!m_skip_start)
	{
		log("[Start] " + m_message);
	}
}

CFlyLog::~CFlyLog()
{
	if (!m_skip_stop)
	{
		const uint64_t l_current = GET_TICK();
		log("[Stop] " + m_message + " [" + Util::toString(l_current - m_tc) + " ms, Total: " + Util::toString(l_current - m_start) + " ms]");
	}
}

uint64_t CFlyLog::calcSumTime() const
{
	const uint64_t l_current = GET_TICK();
	return l_current - m_start;
}

void CFlyLog::step(const string& p_message_step, const bool p_reset_count /*= true */)
{
	const uint64_t l_current = GET_TICK();
	dcassert(p_message_step.size() == string(p_message_step.c_str()).size());
	log("[Step] " + m_message + ' ' + p_message_step + " [" + Util::toString(l_current - m_tc) + " ms]");
	if (p_reset_count)
		m_tc = l_current;
}
void CFlyLog::loadStep(const string& p_message_step, const bool p_reset_count /*= true */)
{
	const uint64_t l_current = GET_TICK();
	const uint64_t l_step = l_current - m_tc;
	const uint64_t l_total = l_current - m_start;
	
	m_tc = l_current;
	if (p_reset_count)
	{
		log("[Step] " + m_message + " Begin load " + p_message_step + " [" + Util::toString(l_step) + " ms]");
	}
	else
	{
		log("[Step] " + m_message + " End load " + p_message_step + " [" + Util::toString(l_step) + " ms and " + Util::toString(l_total) + " ms after start]");
	}
}
