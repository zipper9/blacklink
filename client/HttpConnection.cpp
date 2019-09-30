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
#include "BufferedSocket.h"
#include "SettingsManager.h"
#include "version.h"

#include "ResourceManager.h"

#include <boost/algorithm/string/trim.hpp>

static const int64_t BODY_SIZE_UNKNOWN = -1;
static const int64_t BODY_SIZE_CHUNKED = -2;
static const unsigned long MAX_CHUNK_SIZE = 512*1024;
static const size_t MAX_HEADERS_SIZE = 32*1024;
static const int MAX_REDIRECTS = 5;

static const string prefixHttp  = "http://";
static const string prefixHttps = "https://";

static void sanitizeUrl(string& url) noexcept
{
	// FIXME: remove boost
	boost::algorithm::trim_if(url, boost::is_space() || boost::is_any_of("<>\""));
}

HttpConnection::HttpConnection(int id, bool aIsUnique, bool aV4only /*false*/)
	: id(id), isUnique(aIsUnique), v4only(aV4only) {}

HttpConnection::~HttpConnection()
{
	if (socket)
	{
		abortRequest(true);
	}
}

/**
 * Downloads a file and returns it as a string
 * @todo Report exceptions
 * @todo Abort download
 * @param aUrl Full URL of file
 * @return A string with the content, or empty if download failed
 */
void HttpConnection::downloadFile(const string &aFile)
{
	redirCount = 0;
	currentUrl = aFile;
	requestBody.clear();
	prepareRequest(TYPE_GET);
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
	prepareRequest(TYPE_POST);
}

void HttpConnection::postData(const string &url, const string &body)
{
	currentUrl = url;
	requestBody = body;
	prepareRequest(TYPE_POST);
}

void HttpConnection::removeSelf()
{
	if (isUnique)
	{
		removeListeners();
		delete this;
	}
}

void HttpConnection::prepareRequest(RequestType type)
{
	dcassert(Text::isAsciiPrefix2(currentUrl, prefixHttp) ||
	         Text::isAsciiPrefix2(currentUrl, prefixHttps));
	sanitizeUrl(currentUrl);

	connState = STATE_SEND_REQUEST;
	requestType = type;
	bodySize = BODY_SIZE_UNKNOWN;
	receivedBodySize = 0;
	receivedHeadersSize = 0;
	responseCode = 0;
	responseText.clear();
	redirLocation.clear();

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

	if (!socket) socket = BufferedSocket::getBufferedSocket('\n', this);

	try
	{
		socket->connect(server, port, proto == "https", true, false, Socket::PROTO_DEFAULT);
	}
	catch (const Exception &e)
	{
		connState = STATE_FAILED;
		fire(HttpConnectionListener::Failed(), this, e.getError() + " (" + currentUrl + ")");
		removeSelf();
	}
}

void HttpConnection::abortRequest(bool disconnect)
{
	dcassert(socket);

	if (disconnect) socket->disconnect();

	BufferedSocket::destroyBufferedSocket(socket);
	socket = nullptr;
}

#define LIT(n) n, sizeof(n)-1

void HttpConnection::onConnected() noexcept
{
	dcassert(socket);
	string req = (requestType == TYPE_POST ? "POST " : "GET ") + path;
	if (!query.empty()) req += '?' + query;
	req += " HTTP/1.1\r\n";

	string remoteServer = server;
#if 0
	if (!SETTING(HTTP_PROXY).empty())
	{
		string tfile, proto, queryTmp, fragment;
		uint16_t tport;
		Util::decodeUrl(file, proto, remoteServer, tport, tfile, queryTmp, fragment);
	}
#endif

	req += "Host: " + remoteServer + "\r\n";
	if (!userAgent.empty()) req += "User-Agent: " + userAgent + "\r\n";
	req.append(LIT("Connection: close\r\n"));
	req.append(LIT("Cache-Control: no-cache\r\n"));
	req += "Content-Length: " + Util::toString(requestBody.length()) + "\r\n";
	req.append(LIT("\r\n"));
	req += requestBody;
	socket->write(req);
	connState = STATE_WAIT_RESPONSE;
}

static inline bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

static inline bool isWhiteSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool HttpConnection::parseStatusLine(const string& line) noexcept
{
	static const string strStatusLine = "HTTP/1.";
	responseCode = 0;
	if (line.length() < 12) return false;
	if (line.compare(0, 7, strStatusLine) || !(line[7] == '0' || line[7] == '1')) return false;
	if (!(isDigit(line[9]) && isDigit(line[10]) && isDigit(line[11]))) return false;
	responseCode = (line[9]-'0')*100 + (line[10]-'0')*10 + (line[11]-'0');
	size_t start = 12;
	while (start < line.length() && isWhiteSpace(line[start])) start++;
	size_t end = line.length();
	while (end > start && isWhiteSpace(line[end-1])) end--;
	if (start < end)
		responseText = line.substr(start, end-start);
	else
		responseText.clear();
	return true;
}

bool HttpConnection::parseResponseHeader(const string& line) noexcept
{
	static const string headerContentLength = "content-length";
	static const string headerContentType = "content-type";
	static const string headerTransferEncoding = "transfer-encoding";
	static const string headerLocation = "location";
	static const string encodingChunked = "chunked";
	string lineLower = line;
	Text::asciiMakeLower(lineLower);
	size_t pos = lineLower.find(':');
	if (pos == string::npos) return false;
	size_t nameEnd = pos;
	while (nameEnd > 0 && isWhiteSpace(lineLower[nameEnd-1])) nameEnd--;
	size_t valueStart = pos + 1;
	while (valueStart < lineLower.length() && isWhiteSpace(lineLower[valueStart])) valueStart++;
	size_t valueEnd = lineLower.length();
	while (valueEnd > valueStart && isWhiteSpace(lineLower[valueEnd-1])) valueEnd--;
	if (valueEnd <= valueStart) return false;
	if (lineLower.compare(0, nameEnd, headerContentLength) == 0)
	{
		bodySize = Util::toInt64(lineLower.substr(valueStart, valueEnd-valueStart));
		return true;
	}
	if (lineLower.compare(0, nameEnd, headerContentType) == 0)
	{
		mimeType = lineLower.substr(valueStart, valueEnd-valueStart);
		pos = mimeType.find(';');
		if (pos != string::npos) mimeType.erase(pos);
		return true;
	}
	if (lineLower.compare(0, nameEnd, headerTransferEncoding) == 0)
	{
		string transferEncoding = lineLower.substr(valueStart, valueEnd-valueStart);
		pos = transferEncoding.find(';');
		if (pos != string::npos) transferEncoding.erase(pos);
		if (transferEncoding == encodingChunked)
			bodySize = BODY_SIZE_CHUNKED;
		return true;
	}
	if (lineLower.compare(0, nameEnd, headerLocation) == 0)
	{
		redirLocation = lineLower.substr(valueStart, valueEnd-valueStart);
		return true;
	}
	return true;
}

void HttpConnection::onDataLine(const string &aLine) noexcept
{
	if (connState == STATE_DATA_CHUNKED && aLine.size() > 1)
	{
		string::size_type i;
		string chunkSizeStr;
		if ((i = aLine.find(';')) == string::npos)
		{
			chunkSizeStr = aLine.substr(0, aLine.length() - 1);
		}
		else
			chunkSizeStr = aLine.substr(0, i);

		unsigned long chunkSize = strtoul(chunkSizeStr.c_str(), NULL, 16);
		if (chunkSize == 0)
		{
			abortRequest(true);
			connState = STATE_IDLE;
			fire(HttpConnectionListener::Complete(), this, currentUrl);
			removeSelf();			
		}
		else if (chunkSize > MAX_CHUNK_SIZE)
		{
			abortRequest(true);
			connState = STATE_FAILED;
			fire(HttpConnectionListener::Failed(), this, "Chunked encoding error (" + currentUrl + ")");
			removeSelf();
		}
		else
			socket->setDataMode(chunkSize);
		return;
	}
	if (connState == STATE_WAIT_RESPONSE)
	{
		receivedHeadersSize += aLine.size();
		if (!parseStatusLine(aLine))
		{
			abortRequest(true);
			connState = STATE_FAILED;
			fire(HttpConnectionListener::Failed(), this, "Malformed status line (" + currentUrl + ")");
			removeSelf();
		} else
			connState = STATE_PROCESS_HEADERS;
		return;
	}
	if (connState == STATE_PROCESS_HEADERS)
	{
		receivedHeadersSize += aLine.size();
		if (aLine[0] == '\r')
		{
			if (responseCode == 200)
			{
				if (bodySize >= 0)
				{
					socket->setDataMode(bodySize);
					connState = STATE_DATA;
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
					abortRequest(true);
					connState = STATE_FAILED;
					fire(HttpConnectionListener::Failed(), this, "Malformed response (" + currentUrl + ")");
					removeSelf();
				}
				return;
			}

			abortRequest(true);
			if (requestType == TYPE_GET && (responseCode == 301 || responseCode == 302 || responseCode == 307))
			{
				if (++redirCount > MAX_REDIRECTS)
				{
					connState = STATE_FAILED;
					string error = STRING(HTTP_ENDLESS_REDIRECTION_LOOP);
					error += " (";
					error += currentUrl;
					error += ')';
					fire(HttpConnectionListener::Failed(), this, error);
					removeSelf();
					return;
				}				
				if (!(Text::isAsciiPrefix2(redirLocation, prefixHttp) || Text::isAsciiPrefix2(redirLocation, prefixHttps)))
				{
					connState = STATE_FAILED;
					fire(HttpConnectionListener::Failed(), this, "Bad redirect (" + currentUrl + ")");
					removeSelf();
					return;
				}
				fire(HttpConnectionListener::Redirected(), this, redirLocation);
				currentUrl = redirLocation;
				requestBody.clear();
				prepareRequest(TYPE_GET);
			}
			else
			{
				connState = STATE_FAILED;
				fire(HttpConnectionListener::Failed(), this, responseText + " (" + currentUrl + ")");
				removeSelf();
			}
			return;
		}
		if (receivedHeadersSize > MAX_HEADERS_SIZE)
		{
			abortRequest(true);
			connState = STATE_FAILED;
			fire(HttpConnectionListener::Failed(), this, "Response headers too big (" + currentUrl + ")");
			removeSelf();
			return;
		}
		parseResponseHeader(aLine);
		return;
	}
}

void HttpConnection::onFailed(const string &errorText) noexcept
{
	abortRequest(false);
	connState = STATE_FAILED;
	fire(HttpConnectionListener::Failed(), this, errorText + " (" + currentUrl + ")");
	removeSelf();
}

void HttpConnection::onModeChange() noexcept
{
	if (connState != STATE_DATA_CHUNKED)
	{
		abortRequest(true);
		connState = STATE_IDLE;
		fire(HttpConnectionListener::Complete(), this, currentUrl);
		removeSelf();
	}
}

void HttpConnection::onData(const uint8_t *data, size_t dataSize) noexcept
{
	if (connState != STATE_DATA)
		return;

	if (bodySize != -1 && static_cast<size_t>(bodySize - receivedBodySize) < dataSize)
	{
		abortRequest(true);
		connState = STATE_FAILED;
		fire(HttpConnectionListener::Failed(), this, "Too much data in response body (" + currentUrl + ")");
		removeSelf();
		return;
	}

	fire(HttpConnectionListener::Data(), this, data, dataSize);
	receivedBodySize += dataSize;
	if (receivedBodySize == bodySize)
	{
		connState = STATE_IDLE;
		fire(HttpConnectionListener::Complete(), this, currentUrl);
		removeSelf();
	}
}
