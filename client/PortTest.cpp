#include "stdinc.h"
#include "PortTest.h"
#include "TimerManager.h"
#include "HttpConnection.h"
#include "SettingsManager.h"
#include "LogManager.h"

static const unsigned PORT_TEST_TIMEOUT = 10000;

static const string testURL("http://test2.fly-server.ru:37015/fly-test-port");

static const char* protoName[PortTest::MAX_PORTS] = { "UDP", "TCP", "TLS" };

PortTest g_portTest;

PortTest::PortTest(): nextID(0)
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

	conn->addListener(this);
	conn->setMaxBodySize(0x10000);
	conn->setMaxRedirects(0);
	conn->setUserAgent(getHttpUserAgent());
	conn->postData(testURL, body);
	return true;
}

bool PortTest::isRunning(int type) const noexcept
{
	cs.lock();
	if (ports[type].state != STATE_RUNNING)
	{
		cs.unlock();
		return false;
	}
	uint64_t timeout = ports[type].timeout;
	cs.unlock();
	return GET_TICK() < timeout;
}

int PortTest::getState(int type, int& port, string* reflectedAddress) const noexcept
{
	cs.lock();
	int state = ports[type].state;
	uint64_t timeout = ports[type].timeout;
	port = ports[type].value;
	if (reflectedAddress)
		*reflectedAddress = ports[type].reflectedAddress;
	cs.unlock();
	if (state == STATE_RUNNING)
	{
		if (GET_TICK() < timeout) return STATE_RUNNING;
		state = STATE_FAILURE;
	}
	return state;
}

void PortTest::setPort(int type, int port) noexcept
{
	cs.lock();
	ports[type].value = port;
	cs.unlock();
}

void PortTest::processInfo(int type, const string& reflectedAddress, const string& cid, bool checkCID) noexcept
{
	cs.lock();
	if (ports[type].state == STATE_RUNNING && GET_TICK() < ports[type].timeout
	    && (!checkCID || ports[type].cid == cid))
	{
		ports[type].state = STATE_SUCCESS;
		ports[type].reflectedAddress = reflectedAddress;
	}
	cs.unlock();
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
		if (typeMask & 1<<PORT_TLS)
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

void PortTest::setConnectionUsed(HttpConnection* conn, bool used) noexcept
{
	for (auto i = connections.begin(); i != connections.end(); ++i)
		if (i->conn == conn)
		{
			i->used = used;
			break;
		}
}

void PortTest::on(Data, HttpConnection*, const uint8_t*, size_t) noexcept
{
}

void PortTest::on(Failed, HttpConnection* conn, const string&) noexcept
{
	cs.lock();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING && ports[type].connID == conn->getID())
			ports[type].state = STATE_FAILURE;
	setConnectionUsed(conn, false);
	cs.unlock();
}

void PortTest::on(Complete, HttpConnection* conn, const string&) noexcept
{
	// Reset connID to prevent on(Failed) from signalling the error
	cs.lock();
	for (int type = 0; type < MAX_PORTS; type++)
		if (ports[type].state == STATE_RUNNING && ports[type].connID == conn->getID())
			ports[type].connID = 0;
	setConnectionUsed(conn, false);
	cs.unlock();
}

void PortTest::removeUnusedConnections() noexcept
{
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
	cs.unlock();
}
