/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#include "stdinc.h"
#include "HttpConnection.h"
#include "HttpHeaders.h"
#include "BufferedSocket.h"
#include "SettingsManager.h"
#include "version.h"

#include "ResourceManager.h"

#include <boost/algorithm/string/trim.hpp>

static const int64_t BODY_SIZE_UNKNOWN = -1;
static const int64_t BODY_SIZE_CHUNKED = -2;
static const unsigned long MAX_CHUNK_SIZE = 512*1024;
static const size_t MAX_HEADERS_SIZE = 32*1024;

static const string prefixHttp  = "http://";
static const string prefixHttps = "https://";

#if 0
static void logMessage(const char *msg, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, msg);
	size_t outLen = vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);
	DumpDebugMessage(_T("http-client.log"), buf, outLen, true);
}
#endif

static void sanitizeUrl(string& url) noexcept
{
	// FIXME: remove boost
	boost::algorithm::trim_if(url, boost::is_space() || boost::is_any_of("<>\""));
}

HttpConnection::~HttpConnection()
{
	destroySocket();
}

#if 0
void HttpConnection::postData(const string &url, const StringMap &params)
{
	currentUrl = url;
	requestBody.clear();

	for (StringMap::const_iterator i = params.begin(); i != params.end(); ++i)
		requestBody += "&" + Util::encodeURI(i->first) + "=" + Util::encodeURI(i->second);

	if (!requestBody.empty()) requestBody = requestBody.substr(1);
	prepareRequest(Http::METHOD_POST);
}
#endif

void HttpConnection::setRequestBody(const string& body, const string& type)
{
	requestBody = body;
	requestBodyType = type;
}

void HttpConnection::setRequestBody(string& body, const string& type)
{
	requestBody = std::move(body);
	requestBodyType = type;
}

bool HttpConnection::startRequest(uint64_t reqId, int type, const string& url, int flags)
{
	if (connState != STATE_IDLE) return false;
	currentUrl = url;
	requestId = reqId;
	reqFlags = flags;
	prepareRequest(type);
	return true;
}

bool HttpConnection::checkUrl(const string& url)
{
	return Text::isAsciiPrefix2(url, prefixHttp) || Text::isAsciiPrefix2(url, prefixHttps);
}

void HttpConnection::prepareRequest(int type) noexcept
{
	dcassert(checkUrl(currentUrl));
	sanitizeUrl(currentUrl);

	connState = STATE_SEND_REQUEST;
	requestType = type;
	bodySize = BODY_SIZE_UNKNOWN;
	receivedBodySize = 0;
	receivedHeadersSize = 0;
	resp.clear();

	string proto, fragment;
	port = 0;
	Util::decodeUrl(currentUrl, proto, server, port, path, query, fragment);

	string connectServer;
	uint16_t connectPort = 0;
	if (!proxyServer.empty())
	{
		string tpath, tquery;
		Util::decodeUrl(proxyServer, proto, proxyServerHost, connectPort, tpath, tquery, fragment);
		connectServer = proxyServerHost;
		requestUri = currentUrl;
		if (path.empty())
		{
			requestUri += '/';
			path = "/";
		}
	}
	else
	{
		connectServer = server;
		connectPort = port;
		if (path.empty()) path = "/";
		requestUri = path;
		if (!query.empty())
		{
			requestUri += '?';
			requestUri += query;
		}
	}

	if (socket)
	{
		sendRequest();
		return;
	}

	socket = BufferedSocket::getBufferedSocket('\n', this);
	socket->setIpVersion(ipVersion);

	try
	{
		bool secure = proto == "https";
		if (!connectPort) connectPort = secure ? 443 : 80;
		socket->connect(connectServer, connectPort, secure, true, true, Socket::PROTO_HTTP);
		socket->start();
	}
	catch (const Exception &e)
	{
		setFailedState(e.getError() + " (" + currentUrl + ")");
	}
}

void HttpConnection::onConnected() noexcept
{
	sendRequest();
}

void HttpConnection::setFailedState(const string& error) noexcept
{
	connState = STATE_FAILED;
	receivingData = false;
	client->onFailed(this, error);
	disconnect();
}

void HttpConnection::setIdleState() noexcept
{
	connState = STATE_IDLE;
	receivingData = false;
	client->onCompleted(this, currentUrl);
	if (reqFlags & FLAG_CLOSE_CONN) disconnect();
}

void HttpConnection::sendRequest() noexcept
{
	dcassert(socket);
	Http::Request req;
	req.setMethodId(requestType);
	req.setUri(requestUri);

	if (!userAgent.empty()) req.addHeader(Http::HEADER_USER_AGENT, userAgent);
	if (proxyServer.empty())
	{
		req.addHeader(Http::HEADER_HOST, server);
		req.addHeader(Http::HEADER_CONNECTION, (reqFlags & FLAG_CLOSE_CONN) ? "close" : "keep-alive");
	}
	else
	{
		req.addHeader(Http::HEADER_HOST, proxyServerHost);
		req.addHeader(Http::HEADER_PROXY_CONNECTION, (reqFlags & FLAG_CLOSE_CONN) ? "close" : "keep-alive");
	}
	if (reqFlags & FLAG_NO_CACHE) req.addHeader(Http::HEADER_CACHE_CONTROL, "no-cache");
	req.addHeader(Http::HEADER_CONTENT_LENGTH, Util::toString(requestBody.length()));
	if (!requestBodyType.empty()) req.addHeader(Http::HEADER_CONTENT_TYPE, requestBodyType);
	if (ifModified) req.addHeader(Http::HEADER_IF_MODIFIED_SINCE, Http::printDateTime(ifModified));
#ifdef DEBUG_HTTP_CONNECTION
	req.addHeader("X-Connection-ID", Util::toString(id));
	req.addHeader("X-Request-ID", Util::toString(requestId));
#endif

	string s;
	req.print(s);
	s += requestBody;
	requestBody.clear();
	socket->write(s);
	connState = STATE_PROCESS_RESPONSE;
	receivingData = true;
}

void HttpConnection::parseResponseHeader(const string& line) noexcept
{
	static const string encodingChunked = "chunked";
	resp.parseLine(line);
	if (resp.isComplete() && !resp.isError())
	{
		bodySize = resp.parseContentLength();
		if (resp.isError()) return;
		if (bodySize < 0)
		{
			string transferEncoding = resp.getHeaderValue(Http::HEADER_TRANSFER_ENCODING);
			auto pos = transferEncoding.find(';');
			if (pos != string::npos) transferEncoding.erase(pos);
			boost::algorithm::trim(transferEncoding);
			Text::asciiMakeLower(transferEncoding);
			if (transferEncoding == encodingChunked)
				bodySize = BODY_SIZE_CHUNKED;
		}
	}
}

void HttpConnection::onDataLine(const string &line) noexcept
{
	if (connState == STATE_DATA_CHUNKED && line.size() > 1)
	{
		string::size_type i;
		string chunkSizeStr;
		if ((i = line.find(';')) == string::npos)
			chunkSizeStr = line.substr(0, line.length() - 1);
		else
			chunkSizeStr = line.substr(0, i);

		unsigned long chunkSize = strtoul(chunkSizeStr.c_str(), nullptr, 16);
		if (chunkSize == 0)
			setIdleState();
		else if (chunkSize > MAX_CHUNK_SIZE)
			setFailedState("Chunked encoding error (" + currentUrl + ")");
		else
			socket->setDataMode(chunkSize);
		return;
	}
	if (connState == STATE_PROCESS_RESPONSE)
	{
		receivedHeadersSize += line.size();
		parseResponseHeader(line);
		if (resp.isError())
		{
			setFailedState("Malformed HTTP response (" + currentUrl + ")");
			return;
		}
		if (resp.isComplete())
		{
			if (bodySize == BODY_SIZE_UNKNOWN &&
			    (resp.getResponseCode() == 204 || resp.getResponseCode() == 304))
				bodySize = 0;
			if (bodySize > 0)
			{
				int64_t maxBodySize = resp.getResponseCode() == 200 ? maxRespBodySize : maxErrorBodySize;
				if (bodySize > maxBodySize)
				{
					setFailedState(STRING_F(HTTP_FILE_TOO_LARGE, Util::formatBytes(bodySize)));
				}
				else
				{
					socket->setDataMode(bodySize);
					connState = STATE_DATA;
				}
			}
			else if (bodySize == 0)
				setIdleState();
			else if (bodySize == BODY_SIZE_UNKNOWN)
			{
				socket->setDataMode(-1);
				connState = STATE_DATA;
			}
			else if (bodySize == BODY_SIZE_CHUNKED)
				connState = STATE_DATA_CHUNKED;
			else
				setFailedState("Malformed response (" + currentUrl + ")");
			return;
		}
		if (receivedHeadersSize > MAX_HEADERS_SIZE)
		{
			setFailedState("Response headers too big (" + currentUrl + ")");
			return;
		}
	}
}

void HttpConnection::onFailed(const string &errorText) noexcept
{
	int prevState = connState;
	connState = STATE_FAILED;
	receivingData = false;
	if (prevState == STATE_IDLE)
		client->onDisconnected(this);
	else if (prevState != STATE_FAILED)
		client->onFailed(this, errorText + " (" + currentUrl + ")");
	disconnect();
}

void HttpConnection::onModeChange() noexcept
{
	if (connState == STATE_DATA) setIdleState();
}

void HttpConnection::onData(const uint8_t *data, size_t dataSize) noexcept
{
	if (connState != STATE_DATA && connState != STATE_DATA_CHUNKED)
		return;

	if (bodySize != -1 && static_cast<size_t>(bodySize - receivedBodySize) < dataSize)
	{
		setFailedState("Too much data in response body (" + currentUrl + ")");
		return;
	}

	int64_t newBodySize = receivedBodySize + dataSize;
	int64_t maxBodySize = resp.getResponseCode() == 200 ? maxRespBodySize : maxErrorBodySize;
	if (newBodySize > maxBodySize)
	{
		setFailedState(STRING_F(HTTP_FILE_TOO_LARGE, Util::formatBytes(newBodySize)));
		return;
	}

	client->onData(this, data, dataSize);
	receivedBodySize = newBodySize;
	if (receivedBodySize == bodySize) setIdleState();
}

void HttpConnection::destroySocket() noexcept
{
	if (socket)
	{
		socket->disconnect(true);
		socket->joinThread();
		BufferedSocket::destroyBufferedSocket(socket);
		socket = nullptr;
	}
}

void HttpConnection::disconnect() noexcept
{
	if (socket)
		socket->disconnect(false);
}
