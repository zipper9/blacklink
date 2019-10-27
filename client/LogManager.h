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


#ifndef DCPLUSPLUS_DCPP_LOG_MANAGER_H
#define DCPLUSPLUS_DCPP_LOG_MANAGER_H

#include "Util.h"
#include "File.h"

class LogManager
{
	public:
		enum
		{
			CHAT, PM, DOWNLOAD, UPLOAD, SYSTEM, STATUS,
			WEBSERVER,
			CUSTOM_LOCATION,
			TRACE_SQLITE,
			VIRUS_TRACE,
			DDOS_TRACE,
			CMDDEBUG_TRACE,
			TORRENT_TRACE,
			PSR_TRACE,
			FLOOD_TRACE,
			LAST
		};
		             
		static void init();
		static void log(int area, const string& msg) noexcept;
		static void log(int area, const StringMap& params) noexcept;
		static void ddos_message(const string& message);
		static void virus_message(const string& message);
		static void flood_message(const string& message);
		static void cmd_debug_message(const string& message);
		static void torrent_message(const string& message, bool addToSystem = true);
		static void psr_message(const string& message);
		static void message(const string& msg);
		static void speak_status_message(const string& message);
		static void getOptions(int area, TStringPair& p) noexcept;
		static void setOptions(int area, const TStringPair& p) noexcept;
		
		static HWND g_mainWnd;
		static bool g_isLogSpeakerEnabled;
		static int  g_LogMessageID;
		static void flush_all_log();

	private:
		static bool g_isInit;
		
		LogManager();
		~LogManager()
		{
		}

		struct LogFile
		{
			CriticalSection cs;
			File file;
			string filePath;
			SettingsManager::StrSetting fileOption;
			SettingsManager::StrSetting formatOption;
		};

		static LogFile files[LAST];		

		static void logRaw(int area, const string& msg, const StringMap& params) noexcept;
};

#define LOG(area, msg) LogManager::log(LogManager::area, msg)

class CFlyLog
{
	public:
		const string m_message;
		const uint64_t m_start;
		uint64_t m_tc;
		bool m_skip_start;
		bool m_skip_stop;
		bool m_only_file;
		void log(const string& p_msg) { LogManager::message(p_msg); }

	public:
		CFlyLog(const string& p_message, bool p_skip_start = true);
		~CFlyLog();
		uint64_t calcSumTime() const;
		void step(const string& p_message_step, const bool p_reset_count = true);
		void loadStep(const string& p_message_step, const bool p_reset_count = true);
};

class CFlyLogFile : public CFlyLog
{
	public:
		explicit CFlyLogFile(const string& p_message) : CFlyLog(p_message, true)
		{
		}
};

#endif // DCPLUSPLUS_DCPP_LOG_MANAGER_H
