#ifndef JSON_PARSER_H_
#define JSON_PARSER_H_

#include "typedefs.h"
#include <stdint.h>

// undefine windows.h macro
#ifdef NO_ERROR
#undef NO_ERROR
#endif

class JsonParser
{
public:
	enum
	{
		NO_ERROR,
		ERR_PARSING_ABORTED,
		ERR_NO_DATA,
		ERR_INVALID_STATE,
		ERR_MAX_NESTING_EXCEEDED,
		ERR_UNEXPECTED_CHARACTER,
		ERR_COLON_EXPECTED,
		ERR_STRING_EXPECTED,
		ERR_UNKNOWN_KEYWORD,
		ERR_UNESCAPED_CHARACTER,
		ERR_INVALID_ESCAPE,
		ERR_INVALID_NUMBER,
		ERR_STRING_NOT_TERMINATED,
		ERR_COMPOUND_TYPE_NOT_CLOSED,
		ERR_TRAILING_COMMA_NOT_ALLOWED,
		ERR_KEYWORD_NOT_ALLOWED,
		ERR_INVALID_UTF8,
		ERR_INVALID_BOM
	};

	enum
	{
		TYPE_NULL,
		TYPE_BOOL,
		TYPE_INT,
		TYPE_FLOAT,
		TYPE_STRING,
		TYPE_ARRAY,
		TYPE_OBJECT
	};

	enum
	{
		FLAG_VALIDATE_UTF8        = 0x01,
		FLAG_STRICT_NUMBER_CHECKS = 0x02,
		FLAG_ALLOW_BARE_NAMES     = 0x04,
		FLAG_ALLOW_TRAILING_COMMA = 0x08,
		FLAG_ALLOW_COMMENTS       = 0x10
	};

	JsonParser() noexcept;

public:
	int process(const char* data, size_t size) noexcept;
	int finish() noexcept;
	void reset() noexcept;
	size_t getErrorPos() const noexcept { return errorPos; }
	int getFlags() const noexcept { return flags; }
	void setFlags(int flags) noexcept { this->flags = flags; }

protected:
	virtual bool onNamedValue(const string& key, string& value, int type) noexcept { return true; }
	virtual bool onValue(string& value, int type) noexcept { return true; }
	virtual bool onEndStructure(int type) noexcept { return true; }
	virtual bool onComment(string& text) noexcept { return true; }

	int getNestingLevel() const noexcept { return (int) st.size(); }

private:
	int flags;
	unsigned maxNesting;
	std::vector<int8_t> st;
	int state;
	int stateFlags;
	int expect;
	string token;
	string keyName;
	bool hasKeyName;
	bool valueFound;
	int bomPos;
	size_t totalProcessed;
	size_t errorPos;

	int handleString(const char* data, size_t start, size_t pos) noexcept;
	int handleNumber(const char* data, size_t start, size_t pos) noexcept;
	int handleKeyword(const char* data, size_t start, size_t pos) noexcept;
	int handleComment(const char* data, size_t start, size_t pos) noexcept;
	int beginStructure(int type) noexcept;
	int endStructure(int type) noexcept;
	void updateExpect() noexcept;

public:
	static int unescape(string& s) noexcept;
};

#endif // JSON_PARSER_H_
