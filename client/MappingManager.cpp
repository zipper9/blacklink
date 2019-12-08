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
#include "MappingManager.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "CompatibilityManager.h"
#include "CryptoManager.h"
#include "LogManager.h"
#include "SearchManager.h"
#include "PortTest.h"

string MappingManager::g_externalIP;
string MappingManager::g_defaultGatewayIP;

MappingManager::MappingManager()
{
	g_defaultGatewayIP = Socket::getDefaultGateway();
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

string MappingManager::getPortmapInfo(bool p_show_public_ip)
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
	if (isRouter())
	{
		description += "+Router";
	}
	if (!getExternalIP().empty())
	{
		if (Util::isPrivateIp(getExternalIP()))
		{
			description += "+Private IP";
		}
		else
		{
			description += "+Public IP";
		}
		if (p_show_public_ip)
		{
			description += ": " + getExternalIP();
		}
	}
	int port;
	int state = g_portTest.getState(PortTest::PORT_UDP, port, nullptr);
	description += getTestResult("UDP", state, port);
	state = g_portTest.getState(PortTest::PORT_TCP, port, nullptr);
	description += getTestResult("TCP", state, port);
	/*
	if (CFlyServerJSON::isTestPortOK(SETTING(DHT_PORT), "udp"))
	    {
	        description += calcTestPortInfo("Torrent", SettingsManager::g_TestTorrentLevel, SETTING(DHT_PORT));
	    }
	*/
	if (CryptoManager::TLSOk() && SETTING(TLS_PORT) > 1024)
	{
		state = g_portTest.getState(PortTest::PORT_TLS, port, nullptr);
		description += getTestResult("TLS", state, port);
	}
	return description;
}