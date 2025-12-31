#ifndef HTTP_SERVER_CONNECTION_H_
#define HTTP_SERVER_CONNECTION_H_

#include "BufferedSocketListener.h"
#include "HttpMessage.h"
#include "Socket.h"
#include <limits>
#include <atomic>

class BufferedSocket;
class HttpServerConnection;
class InputStream;

class HttpServerCallback
{
public:
	virtual void onRequest(HttpServerConnection* conn, const Http::Request& req) noexcept = 0;
	virtual void onData(HttpServerConnection* conn, const uint8_t* data, size_t size) noexcept {}
	virtual void onDisconnected(HttpServerConnection* conn) noexcept {}
	virtual void onError(HttpServerConnection* conn, const string& error) noexcept {}
};

class HttpServerConnection : BufferedSocketListener
{
public:
	HttpServerConnection(uint64_t id, HttpServerCallback* server, unique_ptr<Socket>& newSock, uint16_t port);

	HttpServerConnection(const HttpServerConnection&) = delete;
	HttpServerConnection& operator= (const HttpServerConnection&) = delete;

	virtual ~HttpServerConnection();

	void setMaxReqBodySize(int64_t size) { maxReqBodySize = size; }

	uint64_t getID() const { return id; }
	const string& getRequestBody() const { return requestBody; }
	void sendResponse(const Http::Response& resp, const string& body, InputStream* data = nullptr) noexcept;
	void disconnect() noexcept;
	bool getIp(IpAddress& ip) const noexcept;

private:
	enum ConnectionStates
	{
		STATE_RECEIVE_REQUEST,
		STATE_SEND_FILE,
		STATE_DATA,
		STATE_DATA_CHUNKED,
		STATE_FAILED
	};

	uint64_t id = 0;
	HttpServerCallback* const server;

	int64_t bodySize = -1;
	int64_t receivedBodySize = 0;
	size_t receivedHeadersSize = 0;
	int64_t maxReqBodySize = std::numeric_limits<int64_t>::max();
	InputStream* respBodyStream = nullptr;

	ConnectionStates connState = STATE_RECEIVE_REQUEST;
	Http::Request req;
	string requestBody;
	bool collectReqBody = true;

	BufferedSocket* socket = nullptr;

	void parseRequestHeader(const string &line) noexcept;
	void destroySocket() noexcept;
	void setFailedState(const string& error) noexcept;
	void setIdleState() noexcept;

	// BufferedSocketListener
	void onDataLine(const char*, size_t) noexcept override;
	void onData(const uint8_t*, size_t) noexcept override;
	void onModeChange() noexcept override;
	void onFailed(const string&) noexcept override;
	void onTransmitDone() noexcept override;
};

#endif // HTTP_SERVER_CONNECTION_H_
