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

vector<BufferedSocket*> HttpConnection::oldSockets;
FastCriticalSection HttpConnection::csOldSockets;

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

void HttpConnection::downloadFile(const string &url)
{
	currentUrl = url;
	requestBody.clear();
	prepareRequest(Http::METHOD_GET);
}

/**
 * Initiates a basic urlencoded form submission
 * @param aFile Fully qualified file URL
 * @param aData StringMap with the args and values
 */
void HttpConnection::postData(const string &url, const StringMap &params)
{
	currentUrl = url;
	requestBody.clear();

	for (StringMap::const_iterator i = params.begin(); i != params.end(); ++i)
		requestBody += "&" + Util::encodeURI(i->first) + "=" + Util::encodeURI(i->second);

	if (!requestBody.empty()) requestBody = requestBody.substr(1);
	prepareRequest(Http::METHOD_POST);
}

void HttpConnection::postData(const string &url, const string &body)
{
	currentUrl = url;
	requestBody = body;
	prepareRequest(Http::METHOD_POST);
}

void HttpConnection::prepareRequest(int type)
{
	dcassert(Text::isAsciiPrefix2(currentUrl, prefixHttp) ||
	         Text::isAsciiPrefix2(currentUrl, prefixHttps));
	sanitizeUrl(currentUrl);

	detachSocket();

	connState = STATE_SEND_REQUEST;
	requestType = type;
	bodySize = BODY_SIZE_UNKNOWN;
	receivedBodySize = 0;
	receivedHeadersSize = 0;
	resp.clear();

	if (Util::checkFileExt(currentUrl, string(".bz2")))
		mimeType = "application/x-bzip2";
	else
		mimeType.clear();

	string proto, fragment;
#if 0
	if (SETTING(HTTP_PROXY).empty())
#endif
	{
		Util::decodeUrl(currentUrl, proto, server, port, path, query, fragment);
		if (path.empty()) path = "/";
	}
#if 0
	else
	{
		Util::decodeUrl(SETTING(HTTP_PROXY), proto, server, port, path, query, fragment);
		path = currentUrl;
	}
#endif

	if (!port) port = 80;

	socket = BufferedSocket::getBufferedSocket('\n', this);
	socket->setIpVersion(ipVersion);

	try
	{
		socket->connect(server, port, proto == "https", true, true, Socket::PROTO_DEFAULT);
		socket->start();
	}
	catch (const Exception &e)
	{
		connState = STATE_FAILED;
		fire(HttpConnectionListener::Failed(), this, e.getError() + " (" + currentUrl + ")");
		disconnect();
	}
}

void HttpConnection::onConnected() noexcept
{
	dcassert(socket);
	Http::Request req;
	req.setMethodId(requestType);
	if (!query.empty())
		req.setUri(path + '?' + query);
	else
		req.setUri(path);

	string remoteServer = server;
#if 0
	if (!SETTING(HTTP_PROXY).empty())
	{
		string tfile, proto, queryTmp, fragment;
		uint16_t tport;
		Util::decodeUrl(file, proto, remoteServer, tport, tfile, queryTmp, fragment);
	}
#endif
	req.addHeader(Http::HEADER_HOST, remoteServer);
	if (!userAgent.empty()) req.addHeader(Http::HEADER_USER_AGENT, userAgent);
	req.addHeader(Http::HEADER_CONNECTION, "close");
	req.addHeader(Http::HEADER_CACHE_CONTROL, "no-cache");
	req.addHeader(Http::HEADER_CONTENT_LENGTH, Util::toString(requestBody.length()));

	string s;
	req.print(s);
	s += requestBody;
	socket->write(s);
	connState = STATE_PROCESS_RESPONSE;
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
		string params;
		resp.parseContentType(mimeType, params);
	}
}

void HttpConnection::onDataLine(const string &line) noexcept
{
	if (connState == STATE_DATA_CHUNKED && line.size() > 1)
	{
		string::size_type i;
		string chunkSizeStr;
		if ((i = line.find(';')) == string::npos)
		{
			chunkSizeStr = line.substr(0, line.length() - 1);
		}
		else
			chunkSizeStr = line.substr(0, i);

		unsigned long chunkSize = strtoul(chunkSizeStr.c_str(), NULL, 16);
		if (chunkSize == 0)
		{
			connState = STATE_IDLE;
			fire(HttpConnectionListener::Complete(), this, currentUrl);
			disconnect();
		}
		else if (chunkSize > MAX_CHUNK_SIZE)
		{
			connState = STATE_FAILED;
			fire(HttpConnectionListener::Failed(), this, "Chunked encoding error (" + currentUrl + ")");
			disconnect();
		}
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
			connState = STATE_FAILED;
			fire(HttpConnectionListener::Failed(), this, "Malformed HTTP response (" + currentUrl + ")");
			disconnect();
			return;
		}
		if (resp.isComplete())
		{
			int responseCode = resp.getResponseCode();
			if (responseCode == 200)
			{
				if (bodySize >= 0)
				{
					if (bodySize > maxBodySize)
					{
						connState = STATE_FAILED;
						fire(HttpConnectionListener::Failed(), this, "File too large (" + currentUrl + ")");
						disconnect();
					} else
					{
						socket->setDataMode(bodySize);
						connState = STATE_DATA;
					}
				}
				else if (bodySize == BODY_SIZE_UNKNOWN)
				{
					socket->setDataMode(-1);
					connState = STATE_DATA;
				}
				else if (bodySize == BODY_SIZE_CHUNKED)
				{
					connState = STATE_DATA_CHUNKED;
				}
				else
				{
					connState = STATE_FAILED;
					fire(HttpConnectionListener::Failed(), this, "Malformed response (" + currentUrl + ")");
					disconnect();
				}
				return;
			}

			if (requestType == Http::METHOD_GET && (responseCode == 301 || responseCode == 302 || responseCode == 307))
			{
				if (++redirCount > maxRedirects)
				{
					connState = STATE_FAILED;
					string error = STRING(HTTP_ENDLESS_REDIRECTION_LOOP);
					error += " (";
					error += currentUrl;
					error += ')';
					fire(HttpConnectionListener::Failed(), this, error);
					disconnect();
					return;
				}
				int index;
				const string& redirLocation = resp.findSingleHeader(Http::HEADER_LOCATION, index) ? resp.at(index) : Util::emptyString;
				if (!(Text::isAsciiPrefix2(redirLocation, prefixHttp) || Text::isAsciiPrefix2(redirLocation, prefixHttps)))
				{
					connState = STATE_FAILED;
					fire(HttpConnectionListener::Failed(), this, "Bad redirect (" + currentUrl + ")");
					disconnect();
					return;
				}
				connState = STATE_FAILED;
				disconnect();
				fire(HttpConnectionListener::Redirected(), this, redirLocation);
			}
			else
			{
				connState = STATE_FAILED;
				fire(HttpConnectionListener::Failed(), this, resp.getResponsePhrase() + " (" + currentUrl + ")");
				disconnect();
			}
			return;
		}
		if (receivedHeadersSize > MAX_HEADERS_SIZE)
		{
			connState = STATE_FAILED;
			fire(HttpConnectionListener::Failed(), this, "Response headers too big (" + currentUrl + ")");
			disconnect();
			return;
		}
	}
}

void HttpConnection::onFailed(const string &errorText) noexcept
{
	if (connState != STATE_FAILED && connState != STATE_IDLE)
	{
		connState = STATE_FAILED;
		fire(HttpConnectionListener::Failed(), this, errorText + " (" + currentUrl + ")");
		disconnect();
	}
}

void HttpConnection::onModeChange() noexcept
{
	if (connState != STATE_DATA_CHUNKED)
	{
		connState = STATE_IDLE;
		fire(HttpConnectionListener::Complete(), this, currentUrl);
		disconnect();
	}
}

void HttpConnection::onData(const uint8_t *data, size_t dataSize) noexcept
{
	if (connState != STATE_DATA && connState != STATE_DATA_CHUNKED)
		return;

	if (bodySize != -1 && static_cast<size_t>(bodySize - receivedBodySize) < dataSize)
	{
		connState = STATE_FAILED;
		fire(HttpConnectionListener::Failed(), this, "Too much data in response body (" + currentUrl + ")");
		disconnect();
		return;
	}

	int64_t newBodySize = receivedBodySize + dataSize;
	if (newBodySize > maxBodySize)
	{
		connState = STATE_FAILED;
		fire(HttpConnectionListener::Failed(), this, "File too large (" + currentUrl + ")");
		disconnect();
		return;
	}

	fire(HttpConnectionListener::Data(), this, data, dataSize);
	receivedBodySize = newBodySize;
	if (receivedBodySize == bodySize)
	{
		connState = STATE_IDLE;
		fire(HttpConnectionListener::Complete(), this, currentUrl);
		disconnect();
	}
}

void HttpConnection::detachSocket() noexcept
{
	if (socket)
	{
		socket->disconnect(true);
		csOldSockets.lock();
		oldSockets.push_back(socket);
		csOldSockets.unlock();
		socket = nullptr;
	}
}

void HttpConnection::disconnect() noexcept
{
	if (socket)
		socket->disconnect(false);
}

void HttpConnection::cleanup()
{
	csOldSockets.lock();
	auto sockets = std::move(oldSockets);
	oldSockets.clear();
	csOldSockets.unlock();
	for (BufferedSocket* socket : sockets)
	{
		socket->joinThread();
		BufferedSocket::destroyBufferedSocket(socket);
	}
}
