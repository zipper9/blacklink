/*
 * Copyright (C) 2001-2019 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include "BufferedSocketListener.h"
#include "HttpMessage.h"
#include <limits>
#include <atomic>

class BufferedSocket;
class HttpConnection;

class HttpClientCallback
{
public:
	virtual void onData(HttpConnection* conn, const uint8_t* data, size_t size) noexcept = 0;
	virtual void onFailed(HttpConnection* conn, const string& error) noexcept = 0;
	virtual void onCompleted(HttpConnection* conn, const string& requestUrl) noexcept = 0;
	virtual void onDisconnected(HttpConnection* conn) noexcept {}
};

class HttpConnection : BufferedSocketListener
{
public:
	enum Flags
	{
		FLAG_CLOSE_CONN = 1,
		FLAG_NO_CACHE   = 2
	};

	HttpConnection(int id, HttpClientCallback* client): id(id), client(client), receivingData(false) {}

	HttpConnection(const HttpConnection&) = delete;
	HttpConnection& operator= (const HttpConnection&) = delete;

	virtual ~HttpConnection();

	bool startRequest(uint64_t requestId, int type, const string& url, int flags = 0);
	void disconnect() noexcept;

	const string& getCurrentUrl() const { return currentUrl; }
	const string& getServer() const { return server; }
	uint16_t getPort() const { return port; }
	const string& getPath() const { return path; }
	void setUserAgent(const string& agent) { userAgent = agent; }
	void setMaxRespBodySize(int64_t size) { maxRespBodySize = size; }
	void setMaxErrorBodySize(int64_t size) { maxErrorBodySize = size; }
	void setRequestBody(const string& body, const string& type);
	void setRequestBody(string& body, const string& type);
	void setIfModified(time_t t) { ifModified = t; }
	void setProxyServer(const string& server) { proxyServer = server; }
	const string& getProxyServer() const { return proxyServer; }
	uint64_t getRequestId() const { return requestId; }

	uint64_t getID() const { return id; }
	void setIpVersion(int af) { ipVersion = af; }
	const Http::Response& getResponse() const { return resp; }
	int getResponseCode() const { return resp.getResponseCode(); }
	int getRequestType() const { return requestType; }
	bool isReceivingData() const { return receivingData; }
	static bool checkUrl(const string& url);
	static bool checkProtocol(const string& proto);

private:
	enum ConnectionStates
	{
		STATE_IDLE,
		STATE_SEND_REQUEST,
		STATE_PROCESS_RESPONSE,
		STATE_DATA,
		STATE_DATA_CHUNKED,
		STATE_FAILED
	};

	uint64_t id = 0;
	HttpClientCallback* const client;
	string currentUrl;
	string proxyServer;
	string proxyServerHost;
	string requestUri;

	// parsed URL components
	string server;
	string path;
	string query;
	uint16_t port = 80;

	int64_t bodySize = -1;
	int64_t receivedBodySize = 0;
	size_t receivedHeadersSize = 0;
	string requestBody;
	string requestBodyType;
	string userAgent;
	int64_t maxRespBodySize = std::numeric_limits<int64_t>::max();
	int64_t maxErrorBodySize = 128 * 1024;
	time_t ifModified = 0;
	int ipVersion = 0;
	int reqFlags = 0;

	ConnectionStates connState = STATE_IDLE;
	std::atomic_bool receivingData;
	int requestType = -1;
	uint64_t requestId = 0;
	Http::Response resp;

	BufferedSocket* socket = nullptr;

	void prepareRequest(int type) noexcept;
	void sendRequest() noexcept;
	void parseResponseHeader(const string &line) noexcept;
	void destroySocket() noexcept;
	void setFailedState(const string& error) noexcept;
	void setIdleState() noexcept;

	// BufferedSocketListener
	void onConnected() noexcept override;
	void onDataLine(const string&) noexcept override;
	void onData(const uint8_t*, size_t) noexcept override;
	void onModeChange() noexcept override;
	void onFailed(const string&) noexcept override;
};

#endif // !defined(HTTP_CONNECTION_H)
