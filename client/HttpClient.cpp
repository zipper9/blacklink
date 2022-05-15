#include "stdinc.h"
#include "HttpClient.h"
#include "HttpHeaders.h"
#include "Util.h"
#include "StrUtil.h"
#include "TimerManager.h"
#include "CID.h"
#include "ResourceManager.h"

static const unsigned IDLE_CONNECTION_TIMEOUT = 2 * 60000;
static const unsigned BUSY_CONNECTION_TIMEOUT = 5 * 60000;

HttpClient httpClient;

HttpClient::HttpClient() : nextReqId(0), nextConnId(0)
{
}

void HttpClient::removeRequest(uint64_t id) noexcept
{
	cs.lock();
	auto i = requests.find(id);
	if (i != requests.end()) requests.erase(i);
	cs.unlock();
}

bool HttpClient::decodeUrl(const string& url, string& server)
{
	string proto, host, path, query, fragment;
	uint16_t port = 0;
	Util::decodeUrl(url, proto, host, port, path, query, fragment);
	if (host.empty() || !port || (proto != "http" && proto != "https")) return false;
	server = host + ':' + Util::toString(port);
	return true;
}

uint64_t HttpClient::addRequest(const HttpClient::Request& req)
{
	string server;
	if (!decodeUrl(req.url, server)) return 0;
	uint64_t reqId = ++nextReqId;

	RequestStatePtr rs = std::make_shared<RequestState>();
	rs->type = req.type;
	rs->url = req.url;
	rs->requestBody = req.requestBody;
	rs->requestBodyType = req.requestBodyType;
	rs->outputPath = req.outputPath;
	rs->userAgent = req.userAgent;
	rs->closeConn = req.closeConn;
	rs->ipVersion = req.ipVersion;
	rs->maxRespBodySize = req.maxRespBodySize;
	rs->maxErrorBodySize = req.maxErrorBodySize;
	rs->maxRedirects = req.maxRedirects;
	rs->ifModified = req.ifModified;

	uint64_t now = GET_TICK();
	cs.lock();
	findConnectionL(now, rs, server, reqId, req.closeConn, STATE_STARTING);
	cs.unlock();

	return reqId;
}

HttpConnection* HttpClient::findConnectionL(uint64_t now, RequestStatePtr& rs, const string& server, uint64_t reqId, bool closeConn, int state)
{
	HttpConnection* c = nullptr;
	for (auto i = conn.begin(); i != conn.end(); i++)
	{
		auto& cd = i->second;
		if (cd.server == server && cd.state == STATE_IDLE && !cd.closeConn)
		{
			c = cd.conn;
			cd.state = state;
			cd.requestId = reqId;
			cd.closeConn = closeConn;
			cd.lastActivity = now;
			rs->connId = c->getID();
			break;
		}
	}
	if (!c)
	{
		uint64_t connId = ++nextConnId;
		c = new HttpConnection(connId);
		c->addListener(this);
		ConnectionData cd;
		cd.state = state;
		cd.server = std::move(server);
		cd.requestId = reqId;
		cd.closeConn = closeConn;
		cd.conn = c;
		cd.lastActivity = now;
		conn.insert(make_pair(connId, cd));
		rs->connId = connId;
	}
	if (state == STATE_STARTING) requests.insert(make_pair(reqId, rs));
	return c;
}

void HttpClient::startRequest(HttpConnection* c, const RequestStatePtr& rs)
{
	c->setRequestBody(rs->requestBody, rs->requestBodyType);
	int flags = 0;
	if (rs->closeConn) flags |= HttpConnection::FLAG_CLOSE_CONN;
	if (rs->noCache) flags |= HttpConnection::FLAG_NO_CACHE;
	if (!rs->userAgent.empty()) c->setUserAgent(rs->userAgent);
	c->setIpVersion(rs->ipVersion);
	c->setMaxRespBodySize(rs->maxRespBodySize);
	c->setMaxErrorBodySize(rs->maxErrorBodySize);
	c->setIfModified(rs->ifModified);
	c->startRequest(rs->type, rs->url, flags);
}

void HttpClient::cancelRequest(uint64_t id)
{
	cs.lock();
	auto i = requests.find(id);
	if (i != requests.end())
	{
		const RequestStatePtr& data = i->second;
		auto j = conn.find(data->connId);
		if (j != conn.end())
		{
			auto& cd = j->second;
			if (cd.state == STATE_STARTING)
			{
				cd.state = STATE_IDLE;
				cd.requestId = 0;
			}
		}
		requests.erase(i);
	}
	cs.unlock();
}

bool HttpClient::startRequest(uint64_t id)
{
	HttpConnection* c = nullptr;
	RequestStatePtr rs;
	cs.lock();
	auto i = requests.find(id);
	if (i != requests.end())
	{
		const RequestStatePtr& data = i->second;
		auto j = conn.find(data->connId);
		if (j != conn.end())
		{
			auto& cd = j->second;
			if (cd.state == STATE_STARTING)
			{
				cd.state = STATE_BUSY;
				c = cd.conn;
				rs = data;
			}
		}
	}
	cs.unlock();

	if (!c) return false;
	startRequest(c, rs);
	return true;
}

void HttpClient::shutdown() noexcept
{
	cs.lock();
	for (auto i = conn.begin(); i != conn.end(); ++i)
	{
		i->second.conn->removeListeners();
		delete i->second.conn;
	}
	conn.clear();
	cs.unlock();
}

bool HttpClient::isTimedOut(const ConnectionData& cd, uint64_t now) noexcept
{
	if (cd.state == STATE_IDLE)
		return cd.lastActivity + IDLE_CONNECTION_TIMEOUT < now;
	if (cd.state == STATE_BUSY)
		return cd.conn->isReceivingData() && cd.lastActivity + BUSY_CONNECTION_TIMEOUT < now;
	return false;
}

void HttpClient::removeUnusedConnections() noexcept
{
	uint64_t now = GET_TICK();
	cs.lock();
	auto i = conn.begin();
	while (i != conn.end())
	{
		auto& cd = i->second;
		if (cd.state == STATE_FAILED || isTimedOut(cd, now))
		{
			cd.conn->removeListeners();
			delete cd.conn;
			conn.erase(i++);
		}
		else
			++i;
	}
	cs.unlock();
}

void HttpClient::on(Data, HttpConnection* c, const uint8_t* data, size_t size) noexcept
{
	uint64_t now = GET_TICK();
	uint64_t reqId = 0;
	RequestStatePtr rs;
	cs.lock();
	auto i = conn.find(c->getID());
	if (i == conn.end())
	{
		cs.unlock();
		return;
	}
	ConnectionData& cd = i->second;
	if (cd.state == STATE_BUSY)
	{
		cd.lastActivity = now;
		reqId = cd.requestId;
		auto j = requests.find(reqId);
		if (j == requests.end())
			cd.state = STATE_FAILED;
		else
			rs = j->second;
	}
	cs.unlock();
	if (!rs)
	{
		c->disconnect();
		return;
	}
	if (c->getResponseCode() == 200 && !rs->outputPath.empty())
	{
		bool error = false;
		string errorText;
		try
		{
			File& f = rs->outputFile;
			if (!f.isOpen())
			{
				string& filename = rs->outputPath;
				if (filename.back() == PATH_SEPARATOR)
					filename += getFileName(c->getPath());
				f.init(Text::toT(filename), File::WRITE, File::CREATE | File::TRUNCATE);
			}
			f.write(data, size);
		}
		catch (Exception& ex)
		{
			errorText = ex.getError();
			error = true;
		}
		if (error)
		{
			removeRequest(reqId);
			fire(HttpClientListener::Failed(), reqId, errorText);
		}
	}
	else
		rs->responseBody.append((const char *) data, size);
}

void HttpClient::processError(uint64_t connId, const string& error) noexcept
{
	uint64_t reqId = 0;
	cs.lock();
	auto i = conn.find(connId);
	if (i == conn.end())
	{
		cs.unlock();
		return;
	}
	ConnectionData& cd = i->second;
	if (cd.state != STATE_FAILED)
	{
		reqId = cd.requestId;
		cd.requestId = 0;
		cd.state = STATE_FAILED;
		if (reqId)
		{
			auto j = requests.find(reqId);
			if (j != requests.end())
				requests.erase(j);
			else
				reqId = 0;
		}
	}
	cs.unlock();
	if (reqId) fire(HttpClientListener::Failed(), reqId, error);
}

void HttpClient::on(Failed, HttpConnection* c, const string& error) noexcept
{
	processError(c->getID(), error);
}

void HttpClient::on(Completed, HttpConnection* c, const string& requestUrl) noexcept
{
	int respCode = c->getResponseCode();
	HttpClientListener::Result result;
	Http::Response resp;
	string error, redirUrl;
	RequestStatePtr redirRequest;
	HttpConnection* redirConn = nullptr;
	uint64_t reqId = 0;
	cs.lock();
	auto i = conn.find(c->getID());
	if (i == conn.end())
	{
		cs.unlock();
		return;
	}
	ConnectionData& cd = i->second;
	if (cd.state == STATE_BUSY)
	{
		reqId = cd.requestId;
		cd.requestId = 0;
		cd.state = STATE_IDLE;
		cd.lastActivity = GET_TICK();
		if (reqId)
		{
			auto j = requests.find(reqId);
			if (j != requests.end())
			{
				auto& rs = j->second;
				resp = c->getResponse();
				if (c->getRequestType() == Http::METHOD_GET && (respCode == 301 || respCode == 302 || respCode == 307) && rs->maxRedirects)
				{
					if (++rs->redirCount > rs->maxRedirects)
					{
						error = STRING(HTTP_ENDLESS_REDIRECTION_LOOP);
						error += " (";
						error += requestUrl;
						error += ')';
					}
					else
					{
						int index;
						const string& redirLocation = resp.findSingleHeader(Http::HEADER_LOCATION, index) ? resp.at(index) : Util::emptyString;
						string server;
						if (decodeUrl(redirLocation, server))
						{
							redirConn = findConnectionL(cd.lastActivity, rs, server, reqId, cd.closeConn, STATE_BUSY);
							rs->url = redirUrl = redirLocation;
							rs->responseBody.clear();
							redirRequest = rs;
						}
					}
				}
				if (error.empty() && redirUrl.empty())
				{
					result.url = std::move(rs->url);
					result.outputPath = std::move(rs->outputPath);
					result.responseBody = std::move(rs->responseBody);
				}
				if (redirUrl.empty()) requests.erase(j);
			}
			else
				reqId = 0;
		}
	}
	cs.unlock();
	if (!redirUrl.empty())
	{
		fire(HttpClientListener::Redirected(), reqId, redirUrl);
		startRequest(redirConn, redirRequest);
	}
	else if (!error.empty())
		fire(HttpClientListener::Failed(), reqId, error);
	else if (reqId)
		fire(HttpClientListener::Completed(), reqId, resp, result);
}

void HttpClient::on(Disconnected, HttpConnection* c) noexcept
{
	processError(c->getID(), STRING(CONNECTION_CLOSED));
}

string HttpClient::getFileName(const string& path)
{	
	auto pos = path.rfind('/');
	if (pos != string::npos)
	{
		string filename = Util::validateFileName(path.substr(pos + 1));
		if (!filename.empty()) return filename;
	}
	// generate random filename
	CID cid;
	cid.regenerate();
	return cid.toBase32() + ".dat";
}
