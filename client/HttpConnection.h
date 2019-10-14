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
#include "Speaker.h"
#include "Util.h"
#include <limits>

class BufferedSocket;

class HttpConnection : BufferedSocketListener, public Speaker<HttpConnectionListener>
{
public:
	HttpConnection(int id, bool aIsUnique = false, bool v4only = false);
	HttpConnection(const HttpConnection&) = delete;
	HttpConnection& operator= (const HttpConnection&) = delete;
	
	virtual ~HttpConnection();

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

private:
	enum RequestType
	{
		TYPE_UNKNOWN,
		TYPE_GET,
		TYPE_POST,
	 };

	enum ConnectionStates
	{
		STATE_IDLE,
		STATE_SEND_REQUEST,
		STATE_WAIT_RESPONSE,
		STATE_PROCESS_HEADERS,
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
	
	int responseCode = 0;
	string responseText;
	int64_t bodySize = -1;
	int64_t receivedBodySize = 0;
	size_t receivedHeadersSize = 0;
	int redirCount = 0;
	string redirLocation;
	string requestBody;
	string userAgent;
	string mimeType;
	int64_t maxBodySize = std::numeric_limits<int64_t>::max();

	ConnectionStates connState = STATE_IDLE;
	RequestType requestType = TYPE_UNKNOWN;

	BufferedSocket* socket = nullptr;

	void prepareRequest(RequestType type);
	void abortRequest(bool disconnect);
	void removeSelf();
	bool parseStatusLine(const string &line) noexcept;
	bool parseResponseHeader(const string &line) noexcept;
	
	// BufferedSocketListener
	void onConnected() noexcept override;
	void onDataLine(const string&) noexcept override;
	void onData(const uint8_t*, size_t) noexcept override;
	void onModeChange() noexcept override;
	void onFailed(const string&) noexcept override;
	const bool isUnique;
	const bool v4only;
};

#endif // !defined(HTTP_CONNECTION_H)
