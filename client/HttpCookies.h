#ifndef HTTP_COOKIES_H_
#define HTTP_COOKIES_H_

#include "HttpMessage.h"
#include <boost/unordered/unordered_map.hpp>

namespace Http
{

class ServerCookies
{
	public:
		enum
		{
			FLAG_SECURE = 1,
			FLAG_HTTP_ONLY = 2
		};

		const string& get(const string& name) const noexcept;
		void set(const string& name, const string& value, uint64_t expires, const string& path, int flags) noexcept;
		void remove(const string& name, bool del) noexcept;
		void clear() noexcept { data.clear(); }
		void parse(const Http::HeaderList& msg) noexcept;
		void print(Http::HeaderList& msg) const noexcept;

	private:
		struct Item
		{
			string value;
			string path;
			uint64_t expires;
			int flags;
		};
		boost::unordered_map<string, Item> data;

		void parseLine(const string& s) noexcept;
};

}

#endif // HTTP_COOKIES_H_
