#include "stdinc.h"
#include "WebServerManager.h"
#include "AppPaths.h"
#include "HttpHeaders.h"
#include "JsonFormatter.h"
#include "BufferedSocket.h"
#include "SSLSocket.h"
#include "CryptoManager.h"
#include "LogManager.h"
#include "ResourceManager.h"
#include "SimpleXML.h"
#include "SearchManager.h"
#include "QueueManager.h"
#include "FinishedManager.h"
#include "UploadManager.h"
#include "ShareManager.h"
#include "WebServerUtil.h"
#include "MagnetLink.h"
#include "PathUtil.h"
#include "TimeUtil.h"
#include "FormatUtil.h"

static const unsigned SESSION_EXPIRE_TIME = 10; // minutes

static const string htmlStart1 = "<!DOCTYPE html><html><head>\n"
"<link rel='stylesheet' type='text/css' href='/default@";

static const string htmlStart2 = ".css'/>\n"
"<script type='text/javascript' src='/script.js'></script>\n"
"</head><body>\n";

static const string htmlMainDiv = "<div class='main'>\n";

static const string htmlEnd = "</div></body></html>\n";

static const ResourceManager::Strings pageNames[] =
{
	ResourceManager::SEARCH, ResourceManager::DOWNLOAD_QUEUE,  ResourceManager::WAITING_USERS,
	ResourceManager::FINISHED_UPLOADS, ResourceManager::FINISHED_DOWNLOADS, ResourceManager::WEBSERVER_TOOLS,
	ResourceManager::SETTINGS
};

enum
{
	PAGE_SEARCH,
	PAGE_QUEUE,
	PAGE_WAITING_USERS,
	PAGE_FINISHED_UPLOADS,
	PAGE_FINISHED_DOWNLOADS,
	PAGE_TOOLS,
	PAGE_SETTINGS,
	MAX_PAGES
};

static const int searchResultColumnNames[] =
{
	ResourceManager::USER, ResourceManager::FILENAME, ResourceManager::SIZE,
	ResourceManager::TTH, -1
};

static const uint8_t searchResultColumnWidths[] =
{
	WebServerUtil::WIDTH_MEDIUM, WebServerUtil::WIDTH_MEDIUM, WebServerUtil::WIDTH_WRAP_CONTENT,
	WebServerUtil::WIDTH_LARGE, WebServerUtil::WIDTH_WRAP_CONTENT
};

static const int queueColumnNames[] =
{
	ResourceManager::FILENAME, ResourceManager::SIZE, ResourceManager::PATH,
	ResourceManager::SOURCES, ResourceManager::TTH, ResourceManager::STATUS,
	-1
};

static const uint8_t queueColumnWidths[] =
{
	WebServerUtil::WIDTH_MEDIUM, WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_LARGE,
	WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_LARGE, WebServerUtil::WIDTH_LARGE,
	WebServerUtil::WIDTH_WRAP_CONTENT
};

static const int finishedColumnNames[] =
{
	ResourceManager::FILENAME, ResourceManager::SIZE, ResourceManager::PATH,
	ResourceManager::TIME, ResourceManager::TTH, ResourceManager::USER,
	ResourceManager::HUB, ResourceManager::IP, -1
};

static const uint8_t finishedColumnWidths[] =
{
	WebServerUtil::WIDTH_MEDIUM, WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_LARGE,
	WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_LARGE, WebServerUtil::WIDTH_MEDIUM,
	WebServerUtil::WIDTH_MEDIUM, WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_WRAP_CONTENT
};

static const int waitingColumnNames[] =
{
	ResourceManager::POSITION, ResourceManager::USER, ResourceManager::HUB,
	ResourceManager::ADDED, ResourceManager::IP, ResourceManager::FAKE_FILE_COUNT,
	-1
};

static const uint8_t waitingColumnWidthds[] =
{
	WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_MEDIUM, WebServerUtil::WIDTH_MEDIUM,
	WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_WRAP_CONTENT, WebServerUtil::WIDTH_WRAP_CONTENT,
	WebServerUtil::WIDTH_WRAP_CONTENT
};

static WebServerUtil::TableInfo tableInfo[MAX_PAGES] =
{
	{ _countof(searchResultColumnNames), searchResultColumnNames, searchResultColumnWidths, "search", "search-results" },
	{ _countof(queueColumnNames), queueColumnNames, queueColumnWidths, "queue", "queue" },
	{ _countof(waitingColumnNames), waitingColumnNames, waitingColumnWidthds, "waiting", "waiting" },
	{ _countof(finishedColumnNames), finishedColumnNames, finishedColumnWidths, "recent-ul", "finished" },
	{ _countof(finishedColumnNames), finishedColumnNames, finishedColumnWidths, "recent-dl", "finished" },
	{ 0, nullptr, nullptr, "tools", nullptr },
	{ 0, nullptr, nullptr, "settings", nullptr }
};

static const WebServerUtil::ActionInfo searchActions[] =
{
	{ "download24.png", "addToQueue", "download", 0 },
	{ "magnet24.png", "copyMagnet", "magnet", 0 },
	{ "info24.png", nullptr, nullptr, WebServerUtil::ActionInfo::FLAG_TOOLTIP }
};

static const WebServerUtil::ActionInfo queueActions[] =
{
	{ "remove24.png", "removeItem", "remove", 0 },
	{ "magnet24.png", "copyMagnet", "magnet", 0 },
	{ "info24.png", nullptr, nullptr, WebServerUtil::ActionInfo::FLAG_TOOLTIP }
};

static const WebServerUtil::ActionInfo finishedActions[] =
{
	{ "document24.png", nullptr, nullptr, WebServerUtil::ActionInfo::FLAG_DOWNLOAD_LINK },
	{ "magnet24.png", "copyMagnet", "magnet", 0 },
	{ "info24.png", nullptr, nullptr, WebServerUtil::ActionInfo::FLAG_TOOLTIP }
};

static const WebServerUtil::ActionInfo waitingActions[] =
{
	{ "grant24.png", "grantSlot", "grant", 0 },
	{ "info24.png", nullptr, nullptr, WebServerUtil::ActionInfo::FLAG_TOOLTIP }
};

static const int pageSizes[] =
{
	50, 100, 150, 200, 250, 500
};

static int checkPageSize(int size)
{
	const int cnt = _countof(pageSizes);
	if (size < pageSizes[0]) return pageSizes[0];
	if (size > pageSizes[cnt-1]) return pageSizes[cnt-1];
	for (int i = cnt - 1; i >= 0; --i)
		if (size >= pageSizes[i])
		{
			size = pageSizes[i];
			break;
		}
	return size;
}

static int getColorTheme(const Http::ServerCookies* cookies)
{
	int result = 0;
	if (cookies)
	{
		string s = cookies->get("theme");
		if (!s.empty())
		{
			int index = Util::toInt(s);
			if (index == 0 || index == 1) result = index;
		}
	}
	return result;
}

static int getPageSize(const Http::ServerCookies* cookies)
{
	int pageSize = 0;
	if (cookies)
	{
		string s = cookies->get("pagesize");
		if (!s.empty()) pageSize = Util::toInt(s);
	}
	return checkPageSize(pageSize);
}

static uint64_t getIfModified(const Http::Request& req)
{
	const string& s = req.getHeaderValue(Http::HEADER_IF_MODIFIED_SINCE);
	if (s.empty()) return 0;
	time_t t;
	return Http::parseDateTime(t, s) ? t : 0;
}

WebServerManager::WebServerManager() noexcept : csTemplateCache(RWLock::create())
{
	UrlInfo ui;
	ui.type = HANDLER_TYPE_PAGE;
	ui.cf = &WebServerManager::printSearch;
	urlInfo["search"] = ui;
	ui.cf = &WebServerManager::printQueue;
	urlInfo["queue"] = ui;
	ui.cf = &WebServerManager::printFinishedDownloads;
	urlInfo["recent-dl"] = ui;
	ui.cf = &WebServerManager::printFinishedUploads;
	urlInfo["recent-ul"] = ui;
	ui.cf = &WebServerManager::printWaitingUsers;
	urlInfo["waiting"] = ui;
	ui.cf = &WebServerManager::printTools;
	urlInfo["tools"] = ui;
	ui.cf = &WebServerManager::printSettings;
	urlInfo["settings"] = ui;
	ui.type = HANDLER_TYPE_ACTION;
	ui.cf = &WebServerManager::performSearch;
	urlInfo["xsearch"] = ui;
	ui.cf = &WebServerManager::downloadSearchResult;
	urlInfo["xsrdl"] = ui;
	ui.cf = &WebServerManager::clearSearchResults;
	urlInfo["xsrclr"] = ui;
	ui.cf = &WebServerManager::removeQueueItem;
	urlInfo["xqrm"] = ui;
	ui.cf = &WebServerManager::grantSlot;
	urlInfo["xwugrant"] = ui;
	ui.cf = &WebServerManager::addMagnet;
	urlInfo["xmagnet"] = ui;
	ui.cf = &WebServerManager::refreshShare;
	urlInfo["xrefresh"] = ui;
	ui.cf = &WebServerManager::applySettings;
	urlInfo["xsettings"] = ui;
	ui.type = HANDLER_TYPE_FILE;
	ui.cf = &WebServerManager::downloadFinishedItem;
	urlInfo["xfget"] = ui;
	themeAttr[0].timestamp = themeAttr[1].timestamp = 0;
}

WebServerManager::Server::Server(bool tls, const IpAddressEx& ip, uint16_t port): tls(tls), bindIp(ip), stopFlag(false)
{
	sock.create(ip.type, Socket::TYPE_TCP);
	sock.setSocketOpt(SOL_SOCKET, SO_REUSEADDR, 1);
	string msg = "Starting Web server on " + Util::printIpAddress(ip, true) + ':' + Util::toString(port) + " TLS=" + Util::toString(tls);
	LogManager::message(msg, false);
	if (BOOLSETTING(LOG_WEBSERVER)) LogManager::log(LogManager::WEBSERVER, msg);
	serverPort = sock.bind(port, bindIp);
	sock.listen();
	char threadName[64];
	sprintf(threadName, "WebServer%s-v%d", tls ? "-TLS" : "", ip.type == AF_INET6 ? 6 : 4);
	start(64, threadName);
}

static const uint64_t POLL_TIMEOUT = 250;

int WebServerManager::Server::run() noexcept
{
	while (!stopFlag)
	{
		try
		{
			while (!stopFlag)
			{
				auto ret = sock.wait(POLL_TIMEOUT, Socket::WAIT_ACCEPT);
				if (ret == Socket::WAIT_ACCEPT)
				{
					WebServerManager::getInstance()->accept(sock, tls, this);
				}
			}
		}
		catch (const Exception& e)
		{
			LogManager::message("Web server failed: " + e.getError(), false);
		}
		bool failed = false;
		while (!stopFlag)
		{
			try
			{
				dcassert(bindIp.type == AF_INET || bindIp.type == AF_INET6);
				sock.disconnect();
				sock.create(bindIp.type, Socket::TYPE_TCP);
				serverPort = sock.bind(serverPort, bindIp);
				dcassert(serverPort);
				LogManager::message("Web server started on " + Util::printIpAddress(bindIp, true) + ':' + Util::toString(serverPort) + " TLS=" + Util::toString(tls), false);
				sock.listen();
				/*
				if (type != SERVER_TYPE_SSL)
					WebServerManager::getInstance()->updateLocalIp(bindIp.type);
				*/
				failed = false;
				break;
			}
			catch (const SocketException& e)
			{
				if (!failed)
				{
					LogManager::message("Web server bind error: " + e.getError(), false);
					failed = true;
				}

				// Spin for 60 seconds
				for (int i = 0; i < 60 && !stopFlag; ++i)
				{
					sleep(1000);
					LogManager::message("WebServerManager::Server::run - sleep(1000)", false);
				}
			}
		}
	}
	string msg = "Web server stopped";
	LogManager::message(msg, false);
	if (BOOLSETTING(LOG_WEBSERVER)) LogManager::log(LogManager::WEBSERVER, msg);
	return 0;
}

void WebServerManager::stopServer(int af) noexcept
{
	int index = af == AF_INET6 ? SERVER_V6 : 0;
	for (int i = 0; i < 2; ++i)
	{
		delete servers[index + i];
		servers[index + i] = nullptr;
	}
#if 0
	bool hasServer = false;
	for (int i = 0; i < 4; ++i)
		if (servers[i])
		{
			hasServer = true;
			break;
		}
	if (!hasServer)
		ports[0] = ports[1] = 0;
#endif
}

void WebServerManager::start()
{
	initAuthSecret();
	startListen(AF_INET, false);
	SearchManager::getInstance()->addListener(this);
}

void WebServerManager::shutdown() noexcept
{
	SearchManager::getInstance()->removeListener(this);
	stopServer(AF_INET);
	//stopServer(AF_INET6);
	csClients.lock();
	clients.clear();
	csClients.unlock();
	cs.lock();
	for (auto i : connections)
		delete i.second;
	connections.clear();
	cs.unlock();
}

bool WebServerManager::startListen(int af, bool tls)
{
	uint16_t port = SettingsManager::get(SettingsManager::WEBSERVER_PORT);
	int index = tls ? SERVER_SECURE : 0;
	//ports[index] = newServer->getServerPort();
	//if (index == 0 && newServer->getType() == SERVER_TYPE_AUTO_DETECT)
	//	ports[1] = ports[index];
	if (af == AF_INET6) index |= SERVER_V6;
	if (servers[index]) return false;

	string bind = SettingsManager::get(SettingsManager::WEBSERVER_BIND_ADDRESS);
	IpAddressEx bindIp;
	BufferedSocket::getBindAddress(bindIp, af, bind);
	auto newServer = new Server(tls, bindIp, port);
	servers[index] = newServer;
	return true;
}

void WebServerManager::accept(const Socket& sock, bool tls, Server* server) noexcept
{
	uint16_t port;
	unique_ptr<Socket> newSock;
	if (tls)
		newSock.reset(new SSLSocket(CryptoManager::SSL_SERVER, true, Util::emptyString));
	else
		newSock.reset(new Socket);
	try { port = newSock->accept(sock); }
	catch (const Exception&) { return; }
	uint64_t connId = ++nextConnId;
	HttpServerConnection* conn = new HttpServerConnection(connId, this, newSock, port);

	//uc->updateLastActivity();
	LOCK(cs);
	connections.insert(make_pair(connId, conn));
}

void WebServerManager::sendErrorResponse(HttpServerConnection* conn, int code, const char* text) noexcept
{
	Http::Response resp;
	if (text)
		resp.setResponse(code, text);
	else
		resp.setResponse(code);
	resp.addHeader(Http::HEADER_CONTENT_LENGTH, "0");
	resp.addHeader(Http::HEADER_CONNECTION, "keep-alive");
	conn->sendResponse(resp, Util::emptyString);
}

void WebServerManager::sendRedirect(const RequestInfo& inf, const string& location) noexcept
{
	Http::Response resp;
	resp.setResponse(302);
	resp.addHeader(Http::HEADER_CONTENT_LENGTH, "0");
	resp.addHeader(Http::HEADER_CONNECTION, "keep-alive");
	resp.addHeader(Http::HEADER_LOCATION, location);
	if (inf.cookies) inf.cookies->print(resp);
	inf.conn->sendResponse(resp, Util::emptyString);
}

void WebServerManager::sendLoginPage(const RequestInfo& inf) noexcept
{
	string os;
	printLoginPage(os, inf.cookies);

	Http::Response resp;
	resp.setResponse(200);
	resp.addHeader(Http::HEADER_CONTENT_LENGTH, Util::toString(os.length()));
	resp.addHeader(Http::HEADER_CONNECTION, "keep-alive");
	inf.conn->sendResponse(resp, os);
}

void WebServerManager::onRequest(HttpServerConnection* conn, const Http::Request& req) noexcept
{
	int method = req.getMethodId();
	if (method != Http::METHOD_GET && method != Http::METHOD_POST)
	{
		sendErrorResponse(conn, 501);
		return;
	}

	Http::ServerCookies cookies;

	uint64_t curTime = GET_TIME();
	uint32_t userId = 0;

	RequestInfo inf;
	inf.conn = conn;
	inf.req = &req;
	inf.cookies = &cookies;
	inf.query = nullptr;
	inf.clientId = 0;

	string uri = req.getUri();
	string queryStr;
	auto pos = uri.find('?');
	if (pos != string::npos)
	{
		queryStr = uri.substr(pos + 1);
		uri.erase(pos);
	}

	cookies.parse(req);
	if (uri == "/signin")
	{
		string user;
		if (req.getMethodId() == Http::METHOD_POST)
		{
			auto query = Util::decodeQuery(conn->getRequestBody());
			user = WebServerUtil::getStringQueryParam(&query, "user");
			string password = WebServerUtil::getStringQueryParam(&query, "password");
			if (!user.empty() && !password.empty())
				userId = checkUser(user, password);
		}
		if (!userId)
		{
			sendLoginPage(inf);
			return;
		}
		uint64_t expires = curTime + 60 * SESSION_EXPIRE_TIME;
		unsigned char iv[AC_IV_SIZE];
		createAuthIV(iv);
		inf.clientId = createClientContext(userId, expires, iv);
		string auth = createAuthCookie(expires, inf.clientId, iv);
		cookies.set("auth", auth, expires, "/", Http::ServerCookies::FLAG_HTTP_ONLY);
		sendRedirect(inf, "/");
		if (BOOLSETTING(LOG_WEBSERVER))
			LogManager::log(LogManager::WEBSERVER, printClientId(inf.clientId, conn) + ": User '" + user + "' signed in");
		return;
	}

	if (uri == "/signout")
	{
		const string& auth = cookies.get("auth");
		if (!auth.empty())
		{
			uint64_t clientId;
			if (checkAuthCookie(auth, curTime, clientId))
				removeClientContext(clientId);
			if (BOOLSETTING(LOG_WEBSERVER))
				LogManager::log(LogManager::WEBSERVER, printClientId(clientId, conn) + ": User signed out");
			cookies.remove("auth", false);
		}
		sendRedirect(inf, "/");
		return;
	}

	string filename = uri;
	Util::toNativePathSeparators(filename);
	if (filename.empty() || filename.front() != PATH_SEPARATOR || filename.find(PATH_SEPARATOR, 1) != string::npos)
	{
		sendErrorResponse(conn, 404);
		return;
	}
	filename.erase(0, 1);

	if (!filename.empty())
	{
		if (Text::isAsciiSuffix2(filename, string(".js")))
		{
			sendTemplate(inf, "templates", filename, filename, "application/javascript", ST_EXPAND_LANG_STRINGS);
			return;
		}
		if (Text::isAsciiSuffix2(filename, string(".css")))
		{
			string templateName = filename.substr(0, filename.length() - 4);
			auto pos = templateName.rfind('@');
			if (pos != string::npos)
			{
				string theme = templateName.substr(pos + 1);
				int colorTheme = Util::toInt(theme);
				if (theme.empty() || !(colorTheme == 0 || colorTheme == 1))
				{
					sendErrorResponse(conn, 404);
					return;
				}
				loadColorTheme(colorTheme);
				templateName.erase(pos);
				templateName += ".scss";
				sendTemplate(inf, "templates", templateName, filename, "text/css", ST_EXPAND_CSS | ST_USE_CACHE);
				return;
			}
		}
		string path = Util::getWebServerPath();
		Util::appendPathSeparator(path);
		path += "static";
		path += PATH_SEPARATOR;
		path += filename;
		uint64_t timestamp = File::getTimeStamp(path);
		if (timestamp)
		{
			inf.cookies = nullptr;
			uint64_t ifModified = getIfModified(req);
			if (ifModified && File::timeStampToUnixTime(timestamp) <= ifModified)
				sendErrorResponse(conn, 304);
			else
				sendFile(inf, path, false, timestamp);
			return;
		}
	}

	const string& auth = cookies.get("auth");
	if (!checkAuthCookie(auth, curTime, inf.clientId))
	{
		sendLoginPage(inf);
		return;
	}

	if (filename.empty()) filename = "search";
	auto i = urlInfo.find(filename);
	if (i != urlInfo.end())
	{
		int type = i->second.type;
		if (type == HANDLER_TYPE_PAGE || type == HANDLER_TYPE_ACTION || type == HANDLER_TYPE_FILE)
		{
			Query query;
			if (req.getMethodId() == Http::METHOD_POST)
				query = Util::decodeQuery(conn->getRequestBody());
			else
				query = Util::decodeQuery(queryStr);
			updateAuthCookie(cookies, inf.clientId, curTime);
			inf.query = &query;
			if (BOOLSETTING(LOG_WEBSERVER))
				LogManager::log(LogManager::WEBSERVER, printClientId(inf.clientId, conn) + ": " + req.getMethod() + ' ' + req.getUri());
			handleRequest(inf, i->second);
			return;
		}
	}

	sendErrorResponse(conn, 404);
}

void WebServerManager::updateAuthCookie(Http::ServerCookies& cookies, uint64_t clientId, uint64_t curTime) noexcept
{
	unsigned char iv[AC_IV_SIZE];
	uint64_t expires = curTime + 60 * SESSION_EXPIRE_TIME;
	{
		LOCK(csClients);
		auto i = clients.find(clientId);
		if (i == clients.end()) return;
		auto& context = i->second;
		context.expires = expires;
		updateAuthIV(context.iv);
		memcpy(iv, context.iv, AC_IV_SIZE);
	}
	string auth = createAuthCookie(expires, clientId, iv);
	cookies.set("auth", auth, expires, "/", Http::ServerCookies::FLAG_HTTP_ONLY);
}

void WebServerManager::sendFile(const RequestInfo& inf, const string& path, bool sendContentDisposition, uint64_t timestamp) noexcept
{
	File *f = nullptr;
	int64_t size;
	try
	{
		f = new File(path, File::READ, File::OPEN);
		size = f->getSize();
	}
	catch (Exception&)
	{
		delete f;
		sendErrorResponse(inf.conn, 500);
		return;
	}

	Http::Response resp;
	resp.setResponse(200);
	resp.addHeader(Http::HEADER_CONTENT_LENGTH, Util::toString(size));
	resp.addHeader(Http::HEADER_CONTENT_TYPE, WebServerUtil::getContentTypeForFile(path));
	resp.addHeader(Http::HEADER_CONNECTION, "keep-alive");
	if (sendContentDisposition)
	{
		string filename = Util::encodeUriPath(Util::getFileName(path));
		resp.addHeader(Http::HEADER_CONTENT_DISPOSITION, "attachment; filename*=UTF-8''" + filename + "; filename=" + filename);
	}
	if (timestamp)
		resp.addHeader(Http::HEADER_LAST_MODIFIED, Http::printDateTime(File::timeStampToUnixTime(timestamp)));
	if (inf.cookies) inf.cookies->print(resp);
	inf.conn->sendResponse(resp, Util::emptyString, f);
}

void WebServerManager::handleRequest(const RequestInfo& inf, const UrlInfo& ui) noexcept
{
	HandlerResult res;
	(this->*ui.cf)(res, inf);

	if (res.type == HANDLER_RESULT_REDIRECT)
	{
		if (res.data.empty()) res.data = "/";
		sendRedirect(inf, res.data);
	}
	else if (res.type == HANDLER_RESULT_HTML || res.type == HANDLER_RESULT_JSON)
	{
		Http::Response resp;
		resp.setResponse(200);
		resp.addHeader(Http::HEADER_CONTENT_LENGTH, Util::toString(res.data.length()));
		resp.addHeader(Http::HEADER_CONTENT_TYPE,
			res.type == HANDLER_RESULT_JSON ?
			"application/json;charset=utf-8" : "text/html;charset=utf-8");
		resp.addHeader(Http::HEADER_CONNECTION, "keep-alive");
		if (inf.cookies) inf.cookies->print(resp);
		inf.conn->sendResponse(resp, res.data);
	}
	else if (res.type == HANDLER_RESULT_FILE_PATH)
		sendFile(inf, res.data, true, 0);
	else
		sendErrorResponse(inf.conn, 500);
}

void WebServerManager::sendTemplate(const RequestInfo& inf, const string& dir, const string& name, const string& requestedName, const string& mimeType, int flags) noexcept
{
	CacheItem item;
	bool loadedFromCache = false;
	if (flags & ST_USE_CACHE)
	{
		uint64_t ifModified = getIfModified(*inf.req);
		int cache = getCacheItem(item, requestedName, ifModified);
		if (cache == CACHE_ITEM_FOUND)
			loadedFromCache = true;
		else if (cache == CACHE_ITEM_NOT_MODIFIED)
		{
			string path = Util::getWebServerPath();
			Util::appendPathSeparator(path);
			path += dir;
			path += PATH_SEPARATOR;
			path += name;
			uint64_t ts = File::getTimeStamp(path);
			if (!ts)
			{
				sendErrorResponse(inf.conn, 404);
				return;
			}
			if (File::timeStampToUnixTime(ts) <= ifModified)
			{
				sendErrorResponse(inf.conn, 304);
				return;
			}
		}
	}
	if (!loadedFromCache && !loadTemplate(item, dir, name))
	{
		sendErrorResponse(inf.conn, 404);
		return;
	}

	if (!loadedFromCache)
	{
		if (flags & ST_EXPAND_LANG_STRINGS)
			WebServerUtil::expandLangStrings(item.data);
		if (flags & ST_EXPAND_CSS)
		{
			int theme = getColorTheme(inf.cookies);
			csThemeAttr.lock();
			WebServerUtil::expandCssVariables(item.data, themeAttr[theme].vars);
			csThemeAttr.unlock();
		}
		if (flags & ST_USE_CACHE)
		{
			item.timestamp = Util::getFileTime();
			setCacheItem(requestedName, item.data, item.timestamp);
		}
	}

	Http::Response resp;
	resp.setResponse(200);
	resp.addHeader(Http::HEADER_CONTENT_LENGTH, Util::toString(item.data.length()));
	if (flags & ST_VOLATILE)
	{
		resp.addHeader(Http::HEADER_PRAGMA, "no-cache");
		resp.addHeader(Http::HEADER_CACHE_CONTROL, "no-cache");
	}
	else
		resp.addHeader(Http::HEADER_LAST_MODIFIED, Http::printDateTime(File::timeStampToUnixTime(item.timestamp)));
	resp.addHeader(Http::HEADER_CONTENT_TYPE, mimeType + ";charset=utf-8");
	resp.addHeader(Http::HEADER_CONNECTION, "keep-alive");
#if 0
	if (inf.cookies) inf.cookies->print(resp);
#endif
	inf.conn->sendResponse(resp, item.data);
}

bool WebServerManager::loadTemplate(CacheItem& result, const string& dir, const string& name) noexcept
{
	string path = Util::getWebServerPath();
	Util::appendPathSeparator(path);
	string fullName = dir;
	fullName += PATH_SEPARATOR;
	fullName += name;
	path += fullName;
	uint64_t ts = File::getTimeStamp(path);
	if (!ts) return false;

	string key = "in:" + fullName;
	{
		READ_LOCK(*csTemplateCache);
		auto i = templateCache.find(key);
		if (i != templateCache.end() && i->second.timestamp == ts)
		{
			result = i->second;
			return true;
		}
	}

	try
	{
		File f(path, File::READ, File::OPEN);
		result.data = f.read();
	}
	catch (Exception&)
	{
		return false;
	}

	{
		WRITE_LOCK(*csTemplateCache);
		auto& cached = templateCache[key];
		cached.timestamp = ts;
		cached.data = result.data;
	}

	result.timestamp = ts;
	return true;
}

int WebServerManager::getCacheItem(CacheItem& result, const string& name, uint64_t ifModified) noexcept
{
	string key = "out:" + name;
	READ_LOCK(*csTemplateCache);
	auto i = templateCache.find(key);
	if (i == templateCache.end()) return CACHE_ITEM_NOT_FOUND;
	if (ifModified && File::timeStampToUnixTime(i->second.timestamp) <= ifModified) return CACHE_ITEM_NOT_MODIFIED;
	result = i->second;
	return CACHE_ITEM_FOUND;
}

void WebServerManager::setCacheItem(const string& name, const string& data, uint64_t timestamp) noexcept
{
	string key = "out:" + name;
	WRITE_LOCK(*csTemplateCache);
	auto& cached = templateCache[key];
	cached.data = data;
	cached.timestamp = timestamp;
}

void WebServerManager::removeCacheItem(const string& name) noexcept
{
	string key = "out:" + name;
	WRITE_LOCK(*csTemplateCache);
	auto i = templateCache.find(key);
	if (i != templateCache.end()) templateCache.erase(i);
}

void WebServerManager::ClientContext::startSearch(const string& term, int type, bool onlyFreeSlots) noexcept
{
	static const unsigned SEARCH_RESULTS_WAIT_TIME = 10000;
	uint64_t owner = id << 8 | WebServerManager::FRAME_TYPE_WEB_CLIENT;
	ClientManager::cancelSearch(owner);
	searchParam.removeToken();
	searchParam.owner = owner;
	if (type < 0 || type >= NUMBER_OF_FILE_TYPES) type = FILE_TYPE_ANY;

	searchParam.setFilter(term, type);
	if (type == FILE_TYPE_TTH && searchParam.fileType != FILE_TYPE_TTH)
		searchError = STRING(INVALID_TTH);

	searchResults.clear();
	searchStartTime = searchEndTime = 0;
	if (searchParam.filter.empty() || !searchError.empty()) return;

	searchParam.generateToken(false);
	searchTerm = searchParam.filter;
	searchParam.prepareFilter();

	searchOnlyFreeSlots = onlyFreeSlots;
	vector<SearchClientItem> clients;
	unsigned waitTime = ClientManager::multiSearch(searchParam, clients);
	searchStartTime = GET_TICK();
	unsigned dhtSearchTime = 0;
	searchEndTime = searchStartTime + waitTime + max(dhtSearchTime, SEARCH_RESULTS_WAIT_TIME);
}

void WebServerManager::ClientContext::clearSearchResults() noexcept
{
	searchResults.clear();
	searchParam.removeToken();
	searchParam.filter.clear();
	searchParam.filterExclude.clear();
	searchTerm.clear();
	searchStartTime = searchEndTime = 0;
}

void WebServerManager::ClientContext::addSearchResult(const SearchResult& sr) noexcept
{
	if (searchResults.size() >= MAX_SEARCH_RESULTS) return;
	if (!searchParam.matchSearchResult(sr, searchOnlyFreeSlots)) return;
	searchResults.emplace_back(std::make_unique<SearchResult>(sr));
}

void WebServerManager::ClientContext::printSearchTitle(string& os, bool isRunning) noexcept
{
	string tmp;
	os += "<h2>";
	string what = "<em>" + SimpleXML::escape(searchTerm, tmp, false) + "</em>";
	if (isRunning)
	{
		os += STRING(SEARCHING_FOR);
		os += ' ';
		os += what;
		os += " ...";
	}
	else
		os += STRING_F(SEARCHED_FOR_FMT, what);
	os += "</h2>";
}

static void printButton(string& os, ResourceManager::Strings text, const char* buttonId, const char* actionUrl)
{
	string tmp;
	os += "<input type='button' class='misc-button' id='";
	os += buttonId;
	os += "' onclick='performButtonAction(\"";
	os += buttonId;
	os += "\")' data-action-url='";
	os += SimpleXML::escape(actionUrl, tmp, true);
	os += "' value='";
	os += SimpleXML::escape(STRING_I(text), tmp, true);
	os += "'>";
}

void WebServerManager::ClientContext::printSearchResults(string& os, size_t from, size_t count, bool isRunning) noexcept
{
	os += "<div id='search-results-count'>";
	WebServerUtil::printItemCount(os, searchResults.size(), ResourceManager::NO_ITEMS_FOUND, ResourceManager::PLURAL_ITEMS_FOUND);
	if (isRunning)
		printButton(os, ResourceManager::WEBSERVER_REFRESH, "button-refresh", "/search");
	else if (count)
		printButton(os, ResourceManager::CLEAR_RESULTS, "button-clear", "/xsrclr");
	os += "</div>\n";
	if (!count) return;
	string tmp;
	os += "<table id='search-results' class='t'>\n";
	const WebServerUtil::TableInfo& ti = tableInfo[PAGE_SEARCH];
	WebServerUtil::printTableHeader(os, ti, searchResultsSort);
	string actions[3];
	for (size_t i = 0; i < count; ++i)
	{
		const SearchResult* sr = searchResults[from + i].get();
		string rowId = "row-" + Util::toString(from+i);
		string user = sr->getUser()->getLastNick();
		string filename = sr->getFileName();
		string tthStr;
		actions[2] = STRING(FILENAME) + ": " + filename + '\n';
		actions[2] += STRING(USER) + ": " + user + '\n';
		os += "<tr class='data' id='" + rowId + "'";
		if (sr->getType() == SearchResult::TYPE_FILE && !sr->getTTH().isZero())
		{
			tthStr = sr->getTTH().toBase32();
			os += " data-magnet='";
			os += SimpleXML::escape(Util::getMagnet(tthStr, filename, sr->getSize()), tmp, true);
			os += '\'';
		}
		os += '>';
		int column = 0, offset = 0;
		WebServerUtil::printTableCell(os, column, ti, user, true);
		if (sr->getType() == SearchResult::TYPE_FILE)
		{
			WebServerUtil::printTableCell(os, column, ti, filename, true);
			WebServerUtil::printTableCell(os, column, ti, Util::formatBytes(sr->getSize()), false);
			WebServerUtil::printTableCell(os, column, ti, tthStr, false);
			actions[2] += STRING(SIZE) + ": " + Util::formatExactSize(sr->getSize()) + '\n';
			actions[2] += STRING(TTH) + ": " + tthStr + '\n';
			actions[2] += STRING(PATH) + ": " + sr->getFilePath();
		}
		else
		{
			WebServerUtil::printTableCell(os, column, ti, filename, true);
			WebServerUtil::printTableCell(os, column, ti, STRING(DIRECTORY), false);
			WebServerUtil::printTableCell(os, column, ti, sr->getFile(), true);
			actions[2] += STRING(PATH) + ": " + sr->getFile();
			offset = 2;
		}
		os += "<td class='r-border w0'>";
		actions[0] = "/xsrdl?id=" + WebServerUtil::printItemId((uintptr_t) sr);
		WebServerUtil::printActions(os, 3 - offset, searchActions + offset, actions + offset, rowId);
		os += "</td></tr>\n";
	}
	WebServerUtil::printTableFooter(os, ti);
	os += "</table>\n";
}

static bool compareSearchResults(const SearchResult* a, const SearchResult* b, int column) noexcept
{
	int res;
	switch (column)
	{
		case 0: // User
			res = compare(a->getUser()->getLastNick(), b->getUser()->getLastNick());
			if (res) return res < 0;
			// fall through
		case 1: // File name
			res = compare(a->getFileName(), b->getFileName());
			if (res) return res < 0;
			// fall through
		case 3: // TTH / Misc
		{
			compareByTTH:
			bool aDir = a->getType() == SearchResult::TYPE_DIRECTORY;
			bool bDir = b->getType() == SearchResult::TYPE_DIRECTORY;
			if (aDir != bDir) return bDir;
			if (aDir) return a->getFile() < b->getFile();
			return a->getTTH() < b->getTTH();
		}
		case 2: // Size
			res = compare(a->getSize(), b->getSize());
			if (res) return res < 0;
			goto compareByTTH;
	}
	return false;
}

bool WebServerManager::ClientContext::updateSortColumn(int& savedColumn, int& column, int maxColumn, bool& reverse)
{
	reverse = false;
	if (savedColumn == column) return false;
	savedColumn = column;
	if (column < 0)
	{
		reverse = true;
		column = -column;
	}
	--column;
	return column >= 0 && column <= maxColumn;
}

void WebServerManager::ClientContext::sortSearchResults(int sortColumn) noexcept
{
	bool reverse;
	if (!updateSortColumn(searchResultsSort, sortColumn, 3, reverse)) return;
	std::sort(searchResults.begin(), searchResults.end(),
		[=](const auto& a, const auto& b)
		{
			return compareSearchResults(a.get(), b.get(), sortColumn) ^ reverse;
		});
}

void WebServerManager::ClientContext::getQueue() noexcept
{
	uint64_t id = QueueManager::fileQueue.getGenerationId();
	if (queueId == id) return;
	queueId = id;
	queue.clear();
	QueueManager::LockFileQueueShared fileQueue;
	const auto& li = fileQueue.getQueueL();
	for (auto j = li.cbegin(); j != li.cend(); ++j)
	{
		const QueueItemPtr& qi = j->second;
		uint16_t sourcesCount = (uint16_t) std::min(qi->getSourcesL().size(), (size_t) UINT16_MAX);
		uint16_t onlineSourcesCount = (uint16_t) std::min(qi->getOnlineSourceCountL(), (size_t) UINT16_MAX);
		queue.emplace_back(QueueItemEx{ qi, qi->getSourcesVersion(), sourcesCount, onlineSourcesCount });
	}
}

void WebServerManager::ClientContext::QueueItemEx::updateInfo() noexcept
{
	uint32_t sourcesVersion = qi->getSourcesVersion();
	if (sourcesVersion == version) return;
	QueueRLock(*QueueItem::g_cs);
	sourcesCount = (uint16_t) std::min(qi->getSourcesL().size(), (size_t) UINT16_MAX);
	onlineSourcesCount = (uint16_t) std::min(qi->getOnlineSourceCountL(), (size_t) UINT16_MAX);
	version = sourcesVersion;
}

void WebServerManager::ClientContext::QueueItemEx::updateInfoL() noexcept
{
	uint32_t sourcesVersion = qi->getSourcesVersion();
	if (sourcesVersion == version) return;
	sourcesCount = (uint16_t) std::min(qi->getSourcesL().size(), (size_t) UINT16_MAX);
	onlineSourcesCount = (uint16_t) std::min(qi->getOnlineSourceCountL(), (size_t) UINT16_MAX);
	version = sourcesVersion;
}

const string& WebServerManager::ClientContext::getQueueItemStatus(const QueueItemEx& inf, string& tmp) noexcept
{
	if (inf.qi->isFinished())
		return STRING(DOWNLOAD_FINISHED_IDLE);
	if (inf.qi->isWaiting())
	{
		if (inf.onlineSourcesCount)
		{
			if (inf.sourcesCount == 1)
				return STRING(WAITING_USER_ONLINE);
			tmp = STRING_F(WAITING_USERS_ONLINE_FMT, inf.onlineSourcesCount % inf.sourcesCount);
			return tmp;
		}
		switch (inf.sourcesCount)
		{
			case 0:
				return STRING(NO_USERS_TO_DOWNLOAD_FROM);
			case 1:
				return STRING(USER_OFFLINE);
			case 2:
				return STRING(BOTH_USERS_OFFLINE);
			case 3:
				return STRING(ALL_3_USERS_OFFLINE);
			case 4:
				return STRING(ALL_4_USERS_OFFLINE);
		}
		tmp = STRING_F(ALL_USERS_OFFLINE_FMT, inf.sourcesCount);
		return tmp;
	}
	if (inf.onlineSourcesCount == 1)
		return STRING(USER_ONLINE);
	tmp = STRING_F(USERS_ONLINE_FMT, inf.onlineSourcesCount % inf.sourcesCount);
	return tmp;
}

static string getSourceName(const UserPtr& user, bool partial)
{
	string res;
#ifdef BL_FEATURE_IP_DATABASE
	string hubUrl;
	if (user->getLastNickAndHub(res, hubUrl))
	{
		res += " - ";
		res += hubUrl;
	}
#endif
	if (res.empty()) res = user->getLastNick();
	if (partial) res.insert(0, "[P] ");
	return res;
}

static string getSources(const QueueItem* qi, string& tmp) noexcept
{
	tmp.clear();
	{
		int count = 0;
		QueueRLock(*QueueItem::g_cs);
		const auto& sources = qi->getSourcesL();
		for (auto j = sources.cbegin(); j != sources.cend(); ++j)
		{
			if (!tmp.empty()) tmp += ", ";
			if (++count > 4)
			{
				tmp += "...";
				break;
			}
			tmp += getSourceName(j->first, j->second.isAnySet(QueueItem::Source::FLAG_PARTIAL));
		}
	}
	return tmp.empty() ? STRING(NO_USERS) : tmp;
}

void WebServerManager::ClientContext::printQueue(string& os, size_t from, size_t count) noexcept
{
	WebServerUtil::printItemCount(os, queue.size(), ResourceManager::NO_ITEMS_IN_QUEUE, ResourceManager::PLURAL_ITEMS_IN_QUEUE);
	if (!count) return;
	string tmp;
	os += "<table id='queue' class='t'>\n";
	const WebServerUtil::TableInfo& ti = tableInfo[PAGE_QUEUE];
	WebServerUtil::printTableHeader(os, ti, queueSort);
	string actions[3];
	for (size_t i = 0; i < count; ++i)
	{
		QueueItemEx& inf = queue[from + i];
		inf.updateInfo();
		const QueueItem* qi = inf.qi.get();
		string rowId = "row-" + Util::toString(from+i);
		string filename = Util::getFileName(qi->getTarget());
		string path = Util::getFilePath(qi->getTarget());
		string tthStr;
		string status = getQueueItemStatus(inf, tmp);
		if (!qi->getTTH().isZero()) tthStr = qi->getTTH().toBase32();
		string magnet = Util::getMagnet(tthStr, filename, qi->getSize());
		os += "<tr class='data' id='" + rowId + "' data-magnet='" + SimpleXML::escape(magnet, tmp, true) + "'>";
		int column = 0;
		WebServerUtil::printTableCell(os, column, ti, filename, true);
		WebServerUtil::printTableCell(os, column, ti, Util::formatBytes(qi->getSize()), false);
		WebServerUtil::printTableCell(os, column, ti, path, true);
		WebServerUtil::printTableCell(os, column, ti, Util::toString(inf.sourcesCount), false);
		WebServerUtil::printTableCell(os, column, ti, tthStr, false);
		WebServerUtil::printTableCell(os, column, ti, status, true);
		os += "<td class='r-border w0'>";
		actions[2] = STRING(FILENAME) + ": " + filename + '\n';
		actions[2] += STRING(SIZE) + ": " + Util::formatExactSize(qi->getSize()) + '\n';
		actions[2] += STRING(PATH) + ": " + path + '\n';
		actions[2] += STRING(SOURCES) + ": " + getSources(qi, tmp) + '\n';
		actions[2] += STRING(TTH) + ": " + tthStr + '\n';
		actions[2] += STRING(STATUS) + ": " + status;
		actions[0] = "/xqrm?id=" + WebServerUtil::printItemId((uintptr_t) qi);
		WebServerUtil::printActions(os, 3, queueActions, actions, rowId);
		os += "</td></tr>\n";
	}
	WebServerUtil::printTableFooter(os, ti);
	os += "</table>\n";
}

bool WebServerManager::ClientContext::QueueItemEx::compareQueueItems(const QueueItemEx& a, const QueueItemEx& b, int column) noexcept
{
	int res;
	switch (column)
	{
		case 0: // File name
			res = compare(Util::getFileName(a.qi->getTarget()), Util::getFileName(b.qi->getTarget()));
			if (res) return res < 0;
			// fall through
		case 2: // Path
			return a.qi->getTarget() < b.qi->getTarget();
		case 1: // Size
			res = compare(a.qi->getSize(), b.qi->getSize());
			if (res) return res < 0;
			return a.qi->getTarget() < b.qi->getTarget();
		case 3: // Sources
			res = compare(a.sourcesCount, b.sourcesCount);
			if (res) return res < 0;
			return a.qi->getTarget() < b.qi->getTarget();
		case 4: // TTH
			res = compare(a.qi->getTTH(), b.qi->getTTH());
			if (res) return res < 0;
			return a.qi->getTarget() < b.qi->getTarget();
		case 5: // Status
		{
			string tmp1, tmp2;
			res = compare(getQueueItemStatus(a, tmp1), getQueueItemStatus(b, tmp2));
			if (res) return res < 0;
			return a.qi->getTarget() < b.qi->getTarget();
		}
	}
	return false;
}

void WebServerManager::ClientContext::sortQueue(int sortColumn) noexcept
{
	bool reverse;
	if (!updateSortColumn(queueSort, sortColumn, 5, reverse))
		return;
	{
		QueueRLock(*QueueItem::g_cs);
		for (QueueItemEx& inf : queue)
			inf.updateInfoL();
	}
	std::sort(queue.begin(), queue.end(),
		[=](const auto& a, const auto& b)
		{
			return QueueItemEx::compareQueueItems(a, b, sortColumn) ^ reverse;
		});
}

void WebServerManager::ClientContext::printFinishedItems(string& os, size_t from, size_t count, const vector<FinishedItemPtr>& data, int type) noexcept
{
	WebServerUtil::printItemCount(os, data.size(), ResourceManager::NO_RECENT_ITEMS, ResourceManager::PLURAL_RECENT_ITEMS);
	if (!count) return;
	os += "<table id='finished' class='t'>\n";
	string tmp;
	int page = type == FinishedManager::e_Download ? PAGE_FINISHED_DOWNLOADS : PAGE_FINISHED_UPLOADS;
	const WebServerUtil::TableInfo& ti = tableInfo[page];
	WebServerUtil::printTableHeader(os, ti, finishedItemsSort[type]);
	string actions[3];
	for (size_t i = 0; i < count; ++i)
	{
		FinishedItem* fi = data[from + i].get();
		string rowId = "row-" + Util::toString(from+i);
		string filename = Util::getFileName(fi->getTarget());
		string path = Util::getFilePath(fi->getTarget());
		string timeStr = Util::formatDateTime(fi->getTime(), false);
		string tthStr;
		os += "<tr class='data' id='" + rowId + "'";
		if (!fi->getTTH().isZero())
		{
			tthStr = fi->getTTH().toBase32();
			os += " data-magnet='";
			os += SimpleXML::escape(Util::getMagnet(tthStr, filename, fi->getSize()), tmp, true);
			os += '\'';
		}
		os += '>';
		int column = 0;
		WebServerUtil::printTableCell(os, column, ti, filename, true);
		WebServerUtil::printTableCell(os, column, ti, Util::formatBytes(fi->getSize()), false);
		WebServerUtil::printTableCell(os, column, ti, path, true);
		WebServerUtil::printTableCell(os, column, ti, timeStr, true);
		WebServerUtil::printTableCell(os, column, ti, tthStr, false);
		WebServerUtil::printTableCell(os, column, ti, fi->getNick(), true);
		WebServerUtil::printTableCell(os, column, ti, fi->getHub(), true);
		WebServerUtil::printTableCell(os, column, ti, fi->getIP(), false);
		os += "<td class='r-border w0'>";
		actions[2] = STRING(FILENAME) + ": " + filename + '\n';
		actions[2] += STRING(SIZE) + ": " + Util::formatExactSize(fi->getSize()) + '\n';
		actions[2] += STRING(PATH) + ": " + path + '\n';
		actions[2] += STRING(TIME) + ": " + timeStr + '\n';
		actions[2] += STRING(TTH) + ": " + tthStr + '\n';
		actions[2] += STRING(USER) + ": " + fi->getNick() + '\n';
		actions[2] += STRING(HUB) + ": " + fi->getHub() + '\n';
		actions[2] += STRING(IP) + ": " + fi->getIP();
		actions[0] = "/xfget?t=" + Util::toString(type) + "&id=" + WebServerUtil::printItemId((uintptr_t) fi);
		WebServerUtil::printActions(os, 3, finishedActions, actions, rowId);
		os += "</td></tr>\n";
	}
	WebServerUtil::printTableFooter(os, ti);
	os += "</table>\n";
}

static bool compareFinishedItems(const FinishedItem* a, const FinishedItem* b, int column) noexcept
{
	int res;
	switch (column)
	{
		case 0: // File name
			res = compare(Util::getFileName(a->getTarget()), Util::getFileName(b->getTarget()));
			if (res) return res < 0;
			return a->getTTH() < b->getTTH();
		case 1: // Size
			res = compare(a->getSize(), b->getSize());
			if (res) return res < 0;
			return a->getTTH() < b->getTTH();
		case 2: // Path
			res = compare(Util::getFilePath(a->getTarget()), Util::getFilePath(b->getTarget()));
			if (res) return res < 0;
			return a->getTTH() < b->getTTH();
		case 3: // Time
			res = compare(a->getTime(), b->getTime());
			if (res) return res < 0;
			return a->getTTH() < b->getTTH();
		case 4: // TTH
			res = compare(a->getTTH(), b->getTTH());
			if (res) return res < 0;
			return a->getTime() < b->getTime();
		case 5: // User
			res = compare(a->getNick(), b->getNick());
			if (res) return res < 0;
			return a->getTTH() < b->getTTH();
		case 6: // Hub
			res = compare(a->getHub(), b->getHub());
			if (res) return res < 0;
			return a->getTTH() < b->getTTH();
		case 7: // IP
			res = compare(a->getIP(), b->getIP());
			if (res) return res < 0;
			return a->getTTH() < b->getTTH();
	}
	return false;
}

void WebServerManager::ClientContext::sortFinishedItems(int type, int sortColumn)
{
	bool reverse;
	if (!updateSortColumn(finishedItemsSort[type], sortColumn, 7, reverse)) return;
	vector<FinishedItemPtr>& v = type == FinishedManager::e_Download ? finishedDownloads : finishedUploads;
	std::sort(v.begin(), v.end(),
		[=](const auto& a, const auto& b)
		{
			return compareFinishedItems(a.get(), b.get(), sortColumn) ^ reverse;
		});
}

void WebServerManager::ClientContext::getFinishedItems(int type)
{
	auto fm = FinishedManager::getInstance();
	auto& out = type == FinishedManager::e_Upload ? finishedUploads : finishedDownloads;
	uint64_t id;
	const auto& in = fm->lockList((FinishedManager::eType) type, &id);
	if (finishedItemsId[type] != id)
	{
		finishedItemsId[type] = id;
		out.clear();
		out.reserve(in.size());
		for (const auto& item : in) out.push_back(item);
		finishedItemsSort[type] = 0;
	}
	fm->unlockList((FinishedManager::eType) type);
}

void WebServerManager::ClientContext::getWaitingUsers() noexcept
{
	UploadManager::LockInstanceQueue lockedInstance;
	uint64_t id = lockedInstance->getSlotQueueIdL();
	if (waitingUsersId == id) return;
	waitingUsersId = id;
	waitingUsers.clear();
	const auto& users = lockedInstance->getUploadQueueL();
	size_t position = 0;
	for (const WaitingUser& wu : users)
	{
		const auto& waitingFiles = wu.getWaitingFiles();
		uint64_t added = waitingFiles.empty() ? 0 : waitingFiles[0]->getTime();
		waitingUsers.emplace_back(WaitingUsersItem{ wu.getHintedUser(), added, waitingFiles.size(), ++position });
	}
}

void WebServerManager::ClientContext::printWaitingUsers(string& os, size_t from, size_t count) noexcept
{
	WebServerUtil::printItemCount(os, waitingUsers.size(), ResourceManager::NO_WAITING_USERS, ResourceManager::PLURAL_USERS);
	if (!count) return;
	os += "<table id='waiting' class='t'>\n";
	string nick, tmp, timeStr, ipStr;
	const WebServerUtil::TableInfo& ti = tableInfo[PAGE_WAITING_USERS];
	WebServerUtil::printTableHeader(os, ti, waitingUsersSort);
	string actions[2];
	for (size_t i = 0; i < count; ++i)
	{
		const WaitingUsersItem& wu = waitingUsers[from + i];
		string rowId = "row-" + Util::toString(from+i);
		Ip4Address ip4;
		Ip6Address ip6;
		int64_t bytesShared;
		int slots;
		wu.hintedUser.user->getInfo(nick, ip4, ip6, bytesShared, slots);
		if (wu.added)
			timeStr = Util::formatDateTime(wu.added);
		else
			timeStr.clear();
		if (Util::isValidIp4(ip4))
			ipStr = Util::printIpAddress(ip4);
		else if (Util::isValidIp6(ip6))
			ipStr = Util::printIpAddress(ip6);
		else
			ipStr.clear();
		os += "<tr class='data' id='" + rowId + "'>";
		int column = 0;
		WebServerUtil::printTableCell(os, column, ti, Util::toString(wu.position), false);
		WebServerUtil::printTableCell(os, column, ti, nick, true);
		WebServerUtil::printTableCell(os, column, ti, wu.hintedUser.hint, true);
		WebServerUtil::printTableCell(os, column, ti, timeStr, true);
		WebServerUtil::printTableCell(os, column, ti, ipStr, false);
		WebServerUtil::printTableCell(os, column, ti, Util::toString(wu.fileCount), false);
		os += "<td class='r-border w0'>";
		actions[1] = STRING(POSITION) + ": " + Util::toString(wu.position) + '\n';
		actions[1] += STRING(USER) + ": " + nick + '\n';
		actions[1] += STRING(HUB) + ": " + wu.hintedUser.hint + '\n';
		actions[1] += STRING(ADDED) + ": " + timeStr + '\n';
		actions[1] += STRING(IP) + ": " + ipStr + '\n';
		actions[1] += STRING(FAKE_FILE_COUNT) + ": " + Util::toString(wu.fileCount);
		actions[0] = "/xwugrant?id=" + WebServerUtil::printItemId((uintptr_t) wu.hintedUser.user.get());
		WebServerUtil::printActions(os, 2, waitingActions, actions, rowId);
		os += "</td></tr>\n";
	}
	WebServerUtil::printTableFooter(os, ti);
	os += "</table>\n";
}

static bool compareWaitingUsers(const WaitingUsersItem& a, const WaitingUsersItem& b, int column) noexcept
{
	int res;
	switch (column)
	{
		case 0: // Position
			return a.position < b.position;
		case 1: // User
			res = compare(a.hintedUser.user->getLastNick(), b.hintedUser.user->getLastNick());
			if (res) return res < 0;
			return a.position < b.position;
		case 2: // Hub
			res = compare(a.hintedUser.hint, b.hintedUser.hint);
			if (res) return res < 0;
			return a.position < b.position;
		case 3: // Added
			res = compare(a.added, b.added);
			if (res) return res < 0;
			return a.position < b.position;
		case 4: // IP
		{
			Ip4Address a4 = a.hintedUser.user->getIP4();
			Ip4Address b4 = b.hintedUser.user->getIP4();
			if (Util::isValidIp4(a4) && Util::isValidIp4(b4))
			{
				res = compare(a4, b4);
				if (res) return res < 0;
				return a.position < b.position;
			}
			Ip6Address a6 = a.hintedUser.user->getIP6();
			Ip6Address b6 = b.hintedUser.user->getIP6();
			if (Util::isValidIp6(a6) && Util::isValidIp6(b6))
			{
				res = compare(Util::printIpAddress(a6), Util::printIpAddress(b6)); // FIXME
				if (res) return res < 0;
			}
			return a.position < b.position;
		}
		case 5: // File count
			res = compare(a.fileCount, b.fileCount);
			if (res) return res < 0;
			return a.position < b.position;
	}
	return false;
}

void WebServerManager::ClientContext::sortWaitingUsers(int sortColumn) noexcept
{
	bool reverse;
	if (!updateSortColumn(waitingUsersSort, sortColumn, 5, reverse)) return;
	std::sort(waitingUsers.begin(), waitingUsers.end(),
		[=](const auto& a, const auto& b)
		{
			return compareWaitingUsers(a, b, sortColumn) ^ reverse;
		});
}

void WebServerManager::printSearchForm(string& os, const RequestInfo& state) noexcept
{
	int selected = 0;
	os += "<form method='POST' action='/xsearch' id='search-form' class='bottom-margin'>\n"
		"<div><input id='search-string' class='field' name='s' placeholder='";
	os += STRING(ENTER_SEARCH_STRING);
	os += "'>";
	// FIXME
	ResourceManager::Strings resId[NUMBER_OF_FILE_TYPES];
	for (int i = 0; i < NUMBER_OF_FILE_TYPES; ++i)
		resId[i] = SearchManager::getTypeStr(i);
	WebServerUtil::printSelector(os, "type", NUMBER_OF_FILE_TYPES, resId, 0, selected);
	os += "<div class='button-container' id='search-button'>"
		"<input type='submit' class='button' value='";
	os += STRING(SEARCH);
	os += "' onclick='sendSearch()'/></div></div><div class='checkbox'>"
		"<div><input type='checkbox' name='ofs' id='checkbox-ofs'/></div>"
		"<label for='checkbox-ofs'>";
	os += STRING(ONLY_FREE_SLOTS);
	os += "</label></div></form>\n";
}

void WebServerManager::getTableParams(const RequestInfo& state, int& page, int& sortColumn) noexcept
{
	sortColumn = page = 0;
	if (state.query)
	{
		auto i = state.query->find("sort");
		if (i != state.query->end())
			sortColumn = Util::toInt(i->second);
		i = state.query->find("p");
		if (i != state.query->end())
			page = Util::toInt(i->second);
	}
}

void WebServerManager::printHtmlStart(string& os, const Http::ServerCookies* cookies) noexcept
{
	os = htmlStart1;
	os += Util::toString(getColorTheme(cookies));
	os += htmlStart2;
}

void WebServerManager::printNavigationBar(string& os, int selectedPage) noexcept
{
	string tmp;
	os += "<div class='sidebar'>\n";
	os += "<div class='title'>";
	os += SimpleXML::escape(getAppNameVer(), tmp, false);
	os += "</div>";
	os += "<div class='hr'> </div>\n";
	for (int i = 0; i < MAX_PAGES; ++i)
	{
		os += "<div";
		if (i == selectedPage)
		{
			os += " class='selected'>";
			os += SimpleXML::escape(ResourceManager::getString(pageNames[i]), tmp, false);
		}
		else
		{
			os += "><a href='/";
			os += tableInfo[i].url;
			os += "'>";
			os += SimpleXML::escape(ResourceManager::getString(pageNames[i]), tmp, false);
			os += "</a>";
		}
		os += "</div>\n";
	}
	os += "<div class='hr'> </div>\n<div><a href='/signout'>";
	os += SimpleXML::escape(STRING(WEBSERVER_SIGN_OUT), tmp, false);
	os += "</a></div>\n</div>\n";
}

void WebServerManager::printLoginPage(string& os, const Http::ServerCookies* cookies) noexcept
{
	string tmp;
	printHtmlStart(os, cookies);
	os += "<div class='align-center'><div id='login-form'><h3>";
	os += SimpleXML::escape(STRING(WEBSERVER_LOGIN_PAGE_NAME), tmp, false);
	os += "</h3><form action='/signin' method='POST'>\n"
	      "<div><input type='text' id='user' name='user' class='login-field' placeholder='";
	os += SimpleXML::escape(STRING(WEBSERVER_USERNAME), tmp, true);
	os += "'></div>\n"
	      "<div><input type='password' id='password' name='password' class='login-field' placeholder='";
	os += SimpleXML::escape(STRING(PASSWORD), tmp, true);
	os += "'></div>\n"
	      "<div><input type='submit' class='button' id='login-button' value='";
	os += SimpleXML::escape(STRING(WEBSERVER_SIGN_IN), tmp, true);
	os += "'></div></form>\n</div>\n";
	os += htmlEnd;
}

void WebServerManager::printTitle(string& os, int text, const char* cls) noexcept
{
	string tmp;
	os += "<h2";
	if (cls)
	{
		os += " class='";
		os += cls;
		os += '\'';
	}
	os += '>';
	os += SimpleXML::escape(ResourceManager::getString((ResourceManager::Strings) text), tmp, false);
	os += "</h2>\n";
}

void WebServerManager::printSearch(HandlerResult& res, const RequestInfo& state) noexcept
{
	printHtmlStart(res.data, state.cookies);
	printNavigationBar(res.data, PAGE_SEARCH);
	res.data += htmlMainDiv;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		int page, sortColumn;
		WebServerUtil::TablePageInfo pi;
		getTableParams(state, page, sortColumn);
		ClientContext& ctx = i->second;
		if (ctx.searchParam.token && !ctx.searchParam.filter.empty())
		{
			bool isRunning = GET_TICK() < ctx.searchEndTime;
			ctx.printSearchTitle(res.data, isRunning);
			WebServerUtil::getTableRange(page, ctx.searchResults.size(), getPageSize(state.cookies), pi);
			if (sortColumn) ctx.sortSearchResults(sortColumn);
			ctx.printSearchResults(res.data, pi.start, pi.count, isRunning);
			if (pi.pages > 1) WebServerUtil::printPageSelector(res.data, pi.pages, page, tableInfo[PAGE_SEARCH]);
			printTitle(res.data, ResourceManager::WEBSERVER_START_ANOTHER_SEARCH, "top-margin");
		}
		else
			printTitle(res.data, ResourceManager::SEARCH);
		printSearchForm(res.data, state);
		if (!ctx.searchError.empty())
		{
			string tmp;
			res.data += "<div class='info-message'>";
			res.data += SimpleXML::escape(ctx.searchError, tmp, false);
			res.data += "</div>";
			ctx.searchError.clear();
		}
		res.data += htmlEnd;
		res.type = HANDLER_RESULT_HTML;
	}
	else
	{
		res.data = "/signin";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::performSearch(HandlerResult& res, const RequestInfo& state) noexcept
{
	res.type = HANDLER_RESULT_ERROR;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		ClientContext& ctx = i->second;
		if (state.query)
		{
			string term = WebServerUtil::getStringQueryParam(state.query, "s");
			int type = WebServerUtil::getIntQueryParam(state.query, "type");
			bool onlyFreeSlots = WebServerUtil::getIntQueryParam(state.query, "ofs") != 0;
			ctx.startSearch(term, type, onlyFreeSlots);
		}
		if (WebServerUtil::getIntQueryParam(state.query, "json"))
		{
			JsonFormatter f;
			f.open('{');
			if (ctx.searchError.empty())
			{
				f.appendKey("redirect");
				f.appendStringValue("/search");
			}
			else
			{
				f.appendKey("success");
				f.appendBoolValue(false);
				f.appendKey("message");
				f.appendStringValue(ctx.searchError);
				ctx.searchError.clear();
			}
			f.close('}');
			f.moveResult(res.data);
			res.type = HANDLER_RESULT_JSON;
		}
		else
		{
			res.data = "/search";
			res.type = HANDLER_RESULT_REDIRECT;
		}
	}
}

void WebServerManager::printQueue(HandlerResult& res, const RequestInfo& state) noexcept
{
	printHtmlStart(res.data, state.cookies);
	printNavigationBar(res.data, PAGE_QUEUE);
	res.data += htmlMainDiv;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		printTitle(res.data, ResourceManager::DOWNLOAD_QUEUE);
		int page, sortColumn;
		WebServerUtil::TablePageInfo pi;
		getTableParams(state, page, sortColumn);
		ClientContext& ctx = i->second;
		ctx.getQueue();
		WebServerUtil::getTableRange(page, ctx.queue.size(), getPageSize(state.cookies), pi);
		if (sortColumn) ctx.sortQueue(sortColumn);
		ctx.printQueue(res.data, pi.start, pi.count);
		if (pi.pages > 1) WebServerUtil::printPageSelector(res.data, pi.pages, page, tableInfo[PAGE_QUEUE]);
		res.data += htmlEnd;
		res.type = HANDLER_RESULT_HTML;
	}
	else
	{
		res.data = "/signin";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::removeQueueItem(HandlerResult& res, const RequestInfo& state) noexcept
{
	res.type = HANDLER_RESULT_ERROR;
	QueueItemPtr qi;
	{
		LOCK(csClients);
		auto i = clients.find(state.clientId);
		if (i != clients.end())
		{
			ClientContext& ctx = i->second;
			if (state.query)
			{
				uint64_t id = WebServerUtil::parseItemId(WebServerUtil::getStringQueryParam(state.query, "id"));
				if (id)
				{
					for (auto j = ctx.queue.begin(); j != ctx.queue.end(); ++j)
						if (j->qi.get() == (void*) id)
						{
							qi = j->qi;
							ctx.queue.erase(j);
							break;
						}
				}
			}
			res.data = "/queue";
			res.type = HANDLER_RESULT_REDIRECT;
		}
	}
	if (qi)
	{
		bool result = QueueManager::getInstance()->removeTarget(qi->getTarget());
		LOCK(csClients);
		auto i = clients.find(state.clientId);
		if (i != clients.end())
		{
			ClientContext& ctx = i->second;
			ctx.queueId = result ? QueueManager::fileQueue.getGenerationId() : 0;
		}
	}
}

void WebServerManager::printFinishedDownloads(HandlerResult& res, const RequestInfo& state) noexcept
{
	printHtmlStart(res.data, state.cookies);
	printNavigationBar(res.data, PAGE_FINISHED_DOWNLOADS);
	res.data += htmlMainDiv;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		printTitle(res.data, ResourceManager::FINISHED_DOWNLOADS);
		int page, sortColumn;
		WebServerUtil::TablePageInfo pi;
		getTableParams(state, page, sortColumn);
		ClientContext& ctx = i->second;
		ctx.getFinishedItems(FinishedManager::e_Download);
		WebServerUtil::getTableRange(page, ctx.finishedDownloads.size(), getPageSize(state.cookies), pi);
		if (sortColumn) ctx.sortFinishedItems(FinishedManager::e_Download, sortColumn);
		ctx.printFinishedItems(res.data, pi.start, pi.count, ctx.finishedDownloads, FinishedManager::e_Download);
		if (pi.pages > 1) WebServerUtil::printPageSelector(res.data, pi.pages, page, tableInfo[PAGE_FINISHED_DOWNLOADS]);
		res.data += htmlEnd;
		res.type = HANDLER_RESULT_HTML;
	}
	else
	{
		res.data = "/signin";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::printFinishedUploads(HandlerResult& res, const RequestInfo& state) noexcept
{
	printHtmlStart(res.data, state.cookies);
	printNavigationBar(res.data, PAGE_FINISHED_UPLOADS);
	res.data += htmlMainDiv;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		printTitle(res.data, ResourceManager::FINISHED_UPLOADS);
		int page, sortColumn;
		WebServerUtil::TablePageInfo pi;
		getTableParams(state, page, sortColumn);
		ClientContext& ctx = i->second;
		ctx.getFinishedItems(FinishedManager::e_Upload);
		WebServerUtil::getTableRange(page, ctx.finishedUploads.size(), getPageSize(state.cookies), pi);
		if (sortColumn) ctx.sortFinishedItems(FinishedManager::e_Upload, sortColumn);
		ctx.printFinishedItems(res.data, pi.start, pi.count, ctx.finishedUploads, FinishedManager::e_Upload);
		if (pi.pages > 1) WebServerUtil::printPageSelector(res.data, pi.pages, page, tableInfo[PAGE_FINISHED_UPLOADS]);
		res.data += htmlEnd;
		res.type = HANDLER_RESULT_HTML;
	}
	else
	{
		res.data = "/signin";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::printWaitingUsers(HandlerResult& res, const RequestInfo& state) noexcept
{
	printHtmlStart(res.data, state.cookies);
	printNavigationBar(res.data, PAGE_WAITING_USERS);
	res.data += htmlMainDiv;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		printTitle(res.data, ResourceManager::WAITING_USERS);
		int page, sortColumn;
		WebServerUtil::TablePageInfo pi;
		getTableParams(state, page, sortColumn);
		ClientContext& ctx = i->second;
		ctx.getWaitingUsers();
		WebServerUtil::getTableRange(page, ctx.waitingUsers.size(), getPageSize(state.cookies), pi);
		if (sortColumn) ctx.sortWaitingUsers(sortColumn);
		ctx.printWaitingUsers(res.data, pi.start, pi.count);
		if (pi.pages > 1) WebServerUtil::printPageSelector(res.data, pi.pages, page, tableInfo[PAGE_WAITING_USERS]);
		res.data += htmlEnd;
		res.type = HANDLER_RESULT_HTML;
	}
	else
	{
		res.data = "/signin";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::downloadFinishedItem(HandlerResult& res, const RequestInfo& state) noexcept
{
	res.type = HANDLER_RESULT_ERROR;
	if (!state.query) return;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		ClientContext& ctx = i->second;
		uint64_t id = WebServerUtil::parseItemId(WebServerUtil::getStringQueryParam(state.query, "id"));
		if (id)
		{
			int type = WebServerUtil::getIntQueryParam(state.query, "t", -1);
			if (type == FinishedManager::e_Download)
			{
				for (auto j = ctx.finishedDownloads.begin(); j != ctx.finishedDownloads.end(); ++j)
					if (j->get() == (void*) id)
					{
						res.data = (*j)->getTarget();
						break;
					}
			}
			else if (type == FinishedManager::e_Upload)
			{
				for (auto j = ctx.finishedUploads.begin(); j != ctx.finishedUploads.end(); ++j)
					if (j->get() == (void*) id)
					{
						res.data = (*j)->getTarget();
						break;
					}
			}
		}
	}
	if (!res.data.empty()) res.type = HANDLER_RESULT_FILE_PATH;
}

void WebServerManager::downloadSearchResult(HandlerResult& res, const RequestInfo& state) noexcept
{
	if (!state.query) return;
	SearchResult* sr = nullptr;
	int rowId = -1;

	{
		LOCK(csClients);
		auto i = clients.find(state.clientId);
		if (i != clients.end())
		{
			ClientContext& ctx = i->second;
			uint64_t id = WebServerUtil::parseItemId(WebServerUtil::getStringQueryParam(state.query, "id"));
			if (id)
			{
				for (size_t i = 0; i < ctx.searchResults.size(); i++)
				{
					const auto& item = ctx.searchResults[i];
					if ((uint64_t) item.get() == id)
					{
						if (item->getType() == SearchResult::TYPE_FILE)
							sr = new SearchResult(*item);
						rowId = i;
						break;
					}
				}
			}
		}
	}

	if (!sr)
	{
		res.type = HANDLER_RESULT_ERROR;
		return;
	}

	bool error = false;
	string message;
	string file = Util::getFileName(sr->getFileName());
	string dir = FavoriteManager::getInstance()->getDownloadDirectory(Util::getFileExt(file), sr->getUser());
	Util::appendPathSeparator(dir);
	try
	{
		// TODO: handle "Target already exists" error
		bool getConnFlag = true;
		QueueManager::QueueItemParams params;
		params.size = sr->getSize();
		params.root = &sr->getTTH();
		QueueManager::getInstance()->add(dir + file, params, sr->getHintedUser(), 0, 0, getConnFlag);
	}
	catch (QueueException& e)
	{
		if (e.getCode() != QueueException::DUPLICATE_SOURCE)
		{
			message = e.getError();
			error = true;
		}
		else
			message = STRING(WEBSERVER_MAGNET_ALREADY_IN_QUEUE);
	}
	catch (Exception& e)
	{
		message = e.getError();
		error = true;
	}
	delete sr;

	if (WebServerUtil::getIntQueryParam(state.query, "json"))
	{
		JsonFormatter f;
		f.open('{');
		f.appendKey("success");
		f.appendBoolValue(!error);
		f.appendKey("message");
		if (message.empty())
			f.appendStringValue(STRING(WEBSERVER_FILE_QUEUED));
		else
			f.appendStringValue(message);
		f.appendKey("rowId");
		f.appendStringValue("row-" + Util::toString(rowId), false);
		f.close('}');
		f.moveResult(res.data);
		res.type = HANDLER_RESULT_JSON;
	}
	else
	{
		res.data = "/queue";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::clearSearchResults(HandlerResult& res, const RequestInfo& state) noexcept
{
	{
		LOCK(csClients);
		auto i = clients.find(state.clientId);
		if (i != clients.end())
			i->second.clearSearchResults();
	}
	res.data = "/search";
	res.type = HANDLER_RESULT_REDIRECT;
}

void WebServerManager::grantSlot(HandlerResult& res, const RequestInfo& state) noexcept
{
	res.type = HANDLER_RESULT_ERROR;
	if (!state.query) return;
	int rowId = -1;
	HintedUser hintedUser;
	uint64_t id = WebServerUtil::parseItemId(WebServerUtil::getStringQueryParam(state.query, "id"));
	if (id)
	{
		LOCK(csClients);
		auto i = clients.find(state.clientId);
		if (i != clients.end())
		{
			ClientContext& ctx = i->second;
			for (size_t j = 0; j < ctx.waitingUsers.size(); ++j)
				if (ctx.waitingUsers[j].hintedUser.user.get() == (void*) id)
				{
					hintedUser = ctx.waitingUsers[j].hintedUser;
					rowId = j;
					break;
				}
		}
	}
	if (!hintedUser.user) return;
	UploadManager::getInstance()->reserveSlot(hintedUser, 600);
	if (WebServerUtil::getIntQueryParam(state.query, "json"))
	{
		JsonFormatter f;
		f.open('{');
		f.appendKey("success");
		f.appendBoolValue(true);
		f.appendKey("message");
		f.appendStringValue(STRING(SLOT_GRANTED));
		f.appendKey("rowId");
		f.appendStringValue("row-" + Util::toString(rowId), false);
		f.close('}');
		f.moveResult(res.data);
		res.type = HANDLER_RESULT_JSON;
	}
	else
	{
		res.data = "/waiting";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::printTools(HandlerResult& res, const RequestInfo& state) noexcept
{
	printHtmlStart(res.data, state.cookies);
	printNavigationBar(res.data, PAGE_TOOLS);
	res.data += htmlMainDiv;
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		string tmp;
		printTitle(res.data, ResourceManager::WEBSERVER_ADD_MAGNET_LINK);
		res.data += "<form method='POST' action='/xmagnet' id='add-magnet-form' class='bottom-margin'>"
			"<div><input id='magnet-string' class='field' name='magnet' placeholder='";
		res.data += SimpleXML::escape(STRING(WEBSERVER_ADD_MAGNET_DESC), tmp, false);
		res.data += "'/><div class='button-container' id='add-magnet-button'><input type='submit' class='misc-button' value='";
		res.data += SimpleXML::escape(STRING(DOWNLOAD), tmp, false);
		res.data += "' onclick='addMagnet()'/></div></div></form>";

		printTitle(res.data, ResourceManager::SHARED_FILES);
		auto sm = ShareManager::getInstance();
		if (sm->isRefreshing())
		{
			res.data += SimpleXML::escape(STRING(REFRESHING_SHARE), tmp, false);
		}
		else
		{
			res.data += "<form method='POST' action='/xrefresh' id='refresh-share-form' class='bottom-margin'>"
				"<div class='text-container'>";
			res.data += SimpleXML::escape(STRING(TOTAL_SIZE), tmp, false);
			int64_t size = sm->getTotalSharedSize();
			res.data += Util::formatBytes(size);
			if (size)
			{
				res.data += " (";
				res.data += SimpleXML::escape(PLURAL_F(PLURAL_FILES, sm->getTotalSharedFiles()), tmp, false);
				res.data += ')';
			}
			uint64_t lastRefresh = sm->getLastRefreshTime();
			if (lastRefresh)
			{
				res.data += "<br>";
				string str = STRING_F(WEBSERVER_LAST_REFRESH, Util::formatDateTime(lastRefresh));
				res.data += SimpleXML::escape(str, tmp, false);
			}
			res.data += "</div><div class='button-container' id='refresh-share-button'><input type='submit' class='misc-button' value='";
			res.data += SimpleXML::escape(STRING(WEBSERVER_REFRESH), tmp, false);
			res.data += "' onclick='refreshShare()'/></div></form>";
		}
		res.data += htmlEnd;
		res.type = HANDLER_RESULT_HTML;
	}
	else
	{
		res.data = "/signin";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::addMagnet(HandlerResult& res, const RequestInfo& state) noexcept
{
	res.type = HANDLER_RESULT_ERROR;
	if (!state.query) return;
	string magnet = WebServerUtil::getStringQueryParam(state.query, "magnet");
	boost::trim(magnet);
	bool result = false;
	string messageText;
	if (!magnet.empty())
	{
		MagnetLink ml;
		if (ml.parse(magnet))
		{
			const char* tthStr = ml.getTTH();
			const string& fname = ml.getFileName();
			if (tthStr && !fname.empty() && ml.exactLength > 0)
			{
				TTHValue tth(tthStr);
				if (QueueManager::fileQueue.isQueued(tth))
				{
					result = true;
					messageText = STRING(WEBSERVER_MAGNET_ALREADY_IN_QUEUE);
				} else
				{
					const bool isDclst = Util::isDclstFile(fname);
					try
					{
						bool getConnFlag = true;
						QueueItem::MaskType flags = 0;
						QueueItem::MaskType extraFlags = 0;
						if (isDclst)
						{
							flags |= QueueItem::FLAG_DCLST_LIST;
							extraFlags |= QueueItem::XFLAG_DOWNLOAD_CONTENTS;
						}
						QueueManager::QueueItemParams params;
						params.size = ml.exactLength;
						params.root = &tth;
						QueueManager::getInstance()->add(fname, params, HintedUser(), flags, extraFlags, getConnFlag);
						result = true;
						messageText = STRING(WEBSERVER_MAGNET_ADDED);
					}
					catch (const Exception&)
					{
						messageText = STRING(WEBSERVER_ADD_MAGNET_ERROR);
					}
				}
			}
		}
		if (!result && messageText.empty())
			messageText = STRING(WEBSERVER_BAD_MAGNET);
	}
	if (WebServerUtil::getIntQueryParam(state.query, "json"))
	{
		JsonFormatter f;
		f.open('{');
		f.appendKey("success");
		f.appendBoolValue(result);
		if (!messageText.empty())
		{
			f.appendKey("message");
			f.appendStringValue(messageText);
		}
		f.close('}');
		f.moveResult(res.data);
		res.type = HANDLER_RESULT_JSON;
	}
	else
	{
		res.data = "/tools";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::refreshShare(HandlerResult& res, const RequestInfo& state) noexcept
{
	bool result = ShareManager::getInstance()->refreshShare();
	if (WebServerUtil::getIntQueryParam(state.query, "json"))
	{
		JsonFormatter f;
		f.open('{');
		f.appendKey("success");
		f.appendBoolValue(result);
		if (result)
		{
			f.appendKey("redirect");
			f.appendStringValue("/tools");
		}
		else
		{
			f.appendKey("message");
			f.appendStringValue(STRING(WEBSERVER_SHARE_REFRESHING));
		}
		f.close('}');
		f.moveResult(res.data);
		res.type = HANDLER_RESULT_JSON;
	}
	else
	{
		res.data = "/tools";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::printSettings(HandlerResult& res, const RequestInfo& state) noexcept
{
	printHtmlStart(res.data, state.cookies);
	printNavigationBar(res.data, PAGE_SETTINGS);
	res.data += htmlMainDiv;
	int colorTheme = getColorTheme(state.cookies);
	int pageSize = getPageSize(state.cookies);
	LOCK(csClients);
	auto i = clients.find(state.clientId);
	if (i != clients.end())
	{
		static const ResourceManager::Strings themeNames[] =
		{
			ResourceManager::WEBSERVER_UI_THEME_LIGHT, ResourceManager::WEBSERVER_UI_THEME_DARK
		};
		string tmp;
		printTitle(res.data, ResourceManager::SETTINGS);
		res.data += "<form method='POST' action='/xsettings' class='bottom-margin'><div><div class='field-caption'>";
		res.data += SimpleXML::escape(STRING(WEBSERVER_UI_THEME), tmp, false);
		res.data += "</div>";
		WebServerUtil::printSelector(res.data, "theme", _countof(themeNames), themeNames, 0, colorTheme);
		res.data += "</div><div class='field-caption'>";
		res.data += SimpleXML::escape(STRING(WEBSERVER_PAGE_SIZE), tmp, false);
		res.data += "</div><select class='field' name='page'>";
		for (int j = 0; j < _countof(pageSizes); ++j)
		{
			string s = Util::toString(pageSizes[j]);
			res.data += "<option value='" + s + "'";
			if (pageSizes[j] == pageSize) res.data += " selected='selected'";
			res.data += '>';
			res.data += s;
			res.data += "</option>";
		}
		res.data += "</select>";
		res.data += "<div><input type='submit' class='button' value='";
		res.data += SimpleXML::escape(STRING(WEBSERVER_APPLY), tmp, false);
		res.data += "'/></div></form>";
		res.data += htmlEnd;
		res.type = HANDLER_RESULT_HTML;
	}
	else
	{
		res.data = "/signin";
		res.type = HANDLER_RESULT_REDIRECT;
	}
}

void WebServerManager::applySettings(HandlerResult& res, const RequestInfo& state) noexcept
{
	res.type = HANDLER_RESULT_ERROR;
	if (!state.query || !state.cookies) return;

	uint64_t expires = GET_TIME() + 365 * 86400;
	int theme = WebServerUtil::getIntQueryParam(state.query, "theme", -1);
	if (theme != -1)
		state.cookies->set("theme", Util::toString(theme), expires, Util::emptyString, 0);
	int pageSize = WebServerUtil::getIntQueryParam(state.query, "page");
	if (pageSize)
	{
		pageSize = checkPageSize(pageSize);
		state.cookies->set("pagesize", Util::toString(pageSize), expires, Util::emptyString, 0);
	}
	res.data = "/settings";
	res.type = HANDLER_RESULT_REDIRECT;
}

void WebServerManager::loadColorTheme(int index) noexcept
{
	dcassert(index == 0 || index == 1);
	string name = index == 0 ? "light.scss" : "dark.scss";
	CacheItem tpl;
	if (loadTemplate(tpl, "themes", name))
	{
		bool clearCache = false;
		csThemeAttr.lock();
		if (tpl.timestamp > themeAttr[index].timestamp)
		{
			themeAttr[index].timestamp = tpl.timestamp;
			WebServerUtil::loadCssVariables(tpl.data, themeAttr[index].vars);
			clearCache = true;
		}
		csThemeAttr.unlock();
		if (clearCache) removeCacheItem("default@" + Util::toString(index) + ".css");
	}
}

uint32_t WebServerManager::checkUser(const string& user, const string& password) const noexcept
{
	if (user == SETTING(WEBSERVER_USER))
		return password == SETTING(WEBSERVER_PASS) ? ROLE_USER : 0;
	if (user == SETTING(WEBSERVER_POWER_USER))
		return password == SETTING(WEBSERVER_POWER_PASS) ? ROLE_POWER_USER : 0;
	return 0;
}

uint64_t WebServerManager::createClientContext(uint32_t userId, uint64_t expires, const unsigned char iv[]) noexcept
{
	LOCK(csClients);
	uint64_t id = ++nextClientId;
	auto& context = clients[id];
	context.id = id;
	context.expires = expires;
	context.userId = userId;
	memcpy(context.iv, iv, AC_IV_SIZE);
	return id;
}

void WebServerManager::removeClientContext(uint64_t id) noexcept
{
	LOCK(csClients);
	auto i = clients.find(id);
	if (i != clients.end()) clients.erase(i);
}

void WebServerManager::removeExpired() noexcept
{
	uint64_t t = GET_TIME();
	LOCK(csClients);
	for (auto i = clients.begin(); i != clients.end();)
		if (t > i->second.expires)
		{
			if (BOOLSETTING(LOG_WEBSERVER))
				LogManager::log(LogManager::WEBSERVER, printClientId(i->first, nullptr) + ": Session expired");
			i = clients.erase(i);
		}
		else
			++i;
}

string WebServerManager::printClientId(uint64_t id, const HttpServerConnection* conn) noexcept
{
	string s = "Client-";
	s += Util::toString(id);
	if (conn)
	{
		IpAddress ip;
		conn->getIp(ip);
		string ipStr = Util::printIpAddress(ip);
		if (!ipStr.empty())
		{
			s += " | ";
			s += ipStr;
		}
	}
	return s;
}

void WebServerManager::on(SearchManagerListener::SR, const SearchResult& sr) noexcept
{
	if (sr.getToken())
	{
		uint64_t id = SearchTokenList::instance.getTokenOwner(sr.getToken());
		if ((id & 0xFF) != FRAME_TYPE_WEB_CLIENT) return;
		id >>= 8;
		LOCK(csClients);
		auto i = clients.find(id);
		if (i != clients.end()) i->second.addSearchResult(sr);
	}
	else
	{
		uint64_t now = GET_TICK();
		LOCK(csClients);
		for (auto& i : clients)
		{
			ClientContext& ctx = i.second;
			if (ctx.searchParam.token && !ctx.searchParam.filter.empty() && now < ctx.searchEndTime)
				ctx.addSearchResult(sr);
		}
	}
}
