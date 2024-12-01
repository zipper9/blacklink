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
#include "SettingsUtil.h"
#include "ConfCore.h"
#include "dht/DHT.h"

std::atomic_bool ConnectivityManager::ipv6Supported(false);
std::atomic_bool ConnectivityManager::ipv6Enabled(false);

enum
{
	RUNNING_IPV4            = 1,
	RUNNING_IPV6            = 2,
	RUNNING_TEST_IPV4_PORTS = 4,
	RUNNING_TEST_IPV4       = 8,
	RUNNING_TEST_IPV6       = 16,
	RUNNING_OTHER           = 32
};

enum
{
	STATUS_IPV4 = 1,
	STATUS_IPV6 = 2
};

class ListenerException : public Exception
{
	public:
		ListenerException(const char* type, int errorCode, const string& errorText) : Exception(errorText), type(type), errorCode(errorCode) {}
		const char* type;
		int errorCode;
};

ConnectivityManager::ConnectivityManager() : running(0), setupResult(0), autoDetect{false, false}, forcePortTest(false)
{
	memset(reflectedIP, 0, sizeof(reflectedIP));
	memset(localIP, 0, sizeof(localIP));
	memset(reflectedPort, 0, sizeof(reflectedPort));
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
	Conf::IPSettings ips;
	Conf::getIPSettings(ips, af == AF_INET6);

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	// Restore connectivity settings to their default values.
	ss->unsetString(ips.bindAddress);
	ss->unsetString(ips.bindDevice);
	ss->unsetInt(ips.bindOptions);
	ss->setInt(ips.incomingConnections, Conf::INCOMING_FIREWALL_PASSIVE);
	ss->unlockWrite();

	log(STRING(CONN_DETECT_START), SEV_INFO, af);
	listenTCP(af);

	IpAddress ip = getLocalIP(af);
	if (Util::isPrivateIp(ip))
	{
		ss->lockWrite();
		ss->setInt(ips.incomingConnections, Conf::INCOMING_FIREWALL_UPNP);
		ss->unlockWrite();
		log(STRING(CONN_DETECT_LOCAL), SEV_INFO, af);
	}
	else
	{
		ss->lockWrite();
		ss->setInt(ips.incomingConnections, Conf::INCOMING_DIRECT);
		ss->unlockWrite();
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

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool autoTestPorts = ss->getBool(Conf::AUTO_TEST_PORTS);
	int incomingMode = ss->getInt(Conf::INCOMING_CONNECTIONS);
	int portTCP = ss->getInt(Conf::TCP_PORT);
	int portUDP = ss->getInt(Conf::UDP_PORT);
	int portTLS = ss->getInt(Conf::TLS_PORT);
	ss->unlockRead();

	if (!force && !autoTestPorts) return 0;
	if (status & STATUS_IPV4)
	{
		if (force || incomingMode != Conf::INCOMING_FIREWALL_PASSIVE)
		{
			checkReflectedPort(portTCP, AF_INET, AppPorts::PORT_TCP);
			checkReflectedPort(portUDP, AF_INET, AppPorts::PORT_UDP);
			checkReflectedPort(portTLS, AF_INET, AppPorts::PORT_TLS);
			g_portTest.setPort(AppPorts::PORT_TCP, portTCP);
			g_portTest.setPort(AppPorts::PORT_UDP, portUDP);
			int mask = 1<<AppPorts::PORT_UDP | 1<<AppPorts::PORT_TCP;
			if (CryptoManager::getInstance()->isInitialized())
			{
				g_portTest.setPort(AppPorts::PORT_TLS, portTLS);
				mask |= 1<<AppPorts::PORT_TLS;
			}
			if (g_portTest.runTest(mask))
				testFlags |= RUNNING_TEST_IPV4_PORTS;
		}
		else if (g_ipTest.runTest(IpTest::REQ_IP4, 0))
			testFlags |= RUNNING_TEST_IPV4;
	}
	if (!force && (status & STATUS_IPV6))
	{
		if (g_ipTest.runTest(IpTest::REQ_IP6, 0))
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

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	bool enableIP6 = ss->getBool(Conf::ENABLE_IP6);
	ss->unlockRead();

	bool hasIP4 = setup(AF_INET);
	bool hasIP6 = false;
	if (enableIP6 && ipv6Supported)
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
	Conf::IPSettings ips;
	Conf::getIPSettings(ips, af == AF_INET6);
	
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const bool autoDetectFlag = ss->getBool(ips.autoDetect);
	const string savedBindAddress = ss->getString(ips.bindAddress);
	const string savedBindDevice = ss->getString(ips.bindDevice);
	const int savedBindOptions = ss->getInt(ips.bindOptions);
	ss->unlockRead();

	cs.lock();
	autoDetect[index] = autoDetectFlag;
	cs.unlock();

	try
	{
		if (autoDetectFlag)
			detectConnection(af);
		else
			listenTCP(af);
		listenUDP(af);
		ss->lockRead();
		int incomingMode = ss->getInt(ips.incomingConnections);
		ss->unlockRead();
		if (incomingMode == Conf::INCOMING_FIREWALL_UPNP && mappers[index].open())
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
			ss->lockWrite();
			ss->setBool(Conf::ALLOW_NAT_TRAVERSAL, true); // ???
			ss->setString(ips.bindAddress, savedBindAddress);
			ss->setString(ips.bindDevice, savedBindDevice);
			ss->setInt(ips.bindOptions, savedBindOptions);
			ss->setInt(ips.incomingConnections, Conf::INCOMING_FIREWALL_PASSIVE);
			ss->unlockWrite();
			log(STRING_F(UNABLE_TO_OPEN_PORT, e.getError()), SEV_ERROR, af);
		}
		ConnectionManager::getInstance()->fireListenerFailed(e.type, af, e.errorCode, e.getError());
		return false;
	}
	return true;
}

static string getModeString(int mode, int af)
{
	ResourceManager::Strings str;
	switch (mode)
	{
		case Conf::INCOMING_DIRECT:
			str = ResourceManager::CONNECTIVITY_MODE_ACTIVE;
			break;
		case Conf::INCOMING_FIREWALL_UPNP:
			str = ResourceManager::CONNECTIVITY_MODE_AUTO_FORWARDING;
			break;
		case Conf::INCOMING_FIREWALL_NAT:
			str = ResourceManager::CONNECTIVITY_MODE_MANUAL_FORWARDING;
			break;
		case Conf::INCOMING_FIREWALL_PASSIVE:
			str = ResourceManager::CONNECTIVITY_MODE_PASSIVE;
			break;
		default:
			return Util::emptyString;
	}

	int v = af == AF_INET6 ? 6 : 4;
	string modeString = STRING_I(str);
	return STRING_F(CONNECTIVITY_MODE, v % modeString);
}

string ConnectivityManager::getInformation() const
{
	cs.lock();
	unsigned rf = running;
	unsigned result = setupResult;
	cs.unlock();

	if (rf)
		return STRING(CONNECTIVITY_RUNNING);

	if (!result)
		return STRING(NO_CONNECTIVITY);

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const int incomingMode4 = ss->getInt(Conf::INCOMING_CONNECTIONS);
	const int incomingMode6 = ss->getInt(Conf::INCOMING_CONNECTIONS6);
	const string bindAddr4 = ss->getString(Conf::BIND_ADDRESS);
	const string bindAddr6 = ss->getString(Conf::BIND_ADDRESS6);
	ss->unlockRead();

	string s = STRING(CONNECTIVITY_TITLE);
	s += "\n";
	if (result & STATUS_IPV4)
	{
		s += "\t" + getModeString(incomingMode4, AF_INET);
		s += '\n';
		IpAddress ip = getReflectedIP(AF_INET);
		if (!Util::isEmpty(ip))
		{
			s += "\t" + STRING_F(CONNECTIVITY_EXTERNAL_IP, 4 % Util::printIpAddress(ip));
			s += '\n';
		}
		if (!bindAddr4.empty())
		{
			s += "\t" + STRING_F(CONNECTIVITY_BOUND_INTERFACE, 4 % bindAddr4);
			s += '\n';
		}
	}
	if (result & STATUS_IPV6)
	{
		s += "\t" + getModeString(incomingMode6, AF_INET6);
		s += '\n';
		IpAddress ip = getReflectedIP(AF_INET6);
		if (!Util::isEmpty(ip))
		{
			s += "\t" + STRING_F(CONNECTIVITY_EXTERNAL_IP, 6 % Util::printIpAddress(ip));
			s += '\n';
		}
		if (!bindAddr6.empty())
		{
			s += "\t" + STRING_F(CONNECTIVITY_BOUND_INTERFACE, 6 % bindAddr6);
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
	port = SearchManager::getLocalPort();
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
		{
			auto ss = SettingsManager::instance.getCoreSettings();
			ss->lockWrite();
			ss->setString(af == AF_INET6 ? Conf::MAPPER6 : Conf::MAPPER, mapper);
			ss->unlockWrite();
		}
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
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockWrite();
	ss->setInt(af == AF_INET6 ? Conf::INCOMING_CONNECTIONS6 : Conf::INCOMING_CONNECTIONS, Conf::INCOMING_FIREWALL_PASSIVE);
	ss->setBool(Conf::ALLOW_NAT_TRAVERSAL, true);
	ss->unlockWrite();
	log(STRING(CONN_DETECT_NO_ACTIVE_MODE), SEV_WARNING, af);
}

void ConnectivityManager::processPortTestResult() noexcept
{
	if (g_portTest.isRunning()) return;
	cs.lock();
	if (!(running & RUNNING_TEST_IPV4_PORTS))
	{
		cs.unlock();
		return;
	}
	running &= ~RUNNING_TEST_IPV4_PORTS;
	bool autoDetectFlag = autoDetect[0];
	autoDetect[0] = false;
	unsigned runningFlags = running;
	cs.unlock();
	if (autoDetectFlag)
	{
		int unused;
		if (g_portTest.getState(AppPorts::PORT_TCP, unused, nullptr) != PortTest::STATE_SUCCESS ||
		    g_portTest.getState(AppPorts::PORT_UDP, unused, nullptr) != PortTest::STATE_SUCCESS)
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
	if (g_ipTest.isRunning(req)) return;
	unsigned flag = req == IpTest::REQ_IP6 ? RUNNING_TEST_IPV6 : RUNNING_TEST_IPV4;
	bool completed = false;
	cs.lock();
	if (!(running & flag))
	{
		cs.unlock();
		return;
	}
	running &= ~flag;
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
	Conf::IPSettings ips;
	Conf::getIPSettings(ips, af == AF_INET6);

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const bool autoDetectFlag = ss->getBool(ips.autoDetect);
	const int connMode = ss->getInt(ips.incomingConnections);
	ss->unlockRead();

	bool fixedPort = af == AF_INET6 ||
		(!autoDetectFlag && (connMode == Conf ::INCOMING_FIREWALL_NAT || connMode == Conf::INCOMING_FIREWALL_PASSIVE));
	auto cm = ConnectionManager::getInstance();
	cm->stopServer(af);

	bool useTLS = CryptoManager::getInstance()->isInitialized();
	int type = ConnectionManager::SERVER_TYPE_TCP;
	if (useTLS && fixedPort)
	{
		ss->lockRead();
		if (ss->getInt(Conf::TLS_PORT) == ss->getInt(Conf::TCP_PORT))
			type = ConnectionManager::SERVER_TYPE_AUTO_DETECT;
		ss->unlockRead();
	}
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
				" listener: " + e.getError() + " (i=" + Util::toString(i) + ')');
			if (fixedPort || e.getErrorCode() != SE_EADDRINUSE || i)
				throw ListenerException("TCP", e.getErrorCode(), e.getError());
			ss->lockWrite();
			ss->setInt(Conf::TCP_PORT, 0);
			ss->unlockWrite();
			continue;
		}
		break;
	}
	if (useTLS && type != ConnectionManager::SERVER_TYPE_AUTO_DETECT)
	{
		ss->lockWrite();
		if (ss->getInt(Conf::TLS_PORT) == ss->getInt(Conf::TCP_PORT))
			ss->setInt(Conf::TLS_PORT, 0);
		ss->unlockWrite();
		for (int i = 0; i < 2; ++i)
		{
			try
			{
				cm->startListen(af, ConnectionManager::SERVER_TYPE_SSL);
			}
			catch (const SocketException& e)
			{
				LogManager::message("Could not start TLSv" + Util::toString(af == AF_INET6 ? 6 : 4) +
					" listener: " + e.getError() + " (i=" + Util::toString(i) + ')');
				if (fixedPort || e.getErrorCode() != SE_EADDRINUSE || i)
					throw ListenerException("TLS", e.getErrorCode(), e.getError());
				ss->lockWrite();
				ss->setInt(Conf::TLS_PORT, 0);
				ss->unlockWrite();
				continue;
			}
			break;
		}
	}
}

void ConnectivityManager::listenUDP(int af)
{
	Conf::IPSettings ips;
	Conf::getIPSettings(ips, af == AF_INET6);

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const bool autoDetectFlag = ss->getBool(ips.autoDetect);
	const int connMode = ss->getInt(ips.incomingConnections);
	ss->unlockRead();

	bool fixedPort = af == AF_INET6 ||
		(!autoDetectFlag && connMode == Conf::INCOMING_FIREWALL_NAT);
	for (int i = 0; i < 2; ++i)
	{
		try
		{
			SearchManager::getInstance()->listenUDP(af);
		}
		catch (const SocketException& e)
		{
			LogManager::message("Could not start UDPv" + Util::toString(af == AF_INET6 ? 6 : 4) +
				" listener: " + e.getError() + " (i=" + Util::toString(i) + ')');
			if (fixedPort || e.getErrorCode() != SE_EADDRINUSE || i)
				throw ListenerException("UDP", e.getErrorCode(), e.getError());
			ss->lockWrite();
			ss->setInt(Conf::UDP_PORT, 0);
			ss->unlockWrite();
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

void ConnectivityManager::setReflectedPort(int af, int what, int port) noexcept
{
	int index = getIndex(af);
	if (index == -1) return;
	LOCK(cs);
	reflectedPort[index * AppPorts::MAX_PORTS + what] = port;
}

int ConnectivityManager::getReflectedPort(int af, int what) const noexcept
{
	int index = getIndex(af);
	if (index == -1) return 0;
	LOCK(cs);
	return reflectedPort[index * AppPorts::MAX_PORTS + what];
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

void ConnectivityManager::checkReflectedPort(int& port, int af, int what) const noexcept
{
	int index = getIndex(af);
	cs.lock();
	int rport = reflectedPort[index * AppPorts::MAX_PORTS + what];
	cs.unlock();
	if (rport) port = rport;
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
