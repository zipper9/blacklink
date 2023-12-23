#include "stdinc.h"
#include "HttpServerConnection.h"
#include "HttpHeaders.h"
#include "BufferedSocket.h"
#include "SettingsManager.h"
#include "FormatUtil.h"
#include "Text.h"
#include "version.h"
#include "ResourceManager.h"
#include "BaseStreams.h"
#include <boost/algorithm/string/trim.hpp>

static const int64_t BODY_SIZE_UNKNOWN = -1;
static const int64_t BODY_SIZE_CHUNKED = -2;
static const unsigned long MAX_CHUNK_SIZE = 512*1024;
static const size_t MAX_HEADERS_SIZE = 32*1024;

HttpServerConnection::HttpServerConnection(int id, HttpServerCallback* server, unique_ptr<Socket>& newSock, uint16_t port)
	: id(id), server(server)
{
	socket = BufferedSocket::getBufferedSocket(0, this);
	socket->addAcceptedSocket(std::move(newSock), port);
	socket->start();
}

HttpServerConnection::~HttpServerConnection()
{
	destroySocket();
}

void HttpServerConnection::setFailedState(const string& error) noexcept
{
	connState = STATE_FAILED;
	//receivingData = false;
	server->onError(this, error);
	disconnect();
}

void HttpServerConnection::setIdleState() noexcept
{
	connState = STATE_RECEIVE_REQUEST;
	server->onRequest(this, req);
	req.clear();
	requestBody.clear();
	/*
	receivingData = false;
	if (reqFlags & FLAG_CLOSE_CONN) disconnect();
	*/
}

void HttpServerConnection::sendResponse(const Http::Response& resp, const string& body, InputStream* data) noexcept
{
	string s;
	resp.print(s);
	s += body;
	socket->write(s);
	if (data && !respBodyStream)
	{
		connState = STATE_SEND_FILE;
		respBodyStream = data;
		//socket->setMode(BufferedSocket::MODE_WRITE);
		socket->transmitFile(data);
	}
}

void HttpServerConnection::parseRequestHeader(const string& line) noexcept
{
	static const string encodingChunked = "chunked";
	req.parseLine(line);
	if (req.isComplete() && !req.isError())
	{
		bodySize = req.parseContentLength();
		if (req.isError()) return;
		if (bodySize < 0)
		{
			string transferEncoding = req.getHeaderValue(Http::HEADER_TRANSFER_ENCODING);
			auto pos = transferEncoding.find(';');
			if (pos != string::npos) transferEncoding.erase(pos);
			boost::algorithm::trim(transferEncoding);
			Text::asciiMakeLower(transferEncoding);
			if (transferEncoding == encodingChunked)
				bodySize = BODY_SIZE_CHUNKED;
		}
	}
}

void HttpServerConnection::onDataLine(const string &line) noexcept
{
	if (connState == STATE_DATA_CHUNKED && line.length() > 1)
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
			setFailedState("Chunked encoding error");
		else
			socket->setDataMode(chunkSize);
		return;
	}
	if (connState == STATE_RECEIVE_REQUEST)
	{
		receivedHeadersSize += line.size();
		parseRequestHeader(line);
		if (req.isError())
		{
			setFailedState("Malformed HTTP request");
			return;
		}
		if (req.isComplete())
		{
			if (bodySize == BODY_SIZE_UNKNOWN)
				bodySize = 0;
			if (bodySize > 0)
			{
				if (bodySize > maxReqBodySize)
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
				setFailedState("Malformed request");
			return;
		}
		if (receivedHeadersSize > MAX_HEADERS_SIZE)
		{
			setFailedState("Request headers too big)");
			return;
		}
	}
}

void HttpServerConnection::onFailed(const string &errorText) noexcept
{
	connState = STATE_FAILED;
	server->onError(this, errorText);
	disconnect();
}

void HttpServerConnection::onModeChange() noexcept
{
	if (connState == STATE_DATA) setIdleState();
}

void HttpServerConnection::onData(const uint8_t *data, size_t dataSize) noexcept
{
	if (connState != STATE_DATA && connState != STATE_DATA_CHUNKED)
		return;

	if (bodySize != -1 && static_cast<size_t>(bodySize - receivedBodySize) < dataSize)
	{
		setFailedState("Too much data in request body");
		return;
	}

	int64_t newBodySize = receivedBodySize + dataSize;
	if (newBodySize > maxReqBodySize)
	{
		setFailedState(STRING_F(HTTP_FILE_TOO_LARGE, Util::formatBytes(newBodySize)));
		return;
	}

	if (collectReqBody)
		requestBody.append((char *) data, dataSize);
	else
		server->onData(this, data, dataSize);
	receivedBodySize = newBodySize;
	if (receivedBodySize == bodySize) setIdleState();
}

void HttpServerConnection::onTransmitDone() noexcept
{
	dcassert(connState == STATE_SEND_FILE);
	//socket->setMode(BufferedSocket::MODE_LINE);
	connState = STATE_RECEIVE_REQUEST;
	delete respBodyStream;
	respBodyStream = nullptr;
}

void HttpServerConnection::destroySocket() noexcept
{
	if (socket)
	{
		socket->disconnect(true);
		socket->joinThread();
		BufferedSocket::destroyBufferedSocket(socket);
		socket = nullptr;
	}
	delete respBodyStream;
	respBodyStream = nullptr;
}

void HttpServerConnection::disconnect() noexcept
{
	if (socket)
		socket->disconnect(false);
}

bool HttpServerConnection::getIp(IpAddress& ip) const noexcept
{
	if (socket)
	{
		ip = socket->getIp();
		return true;
	}
	memset(&ip, 0, sizeof(ip));
	return false;
}
