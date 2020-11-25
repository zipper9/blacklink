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
#include "dht/DHT.h"

ConnectivityManager::ConnectivityManager() : mapperV4(false), running(false), autoDetect(false), forcePortTest(false) {}

ConnectivityManager::~ConnectivityManager()
{
	mapperV4.close();
}

void ConnectivityManager::startSocket()
{
	disconnect();
	listen();
	// must be done after listen calls; otherwise ports won't be set
	if (SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_UPNP)
		mapperV4.open();
}

void ConnectivityManager::detectConnection()
{
	status.clear();
	
	const string savedBindAddress = SETTING(BIND_ADDRESS);
	// restore connectivity settings to their default value.
	SettingsManager::unset(SettingsManager::BIND_ADDRESS);
	
	disconnect();
	
	log(STRING(CONN_DETECT_START), SEV_INFO, TYPE_V4);
	SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_PASSIVE);
	try
	{
		listen();
	}
	catch (const Exception& e)
	{
		SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
		SET_SETTING(BIND_ADDRESS, savedBindAddress);
		log(STRING_F(UNABLE_TO_OPEN_PORT, e.getError()), SEV_ERROR, TYPE_V4);
		return;
	}
	
	const string ip = Util::getLocalOrBindIp(false);
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
	try
	{
		if (autoDetectFlag)
			detectConnection();
		else
			startSocket();
		if (SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_UPNP)
			mapperRunning = mapperV4.open();
	}
	catch (const Exception&)
	{
		cs.lock();
		running = false;
		cs.unlock();
		dcassert(0);
	}
	if (mapperRunning) return true;
	testPorts();
	if (!g_portTest.isRunning())
	{
		cs.lock();
		running = false;
		cs.unlock();
	}
	return true;
}

string ConnectivityManager::getInformation() const
{
	if (isSetupInProgress())
		return "Connectivity settings are being configured; try again later";
	
//	string autoStatus = ok() ? str(F_("enabled - %1%") % getStatus()) : "disabled";

	string mode;
	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
		{
			mode = "Active mode";
			break;
		}
		case SettingsManager::INCOMING_FIREWALL_UPNP:
		{
			break;
		}
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
		{
			mode = "Passive mode";
			break;
		}
	}
	
	auto field = [](const string & s)
	{
		return s.empty() ? "undefined" : s;
	};
	
	return str(dcpp_fmt(
	               "Connectivity information:\n"
	               "\tExternal IP (v4): %1%\n"
	               "\tBound interface (v4): %2%\n"
	               "\tTransfer port: %3%\n"
	               "\tEncrypted transfer port: %4%\n"
	               "\tSearch port: %5%\n"
#ifdef FLYLINKDC_USE_TORRENT
	               "\tTorrent port: %6%\n"
	               "\tTorrent SSL port: %6%\n"
#endif
	               "\tStatus: %7%"
	           ) %
	           field(SETTING(EXTERNAL_IP)) %
	           field(SETTING(BIND_ADDRESS)) %
	           field(Util::toString(ConnectionManager::getInstance()->getPort())) %
	           field(Util::toString(ConnectionManager::getInstance()->getSecurePort())) %
	           field(SearchManager::getSearchPort()) %
#ifdef FLYLINKDC_USE_TORRENT
	           field(Util::toString(DownloadManager::getInstance()->listen_torrent_port())) %
	           field(Util::toString(DownloadManager::getInstance()->ssl_listen_torrent_port())) %
#endif
	           field(status)
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
		if (autoDetectFlag)
		{
			if (mapper.empty())
			{
				//StrongDC++: don't disconnect when mapping fails else DHT and active mode in favorite hubs won't work
				//disconnect();
				setPassiveMode();
			}
		}
		log(getInformation(), SEV_INFO, TYPE_V4);
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
}

void ConnectivityManager::listen()
{
	bool fixedPort = !BOOLSETTING(AUTO_DETECT_CONNECTION) && SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_NAT;
	string exceptions;
	for (int i = 0; i < 5; ++i)
	{
		try
		{
			ConnectionManager::getInstance()->startListen();
		}
		catch (const SocketException& e)
		{
			LogManager::message("Could not start listener: error " + Util::toString(e.getErrorCode()) + " i=" + Util::toString(i));
			if (!fixedPort)
			{
				if (e.getErrorCode() == WSAEADDRINUSE)
				{
					if (i == 0)
					{
						SET_SETTING(TCP_PORT, 0);
						LogManager::message("Try bind free TCP Port");
						continue;
					}
					SettingsManager::generateNewTCPPort();
					LogManager::message("Try bind random TCP Port = " + Util::toString(SETTING(TCP_PORT)));
					continue;
				}
			}
			exceptions += " * TCP/TLS listen TCP Port = " + Util::toString(SETTING(TCP_PORT)) + " error = " + e.getError() + "\r\n";
			if (fixedPort)
			{
				// FIXME: ConnectivityManager should not show Message Boxes
				::MessageBox(nullptr, Text::toT(exceptions).c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR);
			}
		}
		break;
	}
	for (int j = 0; j < 2; ++j)
	{
		try
		{
			SearchManager::getInstance()->start();
			dht::DHT::getInstance()->start();
		}
		catch (const SocketException& e)
		{
			if (!fixedPort)
			{
				if (e.getErrorCode() == WSAEADDRINUSE)
				{
					if (j == 0)
					{
						SET_SETTING(UDP_PORT, 0);
						LogManager::message("Try bind free UDP Port");
						continue;
					}
					SettingsManager::generateNewUDPPort();
					LogManager::message("Try bind random UDP Port = " + Util::toString(SETTING(UDP_PORT)));
					continue;
				}
			}
			exceptions += " * UDP listen UDP Port = " + Util::toString(SETTING(UDP_PORT))  + " error = " + e.getError() + "\r\n";
			if (fixedPort)
			{
				// FIXME: ConnectivityManager should not show Message Boxes
				::MessageBox(nullptr, Text::toT(exceptions).c_str(), getAppNameVerT().c_str(), MB_OK | MB_ICONERROR);
			}
		}
		catch (const Exception& e)
		{
			exceptions += " * UDP listen UDP Port = " + Util::toString(SETTING(UDP_PORT)) + " error = " + e.getError() + "\r\n";
		}
		break;
	}
	
	if (!exceptions.empty())
	{
		throw Exception("ConnectivityManager::listen() error:\r\n" + exceptions);
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
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		status = message;
		LogManager::message(STRING(CONNECTIVITY) + ' ' + status);
	}
	else
	{
		LogManager::message(message);
	}
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

void ConnectivityManager::setReflectedIP(const string& ip)
{
	CFlyFastLock(cs);
	reflectedIP = ip;
}

string ConnectivityManager::getReflectedIP() const
{
	CFlyFastLock(cs);
	return reflectedIP;
}
