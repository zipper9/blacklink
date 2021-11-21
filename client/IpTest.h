#ifndef IP_TEST_H_
#define IP_TEST_H_

#include "HttpConnectionListener.h"
#include "Thread.h"
#include "TimerManager.h"
#include <regex>

class IpTest: private HttpConnectionListener, private TimerManagerListener
{
public:
	enum
	{
		REQ_IP4,
		REQ_IP6,
		MAX_REQ
	};

	enum
	{
		STATE_UNKNOWN,
		STATE_FAILURE,
		STATE_SUCCESS,
		STATE_RUNNING
	};

	IpTest();
	bool runTest(int type) noexcept;
	bool isRunning(int type) const noexcept;
	bool isRunning() const noexcept;
	int getState(int type, string* reflectedAddress) const noexcept;
	void shutdown() noexcept;

private:
	struct Request
	{
		int state;
		uint64_t timeout;
		uint64_t connID;
		string reflectedAddress;
		Request(): state(STATE_UNKNOWN), timeout(0), connID(0) {}
	};

	struct Connection
	{
		HttpConnection* conn;
		bool used;
	};

	Request req[MAX_REQ];
	mutable CriticalSection cs;
	uint64_t nextID;
	std::list<Connection> connections;
	bool hasListener;
	bool shutDown;
	string responseBody;
	const std::regex reflectedAddrRe;

	void setConnectionUnusedL(HttpConnection* conn) noexcept;

	void on(Data, HttpConnection*, const uint8_t*, size_t) noexcept;
	void on(Failed, HttpConnection*, const string&) noexcept;
	void on(Complete, HttpConnection*, const string&) noexcept;
	/*
	void on(Redirected, HttpConnection*, const string&) noexcept;
	*/

	void on(Second, uint64_t) noexcept;
};

extern IpTest g_ipTest;

#endif // IP_TEST_H_
