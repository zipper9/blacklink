#include "stdinc.h"
#include "IpTest.h"
#include "HttpConnection.h"
#include "SettingsManager.h"
#include "ConnectivityManager.h"
#include "Ip4Address.h"
#include "Ip6Address.h"
#include "LogManager.h"
#include "SockDefs.h"

static const unsigned IP_TEST_TIMEOUT = 10000;

IpTest g_ipTest;

IpTest::IpTest(): nextID(0), hasListener(false), shutDown(false),
	reflectedAddrRe("Current IP Address:\\s*([a-fA-F0-9.:]+)", std::regex_constants::ECMAScript)
{
}

bool IpTest::runTest(int type) noexcept
{
	uint64_t id = ++nextID;
	HttpConnection* conn = new HttpConnection(id);

	uint64_t tick = GET_TICK();
	cs.lock();
	if (shutDown)
	{
		cs.unlock();
		delete conn;
		return false;
	}

	if (req[type].state == STATE_RUNNING && tick < req[type].timeout)
	{
		cs.unlock();
		delete conn;
		return false;
	}
	req[type].state = STATE_RUNNING;
	req[type].timeout = tick + IP_TEST_TIMEOUT;
	req[type].connID = id;

	connections.emplace_back(Connection{ conn, true });
	cs.unlock();

	string url = type == REQ_IP4 ? SETTING(URL_GET_IP) : SETTING(URL_GET_IP6);
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message(string("Detecting public IPv") + (type == REQ_IP4 ? '4' : '6') + " using URL " + url);

	responseBody.clear();
	conn->addListener(this);
	conn->setMaxBodySize(0x10000);
	conn->setMaxRedirects(0);
	conn->setUserAgent(getHttpUserAgent());
	conn->setIpVersion(type == REQ_IP4 ? AF_INET : AF_INET6);
	conn->downloadFile(url);
	return true;
}

bool IpTest::isRunning(int type) const noexcept
{
	cs.lock();
	bool result = req[type].state == STATE_RUNNING;
	cs.unlock();
	return result;
}

bool IpTest::isRunning() const noexcept
{
	bool result = false;
	cs.lock();
	for (int type = 0; type < MAX_REQ; type++)
		if (req[type].state == STATE_RUNNING)
		{
			result = true;
			break;
		}
	cs.unlock();
	return result;
}

int IpTest::getState(int type, string* reflectedAddress) const noexcept
{
	cs.lock();
	int state = req[type].state;
	if (reflectedAddress)
		*reflectedAddress = req[type].reflectedAddress;
	cs.unlock();
	return state;
}

void IpTest::setConnectionUnusedL(HttpConnection* conn) noexcept
{
	for (auto i = connections.begin(); i != connections.end(); ++i)
		if (i->conn == conn)
		{
			i->used = false;
			break;
		}
}

void IpTest::on(Data, HttpConnection*, const uint8_t* data, size_t size) noexcept
{
	responseBody.append(reinterpret_cast<const char*>(data), size);
}

void IpTest::on(Failed, HttpConnection* conn, const string&) noexcept
{
	bool hasFailed = false;
	bool addListener = false;
	cs.lock();
	for (int type = 0; type < MAX_REQ; type++)
		if (req[type].state == STATE_RUNNING && req[type].connID == conn->getID())
		{
			req[type].state = STATE_FAILURE;
			hasFailed = true;
		}
	setConnectionUnusedL(conn);
	if (!hasListener)
	{
		hasListener = true;
		addListener = true;
	}
	cs.unlock();
	if (addListener)
		TimerManager::getInstance()->addListener(this);
}

void IpTest::on(Complete, HttpConnection* conn, const string&) noexcept
{
	string newReflectedAddress[MAX_REQ];
	// Reset connID to prevent on(Failed) from signalling the error
	bool addListener = false;
	cs.lock();
	for (int type = 0; type < MAX_REQ; type++)
		if (req[type].state == STATE_RUNNING && req[type].connID == conn->getID())
		{
			req[type].state = STATE_FAILURE;
			req[type].connID = 0;
			req[type].reflectedAddress.clear();
			std::smatch sm;
			bool result = false;
			if (std::regex_search(responseBody, sm, reflectedAddrRe))
			{
				string s = sm[1].str();
				if (type == REQ_IP4)
				{
					Ip4Address addr;
					result = Util::parseIpAddress(addr, s) && addr != 0;
				}
				else
				{
					Ip6Address addr;
					result = Util::parseIpAddress(addr, s) && !Util::isEmpty(addr);
				}
				if (result)
				{
					newReflectedAddress[type] = req[type].reflectedAddress = s;
					req[type].state = STATE_SUCCESS;
				}
			}
			if (!result)
				req[type].reflectedAddress.clear();
		}
	if (!hasListener)
	{
		hasListener = true;
		addListener = true;
	}
	cs.unlock();
	if (addListener)
		TimerManager::getInstance()->addListener(this);
	for (int type = 0; type < MAX_REQ; type++)
		if (!newReflectedAddress[type].empty())
		{
			int af = type == REQ_IP4 ? AF_INET : AF_INET6;
			ConnectivityManager::getInstance()->setReflectedIP(af, newReflectedAddress[type]);
		}
}

void IpTest::on(Second, uint64_t tick) noexcept
{
	bool hasRunning = false;
	cs.lock();
	auto i = connections.begin();
	while (i != connections.end())
	{
		if (!i->used)
		{
			i->conn->removeListeners();
			delete i->conn;
			connections.erase(i++);
		} else i++;
	}
	for (int type = 0; type < MAX_REQ; type++)
		if (req[type].state == STATE_RUNNING)
		{
			if (tick > req[type].timeout)
				req[type].state = STATE_FAILURE;
			else
				hasRunning = true;
		}
	if (!hasRunning) hasListener = false;
	cs.unlock();
	if (!hasRunning)
		TimerManager::getInstance()->removeListener(this);
}

void IpTest::shutdown() noexcept
{
	cs.lock();
	shutDown = true;
	bool removeListener = hasListener;
	hasListener = false;
	for (auto i = connections.begin(); i != connections.end(); ++i)
	{
		i->conn->removeListeners();
		delete i->conn;
	}
	connections.clear();
	for (int type = 0; type < MAX_REQ; type++)
	{
		req[type].state = STATE_UNKNOWN;
		req[type].connID = 0;
	}
	cs.unlock();
	if (removeListener)
		TimerManager::getInstance()->removeListener(this);
}
