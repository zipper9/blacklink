#ifndef HTTP_MESSAGE_H_
#define HTTP_MESSAGE_H_

#include <string>
#include <vector>
#include <stdint.h>
#include <time.h>

using std::string;

namespace Http
{

enum
{
	METHOD_CONNECT,
	METHOD_DELETE,
	METHOD_GET,
	METHOD_HEAD,
	METHOD_OPTIONS,
	METHOD_POST,
	METHOD_PUT,
	METHOD_TRACE,
	METHODS
};

class HeaderList
{
	public:
		bool parseLine(const string& s) noexcept;
		void print(string& s) const noexcept;
		void addHeader(int id, const string& value) noexcept;
		void addHeader(const string& name, const string& value) noexcept;
		int findHeader(int id) const noexcept;
		int findHeader(const string& name) const noexcept;
		bool findSingleHeader(int id, int& index) const noexcept;
		const string& at(int index) const noexcept { return items[index].value; }
		const string& getHeaderValue(int id) const noexcept;
		void clear() noexcept;
		bool isError() const noexcept { return error; }
		int64_t parseContentLength() noexcept;
		void parseContentType(string& mediaType, string& params) const noexcept;

	private:
		struct Item
		{
			int id;
			string name;
			string value;
		};
		std::vector<Item> items;

	protected:
		bool error = false;
};

class Request : public HeaderList
{
	public:
		bool parseLine(const string& s) noexcept;
		void print(string& s) const noexcept;
		void clear() noexcept;
		bool isComplete() const noexcept { return complete; }
		void setVersion(uint8_t major, uint8_t minor) noexcept { version[0] = major; version[1] = minor; }
		const uint8_t* getVersion() const noexcept { return version; }
		const string& getMethod() const noexcept { return method; }
		void setMethod(const string& s) noexcept;
		int getMethodId() const noexcept { return methodId; }
		void setMethodId(int id) noexcept;
		void setUri(const string& s) noexcept { uri = s; }

	private:
		bool parseRequestLine = true;
		bool complete = false;
		uint8_t version[2] = { 1, 1 };
		string uri;
		string method;
		int methodId = -1;
};

class Response : public HeaderList
{
	public:
		bool parseLine(const string& s) noexcept;
		void print(string& s) const noexcept;
		void clear() noexcept;
		bool isComplete() const noexcept { return complete; }
		void setVersion(uint8_t major, uint8_t minor) noexcept { version[0] = major; version[1] = minor; }
		const uint8_t* getVersion() const noexcept { return version; }
		void setResponse(int code, const string& phrase) noexcept;
		void setResponse(int code) noexcept;
		int getResponseCode() const noexcept { return code; }
		const string& getResponsePhrase() const noexcept { return phrase; }

	private:
		bool parseResponseLine = true;
		bool complete = false;
		uint8_t version[2] = { 1, 1 };
		int code = 0;
		string phrase;
};

string printDateTime(time_t t) noexcept;

}

#endif // HTTP_MESSAGE_H_
