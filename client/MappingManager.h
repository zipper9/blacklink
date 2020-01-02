/*
 * Copyright (C) 2001-2019 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef MAPPING_MANAGER_H
#define MAPPING_MANAGER_H

#include "forward.h"
#include "typedefs.h"

#include "Mapper.h"
#include "CFlyThread.h"
#include "TimerManager.h"
#include "Severity.h"
#include <atomic>

class MappingManager : private Thread, private TimerManagerListener
{
	public:
		enum
		{
			PORT_UDP,
			PORT_TCP,
			PORT_TLS,
			MAX_PORTS
		};

		enum
		{
			STATE_UNKNOWN,
			STATE_FAILURE,
			STATE_SUCCESS,
			STATE_RENEWAL_FAILURE,
			STATE_RUNNING
		};

		MappingManager(bool v6);
		
		/** add an implementation derived from the base Mapper class, passed as template parameter.
		the first added mapper will be tried first, unless the "MAPPER" setting is not empty. */
		template <typename T> void addMapper()
		{
			mappers.emplace_back(T::name, [](const string &localIp, bool aV6) { return new T(localIp, aV6); });
		}
		StringList getMappers() const;

		bool open();
		void close();

		/** whether a working port mapping implementation is currently in use. */
		bool getOpened() const;

		int getState(int type) const noexcept;
		bool isRunning() const noexcept;

	private:
		struct Mapping
		{
			int port = 0;
			int state = STATE_UNKNOWN;
		};

		const bool v6;
		mutable CriticalSection cs;
		Mapping mappings[MAX_PORTS];

		vector<pair<string, std::function<Mapper *(const string &, bool)>>> mappers;
		std::unique_ptr<Mapper> working; // currently working implementation.
		
		uint64_t renewal = 0; // when the next renewal should happen, if requested by the mapper.
		std::atomic_flag threadRunning;
		bool sharedTLSPort = false;

		virtual int run() override;

		bool open(Mapper &mapper, int port, int type, const string &description) noexcept;
		void renew(Mapper &mapper, int port, int type, const string &description) noexcept;
		void close(Mapper &mapper, bool quiet) noexcept;
		void log(const string &message, Severity sev);
		string deviceString(Mapper &mapper) const;
		void renewLater(Mapper &mapper);
		static string formatDescription(const string &description, int type, int port);
		
		virtual void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
};

#endif
