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

#include "File.h"
#include "Locks.h"

namespace Util
{
	class ParamExpander;
}

class LogManager
{
	public:
		enum
		{
			CHAT,
			PM,
			DOWNLOAD,
			UPLOAD,
			SYSTEM,
			STATUS,
			WEBSERVER,
			SQLITE_TRACE,
#ifdef FLYLINKDC_USE_TORRENT
			TORRENT_TRACE,
#endif
			SEARCH_TRACE,
			DHT_TRACE,
			PSR_TRACE,
			FLOOD_TRACE,
			TCP_MESSAGES,
			UDP_PACKETS,
			LAST
		};

		// Flags for commandTrace
		enum
		{
			FLAG_IN  = 1,
			FLAG_UDP = 2
		};

		enum
		{
			OPT_LOG_SYSTEM       = 0x001,
			OPT_LOG_STATUS       = 0x002,
			OPT_LOG_SOCKET_INFO  = 0x004,
			OPT_LOG_TCP_MESSAGES = 0x008, // BOOLSETTING(LOG_TCP_MESSAGES)
			OPT_LOG_UDP_PACKETS  = 0x010,
			OPT_LOG_CERTIFICATES = 0x020,
			OPT_LOG_PSR          = 0x040,
			OPT_LOG_DHT          = 0x080, // BOOLSETTING(LOG_DHT_TRACE)
			OPT_LOG_SEARCH       = 0x100,
			OPT_LOG_SQLITE       = 0x200,
			OPT_LOG_WEB_SERVER   = 0x400  // BOOLSETTING(LOG_WEBSERVER)
		};

		static void init();
		static void log(int area, const string& msg) noexcept;
		static void log(int area, const StringMap& params) noexcept;
		static void log(int area, Util::ParamExpander* ex) noexcept;
		static void message(const string& msg, bool useStatus = true) noexcept;
		static void commandTrace(const string& msg, int flags, const string& ip, int port) noexcept;
		static void speakStatusMessage(const string& message) noexcept;
		static void getOptions(int area, TStringPair& p) noexcept;
		static void setOptions(int area, const TStringPair& p) noexcept;
		static void closeOldFiles(int64_t now) noexcept;
		static string getLogFileName(int area, const StringMap& params) noexcept;
		static int getLogOptions() noexcept { return options.load(); }
		static void updateSettings() noexcept;
		static string getLogDirectory() noexcept;

#ifdef _WIN32
		static HWND g_mainWnd;
#endif
		static bool g_isLogSpeakerEnabled;
		static int  g_LogMessageID;

	private:
		static bool g_isInit;
		static int64_t nextCloseTime;
		static std::atomic_int options;

		LogManager();
		~LogManager()
		{
		}

		struct LogFile
		{
			File file;
			int64_t timeout;

			LogFile() {}
			LogFile(const LogFile&) = delete;
			LogFile& operator= (LogFile&) = delete;
		};
		
		struct LogArea
		{
			CriticalSection cs;
			boost::unordered_map<string, LogFile> files;
			string logDirectory;
			string filenameTemplate;
			int fileOption;
			int formatOption;
		};

		static LogArea types[LAST];		

		static void logRaw(int area, const string& msg, Util::ParamExpander* ex) noexcept;
};

#define LOG(area, msg) LogManager::log(LogManager::area, msg)

class StepLogger
{
	public:
		StepLogger(const string& message, bool skipStart = true, bool skipStop = false);
		~StepLogger();
		void step(const string& what);

	private:
		const string message;
		const bool skipStop;
		uint64_t startTime;
		uint64_t stepTime;
};

#endif // DCPLUSPLUS_DCPP_LOG_MANAGER_H
