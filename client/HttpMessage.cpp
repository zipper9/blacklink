#include "stdinc.h"
#include "HttpMessage.h"
#include "HttpHeaders.h"
#include "Text.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include <boost/algorithm/string/trim.hpp>

struct ResponseCode
{
	int code;
	string text;
};

static const ResponseCode stdResp[] =
{
	{ 100, "Continue" },
	{ 101, "Switching Protocols" },
	{ 102, "Processing" },
	{ 103, "Early Hints" },
	{ 200, "OK" },
	{ 201, "Created" },
	{ 202, "Accepted" },
	{ 203, "Non-Authoritative Information" },
	{ 204, "No Content" },
	{ 205, "Reset Content" },
	{ 206, "Partial Content" },
	{ 207, "Multi-Status" },
	{ 208, "Already Reported" },
	{ 226, "IM Used" },
	{ 300, "Multiple Choices" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 304, "Not Modified" },
	{ 305, "Use Proxy" },
	{ 307, "Temporary Redirect" },
	{ 308, "Permanent Redirect" },
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Timeout" },
	{ 409, "Conflict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Content Too Large" },
	{ 414, "URI Too Long" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Range Not Satisfiable" },
	{ 417, "Expectation Failed" },
	{ 421, "Misdirected Request" },
	{ 422, "Unprocessable Content" },
	{ 423, "Locked" },
	{ 424, "Failed Dependency" },
	{ 425, "Too Early" },
	{ 426, "Upgrade Required" },
	{ 428, "Precondition Required" },
	{ 429, "Too Many Requests" },
	{ 431, "Request Header Fields Too Large" },
	{ 451, "Unavailable For Legal Reasons" },
	{ 500, "Internal Server Error" },
	{ 501, "Not Implemented" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Unavailable" },
	{ 504, "Gateway Timeout" },
	{ 505, "HTTP Version Not Supported" },
	{ 506, "Variant Also Negotiates" },
	{ 507, "Insufficient Storage" },
	{ 508, "Loop Detected" },
	{ 510, "Not Extended" },
	{ 511, "Network Authentication Required" }
};

static const string& getStdResp(int code) noexcept
{
	const ResponseCode* end = stdResp + sizeof(stdResp)/sizeof(stdResp[0]);
	auto i = std::lower_bound(stdResp, end, code, [](const ResponseCode& r, int code) { return r.code < code; });
	if (i == end || i->code != code)
		return Util::emptyString;
	return i->text;
}

static const string methodNames[] = { "CONNECT", "DELETE", "GET", "HEAD", "OPTIONS", "POST", "PUT", "TRACE" };

static int getMethodId(const string& s) noexcept
{
	for (int i = 0; i < Http::METHODS; i++)
		if (Text::asciiEqual(s, methodNames[i]))
			return i;
	return -1;
}

static inline bool isWhiteSpace(char c)
{
	return c == ' ' || c == '\t';
}

static inline bool isWhiteSpace2(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static inline bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

static bool parseVersion(uint8_t version[], const string& s, size_t start, size_t end) noexcept
{
	if (start + 8 > end) return false;
	if (s.compare(start, 5, "HTTP/")) return false;
	char major = s[start+5];
	char minor = s[start+7];
	if (!isDigit(major) || !isDigit(minor)) return false;
	version[0] = major-'0';
	version[1] = minor-'0';
	return true;
}

bool Http::HeaderList::parseLine(const string& s) noexcept
{
	if (!s.empty() && isWhiteSpace(s[0]))
	{
		if (items.empty())
		{
			error = true;
			return false;
		}
		size_t startPos = 0;
		while (startPos < s.length() && isWhiteSpace(s[startPos])) startPos++;
		auto endPos = s.length();
		while (endPos > startPos && isWhiteSpace2(s[endPos-1])) endPos--;
		string& value = items.back().value;
		value += ' ';
		value += s.substr(startPos, endPos-startPos);
		return true;
	}
	auto pos = s.find(':');
	if (pos == string::npos || pos == 0 || isWhiteSpace(s[pos-1]))
	{
		error = true;
		return false;
	}
	auto endPos = s.length();
	while (endPos > pos && isWhiteSpace2(s[endPos-1])) endPos--;
	auto startPos = pos + 1;
	while (startPos < endPos && isWhiteSpace(s[startPos])) startPos++;
	int id = Http::getHeaderId(s.c_str(), pos);
	items.emplace_back(Item{ id, id == -1 ? s.substr(0, pos) : string(), s.substr(startPos, endPos-startPos)});
	return true;
}

void Http::HeaderList::print(string& s) const noexcept
{
	for (const auto& item : items)
	{
		if (item.id < 0)
			s.append(item.name);
		else
			s.append(Http::getHeader(item.id));
		s.append(": ", 2);
		s.append(item.value);
		s.append("\r\n", 2);
	}
}

void Http::HeaderList::clear() noexcept
{
	error = false;
	items.clear();
}

int Http::HeaderList::findHeader(int id) const noexcept
{
	for (size_t i = 0; i < items.size(); ++i)
		if (items[i].id == id)
			return i;
	return -1;
}

bool Http::HeaderList::findSingleHeader(int id, int& index) const noexcept
{
	index = -1;
	for (size_t i = 0; i < items.size(); ++i)
		if (items[i].id == id)
		{
			if (index != -1) return false;
			index = i;
		}
	return index != -1;
}

int Http::HeaderList::findHeader(const string& name) const noexcept
{
	int id = Http::getHeaderId(name);
	if (id != -1)
		return findHeader(id);
	for (size_t i = 0; i < items.size(); ++i)
		if (Text::asciiEqual(name, items[i].name))
			return i;
	return -1;
}

const string& Http::HeaderList::getHeaderValue(int id) const noexcept
{
	int index = findHeader(id);
	return index == -1 ? Util::emptyString : items[index].value;
}

void Http::HeaderList::addHeader(int id, const string& value) noexcept
{
	items.emplace_back(Item{ id, string(), value });
}

void Http::HeaderList::addHeader(const string& name, const string& value) noexcept
{
	items.emplace_back(Item{ -1, name, value });
}

int64_t Http::HeaderList::parseContentLength() noexcept
{
	int index;
	if (!findSingleHeader(Http::HEADER_CONTENT_LENGTH, index))
	{
		if (index != -1) error = true;
		return -1;
	}
	const string& value = items[index].value;
	if (value.length() > 18)
	{
		error = true;
		return -1;
	}
	size_t pos = 0;
	int64_t result = Util::stringToInt<int64_t, char>(value.c_str(), pos);
	if (pos != value.length())
	{
		error = true;
		return -1;
	}
	return result;
}

void Http::HeaderList::parseContentType(string& mediaType, string& params) const noexcept
{
	const string& val = getHeaderValue(Http::HEADER_CONTENT_TYPE);
	if (val.empty())
	{
		mediaType.clear();
		params.clear();
		return;
	}
	auto pos = val.find(';');
	if (pos != string::npos)
	{
		mediaType = val.substr(0, pos);
		params = val.substr(pos + 1);
		boost::algorithm::trim(mediaType);
		boost::algorithm::trim(params);
	}
	else
	{
		mediaType = val;
		params.clear();
	}
}

void Http::Request::clear() noexcept
{
	HeaderList::clear();
	version[0] = version[1] = 1;
	parseRequestLine = true;
	complete = false;
	uri.clear();
	method.clear();
	method = -1;
}

bool Http::Request::parseLine(const string& s) noexcept
{
	auto len = s.length();
	while (len && (s[len-1] == '\r' || s[len-1] == '\n')) len--;
	if (!len)
	{
		if (parseRequestLine) return true;
		complete = true;
		return false;
	}
	if (parseRequestLine)
	{
		parseRequestLine = false;
		auto p1 = s.find(' ');
		if (p1 == string::npos || p1 == 0) { error = true; return false; }
		auto p2 = s.find(' ', p1 + 1);
		if (p2 == string::npos) { error = true; return false; }
		if (!parseVersion(version, s, p2 + 1, len)) { error = true; return false; }
		method = s.substr(0, p1);
		uri = s.substr(p1 + 1, p2 - (p1 + 1));
		if (uri.empty()) { error = true; return false; }
		methodId = ::getMethodId(method);
		return true;
	}
	return HeaderList::parseLine(s);
}

void Http::Request::print(string& s) const noexcept
{
	char buf[64];
	s += method;
	s += ' ';
	s += uri;
	sprintf(buf, " HTTP/%u.%u\r\n", version[0], version[1]);
	s.append(buf);
	HeaderList::print(s);
	s.append("\r\n", 2);
}

void Http::Request::setMethod(const string& s) noexcept
{
	method = s;
	methodId = ::getMethodId(s);
}

void Http::Request::setMethodId(int id) noexcept
{
	methodId = id;
	if (id >= 0 && id < METHODS)
		method = methodNames[id];
	else
		method.clear();
}

void Http::Response::clear() noexcept
{
	HeaderList::clear();
	version[0] = version[1] = 1;
	parseResponseLine = true;
	complete = false;
	code = 0;
	phrase.clear();
}

bool Http::Response::parseLine(const string& s) noexcept
{
	auto len = s.length();
	while (len && (s[len-1] == '\r' || s[len-1] == '\n')) len--;
	if (!len)
	{
		if (parseResponseLine) return true;
		complete = true;
		return false;
	}
	if (parseResponseLine)
	{
		parseResponseLine = false;
		if (len < 12 || s[8] != ' ' || !parseVersion(version, s, 0, 8)) { error = true; return false; }
		if (!(isDigit(s[9]) && isDigit(s[10]) && isDigit(s[11]))) { error = true; return false; }
		code = (s[9]-'0')*100 + (s[10]-'0')*10 + s[11]-'0';
		phrase.clear();
		if (len > 12)
		{
			if (s[12] != ' ') { error = true; return false; }
			size_t startPos = 13;
			while (len > startPos && isWhiteSpace(s[len-1])) len--;
			if (len > startPos) phrase = s.substr(startPos, len-startPos);
		}
		return true;
	}
	return HeaderList::parseLine(s);
}

void Http::Response::print(string& s) const noexcept
{
	char buf[64];
	sprintf(buf, "HTTP/%u.%u %03u ", version[0], version[1], code);
	s.append(buf);
	s += phrase;
	s.append("\r\n", 2);
	HeaderList::print(s);
	s.append("\r\n", 2);
}

void Http::Response::setResponse(int code, const string& phrase) noexcept
{
	this->code = code;
	this->phrase = phrase;
}

void Http::Response::setResponse(int code) noexcept
{
	this->code = code;
	const string& phrase = getStdResp(code);
	if (phrase.empty())
		this->phrase = getStdResp(code - code % 100);
	else
		this->phrase = phrase;
}

string Http::printDateTime(time_t t) noexcept
{
	static const char strWeekDay[][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char strMonth[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
#ifdef HAVE_TIME_R
	struct tm bt;
	struct tm* pt = gmtime_r(&bt, &t);
#else
	const tm* pt = gmtime(&t);
#endif
	if (!pt) return Util::emptyString;
	int weekDay = pt->tm_wday;
	if (weekDay < 0 || weekDay > 6) weekDay = 0;
	int month = pt->tm_mon;
	if (month < 0 || month > 11) month = 0;
	char buf[256];
	sprintf(buf, "%.3s, %02d %.3s %d %.2d:%.2d:%.2d GMT",
		strWeekDay[weekDay], pt->tm_mday, strMonth[month], pt->tm_year + 1900,
		pt->tm_hour, pt->tm_min, pt->tm_sec);
	return buf;
}
