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

#include "stdinc.h"
#include "Mapper_NATPMP.h"
#include "BaseUtil.h"
#include "debug.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include "inet_compat.h"

extern "C"
{
#ifndef STATICLIB
#define STATICLIB
#endif
#include <natpmp/natpmp.h>
}

const string Mapper_NATPMP::name = "NAT-PMP";

static const int DEFAULT_LIFETIME = 3600;

Mapper_NATPMP::Mapper_NATPMP(const string &localIp, int af) : Mapper(localIp, af), lifetime(0), publicPort(0)
{
}

bool Mapper_NATPMP::supportsProtocol(int af) const
{
	return af == AF_INET;
}

static natpmp_t nat;

bool Mapper_NATPMP::init()
{
	if (initnatpmp(&nat, 0, 0) >= 0)
	{
		char buf[16] = {};
		inet_ntop_compat(AF_INET, (struct in_addr *) &nat.gateway, buf, sizeof(buf));
		gateway = buf;
		return true;
	}
	return false;
}

void Mapper_NATPMP::uninit()
{
	closenatpmp(&nat);
}

static int reqType(Mapper::Protocol protocol)
{
	if (protocol == Mapper::PROTOCOL_TCP)
		return NATPMP_PROTOCOL_TCP;
	dcassert(protocol == Mapper::PROTOCOL_UDP);
	return NATPMP_PROTOCOL_UDP;
}

static int respType(Mapper::Protocol protocol)
{
	if (protocol == Mapper::PROTOCOL_TCP)
		return NATPMP_RESPTYPE_TCPPORTMAPPING;
	dcassert(protocol == Mapper::PROTOCOL_UDP);
	return NATPMP_RESPTYPE_UDPPORTMAPPING;
}

static bool sendRequest(uint16_t port, Mapper::Protocol protocol, uint32_t lifetime)
{
	return sendnewportmappingrequest(&nat, reqType(protocol), port, port, lifetime) >= 0;
}

static bool read(natpmpresp_t &response)
{
	int res;
	do
	{
		// wait for the previous request to complete.
		timeval timeout;
		if (getnatpmprequesttimeout(&nat, &timeout) >= 0)
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(nat.s, &fds);
			select(FD_SETSIZE, &fds, 0, 0, &timeout);
		}

		res = readnatpmpresponseorretry(&nat, &response);
	} while (res == NATPMP_TRYAGAIN && nat.try_number <= 5); // don't wait for 9 failures as that takes too long.
	return res >= 0;
}

bool Mapper_NATPMP::addMapping(int port, Protocol protocol, const string &)
{
	if (sendRequest(static_cast<uint16_t>(port), protocol, DEFAULT_LIFETIME))
	{
		natpmpresp_t response;
		if (read(response) && response.type == respType(protocol) &&
		    response.pnu.newportmapping.privateport == port &&
		    response.pnu.newportmapping.mappedpublicport &&
		    response.pnu.newportmapping.lifetime)
		{
			lifetime = std::min(3600u, response.pnu.newportmapping.lifetime);
			publicPort = response.pnu.newportmapping.mappedpublicport;
			return true;
		}
	}
	return false;
}

bool Mapper_NATPMP::removeMapping(int port, Protocol protocol)
{
	if (sendRequest(static_cast<uint16_t>(port), protocol, 0))
	{
		natpmpresp_t response;
		if (read(response) && response.type == respType(protocol) &&
		    response.pnu.newportmapping.privateport == port &&
		    response.pnu.newportmapping.lifetime == 0)
		{
			lifetime = 0;
			publicPort = 0;
			return true;
		}
	}
	return false;
}

string Mapper_NATPMP::getDeviceName() const
{
	return gateway; // in lack of the router's name, give its IP.
}

IpAddress Mapper_NATPMP::getExternalIP()
{
	IpAddress addr{};
	if (sendpublicaddressrequest(&nat) >= 0)
	{
		natpmpresp_t response;
		if (read(response) && response.type == NATPMP_RESPTYPE_PUBLICADDRESS)
		{
			addr.data.v4 = ntohl(response.pnu.publicaddress.addr.s_addr);
			addr.type = AF_INET;
		}
	}
	return addr;
}
