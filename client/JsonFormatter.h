#ifndef JSON_FORMATTER_H_
#define JSON_FORMATTER_H_

#include "typedefs.h"

class JsonFormatter
{
public:
	JsonFormatter(): decorate(true), indent(0), expectValue(false), wantComma(false)
	{
	}

	const string& getResult() const { return s; }
	void moveResult(string& res) noexcept { res = std::move(s); s.clear(); }

	void open(char c) noexcept;
	void close(char c) noexcept;
	void appendKey(const char* key) noexcept;
	void appendKey(const string& key) noexcept;
	void appendStringValue(const string& val, bool escape = true) noexcept;
	void appendIntValue(int val) noexcept;
	void appendInt64Value(int64_t val) noexcept;
	void appendBoolValue(bool val) noexcept;
	void setDecorate(bool flag) noexcept { decorate = flag; }

private:
	string s;
	int indent;
	bool expectValue;
	bool wantComma;
	bool decorate;
};

#endif // JSON_FORMATTER_H_
