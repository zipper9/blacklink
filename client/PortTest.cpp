#include "stdinc.h"
#include "PortTest.h"
#include "HttpConnection.h"
#include "SettingsManager.h"
#include "ConnectivityManager.h"
#include "LogManager.h"
#include "SockDefs.h"

static const unsigned PORT_TEST_TIMEOUT = 10000;

static const char* protoName[PortTest::MAX_PORTS] = { "UDP", "TCP", "TLS" };

PortTest g_portTest;

PortTest::PortTest(): nextID(0), hasListener(false), shutDown(false),
	reflectedAddrRe(R"|("ip"\s*:\s*"([^"]+)")|", std::regex_constants::ECMAScript)
{
}

bool PortTest::runTest(int typeMask) noexcept
{
	int portToTest[PortTest::MAX_PORTS];

	uint64_t id = ++nextID;
	HttpConnection* conn = new HttpConnection(id);

	CID pid;
	pid.regenerate();
	TigerHash tiger;
	tiger.update(pid.data(), CID::SIZE);
	string strCID = CID(tiger.finalize()).toBase32();

	uint64_t tick = GET_TICK();
	cs.lock();
	if (shutDown)
	{
		cs.unlock();
		delete conn;
		return false;
	}

	for (int type = 0; type < MAX_PORTS; type++)
		if ((typeMask & 1<<type) && ports[type].state == STATE_RUNNING && tick < ports[type].timeout)
		{
			cs.unlock();
			delete conn;
			return false;
		}
	uint64_t timeout = tick + PORT_TEST_TIMEOUT;
	for (int type = 0; type < MAX_PORTS; type++)
		if (typeMask & 1<<type)
		{
			ports[type].state = STATE_RUNNING;
			ports[type].timeout = timeout;
			ports[type].connID = id;
			ports[type].cid = strCID;
			portToTest[type] = ports[type].value;
		} else portToTest[type] = 0;
	string body = createBody(pid.toBase32(), strCID, typeMask);
	Connection ci;
	ci.conn = conn;
	ci.used = true;
	connections.push_back(ci);
	cs.unlock();

	if (BOOLSETTING(LOG_SYSTEM))
	{
		for (int type = 0; type < MAX_PORTS; type++)
			if (portToTest[type])
				LogManager::message("Starting test for " + string(protoName[type]) + " port " + Util::toString(portToTest[type]), false);
	}

	responseBody.clear();
	conn->addListener(this);
	conn->setMaxBodySize(0x10000);
	conn->setMaxRedirects(0);
	conn->setUserAgent(getHttpUserAgent());
	conn->postData(SETTING(URL_PORT_TEST), body);
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

struct JsonFormatter
{
public:
	JsonFormatter(): indent(0), expectValue(false), wantComma(false)
	{
	}
	
	const string& getResult() const { return s; }

	void open(char c)
	{
		if (expectValue)
		{
			s += '\n';
			expectValue = false;
		}
		else if (wantComma) s += ",\n";
		s.append(indent, '\t');
		s += c;
		s += '\n';
		indent++;
		wantComma = false;
	}

	void close(char c)
	{
		s += '\n';
		s.append(--indent, '\t');
		s += c;
		wantComma = true;
	}

	void appendKey(const char* key)
	{
		if (wantComma) s += ",\n";
		s.append(indent, '\t');
		s += '"';
		s += key;
		s += "\" : ";
		wantComma = false;
		expectValue = true;
	}

	void appendStringValue(const string& val)
	{
		s += '"';
		s += val;
		s += '"';
		wantComma = true;
		expectValue = false;
	}

	void appendIntValue(int val)
	{
		s += Util::toString(val);
		wantComma = true;
		expectValue = false;
	}

private:
	string s;
	int indent;
	bool expectValue;
	bool wantComma;
};

string PortTest::createBody(const string& pid, const string& cid, int typeMask) const noexcept
{
	JsonFormatter f;
	f.open('{');
	f.appendKey("CID");
	f.appendStringValue(cid);
	f.appendKey("Client");
	f.appendStringValue(getHttpUserAgent());
	f.appendKey("Name");
	f.appendStringValue("Manual");
	f.appendKey("PID");
	f.appendStringValue(pid);
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

void PortTest::setConnectionUnusedL(HttpConnection* conn) noexcept
{
	for (auto i = connections.begin(); i != connections.end(); ++i)
		if (i->conn == conn)
		{
			i->used = false;
			break;
		}
}

void PortTest::on(Data, HttpConnection*, const uint8_t* data, size_t size) noexcept
{
	responseBody.append(reinterpret_cast<const char*>(data), size);
}

void PortTest::on(Failed, HttpConnection* conn, const string&) noexcept
{
	bool hasFailed = false;
	bool addListener = false;
	cs.lock();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING && ports[type].connID == conn->getID())
		{
			ports[type].state = STATE_FAILURE;
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
	if (hasFailed)
		ConnectivityManager::getInstance()->processPortTestResult();
}

void PortTest::on(Complete, HttpConnection* conn, const string&) noexcept
{
	std::smatch sm;
	bool hasAddress = std::regex_search(responseBody, sm, reflectedAddrRe);

	// Reset connID to prevent on(Failed) from signalling the error
	bool addListener = false;
	cs.lock();
	if (hasAddress)
		reflectedAddrFromResponse = sm[1].str();
	else
		reflectedAddrFromResponse.clear();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING && ports[type].connID == conn->getID())
			ports[type].connID = 0;
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

void PortTest::on(Second, uint64_t tick) noexcept
{
	bool hasRunning = false;
	bool hasFailed = false;
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
		TimerManager::getInstance()->removeListener(this);
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
	for (auto i = connections.begin(); i != connections.end(); ++i)
	{
		i->conn->removeListeners();
		delete i->conn;
	}
	connections.clear();
	for (int type = 0; type < MAX_PORTS; type++)
	{
		ports[type].state = STATE_UNKNOWN;
		ports[type].connID = 0;
	}
	cs.unlock();
	if (removeListener)
		TimerManager::getInstance()->removeListener(this);
}
