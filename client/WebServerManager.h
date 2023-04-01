#ifndef WEB_SERVER_MANAGER_H_
#define WEB_SERVER_MANAGER_H_

#include "Singleton.h"
#include "Speaker.h"
#include "HttpServerConnection.h"
#include "HttpCookies.h"
#include "WebServerAuth.h"
#include "SearchManagerListener.h"
#include "SearchParam.h"
#include "Locks.h"
#include "RWLock.h"
#include "Thread.h"

class WebServerListener
{
	public:
		template<int I> struct X
		{
			enum { TYPE = I };
		};

		typedef X<0> ShutdownPC;

		virtual void on(ShutdownPC, int action) noexcept = 0;
};

struct WaitingUsersItem
{
	HintedUser hintedUser;
	uint64_t added;
	size_t fileCount;
	size_t position;
};

class WebServerManager :
	public Singleton<WebServerManager>,
	public Speaker<WebServerListener>,
	private HttpServerCallback,
	private SearchManagerListener,
	private WebServerAuth
{
	friend class Singleton<WebServerManager>;

public:
	enum
	{
		ROLE_USER = 1,
		ROLE_POWER_USER = 2
	};

	static const int FRAME_TYPE_WEB_CLIENT = 16;

	void start();
	void shutdown() noexcept;
	void removeExpired() noexcept;

private:
	struct ClientContext
	{
	public:
		uint64_t id;
		uint64_t expires;
		uint32_t userId;
		unsigned char iv[AC_IV_SIZE];

		static bool updateSortColumn(int& savedColumn, int& column, int maxColumn, bool& reverse);

		// Search
		static const size_t MAX_SEARCH_RESULTS = 500;
		vector<std::unique_ptr<SearchResult>> searchResults;
		int searchResultsSort = 0;
		SearchParam searchParam;
		uint64_t searchStartTime = 0;
		uint64_t searchEndTime = 0;
		bool searchOnlyFreeSlots = false;
		string searchError;

		void startSearch(const string& term, int type, bool onlyFreeSlots) noexcept;
		void clearSearchResults() noexcept;
		void addSearchResult(const SearchResult& sr) noexcept;
		void printSearchTitle(string& os, bool isRunning) noexcept;
		void printSearchResults(string& os, size_t from, size_t count, bool isRunning) noexcept;
		void sortSearchResults(int sortColumn) noexcept;

		// Queue
		vector<QueueItemPtr> queue;
		int queueSort = 0;
		uint64_t queueId = 0;

		void getQueue() noexcept;
		void printQueue(string& os, size_t from, size_t count) noexcept;
		void sortQueue(int sortColumn) noexcept;

		// Recent transfers
		vector<FinishedItemPtr> finishedUploads;
		vector<FinishedItemPtr> finishedDownloads;
		int finishedItemsSort[2] = { 0, 0 };
		uint64_t finishedItemsId[2] = { 0, 0 };

		void printFinishedItems(string& os, size_t from, size_t count, const vector<FinishedItemPtr>& data, int type) noexcept;
		void sortFinishedItems(int type, int sortColumn);
		void getFinishedItems(int type);

		// Waiting users
		vector<WaitingUsersItem> waitingUsers;
		int waitingUsersSort = 0;
		uint64_t waitingUsersId = 0;

		void getWaitingUsers() noexcept;
		void printWaitingUsers(string& os, size_t from, size_t count) noexcept;
		void sortWaitingUsers(int sortColumn) noexcept;
	};

	class Server : public Thread
	{
	public:
		Server(bool tls, const IpAddressEx& ip, uint16_t port);
		uint16_t getServerPort() const
		{
			dcassert(serverPort);
			return serverPort;
		}
#if 0
		IpAddress getServerIP() const
		{
			return sock.getLocalIp();
		}
#endif
		~Server()
		{
			stopFlag.store(true);
			join();
		}
		int isTLS() const { return tls; }

	private:
		int run() noexcept;

		std::atomic_bool stopFlag;
		Socket sock;
		uint16_t serverPort;
		const bool tls;
		IpAddressEx bindIp;
	};

	using Query = std::map<string, string>;

	struct RequestInfo
	{
		HttpServerConnection* conn;
		const Http::Request* req;
		Http::ServerCookies* cookies;
		const Query* query;
		uint64_t clientId;
	};

	struct HandlerResult
	{
		int type;
		string data;
	};

	enum
	{
		HANDLER_RESULT_ERROR,
		HANDLER_RESULT_REDIRECT,
		HANDLER_RESULT_HTML,
		HANDLER_RESULT_JSON,
		HANDLER_RESULT_FILE_PATH
	};

	using ContentFunc = void (WebServerManager::*)(HandlerResult& res, const RequestInfo& state);

	struct CacheItem
	{
		uint64_t timestamp;
		string data;
	};

	struct UrlInfo
	{
		int type;
		ContentFunc cf;
	};

	enum
	{
		HANDLER_TYPE_PAGE = 1,
		HANDLER_TYPE_ACTION,
		HANDLER_TYPE_FILE
	};

	enum
	{
		ST_EXPAND_LANG_STRINGS = 1,
		ST_EXPAND_CSS          = 2,
		ST_USE_CACHE           = 4,
		ST_VOLATILE            = 8
	};

	enum
	{
		CACHE_ITEM_NOT_FOUND,
		CACHE_ITEM_FOUND,
		CACHE_ITEM_NOT_MODIFIED
	};

	struct ThemeAttributes
	{
		StringMap vars;
		uint64_t timestamp;
	};

	WebServerManager() noexcept;
	static string printClientId(uint64_t id, const HttpServerConnection* conn) noexcept;
	void stopServer(int af) noexcept;
	bool startListen(int af, bool tls);
	void accept(const Socket& sock, bool tls, Server* server) noexcept;
	void sendErrorResponse(HttpServerConnection* conn, int resp, const char* text = nullptr) noexcept;
	void sendRedirect(const RequestInfo& inf, const string& location) noexcept;
	void sendFile(const RequestInfo& inf, const string& path, bool sendContentDisposition, uint64_t timestamp) noexcept;
	void sendTemplate(const RequestInfo& inf, const string& dir, const string& name, const string& requestName, const string& mimeType, int flags) noexcept;
	void sendLoginPage(const RequestInfo& inf) noexcept;
	void handleRequest(const RequestInfo& inf, const UrlInfo& ui) noexcept;
	uint32_t checkUser(const string& user, const string& password) const noexcept;
	uint64_t createClientContext(uint32_t userId, uint64_t expires, const unsigned char iv[]) noexcept;
	void removeClientContext(uint64_t id) noexcept;
	void updateAuthCookie(Http::ServerCookies& cookies, uint64_t clientId, uint64_t curTime) noexcept;
	void loadColorTheme(int index) noexcept;

	bool loadTemplate(CacheItem& result, const string& dir, const string& name) noexcept;
	int getCacheItem(CacheItem& result, const string& name, uint64_t ifModified) noexcept;
	void setCacheItem(const string& name, const string& data, uint64_t timestamp) noexcept;
	void removeCacheItem(const string& name) noexcept;

	static void printHtmlStart(string& os, const Http::ServerCookies* cookies) noexcept;
	static void printNavigationBar(string& os, int selectedPage) noexcept;
	static void printTitle(string& os, int text, const char* cls = nullptr) noexcept;
	static void printLoginPage(string& os, const Http::ServerCookies* cookies) noexcept;
	static void getTableParams(const RequestInfo& state, int& page, int& sortColumn) noexcept;

	void printSearchForm(string& os, const RequestInfo& state) noexcept;
	void printSearch(HandlerResult& res, const RequestInfo& state) noexcept;
	void printQueue(HandlerResult& res, const RequestInfo& state) noexcept;
	void printFinishedUploads(HandlerResult& res, const RequestInfo& state) noexcept;
	void printFinishedDownloads(HandlerResult& res, const RequestInfo& state) noexcept;
	void printWaitingUsers(HandlerResult& res, const RequestInfo& state) noexcept;
	void printTools(HandlerResult& res, const RequestInfo& state) noexcept;
	void printSettings(HandlerResult& res, const RequestInfo& state) noexcept;
	void performSearch(HandlerResult& res, const RequestInfo& state) noexcept;
	void downloadSearchResult(HandlerResult& res, const RequestInfo& state) noexcept;
	void clearSearchResults(HandlerResult& res, const RequestInfo& state) noexcept;
	void removeQueueItem(HandlerResult& res, const RequestInfo& state) noexcept;
	void downloadFinishedItem(HandlerResult& res, const RequestInfo& state) noexcept;
	void grantSlot(HandlerResult& res, const RequestInfo& state) noexcept;
	void addMagnet(HandlerResult& res, const RequestInfo& state) noexcept;
	void refreshShare(HandlerResult& res, const RequestInfo& state) noexcept;
	void applySettings(HandlerResult& res, const RequestInfo& state) noexcept;

	void onRequest(HttpServerConnection* conn, const Http::Request& req) noexcept override;
	void onData(HttpServerConnection* conn, const uint8_t* data, size_t size) noexcept override {}
	//void onDisconnected(HttpServerConnection* conn) noexcept override;
	//void onError(HttpServerConnection* conn, const string& error) noexcept override;

	void on(SearchManagerListener::SR, const SearchResult&) noexcept override;

private:
	static const int SERVER_SECURE = 1;
	static const int SERVER_V6     = 2;

	CriticalSection cs;
	boost::unordered_map<uint64_t, HttpServerConnection*> connections;
	uint64_t nextConnId = 0;

	CriticalSection csClients;
	boost::unordered_map<uint64_t, ClientContext> clients;
	uint64_t nextClientId = 0;

	boost::unordered_map<string, UrlInfo> urlInfo;

	Server* servers[4] = {};

	boost::unordered_map<string, CacheItem> templateCache;
	std::unique_ptr<RWLock> csTemplateCache;

	ThemeAttributes themeAttr[2];
	CriticalSection csThemeAttr;
};

#endif // WEB_SERVER_MANAGER_H_
