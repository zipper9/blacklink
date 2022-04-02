#ifndef PORT_TEST_H
#define PORT_TEST_H

#include "HttpClientListener.h"
#include "CID.h"
#include "Locks.h"
#include "TimerManager.h"
#include <regex>

class PortTest: private HttpClientListener, private TimerManagerListener
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
	void getReflectedAddress(string& reflectedAddress) const noexcept;
	void setPort(int type, int port) noexcept;
	bool processInfo(int firstType, int lastType, int port, const string& reflectedAddress, const string& cid, bool checkCID = true) noexcept;
	void resetState(int typeMask) noexcept;
	void shutdown();

private:
	struct Port
	{
		int value;
		int state;
		uint64_t timeout;
		uint64_t reqId;
		string cid;
		string reflectedAddress;
		Port(): value(0), state(STATE_UNKNOWN), timeout(0), reqId(0) {}
	};
	
	Port ports[MAX_PORTS];
	mutable CriticalSection cs;
	bool hasListener;
	bool shutDown;
	string reflectedAddrFromResponse;
	const std::regex reflectedAddrRe;

	string createBody(const string& pid, const string& cid, int typeMask) const noexcept;
	void addListeners();
	void removeListeners();

	void on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept;
	void on(Failed, uint64_t id, const string& error) noexcept;
	void on(Redirected, uint64_t id, const string& redirUrl) noexcept {}

	void on(Second, uint64_t) noexcept;
};

extern PortTest g_portTest;

#endif /* PORT_TEST_H */
