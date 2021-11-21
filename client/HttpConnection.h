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
#include "HttpConnectionListener.h"
#include "HttpMessage.h"
#include "Speaker.h"
#include <limits>

class BufferedSocket;

class HttpConnection : BufferedSocketListener, public Speaker<HttpConnectionListener>
{
public:
	HttpConnection(int id): id(id) {}
	HttpConnection(const HttpConnection&) = delete;
	HttpConnection& operator= (const HttpConnection&) = delete;

	virtual ~HttpConnection() { detachSocket(); }
	static void cleanup();

	void downloadFile(const string& url);
	void postData(const string& url, const StringMap& data);
	void postData(const string& url, const string& body);

	const string& getCurrentUrl() const { return currentUrl; }
	const string& getServer() const { return server; }
	uint16_t getPort() const { return port; }
	const string& getPath() const { return path; }
	const string& getMimeType() const { return mimeType; }
	void setUserAgent(const string& agent) { userAgent = agent; }
	void setMaxBodySize(int64_t size) { maxBodySize = size; }

	uint64_t getID() const { return id; }
	void clearRedirCount() { redirCount = 0; }
	void setMaxRedirects(int count) { maxRedirects = count; }
	void setIpVersion(int af) { ipVersion = af; }

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
	string currentUrl;

	// parsed URL components
	string server;
	string path;
	string query;
	uint16_t port = 80;

	int64_t bodySize = -1;
	int64_t receivedBodySize = 0;
	size_t receivedHeadersSize = 0;
	int redirCount = 0;
	string requestBody;
	string userAgent;
	string mimeType;
	int64_t maxBodySize = std::numeric_limits<int64_t>::max();
	int maxRedirects = 5;
	int ipVersion = 0;

	ConnectionStates connState = STATE_IDLE;
	int requestType = -1;
	Http::Response resp;

	BufferedSocket* socket = nullptr;

	void prepareRequest(int type);
	void parseResponseHeader(const string &line) noexcept;
	void detachSocket() noexcept;
	void disconnect() noexcept;

	// BufferedSocketListener
	void onConnected() noexcept override;
	void onDataLine(const string&) noexcept override;
	void onData(const uint8_t*, size_t) noexcept override;
	void onModeChange() noexcept override;
	void onFailed(const string&) noexcept override;

	static vector<BufferedSocket*> oldSockets;
	static FastCriticalSection csOldSockets;
};

#endif // !defined(HTTP_CONNECTION_H)
