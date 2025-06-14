#include "stdinc.h"
#include "HttpCookies.h"
#include "HttpHeaders.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include <boost/algorithm/string.hpp>

const string& Http::ServerCookies::get(const string& name) const noexcept
{
	auto i = data.find(name);
	return i == data.end() ? Util::emptyString : i->second.value;
}

void Http::ServerCookies::set(const string& name, const string& value, uint64_t expires, const string& path, int flags) noexcept
{
	auto& item = data[name];
	item.value = value;
	item.path = path;
	item.expires = expires;
	item.flags = flags;
}

void Http::ServerCookies::remove(const string& name, bool del) noexcept
{
	auto i = data.find(name);
	if (i == data.end()) return;
	if (del)
		data.erase(i);
	else
	{
		i->second.expires = 1;
		i->second.value.clear();
	}
}

void Http::ServerCookies::parse(const Http::HeaderList& msg) noexcept
{
	size_t start = 0;
	while (true)
	{
		int index = msg.findHeader(Http::HEADER_COOKIE, start);
		if (index == -1) break;
		parseLine(msg.at(index));
		start = index + 1;
	}
}

void Http::ServerCookies::parseLine(const string& s) noexcept
{
	string::size_type start = 0;
	while (start < s.length())
	{
		string::size_type end = s.find(';', start);
		if (end == string::npos) end = s.length();
		string::size_type delim = s.find('=', start);
		if (delim != string::npos && delim < end)
		{
			string name = s.substr(start, delim - start);
			boost::algorithm::trim(name);
			if (!name.empty())
			{
				string value = s.substr(delim + 1, end - (delim + 1));
				boost::algorithm::trim(value);
				auto& item = data[name];
				item.value = std::move(value);
				item.expires = 0;
				item.flags = 0;
			}
		}
		start = end + 1;
	}
}

void Http::ServerCookies::print(Http::HeaderList& msg) const noexcept
{
	for (auto i : data)
	{
		const auto& item = i.second;
		string s = i.first;
		s += '=';
		s += item.value;
		if (item.expires) s += "; Expires=" + Http::printDateTime((time_t) item.expires);
		if (!item.path.empty()) s += "; Path=" + item.path;
		if (item.flags & FLAG_SECURE) s += "; Secure";
		if (item.flags & FLAG_HTTP_ONLY) s += "; HttpOnly";
		msg.addHeader(Http::HEADER_SET_COOKIE, s);
	}
}
