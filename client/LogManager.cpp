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

bool LogManager::g_isInit = false;
HWND LogManager::g_mainWnd = nullptr;
int  LogManager::g_LogMessageID = 0;
bool LogManager::g_isLogSpeakerEnabled = false;

LogManager::LogFile LogManager::files[LogManager::LAST]; 

void LogManager::init()
{
	files[UPLOAD].fileOption            = SettingsManager::LOG_FILE_UPLOAD;
	files[UPLOAD].formatOption          = SettingsManager::LOG_FORMAT_UPLOAD;
	files[DOWNLOAD].fileOption          = SettingsManager::LOG_FILE_DOWNLOAD;
	files[DOWNLOAD].formatOption        = SettingsManager::LOG_FORMAT_DOWNLOAD;
	files[CHAT].fileOption              = SettingsManager::LOG_FILE_MAIN_CHAT;
	files[CHAT].formatOption            = SettingsManager::LOG_FORMAT_MAIN_CHAT;
	files[PM].fileOption                = SettingsManager::LOG_FILE_PRIVATE_CHAT;
	files[PM].formatOption              = SettingsManager::LOG_FORMAT_PRIVATE_CHAT;
	files[SYSTEM].fileOption            = SettingsManager::LOG_FILE_SYSTEM;
	files[SYSTEM].formatOption          = SettingsManager::LOG_FORMAT_SYSTEM;
	files[STATUS].fileOption            = SettingsManager::LOG_FILE_STATUS;
	files[STATUS].formatOption          = SettingsManager::LOG_FORMAT_STATUS;
	files[WEBSERVER].fileOption         = SettingsManager::LOG_FILE_WEBSERVER;
	files[WEBSERVER].formatOption       = SettingsManager::LOG_FORMAT_WEBSERVER;
	files[CUSTOM_LOCATION].fileOption   = SettingsManager::LOG_FILE_CUSTOM_LOCATION;
	files[CUSTOM_LOCATION].formatOption = SettingsManager::LOG_FORMAT_CUSTOM_LOCATION;
	
	files[TRACE_SQLITE].fileOption      = SettingsManager::LOG_FILE_SQLITE_TRACE;
	files[TRACE_SQLITE].formatOption    = SettingsManager::LOG_FORMAT_SQLITE_TRACE;
	files[VIRUS_TRACE].fileOption       = SettingsManager::LOG_FILE_VIRUS_TRACE;
	files[VIRUS_TRACE].formatOption     = SettingsManager::LOG_FORMAT_VIRUS_TRACE;
	files[DDOS_TRACE].fileOption        = SettingsManager::LOG_FILE_DDOS_TRACE;
	files[DDOS_TRACE].formatOption      = SettingsManager::LOG_FORMAT_DDOS_TRACE;
	files[CMDDEBUG_TRACE].fileOption    = SettingsManager::LOG_FILE_CMDDEBUG_TRACE;
	files[CMDDEBUG_TRACE].formatOption  = SettingsManager::LOG_FORMAT_CMDDEBUG_TRACE;
	files[PSR_TRACE].fileOption         = SettingsManager::LOG_FILE_PSR_TRACE;
	files[PSR_TRACE].formatOption       = SettingsManager::LOG_FORMAT_PSR_TRACE;
	files[FLOOD_TRACE].fileOption       = SettingsManager::LOG_FILE_FLOOD_TRACE;
	files[FLOOD_TRACE].formatOption     = SettingsManager::LOG_FORMAT_FLOOD_TRACE;
	files[TORRENT_TRACE].fileOption     = SettingsManager::LOG_FILE_TORRENT_TRACE;
	files[TORRENT_TRACE].formatOption   = SettingsManager::LOG_FORMAT_TORRENT_TRACE;
	
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
	files[area].cs.lock();
	File& f = files[area].file;
	try
	{
		if (!f.isOpen())
		{
			string path = SETTING(LOG_DIRECTORY);
			string filenameTemplate = SettingsManager::get(files[area].fileOption);
			path += Util::validateFileName(Util::formatParams(filenameTemplate, params, true));
			f.init(Text::toT(path), File::WRITE, File::OPEN | File::CREATE, true);
			files[area].filePath = path;
			// move to the end of file
			if (f.setEndPos(0) == 0)
				f.write("\xef\xbb\xbf");
		}
		f.write(msg);
	}
	catch (...)
	{
	}
	files[area].cs.unlock();
}

void LogManager::log(int area, const StringMap& params) noexcept
{
	dcassert(area >= 0 && area < LAST);
	const string msg = Util::formatParams(SettingsManager::get(files[area].formatOption, true), params, false) + "\r\n";
	logRaw(area, msg, params);
}

void LogManager::log(int area, const string& msg) noexcept
{
	StringMap params;
	params["message"] = msg;
	log(area, params);
}

void LogManager::getOptions(int area, TStringPair& p) noexcept
{
	dcassert(area >= 0 && area < LAST);
	const LogFile& lf = files[area];
	p.first = Text::toT(SettingsManager::get(lf.fileOption, true));
	p.second = Text::toT(SettingsManager::get(lf.formatOption, true));	
}

void LogManager::setOptions(int area, const TStringPair& p) noexcept
{
	dcassert(area >= 0 && area < LAST);
	LogFile& lf = files[area];
	string filename = Text::fromT(p.first);
	SettingsManager::set(lf.fileOption, filename);
	SettingsManager::set(lf.formatOption, Text::fromT(p.second));
	lf.cs.lock();
	if (lf.file.isOpen())
	{
		string path = SETTING(LOG_DIRECTORY);
		path += Util::validateFileName(filename);
		if (path != lf.filePath)
		{
			lf.filePath = path;
			lf.file.close();
		}
	}
	lf.cs.unlock();
}

void LogManager::flush_all_log()
{
	// does nothing
}

void LogManager::virus_message(const string& message)
{
	if (BOOLSETTING(LOG_VIRUS_TRACE))
		LOG(VIRUS_TRACE, message);
}

void LogManager::ddos_message(const string& message)
{
	if (BOOLSETTING(LOG_DDOS_TRACE))
		LOG(DDOS_TRACE, message);
}

void LogManager::cmd_debug_message(const string& message)
{
	if (BOOLSETTING(LOG_CMDDEBUG_TRACE))
		LOG(CMDDEBUG_TRACE, message);
}

void LogManager::flood_message(const string& message)
{
	if (BOOLSETTING(LOG_FLOOD_TRACE))
		LOG(FLOOD_TRACE, message);
}

void LogManager::psr_message(const string& message)
{
	if (BOOLSETTING(LOG_PSR_TRACE))
		LOG(PSR_TRACE, message);
}

void LogManager::torrent_message(const string& message, bool addToSystem /*= true*/)
{
	if (BOOLSETTING(LOG_TORRENT_TRACE))
		LOG(TORRENT_TRACE, message);
	if (addToSystem)
		LOG(SYSTEM, message);
}

void LogManager::speak_status_message(const string& p_msg)
{
	if (LogManager::g_isLogSpeakerEnabled == true && ClientManager::isStartup() == false && ClientManager::isBeforeShutdown() == false)
	{
		if (LogManager::g_mainWnd)
		{
			auto l_str_messages = new string(p_msg);
			// TODO safe_post_message(LogManager::g_mainWnd, g_LogMessageID, l_str_messages);
			if (::PostMessage(LogManager::g_mainWnd, WM_SPEAKER, g_LogMessageID, (LPARAM)l_str_messages) == FALSE)
			{
				// TODO - LOG dcassert(0);
				dcdebug("[LogManager::g_mainWnd] PostMessage error %d\n", GetLastError());
				delete l_str_messages;
				LogManager::g_isLogSpeakerEnabled = false; // Fix error 1816
			}
		}
	}
}

void LogManager::message(const string& message)
{
	if (BOOLSETTING(LOG_SYSTEM))
		log(SYSTEM, message);
	speak_status_message(message);
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
