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
#include "IpAddress.h"
#include "AppPorts.h"
#include <atomic>

class ConnectivityManager : public Singleton<ConnectivityManager>
{
	public:
		enum
		{
			STATUS_IPV4 = 1,
			STATUS_IPV6 = 2,
			STATUS_DUAL_STACK = STATUS_IPV4 | STATUS_IPV6
		};

		void setupConnections(bool forcePortTest = false);
		bool isSetupInProgress() const noexcept { return getRunningFlags() != 0; }
		void processPortTestResult() noexcept;
		void processGetIpResult(int req) noexcept;
		void setReflectedIP(const IpAddress& ip) noexcept;
		IpAddress getReflectedIP(int af) const noexcept;
		void setReflectedPort(int af, int what, int port) noexcept;
		int getReflectedPort(int af, int what) const noexcept;
		void setLocalIP(const IpAddress& ip) noexcept;
		IpAddress getLocalIP(int af) const noexcept;
		const MappingManager& getMapper(int af) const;
		void checkReflectedPort(int& port, int af, int what) const noexcept;
		string getInformation() const;
		unsigned getConnectivity() const;
		static bool isIP6Supported() { return ipv6Supported; }
		static void checkIP6();
		static bool hasIP6() { return ipv6Enabled; }

	private:
		friend class Singleton<ConnectivityManager>;
		friend class MappingManager;
		
		ConnectivityManager();
		virtual ~ConnectivityManager();

		void mappingFinished(const string& mapper, int af);
		void log(const string& msg, Severity sev, int af);
		
		void detectConnection(int af);
		void listenTCP(int af);
		void listenUDP(int af);
		void setPassiveMode(int af);
		unsigned testPorts();
		bool setup(int af);
		void disconnect();
		unsigned getRunningFlags() const noexcept;

		IpAddress reflectedIP[2];
		int reflectedPort[2 * AppPorts::MAX_PORTS];
		IpAddress localIP[2];
		MappingManager mappers[2];
		mutable FastCriticalSection cs;
		unsigned running;
		unsigned setupResult;
		bool forcePortTest;
		bool autoDetect[2];
		static std::atomic_bool ipv6Supported;
		static std::atomic_bool ipv6Enabled;
};

#endif // !defined(CONNECTIVITY_MANAGER_H)
