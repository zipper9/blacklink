#include "stdinc.h"
#include "IpTest.h"
#include "TimeUtil.h"
#include "HttpClient.h"
#include "SettingsManager.h"
#include "ConnectivityManager.h"
#include "Resolver.h"
#include "LogManager.h"
#include "ResourceManager.h"
#include "ConfCore.h"

static const unsigned IP_TEST_TIMEOUT = 10000;

IpTest g_ipTest;

IpTest::IpTest(): hasListener(false), shutDown(false),
	reflectedAddrRe("Current IP Address:\\s*([a-fA-F0-9.:]+)", std::regex_constants::ECMAScript),
	commandCallback(nullptr)
{
}

bool IpTest::runTest(int type, uint64_t frameId, string* message) noexcept
{
	HttpClient::Request cr;
	auto ss = SettingsManager::instance.getCoreSettings();
	ss->lockRead();
	string url = ss->getString(type == REQ_IP4 ? Conf::URL_GET_IP : Conf::URL_GET_IP6);
	cr.userAgent = ss->getString(Conf::HTTP_USER_AGENT);
	ss->unlockRead();

	cr.type = Http::METHOD_GET;
	cr.url = url;
	cr.ipVersion = (type == REQ_IP4 ? AF_INET : AF_INET6) | Resolver::RESOLVE_TYPE_EXACT;
	cr.maxRedirects = 0;
	cr.noCache = true;
	cr.closeConn = true;
	cr.maxErrorBodySize = cr.maxRespBodySize = 64 * 1024;
	uint64_t id = httpClient.addRequest(cr);
	if (!id) return false;

	bool addListener = false;
	uint64_t tick = GET_TICK();
	cs.lock();
	if (shutDown)
	{
		cs.unlock();
		httpClient.cancelRequest(id);
		return false;
	}
	if (req[type].state == STATE_RUNNING && tick < req[type].timeout)
	{
		cs.unlock();
		httpClient.cancelRequest(id);
		return false;
	}
	req[type].state = STATE_RUNNING;
	req[type].timeout = tick + IP_TEST_TIMEOUT;
	req[type].reqId = id;
	req[type].frameId = frameId;
	if (!hasListener)
		hasListener = addListener = true;
	cs.unlock();

	string str = STRING_F(PORT_TEST_GETTING_IP, (type == REQ_IP4 ? 4 : 6) % url);
	LogManager::message(str);
	if (message) *message = std::move(str);

	if (addListener)
		addListeners();
	httpClient.startRequest(id);
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

void IpTest::on(Failed, uint64_t id, const string& error) noexcept
{
	bool addListener = false;
	int failedReqType = -1;
	uint64_t frameId = 0;
	cs.lock();
	for (int type = 0; type < MAX_REQ; type++)
		if (req[type].state == STATE_RUNNING && req[type].reqId == id)
		{
			failedReqType = type;
			req[type].state = STATE_FAILURE;
			frameId = req[type].frameId;
		}
	if (!hasListener)
		hasListener = addListener = true;
	cs.unlock();
	if (addListener)
		addListeners();
	if (failedReqType != -1)
		ConnectivityManager::getInstance()->processGetIpResult(failedReqType);
	if (frameId && commandCallback)
		commandCallback->onCommandCompleted(SEV_ERROR, frameId, STRING(PORT_TEST_GETIP_FAILED));
}

void IpTest::on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept
{
	IpAddress newReflectedAddress[MAX_REQ];
	memset(newReflectedAddress, 0, sizeof(newReflectedAddress));
	bool addListener = false;
	int completedReqType = -1;
	uint64_t frameId = 0;
	cs.lock();
	for (int type = 0; type < MAX_REQ; type++)
		if (req[type].state == STATE_RUNNING && req[type].reqId == id)
		{
			completedReqType = type;
			req[type].state = STATE_FAILURE;
			req[type].reqId = 0;
			req[type].reflectedAddress.clear();
			frameId = req[type].frameId;
			std::smatch sm;
			bool result = false;
			if (resp.getResponseCode() == 200 && std::regex_search(data.responseBody, sm, reflectedAddrRe))
			{
				string s = sm[1].str();
				if (type == REQ_IP4)
				{
					Ip4Address addr;
					result = Util::parseIpAddress(addr, s) && addr != 0;
					if (result)
					{
						newReflectedAddress[type].type = AF_INET;
						newReflectedAddress[type].data.v4 = addr;
					}
				}
				else
				{
					Ip6Address addr;
					result = Util::parseIpAddress(addr, s) && !Util::isEmpty(addr);
					if (result)
					{
						newReflectedAddress[type].type = AF_INET6;
						newReflectedAddress[type].data.v6 = addr;
					}
				}
				if (result)
				{
					req[type].reflectedAddress = std::move(s);
					req[type].state = STATE_SUCCESS;
				}
			}
			if (!result)
				req[type].reflectedAddress.clear();
		}
	if (!hasListener)
		hasListener = addListener = true;
	cs.unlock();
	if (addListener)
		addListeners();
	if (completedReqType != -1)
	{
		auto cm = ConnectivityManager::getInstance();
		string publicIp;
		if (newReflectedAddress[completedReqType].type)
		{
			publicIp = Util::printIpAddress(newReflectedAddress[completedReqType]);
			cm->setReflectedIP(newReflectedAddress[completedReqType]);
		}
		cm->processGetIpResult(completedReqType);
		if (frameId && commandCallback)
		{
			if (publicIp.empty())
				commandCallback->onCommandCompleted(SEV_ERROR, frameId, STRING(PORT_TEST_GETIP_FAILED));
			else
				commandCallback->onCommandCompleted(SEV_INFO, frameId, STRING_F(PORT_TEST_IP_RESULT, publicIp));
		}
	}
}

void IpTest::on(Second, uint64_t tick) noexcept
{
	bool hasRunning = false;
	uint64_t frameId = 0;
	cs.lock();
	for (int type = 0; type < MAX_REQ; type++)
		if (req[type].state == STATE_RUNNING)
		{
			if (tick > req[type].timeout)
			{
				req[type].state = STATE_FAILURE;
				frameId = req[type].frameId;
			}
			else
				hasRunning = true;
		}
	if (!hasRunning) hasListener = false;
	cs.unlock();
	if (!hasRunning)
		removeListeners();
	if (frameId && commandCallback)
		commandCallback->onCommandCompleted(SEV_ERROR, frameId, STRING(PORT_TEST_GETIP_FAILED));
}

void IpTest::shutdown() noexcept
{
	cs.lock();
	shutDown = true;
	bool removeListener = hasListener;
	hasListener = false;
	for (int type = 0; type < MAX_REQ; type++)
	{
		req[type].state = STATE_UNKNOWN;
		req[type].reqId = 0;
	}
	cs.unlock();
	if (removeListener)
		removeListeners();
}

void IpTest::addListeners()
{
	httpClient.addListener(this);
	TimerManager::getInstance()->addListener(this);
}

void IpTest::removeListeners()
{
	httpClient.removeListener(this);
	TimerManager::getInstance()->removeListener(this);
}
