#ifndef PORT_TEST_H
#define PORT_TEST_H

#include "HttpConnectionListener.h"
#include "CID.h"
#include "CFlyThread.h"

class PortTest: private HttpConnectionListener
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
	int getState(int type, int& port) const noexcept;
	void setPort(int type, int port) noexcept;
	void processInfo(int type, const string& cid, bool checkCID = true) noexcept;
	void removeUnusedConnections() noexcept; // call on timer to release resources
	
private:
	struct Port
	{
		int value;
		int state;
		uint64_t timeout;
		uint64_t connID;
		string cid;
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

	string createBody(const CID& cid, int typeMask) const noexcept;
	void setConnectionUsed(HttpConnection* conn, bool used) noexcept;

	void on(Data, HttpConnection*, const uint8_t*, size_t) noexcept;
	void on(Failed, HttpConnection*, const string&) noexcept;
	void on(Complete, HttpConnection*, const string&) noexcept;
	/*
	void on(Redirected, HttpConnection*, const string&) noexcept;
	*/
};

extern PortTest g_portTest;

#endif /* PORT_TEST_H */