#include "stdinc.h"
#include "PortTest.h"
#include "TimeUtil.h"
#include "HttpClient.h"
#include "JsonFormatter.h"
#include "SettingsManager.h"
#include "ConnectivityManager.h"
#include "TigerHash.h"
#include "LogManager.h"
#include "ResourceManager.h"

static const unsigned PORT_TEST_TIMEOUT = 10000;

static const char* protoName[PortTest::MAX_PORTS] = { "UDP", "TCP", "TLS" };
static const string userAgent = "FlylinkDC++ r600-x64 build 22434";

PortTest g_portTest;

PortTest::PortTest(): hasListener(false), shutDown(false),
	reflectedAddrRe(R"|("ip"\s*:\s*"([^"]+)")|", std::regex_constants::ECMAScript)
{
}

bool PortTest::runTest(int typeMask) noexcept
{
	int portToTest[PortTest::MAX_PORTS];

	CID pid;
	pid.regenerate();
	TigerHash tiger;
	tiger.update(pid.data(), CID::SIZE);
	string strCID = CID(tiger.finalize()).toBase32();

	HttpClient::Request req;
	req.type = Http::METHOD_POST;
	req.url = SETTING(URL_PORT_TEST);
	req.requestBody = createBody(pid.toBase32(), strCID, typeMask);
	req.maxRedirects = 0;
	req.noCache = true;
	req.closeConn = true;
	req.userAgent = userAgent;
	req.maxErrorBodySize = req.maxRespBodySize = 64 * 1024;
	uint64_t id = httpClient.addRequest(req);
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

	for (int type = 0; type < MAX_PORTS; type++)
		if ((typeMask & 1<<type) && ports[type].state == STATE_RUNNING && tick < ports[type].timeout)
		{
			cs.unlock();
			httpClient.cancelRequest(id);
			return false;
		}
	uint64_t timeout = tick + PORT_TEST_TIMEOUT;
	for (int type = 0; type < MAX_PORTS; type++)
		if (typeMask & 1<<type)
		{
			ports[type].state = STATE_RUNNING;
			ports[type].timeout = timeout;
			ports[type].reqId = id;
			ports[type].cid = strCID;
			portToTest[type] = ports[type].value;
		} else portToTest[type] = 0;
	if (!hasListener)
		hasListener = addListener = true;
	cs.unlock();

	LogManager::message(STRING_F(PORT_TEST_STARTED, req.url));
	if (BOOLSETTING(LOG_SYSTEM))
	{
		for (int type = 0; type < MAX_PORTS; type++)
			if (portToTest[type])
				LogManager::message("Starting test for " + string(protoName[type]) + " port " + Util::toString(portToTest[type]), false);
	}

	if (addListener)
		addListeners();
	httpClient.startRequest(id);
	return true;
}

bool PortTest::isRunning(int type) const noexcept
{
	cs.lock();
	bool result = ports[type].state == STATE_RUNNING;
	cs.unlock();
	return result;
}

bool PortTest::isRunning() const noexcept
{
	bool result = false;
	cs.lock();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING)
		{
			result = true;
			break;
		}
	cs.unlock();
	return result;
}

int PortTest::getState(int type, int& port, string* reflectedAddress) const noexcept
{
	cs.lock();
	int state = ports[type].state;
	port = ports[type].value;
	if (reflectedAddress)
		*reflectedAddress = ports[type].reflectedAddress;
	cs.unlock();
	return state;
}

void PortTest::resetState(int typeMask) noexcept
{
	cs.lock();
	for (int type = 0; type < MAX_PORTS; type++)
		if ((typeMask & 1<<type) && ports[type].state != STATE_RUNNING)
			ports[type].state = STATE_UNKNOWN;
	cs.unlock();
}

void PortTest::getReflectedAddress(string& reflectedAddress) const noexcept
{
	cs.lock();
	reflectedAddress = reflectedAddrFromResponse;
	cs.unlock();
}

void PortTest::setPort(int type, int port) noexcept
{
	cs.lock();
	ports[type].value = port;
	cs.unlock();
}

bool PortTest::processInfo(int firstType, int lastType, int port, const string& reflectedAddress, const string& cid, bool checkCID) noexcept
{
	bool stateChanged = false;
	cs.lock();
	for (int type = firstType; type <= lastType; type++)
		if (ports[type].state == STATE_RUNNING && 
		    (port == 0 || ports[type].value == port) &&
		    (!checkCID || ports[type].cid == cid))
		{
			ports[type].state = STATE_SUCCESS;
			ports[type].reflectedAddress = reflectedAddress;
			stateChanged = true;
		}
	cs.unlock();
	return stateChanged;
}

string PortTest::createBody(const string& pid, const string& cid, int typeMask) const noexcept
{
	JsonFormatter f;
	f.open('{');
	f.appendKey("CID");
	f.appendStringValue(cid, false);
	f.appendKey("Client");
	f.appendStringValue(userAgent);
	f.appendKey("Name");
	f.appendStringValue("Manual", false);
	f.appendKey("PID");
	f.appendStringValue(pid, false);
	if (typeMask & 1<<PORT_TCP)
	{
		f.appendKey("tcp");
		f.open('[');
		f.open('{');
		f.appendKey("port");
		f.appendIntValue(ports[PORT_TCP].value);
		f.close('}');
		if ((typeMask & 1<<PORT_TLS) && ports[PORT_TLS].value != ports[PORT_TCP].value)
		{
			f.open('{');
			f.appendKey("port");
			f.appendIntValue(ports[PORT_TLS].value);
			f.close('}');
		}
		f.close(']');
	}
	if (typeMask & 1<<PORT_UDP)
	{
		f.appendKey("udp");
		f.open('[');
		f.open('{');
		f.appendKey("port");
		f.appendIntValue(ports[PORT_UDP].value);
		f.close('}');
		f.close(']');
	}
	f.close('}');
	return f.getResult();
}

void PortTest::on(Failed, uint64_t id, const string& error) noexcept
{
	bool hasFailed = false;
	bool addListener = false;
	cs.lock();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING && ports[type].reqId == id)
		{
			ports[type].state = STATE_FAILURE;
			hasFailed = true;
		}
	if (!hasListener)
	{
		hasListener = true;
		addListener = true;
	}
	cs.unlock();
	if (addListener)
		addListeners();
	if (hasFailed)
		ConnectivityManager::getInstance()->processPortTestResult();
}

void PortTest::on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept
{
	bool hasFailed = false;
	bool success = resp.getResponseCode() == 200;
	std::smatch sm;
	bool hasAddress = success && std::regex_search(data.responseBody, sm, reflectedAddrRe);

	bool addListener = false;
	cs.lock();
	if (hasAddress)
		reflectedAddrFromResponse = sm[1].str();
	else
		reflectedAddrFromResponse.clear();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING && ports[type].reqId == id)
		{
			ports[type].reqId = 0;
			if (!success)
			{
				ports[type].state = STATE_FAILURE;
				hasFailed = true;
			}
		}
	if (!hasListener)
	{
		hasListener = true;
		addListener = true;
	}
	cs.unlock();
	if (addListener)
		addListeners();
	if (hasFailed)
		ConnectivityManager::getInstance()->processPortTestResult();
}

void PortTest::on(Second, uint64_t tick) noexcept
{
	bool hasRunning = false;
	bool hasFailed = false;
	cs.lock();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING)
		{
			if (tick > ports[type].timeout)
			{
				ports[type].state = STATE_FAILURE;
				hasFailed = true;
			} else
				hasRunning = true;
		}	
	if (!hasRunning)
		hasListener = false;
	string reflectedAddress = reflectedAddrFromResponse;
	cs.unlock();
	if (!hasRunning)
	{
		removeListeners();
		if (hasFailed)
		{
			auto cm = ConnectivityManager::getInstance();
			if (!reflectedAddress.empty())
			{
				IpAddress ip;
				if (Util::parseIpAddress(ip, reflectedAddress))
					cm->setReflectedIP(ip);
			}
			cm->processPortTestResult();
		}
	}
}

void PortTest::shutdown()
{
	cs.lock();
	shutDown = true;
	bool removeListener = hasListener;
	hasListener = false;
	for (int type = 0; type < MAX_PORTS; type++)
	{
		ports[type].state = STATE_UNKNOWN;
		ports[type].reqId = 0;
	}
	cs.unlock();
	if (removeListener)
		removeListeners();
}

void PortTest::addListeners()
{
	httpClient.addListener(this);
	TimerManager::getInstance()->addListener(this);
}

void PortTest::removeListeners()
{
	httpClient.removeListener(this);
	TimerManager::getInstance()->removeListener(this);
}
