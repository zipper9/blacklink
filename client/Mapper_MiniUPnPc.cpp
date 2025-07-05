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
#include "Mapper_MiniUPnPc.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "NetworkUtil.h"
#include "UriUtil.h"
#include "Resolver.h"

extern "C"
{
#ifndef MINIUPNP_STATICLIB
#define MINIUPNP_STATICLIB
#endif
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
}

const string Mapper_MiniUPnPc::name = "MiniUPnP";

Mapper_MiniUPnPc::Mapper_MiniUPnPc(const string &localIp, int af) : Mapper(localIp, af)
{
}

bool Mapper_MiniUPnPc::supportsProtocol(int af) const
{
	return af == AF_INET || af == AF_INET6;
}

bool Mapper_MiniUPnPc::init()
{
	if (!url.empty()) return true;

#if MINIUPNPC_API_VERSION < 14
	UPNPDev *devices = upnpDiscover(2000, Util::isValidIp4(localIp) ? localIp.c_str() : nullptr, nullptr, 0, af == AF_INET6, nullptr);
#else
	UPNPDev *devices = upnpDiscover(2000, Util::isValidIp4(localIp) ? localIp.c_str() : nullptr, nullptr, 0, af == AF_INET6, 2, nullptr);
#endif
	if (!devices) return false;

	UPNPUrls urls;
	IGDdatas data;
	memset(&urls, 0, sizeof(urls));

#if MINIUPNPC_API_VERSION >= 18
	auto ret = UPNP_GetValidIGD(devices, &urls, &data, nullptr, 0, nullptr, 0);
#else
	auto ret = UPNP_GetValidIGD(devices, &urls, &data, nullptr, 0);
#endif

	bool ok = ret == 1;
	if (ok)
	{
		if (!(af == AF_INET6 ? Util::isValidIp6(localIp) : Util::isValidIp4(localIp)))
		{
			// We have no bind address configured in settings
			// Try to avoid choosing a random adapter for port mapping

			// Parse router IP from the control URL address
			string controlUrl = data.urlbase;
			if (controlUrl.empty()) controlUrl = urls.controlURL;

			Util::ParsedUrl url;
			Util::decodeUrl(controlUrl, url, "http");

			IpAddressEx addr;
			if (Resolver::resolveHost(addr, af, url.host))
			{
				vector<Util::AdapterInfo> adapters;
				Util::getNetworkAdapters(af, adapters);

				// Find a local IP that is within the same subnet
				auto p = std::find_if(adapters.cbegin(), adapters.cend(),
					[&addr](const Util::AdapterInfo &ai)
					{
						return Util::isSameNetwork(ai.ip, addr, ai.prefix);
					});
				if (p != adapters.end())
					localIp = Util::printIpAddress(p->ip);
			}
		}

		url = urls.controlURL;
		service = data.first.servicetype;
#ifdef MINIUPNPC_HAVE_FRIENDLY_NAME
		device = data.friendlyName;
#endif
	}

	FreeUPNPUrls(&urls);
	freeUPNPDevlist(devices);
	return ok;
}

void Mapper_MiniUPnPc::uninit()
{
}

bool Mapper_MiniUPnPc::addMapping(int port, Protocol protocol, const string &description)
{
	string portStr = Util::toString(port);
#ifdef HAVE_OLD_MINIUPNPC
	return UPNP_AddPortMapping(url.c_str(), service.c_str(), portStr.c_str(), portStr.c_str(), localIp.c_str(),
		description.c_str(), protocols[protocol], 0) == UPNPCOMMAND_SUCCESS;
#else
	return UPNP_AddPortMapping(url.c_str(), service.c_str(), portStr.c_str(), portStr.c_str(),
		localIp.c_str(), description.c_str(), protocols[protocol], nullptr, nullptr) == UPNPCOMMAND_SUCCESS;
#endif
}

bool Mapper_MiniUPnPc::removeMapping(int port, Protocol protocol)
{
	string portStr = Util::toString(port);
	return UPNP_DeletePortMapping(url.c_str(), service.c_str(), portStr.c_str(), protocols[protocol], nullptr) == UPNPCOMMAND_SUCCESS;
}

string Mapper_MiniUPnPc::getDeviceName() const
{
	return device;
}

IpAddress Mapper_MiniUPnPc::getExternalIP()
{
	char buf[256] = {};	
	IpAddress addr{};
	if (UPNP_GetExternalIPAddress(url.c_str(), service.c_str(), buf) == UPNPCOMMAND_SUCCESS)
		Util::parseIpAddress(addr, string(buf));
	return addr;
}
