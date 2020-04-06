#ifndef PORT_TEST_H
#define PORT_TEST_H

#include "HttpConnectionListener.h"
#include "CID.h"
#include "Thread.h"
#include "TimerManager.h"

class PortTest: private HttpConnectionListener, private TimerManagerListener
{
public:
	enum
	{
		PORT_UDP,
		PORT_TCP,
		PORT_TLS,
		MAX_PORTS
	};
	
	enum
	{
		STATE_UNKNOWN,
		STATE_FAILURE,
		STATE_SUCCESS,
		STATE_RUNNING
	};

	PortTest();
	bool runTest(int typeMask) noexcept;
	bool isRunning(int type) const noexcept;
	bool isRunning() const noexcept;
	int getState(int type, int& port, string* reflectedAddress) const noexcept;
	void setPort(int type, int port) noexcept;
	bool processInfo(int firstType, int lastType, int port, const string& reflectedAddress, const string& cid, bool checkCID = true) noexcept;
	void shutdown();

private:
	struct Port
	{
		int value;
		int state;
		uint64_t timeout;
		uint64_t connID;
		string cid;
		string reflectedAddress;
		Port(): value(0), state(STATE_UNKNOWN), timeout(0), connID(0) {}
	};
	
	struct Connection
	{
		HttpConnection* conn;
		bool used;
	};

	Port ports[MAX_PORTS];
	mutable CriticalSection cs;
	uint64_t nextID;
	std::list<Connection> connections;
	bool hasListener;
	bool shutDown;

	string createBody(const string& pid, const string& cid, int typeMask) const noexcept;
	void setConnectionUnusedL(HttpConnection* conn) noexcept;

	void on(Data, HttpConnection*, const uint8_t*, size_t) noexcept;
	void on(Failed, HttpConnection*, const string&) noexcept;
	void on(Complete, HttpConnection*, const string&) noexcept;
	/*
	void on(Redirected, HttpConnection*, const string&) noexcept;
	*/
	
	void on(Second, uint64_t) noexcept;
};

extern PortTest g_portTest;

#endif /* PORT_TEST_H */
