#include "stdinc.h"
#include "SocketPool.h"
#include "SSLSocket.h"
#include "CryptoManager.h"
#include "Random.h"
#include "TimeUtil.h"

static const unsigned PORT_RANGE_MIN = 42000;
static const unsigned PORT_RANGE_MAX = 65535;

static const int BIND_ATTEMPTS = 16;

static const unsigned EXPIRE_TIME = 120 * 1000;
static const unsigned CHECK_TIME = 60 * 1000;

SocketPool socketPool;

SocketPool::SocketPool()
{
	removeTime = 0;
}

SocketPool::~SocketPool()
{
#ifdef _DEBUG
	for (auto i : socketsByPort)
		delete i.second->s;
#endif
}

uint16_t SocketPool::addSocket(const string& userKey, int af, bool serverRole, bool useTLS, bool allowUntrusted, const string& expKP) noexcept
{
	uint64_t expires = GET_TICK() + EXPIRE_TIME;
	uint16_t port = 0;
	csData.lock();
	auto i = socketsByUser.find(userKey);
	if (i != socketsByUser.end())
	{
		SocketInfoPtr& si = i->second;
		port = si->port;
		si->expires = expires;
		csData.unlock();
		return port;
	}
	csData.unlock();

	IpAddressEx addr;
	memset(&addr, 0, sizeof(addr));
	addr.type = af;
	Socket* sock;
	if (useTLS)
		sock = serverRole ?
			CryptoManager::getInstance()->getServerSocket(allowUntrusted) :
			CryptoManager::getInstance()->getClientSocket(allowUntrusted, expKP, Socket::PROTO_DEFAULT);
	else
		sock = new Socket;

	try
	{
		sock->create(af, Socket::TYPE_TCP);
	}
	catch (Exception&)
	{
		delete sock;
		return 0;
	}

	int attempts = BIND_ATTEMPTS;
	csData.lock();
	while (attempts)
	{
		port = Util::rand(PORT_RANGE_MIN, PORT_RANGE_MAX);
		if (socketsByPort.find(port) != socketsByPort.end())
		{
			attempts--;
			continue;
		}
		try
		{
			sock->bind(port, addr);
			break;
		}
		catch (Exception&)
		{
			attempts--;
		}
	}
	if (!attempts)
	{
		csData.unlock();
		delete sock;
		return 0;
	}

	SocketInfoPtr si = std::make_shared<SocketInfo>(SocketInfo{ sock, userKey, port, expires, af });
	auto p1 = socketsByPort.insert(std::make_pair(port, si));
	bool result = p1.second;
	if (result)
	{
		auto p2 = socketsByUser.insert(std::make_pair(userKey, si));
		if (!p2.second)
		{
			result = false;
			socketsByPort.erase(p1.first);
		}
	}
	csData.unlock();
	if (!result)
	{
		delete sock;
		return 0;
	}
	return port;
}

Socket* SocketPool::takeSocket(uint16_t port) noexcept
{
	Socket* sock = nullptr;
	csData.lock();
	auto i = socketsByPort.find(port);
	if (i != socketsByPort.end())
	{
		SocketInfoPtr& si = i->second;
		auto j = socketsByUser.find(si->userKey);
		if (j != socketsByUser.end()) socketsByUser.erase(j);
		sock = si->s;
		socketsByPort.erase(i);
	}
	csData.unlock();
	return sock;
}

bool SocketPool::getPortForUser(const string& userKey, uint16_t& port, int& af) const noexcept
{
	bool result = false;
	csData.lock();
	auto i = socketsByUser.find(userKey);
	if (i != socketsByUser.end())
	{
		port = i->second->port;
		af = i->second->af;
		result = true;
	}
	csData.unlock();
	return result;
}

void SocketPool::removeSocket(const string& userKey) noexcept
{
	csData.lock();
	auto i = socketsByUser.find(userKey);
	if (i != socketsByUser.end())
	{
		SocketInfoPtr& si = i->second;
		auto j = socketsByPort.find(si->port);
		if (j != socketsByPort.end()) socketsByPort.erase(j);
		try { delete si->s; }
		catch (Exception&) {}
		socketsByUser.erase(i);
	}
	csData.unlock();
}

void SocketPool::removeExpired(uint64_t tick) noexcept
{
	if (tick < removeTime) return;
	csData.lock();
	for (auto i = socketsByPort.begin(); i != socketsByPort.end();)
	{
		const SocketInfoPtr& si = i->second;
		if (si->expires < tick)
		{
			++i;
			continue;
		}
		auto j = socketsByUser.find(si->userKey);
		if (j != socketsByUser.end()) socketsByUser.erase(j);
		try { delete si->s; }
		catch (Exception&) {}
		i = socketsByPort.erase(i);
	}
	csData.unlock();
	removeTime = tick + CHECK_TIME;
}
