#ifndef HTTP_CLIENT_H_
#define HTTP_CLIENT_H_

#include "HttpClientListener.h"
#include "HttpConnection.h"
#include "Speaker.h"
#include "File.h"
#include <atomic>
#include <limits>

class HttpClient : public Speaker<HttpClientListener>, private HttpClientCallback
{
public:
	HttpClient();
	~HttpClient() { shutdown(); }
	HttpClient(const HttpClient&) = delete;
	HttpClient& operator= (const HttpClient&) = delete;

	struct Request
	{
		int type = Http::METHOD_GET;
		string url;
		string requestBody;
		string requestBodyType;
		string outputPath;
		string userAgent;
		bool closeConn = false;
		bool noCache = false;
		int ipVersion = 0;
		int maxRedirects = 0;
		time_t ifModified = 0;
		int64_t maxRespBodySize = std::numeric_limits<int64_t>::max();
		int64_t maxErrorBodySize = 128 * 1024;
	};

	uint64_t addRequest(const Request& req);
	bool startRequest(uint64_t id);
	void cancelRequest(uint64_t id);
	void removeUnusedConnections() noexcept;
	void shutdown() noexcept;

private:
	enum
	{
		STATE_IDLE,
		STATE_STARTING,
		STATE_BUSY,
		STATE_FAILED
	};

	struct ConnectionData
	{
		int state;
		string server;
		uint64_t requestId;
		bool closeConn;
		uint64_t lastActivity;
		HttpConnection* conn;
	};

	struct RequestState : public Request
	{
		File outputFile;
		string responseBody;
		uint64_t connId;
		int redirCount;
	};

	typedef std::shared_ptr<RequestState> RequestStatePtr;

	mutable CriticalSection cs;
	boost::unordered_map<uint64_t, ConnectionData> conn;
	boost::unordered_map<uint64_t, RequestStatePtr> requests;
	std::atomic<uint64_t> nextReqId;
	std::atomic<uint64_t> nextConnId;

	// HttpClientCallback
	void onData(HttpConnection*, const uint8_t*, size_t) noexcept override;
	void onFailed(HttpConnection*, const string&) noexcept override;
	void onCompleted(HttpConnection*, const string&) noexcept override;
	void onDisconnected(HttpConnection*) noexcept override;

	void startRequest(HttpConnection* c, uint64_t id, const RequestStatePtr& rs);
	HttpConnection* findConnectionL(uint64_t now, RequestStatePtr& rs, const string& server, uint64_t reqId, bool closeConn, int state);
	void removeRequest(uint64_t id) noexcept;
	void processError(uint64_t connId, const string& error) noexcept;
	static bool decodeUrl(const string& url, string& server);
	static string getFileName(const string& path);
	static bool isTimedOut(const ConnectionData& cd, uint64_t now) noexcept;
};

extern HttpClient httpClient;

#endif // HTTP_CLIENT_H_
