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
#include "ConnectivityManager.h"
#include "ClientManager.h"
#include "ConnectionManager.h"
#include "LogManager.h"
#include "MappingManager.h"
#include "SearchManager.h"
#include "SettingsManager.h"
#include "DownloadManager.h"
#include "CryptoManager.h"
#include "PortTest.h"
#include "IpTest.h"
#include "NetworkUtil.h"
#include "dht/DHT.h"

std::atomic_bool ConnectivityManager::ipv6Supported(false);
std::atomic_bool ConnectivityManager::ipv6Enabled(false);

enum
{
	RUNNING_IPV4      = 1,
	RUNNING_IPV6      = 2,
	RUNNING_TEST_IPV4 = 4,
	RUNNING_TEST_IPV6 = 8,
	RUNNING_OTHER     = 16
};

enum
{
	STATUS_IPV4 = 1,
	STATUS_IPV6 = 2
};

class ListenerException : public Exception
{
	public:
		ListenerException(const char* type, int errorCode) : type(type), errorCode(errorCode) {}
		const char* type;
		int errorCode;
};

ConnectivityManager::ConnectivityManager() : running(0), setupResult(0), autoDetect{false, false}, forcePortTest(false)
{
	memset(reflectedIP, 0, sizeof(reflectedIP));
	memset(localIP, 0, sizeof(localIP));
	mappers[0].init(AF_INET);
	mappers[1].init(AF_INET6);
	checkIP6();
}

ConnectivityManager::~ConnectivityManager()
{
	mappers[0].close();
	mappers[1].close();
}

void ConnectivityManager::detectConnection(int af)
{
	SettingsManager::IPSettings ips;
	SettingsManager::getIPSettings(ips, af == AF_INET6);

	// restore connectivity settings to their default value.
	SettingsManager::unset(ips.bindAddress);

	log(STRING(CONN_DETECT_START), SEV_INFO, af);
	SettingsManager::set(ips.incomingConnections, SettingsManager::INCOMING_FIREWALL_PASSIVE);
	listenTCP(af);

	IpAddress ip = getLocalIP(af);
	if (Util::isPrivateIp(ip))
	{
		SettingsManager::set(ips.incomingConnections, SettingsManager::INCOMING_FIREWALL_UPNP);
		log(STRING(CONN_DETECT_LOCAL), SEV_INFO, af);
	}
	else
	{
		SettingsManager::set(ips.incomingConnections, SettingsManager::INCOMING_DIRECT);
		log(STRING_F(CONN_DETECT_PUBLIC, Util::printIpAddress(ip)), SEV_INFO, af);
	}
}

unsigned ConnectivityManager::testPorts()
{
	unsigned testFlags = 0;
	bool force = false;
	cs.lock();
	std::swap(force, forcePortTest);
	unsigned status = setupResult;
	cs.unlock();
	if (!force && !BOOLSETTING(AUTO_TEST_PORTS)) return 0;
	if (status & STATUS_IPV4)
	{
		int portTCP = SETTING(TCP_PORT);
		g_portTest.setPort(PortTest::PORT_TCP, portTCP);
		int portUDP = SETTING(UDP_PORT);
		g_portTest.setPort(PortTest::PORT_UDP, portUDP);
		int mask = 1<<PortTest::PORT_UDP | 1<<PortTest::PORT_TCP;
		if (CryptoManager::TLSOk())
		{
			int portTLS = SETTING(TLS_PORT);
			g_portTest.setPort(PortTest::PORT_TLS, portTLS);
			mask |= 1<<PortTest::PORT_TLS;
		}
		if (g_portTest.runTest(mask))
			testFlags |= RUNNING_TEST_IPV4;
	}
	if (!force && (status & STATUS_IPV6))
	{
		if (g_ipTest.runTest(IpTest::REQ_IP6))
			testFlags |= RUNNING_TEST_IPV6;
	}
	if (testFlags)
	{
		cs.lock();
		running |= testFlags;
		cs.unlock();
	}
	return testFlags;
}

void ConnectivityManager::setupConnections(bool forcePortTest)
{
	cs.lock();
	if (running)
	{
		cs.unlock();
		return;
	}
	this->forcePortTest = forcePortTest;
	running = RUNNING_OTHER;
	cs.unlock();

	disconnect();
	bool hasIP4 = setup(AF_INET);
	bool hasIP6 = false;
	if (BOOLSETTING(ENABLE_IP6) && ipv6Supported)
		hasIP6 = setup(AF_INET6);
	ipv6Enabled = hasIP6;
	if (!hasIP4 && !hasIP6)
	{
		cs.lock();
		running = 0;
		setupResult = 0;
		cs.unlock();
		return;
	}

	cs.lock();
	setupResult = 0;
	if (hasIP4) setupResult |= STATUS_IPV4;
	if (hasIP6) setupResult |= STATUS_IPV6;
	cs.unlock();

	dht::DHT::getInstance()->start();
	SearchManager::getInstance()->start();
	ConnectionManager::getInstance()->fireListenerStarted();
	if (!(getRunningFlags() & (RUNNING_IPV4 | RUNNING_IPV6)))
	{
		if (!testPorts())
		{
			cs.lock();
			running = 0;
			cs.unlock();
			log(getInformation(), SEV_INFO, 0);
		}
	}
}

bool ConnectivityManager::setup(int af)
{
	int index = af == AF_INET6 ? 1 : 0;
	SettingsManager::IPSettings ips;
	SettingsManager::getIPSettings(ips, af == AF_INET6);
	bool autoDetectFlag = SettingsManager::get(ips.autoDetect);

	cs.lock();
	autoDetect[index] = autoDetectFlag;
	cs.unlock();

	const string savedBindAddress = SettingsManager::get(ips.bindAddress);
	try
	{
		if (autoDetectFlag)
			detectConnection(af);
		else
			listenTCP(af);
		listenUDP(af);
		if (SettingsManager::get(ips.incomingConnections) == SettingsManager::INCOMING_FIREWALL_UPNP && mappers[index].open())
		{
			cs.lock();
			running |= af == AF_INET6 ? RUNNING_IPV6 : RUNNING_IPV4;
			cs.unlock();
		}
	}
	catch (const ListenerException& e)
	{
		if (autoDetectFlag)
		{
			SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
			SettingsManager::set(ips.bindAddress, savedBindAddress);
			SettingsManager::set(ips.incomingConnections, SettingsManager::INCOMING_FIREWALL_PASSIVE);
			log(STRING_F(UNABLE_TO_OPEN_PORT, e.getError()), SEV_ERROR, af);
		}
		ConnectionManager::getInstance()->fireListenerFailed(e.type, af, e.errorCode);
		return false;
	}
	return true;
}

static string getModeString(int af)
{
	int ic = SettingsManager::get(af == AF_INET6 ? SettingsManager::INCOMING_CONNECTIONS6 : SettingsManager::INCOMING_CONNECTIONS);
	ResourceManager::Strings str;
	switch (ic)
	{
		case SettingsManager::INCOMING_DIRECT:
			str = ResourceManager::CONNECTIVITY_MODE_ACTIVE;
			break;
		case SettingsManager::INCOMING_FIREWALL_UPNP:
			str = ResourceManager::CONNECTIVITY_MODE_AUTO_FORWARDING;
			break;
		case SettingsManager::INCOMING_FIREWALL_NAT:
			str = ResourceManager::CONNECTIVITY_MODE_MANUAL_FORWARDING;
			break;
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
			str = ResourceManager::CONNECTIVITY_MODE_PASSIVE;
			break;
		default:
			return Util::emptyString;
	}

	int v = af == AF_INET6 ? 6 : 4;
	string mode = STRING_I(str);
	return STRING_F(CONNECTIVITY_MODE, v % mode);
}

string ConnectivityManager::getInformation() const
{
	if (isSetupInProgress())
		return STRING(CONNECTIVITY_RUNNING);
	
	string s = STRING(CONNECTIVITY_TITLE);
	s += "\n\t";
	string mode = getModeString(AF_INET);
	s += mode;
	s += '\n';
	IpAddress ip = getReflectedIP(AF_INET);
	if (!Util::isEmpty(ip))
	{
		s += "\t" + STRING_F(CONNECTIVITY_EXTERNAL_IP, 4 % Util::printIpAddress(ip));
		s += '\n';
	}
	const string& bindV4 = SETTING(BIND_ADDRESS);
	if (!bindV4.empty())
	{
		s += "\t" + STRING_F(CONNECTIVITY_BOUND_INTERFACE, 4 % bindV4);
		s += '\n';
	}
	if (ipv6Enabled)
	{
		mode = getModeString(AF_INET6);
		s += "\t" + mode;
		s += '\n';
		ip = getReflectedIP(AF_INET6);
		if (!Util::isEmpty(ip))
		{
			s += "\t" + STRING_F(CONNECTIVITY_EXTERNAL_IP, 6 % Util::printIpAddress(ip));
			s += '\n';
		}
		const string& bindV6 = SETTING(BIND_ADDRESS6);
		if (!bindV6.empty())
		{
			s += "\t" + STRING_F(CONNECTIVITY_BOUND_INTERFACE, 6 % bindV6);
			s += '\n';
		}
	}
	int port = ConnectionManager::getInstance()->getPort();
	s += "\t" + STRING_F(CONNECTIVITY_TRANSFER_PORT, port);
	s += '\n';
	port = ConnectionManager::getInstance()->getSecurePort();
	if (port)
	{
		s += "\t" + STRING_F(CONNECTIVITY_ENCRYPTED_TRANSFER_PORT, port);
		s += '\n';
	}
	port = SearchManager::getUdpPort();
	s += "\t" + STRING_F(CONNECTIVITY_SEARCH_PORT, port);
	s += '\n';
	return s;
}

void ConnectivityManager::mappingFinished(const string& mapper, int af)
{
	if (!ClientManager::isBeforeShutdown())
	{
		cs.lock();
		bool autoDetectFlag = autoDetect[af == AF_INET6 ? 1 : 0];
		running &= ~(af == AF_INET6 ? RUNNING_IPV6 : RUNNING_IPV4);
		unsigned runningFlags = running;
		cs.unlock();
		if (autoDetectFlag && mapper.empty())
			setPassiveMode(af);
		if (!mapper.empty())
			SettingsManager::set(af == AF_INET6 ? SettingsManager::MAPPER6 : SettingsManager::MAPPER, mapper);
		if (!(runningFlags & (RUNNING_IPV4 | RUNNING_IPV6)))
		{
			if (!testPorts())
			{
				cs.lock();
				running = 0;
				cs.unlock();
				log(getInformation(), SEV_INFO, 0);
			}
		}
	}
	else
	{
		cs.lock();
		running = 0;
		cs.unlock();
	}
}

void ConnectivityManager::setPassiveMode(int af)
{
	SettingsManager::set(af == AF_INET6 ? SettingsManager::INCOMING_CONNECTIONS6 : SettingsManager::INCOMING_CONNECTIONS,	SettingsManager::INCOMING_FIREWALL_PASSIVE);
	SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
	log(STRING(CONN_DETECT_NO_ACTIVE_MODE), SEV_WARNING, af);
}

void ConnectivityManager::processPortTestResult() noexcept
{
	if (g_portTest.isRunning()) return;
	cs.lock();
	if (!(running & RUNNING_TEST_IPV4))
	{
		cs.unlock();
		return;
	}
	running &= ~RUNNING_TEST_IPV4;
	bool autoDetectFlag = autoDetect[0];
	autoDetect[0] = false;
	unsigned runningFlags = running;
	cs.unlock();
	if (autoDetectFlag)
	{
		int unused;
		if (g_portTest.getState(PortTest::PORT_TCP, unused, nullptr) != PortTest::STATE_SUCCESS ||
		    g_portTest.getState(PortTest::PORT_UDP, unused, nullptr) != PortTest::STATE_SUCCESS)
		{
			setPassiveMode(AF_INET);
		}
	}
	if (!(runningFlags & ~RUNNING_OTHER))
	{
		cs.lock();
		running = 0;
		cs.unlock();
		log(getInformation(), SEV_INFO, 0);
	}
}

void ConnectivityManager::processGetIpResult(int req) noexcept
{
	if (req != IpTest::REQ_IP6 || g_ipTest.isRunning(req)) return;
	bool completed = false;
	cs.lock();
	if (!(running & RUNNING_TEST_IPV6))
	{
		cs.unlock();
		return;
	}
	running &= ~RUNNING_TEST_IPV6;
	if (!(running & ~RUNNING_OTHER))
	{
		running = 0;
		completed = true;
	}
	cs.unlock();
	if (completed) log(getInformation(), SEV_INFO, 0);
}

void ConnectivityManager::listenTCP(int af)
{
	SettingsManager::IPSettings ips;
	SettingsManager::getIPSettings(ips, af == AF_INET6);
	bool autoDetectFlag = SettingsManager::get(ips.autoDetect);

	bool fixedPort = af == AF_INET6 || (!autoDetectFlag && SettingsManager::get(ips.incomingConnections) == SettingsManager::INCOMING_FIREWALL_NAT);
	auto cm = ConnectionManager::getInstance();
	cm->stopServer(af);

	bool useTLS = CryptoManager::TLSOk();
	int type = (useTLS && fixedPort && SETTING(TLS_PORT) == SETTING(TCP_PORT)) ?
		ConnectionManager::SERVER_TYPE_AUTO_DETECT : ConnectionManager::SERVER_TYPE_TCP;
	for (int i = 0; i < 2; ++i)
	{
		try
		{
			cm->startListen(af, type);
			cm->updateLocalIp(af);
		}
		catch (const SocketException& e)
		{
			LogManager::message("Could not start TCPv" + Util::toString(af == AF_INET6 ? 6 : 4) +
				" listener: error " + Util::toString(e.getErrorCode()) + " i=" + Util::toString(i));
			if (fixedPort || e.getErrorCode() != SE_EADDRINUSE || i)
				throw ListenerException("TCP", e.getErrorCode());
			SET_SETTING(TCP_PORT, 0);
			continue;
		}
		break;
	}
	if (useTLS && type != ConnectionManager::SERVER_TYPE_AUTO_DETECT)
	{
		if (SETTING(TLS_PORT) == SETTING(TCP_PORT))
			SET_SETTING(TLS_PORT, 0);
		for (int i = 0; i < 2; ++i)
		{
			try
			{
				cm->startListen(af, ConnectionManager::SERVER_TYPE_SSL);
			}
			catch (const SocketException& e)
			{
				LogManager::message("Could not start TLSv" + Util::toString(af == AF_INET6 ? 6 : 4) +
					" listener: error " + Util::toString(e.getErrorCode()) + " i=" + Util::toString(i));
				if (fixedPort || e.getErrorCode() != SE_EADDRINUSE || i)
					throw ListenerException("TLS", e.getErrorCode());
				SET_SETTING(TLS_PORT, 0);
				continue;
			}
			break;
		}
	}
}

void ConnectivityManager::listenUDP(int af)
{
	SettingsManager::IPSettings ips;
	SettingsManager::getIPSettings(ips, af == AF_INET6);
	bool autoDetectFlag = SettingsManager::get(ips.autoDetect);

	bool fixedPort = af == AF_INET6 || (!autoDetectFlag && SettingsManager::get(ips.incomingConnections) == SettingsManager::INCOMING_FIREWALL_NAT);
	for (int i = 0; i < 2; ++i)
	{
		try
		{
			SearchManager::getInstance()->listenUDP(af);
		}
		catch (const SocketException& e)
		{
			LogManager::message("Could not start UDPv" + Util::toString(af == AF_INET6 ? 6 : 4) +
				" listener: error " + Util::toString(e.getErrorCode()) + " i=" + Util::toString(i));
			if (fixedPort || e.getErrorCode() != SE_EADDRINUSE || i)
				throw ListenerException("UDP", e.getErrorCode());
			SET_SETTING(UDP_PORT, 0);
			continue;
		}
		break;
	}
}

void ConnectivityManager::disconnect()
{
	mappers[0].close();
	mappers[1].close();
	SearchManager::getInstance()->shutdown();
	auto cm = ConnectionManager::getInstance();
	cm->stopServer(AF_INET);
	cm->stopServer(AF_INET6);
}

void ConnectivityManager::log(const string& message, Severity sev, int af)
{
	// TODO: show connectivity information to the user
	if (af)
	{
		string ipv = af == AF_INET6 ? "IPv6: " : "IPv4: ";
		LogManager::message(ipv + message, false);
	}
	else
		LogManager::message(message, false);
}

unsigned ConnectivityManager::getRunningFlags() const noexcept
{
	cs.lock();
	unsigned result = running;
	cs.unlock();
	return result;
}

static int getIndex(int af) noexcept
{
	if (af != AF_INET && af != AF_INET6) return -1;
	return af == AF_INET6 ? 1 : 0;
}

void ConnectivityManager::setReflectedIP(const IpAddress& ip) noexcept
{
	int index = getIndex(ip.type);
	if (index == -1) return;
	LOCK(cs);
	reflectedIP[index] = ip;
}

IpAddress ConnectivityManager::getReflectedIP(int af) const noexcept
{
	int index = getIndex(af);
	if (index == -1) return IpAddress{};
	LOCK(cs);
	return reflectedIP[index];
}

void ConnectivityManager::setLocalIP(const IpAddress& ip) noexcept
{
	int index = getIndex(ip.type);
	if (index == -1) return;
	LOCK(cs);
	localIP[index] = ip;
}

IpAddress ConnectivityManager::getLocalIP(int af) const noexcept
{
	int index = getIndex(af);
	if (index == -1) return IpAddress{};
	LOCK(cs);
	return localIP[index];
}

const MappingManager& ConnectivityManager::getMapper(int af) const
{
	return mappers[af == AF_INET6 ? 1 : 0];
}

void ConnectivityManager::checkIP6()
{
	bool result = false;
	socket_t sock = socket(AF_INET6, SOCK_DGRAM, 0);
	if (sock != INVALID_SOCKET)
	{
#ifdef _WIN32
		::closesocket(sock);
#else
		::close(sock);
#endif
		vector<Util::AdapterInfo> adapters;
		Util::getNetworkAdapters(AF_INET6, adapters);
		for (const auto& ai : adapters)
			if (!Util::isReservedIp(ai.ip.data.v6) && !Util::isLinkScopedIp(ai.ip.data.v6))
			{
				result = true;
				break;
			}
	}
	ipv6Supported = result;
}
