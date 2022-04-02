#ifndef IP_TEST_H_
#define IP_TEST_H_

#include "HttpClientListener.h"
#include "Locks.h"
#include "TimerManager.h"
#include <regex>

class IpTest: private HttpClientListener, private TimerManagerListener
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
		uint64_t reqId;
		string reflectedAddress;
		Request(): state(STATE_UNKNOWN), timeout(0), reqId(0) {}
	};

	Request req[MAX_REQ];
	mutable CriticalSection cs;
	bool hasListener;
	bool shutDown;
	const std::regex reflectedAddrRe;

	void addListeners();
	void removeListeners();

	void on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept;
	void on(Failed, uint64_t id, const string& error) noexcept;
	void on(Redirected, uint64_t id, const string& redirUrl) noexcept {}

	void on(Second, uint64_t) noexcept;
};

extern IpTest g_ipTest;

#endif // IP_TEST_H_
