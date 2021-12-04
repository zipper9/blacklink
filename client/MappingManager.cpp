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

#include "Mapper_MiniUPnPc.h"

#ifdef HAVE_NATPMP_H
#include "Mapper_NATPMP.h"
#endif

#include "ResourceManager.h"
#include "SearchManager.h"

static inline Mapper::Protocol getProtocol(int port)
{
	if (port == MappingManager::PORT_UDP) return Mapper::PROTOCOL_UDP;
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
	int portTCP = ConnectionManager::getInstance()->getPort();
	int portTLS = ConnectionManager::getInstance()->getSecurePort();
	int portUDP = SearchManager::getInstance()->getUdpPort();
	cs.lock();
	if (portTLS && portTLS == portTCP)
	{
		portTLS = 0;
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
			renew(mapper, portTCP, PORT_TCP, STRING(MAPPING_TRANSFER));
			renew(mapper, portTLS, PORT_TLS, STRING(MAPPING_ENCRYPTED_TRANSFER));
			renew(mapper, portUDP, PORT_UDP, STRING(MAPPING_SEARCH));
		
			renewLater(mapper);
		}
		mapper.uninit();
		threadRunning.clear();
		return 0;
	}

	// move the preferred mapper to front
	const auto &mapperName = SettingsManager::get(af == AF_INET6 ? SettingsManager::MAPPER6 : SettingsManager::MAPPER);
	for (auto i = mappers.begin(); i != mappers.end(); ++i)
	{
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
	}

	for (auto &i : mappers)
	{
		auto setting = af == AF_INET6 ? SettingsManager::BIND_ADDRESS6 : SettingsManager::BIND_ADDRESS;
		unique_ptr<Mapper> pMapper(i.second(SettingsManager::getInstance()->isDefault(setting) ?
		                           Util::emptyString : SettingsManager::getInstance()->get(setting),
		                           af));
		Mapper &mapper = *pMapper;

		if (!mapper.init())
		{
			log(STRING_F(MAPPER_INIT_FAILED, mapper.getName()), SEV_WARNING);
			mapper.uninit(); // ???
			continue;
		}

		if (!(open(mapper, portTCP, PORT_TCP, STRING(MAPPING_TRANSFER)) &&
		      open(mapper, portTLS, PORT_TLS, STRING(MAPPING_ENCRYPTED_TRANSFER)) &&
			  open(mapper, portUDP, PORT_UDP, STRING(MAPPING_SEARCH))))
		{
			close(mapper, true);
			mapper.uninit();
			continue;
		}

		log(STRING_F(MAPPER_CREATING_SUCCESS_LONG, portTCP % portTLS % portUDP % deviceString(mapper) % mapper.getName()), SEV_INFO);

		working = move(pMapper);
		string externalIP = mapper.getExternalIP();
		if (!externalIP.empty())
		{
			IpAddress ip;
			if (Util::parseIpAddress(ip, externalIP))
				ConnectivityManager::getInstance()->setReflectedIP(ip);
		}
		else
		{
			// no cleanup because the mappings work and hubs will likely provide the correct IP.
			log(STRING(MAPPER_IP_FAILED), SEV_WARNING);
		}

		ConnectivityManager::getInstance()->mappingFinished(mapper.getName(), af);

		renewLater(mapper);
		break;
	}

	if (!getOpened())
	{
		cs.lock();
		for (int i = 0; i < MAX_PORTS; i++)
			mappings[i].state = STATE_FAILURE;
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
		renewal = GET_TICK() + std::max(seconds, 30) * 1000;
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
	cs.lock();
	mappings[type].state = result ? STATE_SUCCESS : STATE_FAILURE;
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
