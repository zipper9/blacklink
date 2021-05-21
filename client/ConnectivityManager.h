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

#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include "Singleton.h"
#include "MappingManager.h"

class ConnectivityManager : public Singleton<ConnectivityManager>
{
	public:
		enum
		{
			TYPE_V4 = 4,
			TYPE_V6 = 6
		};

		bool setupConnections(bool forcePortTest = false);
		bool isSetupInProgress() const noexcept;
		void processPortTestResult();
		void setReflectedIP(const string& ip) noexcept;
		string getReflectedIP() const noexcept;
		void setLocalIP(const string& ip) noexcept;
		string getLocalIP() const noexcept;
		const MappingManager& getMapperV4() const { return mapperV4; }
		string getPortmapInfo(bool showPublicIp) const;
		
	private:
		friend class Singleton<ConnectivityManager>;
		friend class MappingManager;
		
		ConnectivityManager();
		virtual ~ConnectivityManager();
		
		void mappingFinished(const string& mapper, bool v6);
		void log(const string& msg, Severity sev, int type);
		
		string getInformation() const;
		void detectConnection();
		void startSocket();
		void listen();
		void disconnect();
		void setPassiveMode();
		void testPorts();
		
		string reflectedIP;
		string localIP;
		MappingManager mapperV4;
		mutable FastCriticalSection cs;
		bool running;
		bool autoDetect;
		bool forcePortTest;
};

#endif // !defined(CONNECTIVITY_MANAGER_H)
