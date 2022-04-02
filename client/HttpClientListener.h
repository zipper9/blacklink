#ifndef HTTP_CLIENT_LISTENER_H_
#define HTTP_CLIENT_LISTENER_H_

#include "HttpMessage.h"

class HttpClientListener
{
public:
	virtual ~HttpClientListener () {}
	template<int I>	struct X { enum { TYPE = I }; };

	typedef X<0> Completed;
	typedef X<1> Failed;
	typedef X<2> Redirected;

	struct Result
	{
		string url;
		string outputPath;
		string responseBody;
	};

	virtual void on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept = 0;
	virtual void on(Failed, uint64_t id, const string& error) noexcept = 0;
	virtual void on(Redirected, uint64_t id, const string& redirUrl) noexcept = 0;
};

#endif  // HTTP_CLIENT_LISTENER_H_
