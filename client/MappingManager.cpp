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
#include "MappingManager.h"

#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "LogManager.h"
#include "TimeUtil.h"

#include "Mapper_MiniUPnPc.h"

#ifdef HAVE_NATPMP_H
#include "Mapper_NATPMP.h"
#endif

#include "SettingsManager.h"
#include "ResourceManager.h"
#include "SearchManager.h"
#include "ConfCore.h"
#include "version.h"

using namespace AppPorts;

struct MapperPortInfo
{
	int what;
	ResourceManager::Strings description;
};

static const MapperPortInfo portInfo[] =
{
	{ PORT_TCP, ResourceManager::MAPPING_TRANSFER           },
	{ PORT_TLS, ResourceManager::MAPPING_ENCRYPTED_TRANSFER },
	{ PORT_UDP, ResourceManager::MAPPING_SEARCH             }
};

static inline Mapper::Protocol getProtocol(int port)
{
	if (port == PORT_UDP) return Mapper::PROTOCOL_UDP;
	return Mapper::PROTOCOL_TCP;
}

MappingManager::MappingManager() : renewal(0), af(0)
{
	threadRunning.clear();
}

void MappingManager::init(int af)
{
	dcassert(this->af == 0);
	dcassert(af == AF_INET || af == AF_INET6);
	this->af = af;
#ifdef HAVE_NATPMP_H
	if (af == AF_INET) addMapper<Mapper_NATPMP>();
#endif
	addMapper<Mapper_MiniUPnPc>();
}

StringList MappingManager::getMappers() const
{
	StringList ret;
	for (auto &i : mappers)
		ret.push_back(i.first);
	return ret;
}

bool MappingManager::open()
{
	if (getOpened()) return false;

	if (mappers.empty())
	{
		log(STRING(MAPPER_NO_INTERFACE), SEV_ERROR);
		return false;
	}
	if (threadRunning.test_and_set())
	{
		log(STRING(MAPPER_IN_PROGRESS), SEV_INFO);
		return false;
	}

	start(0, "MappingManager");
	return true;
}

void MappingManager::close()
{
	join();

	if (renewal)
	{
		TimerManager::getInstance()->removeListener(this);
		renewal = 0;
	}

	if (working.get())
	{
		close(*working, false);
		working.reset();
	}
}

bool MappingManager::getOpened() const
{
	return working.get() ? true : false;
}

string MappingManager::formatDescription(const string &description, int type, int port)
{
	int protocol = getProtocol(type);
	return STRING_F(MAPPER_X_PORT_X, APPNAME % description % port % Mapper::protocols[protocol]);
}

int MappingManager::run()
{
	int ports[MAX_PORTS];
	ports[PORT_TCP] = ConnectionManager::getInstance()->getPort();
	ports[PORT_TLS] = ConnectionManager::getInstance()->getSecurePort();
	ports[PORT_UDP] = SearchManager::getInstance()->getLocalPort();
	cs.lock();
	if (ports[PORT_TLS] && ports[PORT_TLS] == ports[PORT_TCP])
	{
		ports[PORT_TLS] = 0;
		sharedTLSPort = true;
	}
	else
		sharedTLSPort = false;
	cs.unlock();

	if (renewal)
	{
		Mapper &mapper = *working;
		if (!mapper.init())
		{
			// can't renew; try again later.
			renewLater(mapper);
		}
		else
		{
			for (int i = 0; i < _countof(portInfo); ++i)
			{
				int what = portInfo[i].what;
				renew(mapper, ports[what], what, STRING_I(portInfo[i].description));
			}
			renewLater(mapper);
		}
		mapper.uninit();
		threadRunning.clear();
		return 0;
	}

	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	const string mapperName = ss->getString(af == AF_INET6 ? Conf::MAPPER6 : Conf::MAPPER);
	string bindAddr = ss->getString(af == AF_INET6 ? Conf::BIND_ADDRESS6 : Conf::BIND_ADDRESS);
	ss->unlockRead();

	if (!bindAddr.empty())
	{
		IpAddress ip;
		Util::parseIpAddress(ip, bindAddr);
		if (Util::isEmpty(ip)) bindAddr.clear();
	}

	// move the preferred mapper to front
	for (auto i = mappers.begin(); i != mappers.end(); ++i)
		if (i->first == mapperName)
		{
			if (i != mappers.begin())
			{
				auto mapper = *i;
				mappers.erase(i);
				mappers.insert(mappers.begin(), mapper);
			}
			break;
		}

	for (auto &i : mappers)
	{
		unique_ptr<Mapper> pMapper(i.second(bindAddr, af));
		Mapper &mapper = *pMapper;

		if (!mapper.init())
		{
			log(STRING_F(MAPPER_INIT_FAILED, mapper.getName()), SEV_WARNING);
			mapper.uninit(); // ???
			continue;
		}

		bool ok = true;
		for (int i = 0; i < _countof(portInfo); ++i)
		{
			int what = portInfo[i].what;
			if (!open(mapper, ports[what], what, STRING_I(portInfo[i].description)))
			{
				ok = false;
				break;
			}
		}
		if (!ok)
		{
			close(mapper, true);
			mapper.uninit();
			continue;
		}

		log(STRING_F(MAPPER_CREATING_SUCCESS_LONG,
			ports[PORT_TCP] % ports[PORT_TLS] % ports[PORT_UDP] % deviceString(mapper) % mapper.getName()), SEV_INFO);

		auto cm = ConnectivityManager::getInstance();
		working = move(pMapper);
		IpAddress externalIP = mapper.getExternalIP();
		if (externalIP.type == af)
		{
			cm->setReflectedIP(externalIP);
			getPublicPorts(ports);
		}
		else
		{
			memset(ports, 0, sizeof(ports));
			// no cleanup because the mappings work and hubs will likely provide the correct IP.
			log(STRING(MAPPER_IP_FAILED), SEV_WARNING);
		}

		for (int i = 0; i < MAX_PORTS; ++i)
			cm->setReflectedPort(af, i, ports[i]);
		cm->mappingFinished(mapper.getName(), af);

		renewLater(mapper);
		break;
	}

	if (!getOpened())
	{
		cs.lock();
		for (int i = 0; i < MAX_PORTS; i++)
		{
			mappings[i].state = STATE_FAILURE;
			mappings[i].publicPort = 0;
		}
		cs.unlock();
		log(STRING(MAPPER_CREATING_FAILED), SEV_ERROR);
		ConnectivityManager::getInstance()->mappingFinished(Util::emptyString, af);
	}
	
	threadRunning.clear();
	return 0;
}

void MappingManager::close(Mapper &mapper, bool quiet) noexcept
{
	bool ret = mapper.init();
	if (ret)
	{
		for (int i = 0; i < MAX_PORTS; i++)
		{
			cs.lock();
			int port = mappings[i].port;
			int state = mappings[i].state;
			mappings[i].state = STATE_UNKNOWN;
			mappings[i].publicPort = 0;
			cs.unlock();
			dcassert(state != STATE_RUNNING);
			if ((state == STATE_SUCCESS || state == STATE_RENEWAL_FAILURE) && !mapper.removeMapping(port, getProtocol(i)))
				ret = false;
		}
	}
	mapper.uninit();
	if (!quiet)
	{
		if (ret)
			log(STRING_F(MAPPER_REMOVING_SUCCESS, deviceString(mapper) % mapper.getName()), SEV_INFO);
		else
			log(STRING_F(MAPPER_REMOVING_FAILED, deviceString(mapper) % mapper.getName()), SEV_WARNING);
	}
}

void MappingManager::log(const string &message, Severity sev)
{
	ConnectivityManager::getInstance()->log(STRING(PORT_MAPPING) + ": " + message, sev, af);
}

string MappingManager::deviceString(Mapper &mapper) const
{
	string name(mapper.getDeviceName());
	if (name.empty()) name = STRING(GENERIC);
	return '"' + name + '"';
}

void MappingManager::renewLater(Mapper &mapper)
{
	int seconds = mapper.renewal();
	if (seconds)
	{
		bool addTimer = !renewal;
		renewal = GET_TICK() + std::max(seconds, 15) * 1000;
		if (addTimer)
			TimerManager::getInstance()->addListener(this);
	}
	else if (renewal)
	{
		renewal = 0;
		TimerManager::getInstance()->removeListener(this);
	}
}

int MappingManager::getState(int type) const noexcept
{
	cs.lock();
	if (type == PORT_TLS && sharedTLSPort)
		type = PORT_TCP;
	int state = mappings[type].state;
	cs.unlock();
	return state;
}

bool MappingManager::isRunning() const noexcept
{
	bool result = false;
	cs.lock();
	for (int i = 0; i < MAX_PORTS; i++)
		if (mappings[i].state == STATE_RUNNING)
		{
			result = true;
			break;
		}	
	cs.unlock();
	return result;
}

bool MappingManager::open(Mapper &mapper, int port, int type, const string &description) noexcept
{
	if (!port) return true;
	cs.lock();
	mappings[type].state = STATE_RUNNING;
	mappings[type].port = port;
	cs.unlock();
	bool result = mapper.addMapping(port, getProtocol(type), formatDescription(description, type, port));
	int publicPort = mapper.getExternalPort();
	cs.lock();
	if (result)
	{
		mappings[type].state = STATE_SUCCESS;
		mappings[type].publicPort = publicPort;
	}
	else
	{
		mappings[type].state = STATE_FAILURE;
		mappings[type].publicPort = 0;
	}
	cs.unlock();
	if (!result)
	{
		int protocol = getProtocol(type);
		log(STRING_F(MAPPER_INTERFACE_FAILED, description % port % Mapper::protocols[protocol] % mapper.getName()), SEV_WARNING);	
	}
	return result;
}

void MappingManager::renew(Mapper &mapper, int port, int type, const string &description) noexcept
{
	if (!port) return;
	cs.lock();
	if (mappings[type].state != STATE_SUCCESS && mappings[type].state != STATE_RENEWAL_FAILURE)
	{
		cs.unlock();
		return;
	}
	mappings[type].state = STATE_RUNNING;
	mappings[type].port = port;
	cs.unlock();
	bool result = mapper.addMapping(port, getProtocol(type), formatDescription(description, type, port));
	cs.lock();
	mappings[type].state = result ? STATE_SUCCESS : STATE_RENEWAL_FAILURE;
	mappings[type].port = port;
	cs.unlock();
}

void MappingManager::getPublicPorts(int ports[]) const noexcept
{
	cs.lock();
	for (int i = 0; i < MAX_PORTS; ++i)
		ports[i] = mappings[i].publicPort;
	cs.unlock();
}

void MappingManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (tick >= renewal && !threadRunning.test_and_set())
	{
		try
		{
			start(0, "MappingManager");
		}
		catch (const ThreadException &)
		{
			threadRunning.clear();
		}
	}
}
