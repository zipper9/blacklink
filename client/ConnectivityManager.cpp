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
#include "NetworkUtil.h"
#include "dht/DHT.h"

class ListenerException : public Exception
{
	public:
		ListenerException(const char* type, int errorCode) : type(type), errorCode(errorCode) {}
		const char* type;
		int errorCode;
};

ConnectivityManager::ConnectivityManager() : mapperV4(false), running(false), autoDetect(false), forcePortTest(false) {}

ConnectivityManager::~ConnectivityManager()
{
	mapperV4.close();
}

void ConnectivityManager::startSocket()
{
	disconnect();
	listen();
}

void ConnectivityManager::detectConnection()
{
	// restore connectivity settings to their default value.
	SettingsManager::unset(SettingsManager::BIND_ADDRESS);
	
	disconnect();
	
	log(STRING(CONN_DETECT_START), SEV_INFO, TYPE_V4);
	SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_PASSIVE);
	listen();

	const string ip = getLocalIP();
	if (Util::isPrivateIp(ip))
	{
		SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_UPNP);
		log(STRING(CONN_DETECT_LOCAL), SEV_INFO, TYPE_V4);
	}
	else
	{
		SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_DIRECT);
		log(STRING_F(CONN_DETECT_PUBLIC, ip), SEV_INFO, TYPE_V4);
	}
}

void ConnectivityManager::testPorts()
{
	bool force = false;
	cs.lock();
	std::swap(force, forcePortTest);
	cs.unlock();
	if (!force && !BOOLSETTING(AUTO_TEST_PORTS)) return;
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
	g_portTest.runTest(mask);
}

bool ConnectivityManager::setupConnections(bool forcePortTest)
{
	bool autoDetectFlag = BOOLSETTING(AUTO_DETECT_CONNECTION);
	cs.lock();
	if (running)
	{
		cs.unlock();
		return false;
	}
	this->forcePortTest = forcePortTest;
	running = true;
	autoDetect = autoDetectFlag;
	cs.unlock();

	bool mapperRunning = false;
	const string savedBindAddress = SETTING(BIND_ADDRESS);
	try
	{
		if (autoDetectFlag)
			detectConnection();
		else
			startSocket();
		if (SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_UPNP)
			mapperRunning = mapperV4.open();
	}
	catch (const ListenerException& e)
	{
		if (autoDetectFlag)
		{
			SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
			SET_SETTING(BIND_ADDRESS, savedBindAddress);
			log(STRING_F(UNABLE_TO_OPEN_PORT, e.getError()), SEV_ERROR, TYPE_V4);
		}
		cs.lock();
		running = false;
		cs.unlock();
		ConnectionManager::getInstance()->fireListenerFailed(e.type, e.errorCode);
		return false;
	}
	ConnectionManager::getInstance()->fireListenerStarted();
	if (mapperRunning) return true;
	testPorts();
	if (!g_portTest.isRunning())
	{
		cs.lock();
		running = false;
		cs.unlock();
		log(getInformation(), SEV_INFO, TYPE_V4);
	}
	return true;
}

string ConnectivityManager::getInformation() const
{
	if (isSetupInProgress())
		return "Connectivity settings are being configured; try again later";
	
	string mode;
	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
			mode = "Active mode (direct)";
			break;
		case SettingsManager::INCOMING_FIREWALL_UPNP:
			mode = "Active mode (automatic port forwarding)";
			break;
		case SettingsManager::INCOMING_FIREWALL_NAT:
			mode = "Active mode (manual port forwarding)";
			break;
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
			mode = "Passive mode";
			break;
	}
	
	auto field = [](const string& s) -> const string&
	{
		static const string undefined("undefined");
		return s.empty() ? undefined : s;
	};
	
	string externalIP = getReflectedIP();
	return str(dcpp_fmt(
	               "Connectivity information:\n"
				   "\tMode: %1%\n"
	               "\tExternal IP (v4): %2%\n"
	               "\tBound interface (v4): %3%\n"
	               "\tTransfer port: %4%\n"
	               "\tEncrypted transfer port: %5%\n"
	               "\tSearch port: %6%\n"
#ifdef FLYLINKDC_USE_TORRENT
	               "\tTorrent port: %7%\n"
	               "\tTorrent SSL port: %8%\n"
#endif
	           ) %
	           field(mode) %
			   field(externalIP) %
	           field(SETTING(BIND_ADDRESS)) %
	           field(Util::toString(ConnectionManager::getInstance()->getPort())) %
	           field(Util::toString(ConnectionManager::getInstance()->getSecurePort())) %
	           field(SearchManager::getSearchPort())
#ifdef FLYLINKDC_USE_TORRENT
	           % field(Util::toString(DownloadManager::getInstance()->listen_torrent_port()))
	           % field(Util::toString(DownloadManager::getInstance()->ssl_listen_torrent_port()))
#endif
	          );
}

void ConnectivityManager::mappingFinished(const string& mapper, bool /*v6*/)
{
	bool portTestRunning = false;
	if (!ClientManager::isBeforeShutdown())
	{
		cs.lock();
		bool autoDetectFlag = autoDetect;
		cs.unlock();
		// FIXME: we should perform a port test event when UPnP fails
		if (autoDetectFlag && mapper.empty())
			setPassiveMode();
		if (!mapper.empty())
		{
			SET_SETTING(MAPPER, mapper);
			testPorts();
			portTestRunning = g_portTest.isRunning();
		}
	}
	if (!portTestRunning)
	{
		cs.lock();
		running = false;
		cs.unlock();
		log(getInformation(), SEV_INFO, TYPE_V4);
	}
}

void ConnectivityManager::setPassiveMode()
{
	SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_PASSIVE);
	SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
	log(STRING(CONN_DETECT_NO_ACTIVE_MODE), SEV_WARNING, TYPE_V4);
}

void ConnectivityManager::processPortTestResult()
{
	if (g_portTest.isRunning()) return;
	cs.lock();
	if (!running)
	{
		cs.unlock();
		return;
	}
	bool autoDetectFlag = autoDetect;
	autoDetect = false;
	running = false;
	cs.unlock();
	if (autoDetectFlag)
	{
		int unused;
		if (g_portTest.getState(PortTest::PORT_TCP, unused, nullptr) != PortTest::STATE_SUCCESS ||
		    g_portTest.getState(PortTest::PORT_UDP, unused, nullptr) != PortTest::STATE_SUCCESS)
		{
			setPassiveMode();
		}
	}
	log(getInformation(), SEV_INFO, TYPE_V4);
}

void ConnectivityManager::listen()
{
	bool fixedPort = !BOOLSETTING(AUTO_DETECT_CONNECTION) && SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_NAT;
	auto cm = ConnectionManager::getInstance();
	cm->disconnect();
	
	bool useTLS = CryptoManager::TLSOk();
	int type = (useTLS && fixedPort && SETTING(TLS_PORT) == SETTING(TCP_PORT)) ?
		ConnectionManager::SERVER_TYPE_AUTO_DETECT : ConnectionManager::SERVER_TYPE_TCP;
	for (int i = 0; i < 2; ++i)
	{
		try
		{
			cm->startListen(type);
			cm->updateLocalIp();
		}
		catch (const SocketException& e)
		{
			LogManager::message("Could not start TCP listener: error " + Util::toString(e.getErrorCode()) + " i=" + Util::toString(i));
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
				cm->startListen(ConnectionManager::SERVER_TYPE_SSL);
			}
			catch (const SocketException& e)
			{
				LogManager::message("Could not start TLS listener: error " + Util::toString(e.getErrorCode()) + " i=" + Util::toString(i));
				if (fixedPort || e.getErrorCode() != SE_EADDRINUSE || i)
					throw ListenerException("TLS", e.getErrorCode());
				SET_SETTING(TLS_PORT, 0);
				continue;
			}
			break;
		}
	}
	for (int i = 0; i < 2; ++i)
	{
		try
		{
			SearchManager::getInstance()->start();
			dht::DHT::getInstance()->start();
		}
		catch (const SocketException& e)
		{
			LogManager::message("Could not start UDP listener: error " + Util::toString(e.getErrorCode()) + " i=" + Util::toString(i));
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
	mapperV4.close();
	SearchManager::getInstance()->shutdown();
	ConnectionManager::getInstance()->disconnect();
}

void ConnectivityManager::log(const string& message, Severity sev, int type)
{
	// TODO: show connectivity information to the user
	LogManager::message(message, false);
}

bool ConnectivityManager::isSetupInProgress() const noexcept
{
	cs.lock();
	bool result = running;
	cs.unlock();
	return result;
}

static string getTestResult(const string& name, int state, int port)
{
	string result = "," + name + ":" + Util::toString(port);
	if (state == PortTest::STATE_SUCCESS)
		result += "(+)";
	else if (state == PortTest::STATE_FAILURE)
		result += "(-)";
	else
		result.clear();
	return result;
}

string ConnectivityManager::getPortmapInfo(bool showPublicIp) const
{
	string description;
	description = "Mode:";
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		description = "+Auto";
	}
	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
			description += "+Direct";
			break;
		case SettingsManager::INCOMING_FIREWALL_UPNP:
			description += "+UPnP";
			break;
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
			description += "+Passive";
			break;
		case SettingsManager::INCOMING_FIREWALL_NAT:
			description += "+NAT+Manual";
			break;
		default:
			dcassert(0);
	}
	/*
	if (isRouter())
	{
		description += "+Router";
	}
	*/
	cs.lock();
	if (!reflectedIP.empty())
	{
		if (Util::isPrivateIp(reflectedIP))
		{
			description += "+Private IP";
		}
		else
		{
			description += "+Public IP";
		}
		if (showPublicIp)
		{
			description += ": " + reflectedIP;
		}
	}
	cs.unlock();
	int port;
	int state = g_portTest.getState(PortTest::PORT_UDP, port, nullptr);
	description += getTestResult("UDP", state, port);
	state = g_portTest.getState(PortTest::PORT_TCP, port, nullptr);
	description += getTestResult("TCP", state, port);
	if (CryptoManager::TLSOk() && SETTING(TLS_PORT) > 1024)
	{
		state = g_portTest.getState(PortTest::PORT_TLS, port, nullptr);
		description += getTestResult("TLS", state, port);
	}
	return description;
}

void ConnectivityManager::setReflectedIP(const string& ip) noexcept
{
	LOCK(cs);
	reflectedIP = ip;
}

string ConnectivityManager::getReflectedIP() const noexcept
{
	LOCK(cs);
	return reflectedIP;
}

void ConnectivityManager::setLocalIP(const string& ip) noexcept
{
	LOCK(cs);
	localIP = ip;
}

string ConnectivityManager::getLocalIP() const noexcept
{
	LOCK(cs);
	return localIP;
}
