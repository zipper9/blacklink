#include "stdinc.h"
#include "JsonParser.h"
#include "Text.h"

enum
{
	STATE_WHITESPACE,
	STATE_ERROR,
	STATE_STRING,
	STATE_NUMBER,
	STATE_KEYWORD,
	STATE_COMMENT
};

enum
{
	STATE_FLAG_COMMA               = 0x001,
	// strings
	STATE_FLAG_QUOTE               = 0x002,
	// numbers
	STATE_FLAG_FLOAT               = 0x002,
	STATE_FLAG_WANT_DIGIT          = 0x004,
	STATE_FLAG_FRAC                = 0x008,
	STATE_FLAG_WANT_FRAC_DIGIT     = 0x010,
	STATE_FLAG_EXP                 = 0x020,
	STATE_FLAG_WANT_EXP_DIGIT      = 0x040,
	STATE_FLAG_EXP_SIGN            = 0x080,
	// comments
	STATE_FLAG_START_COMMENT       = 0x002,
	STATE_FLAG_SINGLE_LINE_COMMENT = 0x004,
	STATE_FLAG_END_COMMENT         = 0x008
};

enum
{
	EXPECT_ANYTHING,
	EXPECT_NOTHING,
	EXPECT_ARRAY_ITEM,
	EXPECT_ARRAY_COMMA,
	EXPECT_OBJECT_COMMA,
	EXPECT_KEY_NAME,
	EXPECT_KEY_VALUE,
	EXPECT_COLON
};

enum
{
	KEYWORD_OTHER,
	KEYWORD_NULL,
	KEYWORD_FALSE,
	KEYWORD_TRUE
};

JsonParser::JsonParser() noexcept
{
	flags = FLAG_VALIDATE_UTF8;
	maxNesting = 512;
	state = STATE_WHITESPACE;
	stateFlags = 0;
	expect = EXPECT_ANYTHING;
	hasKeyName = false;
	valueFound = false;
	totalProcessed = 0;
	errorPos = 0;
	bomPos = 0;
}

void JsonParser::reset() noexcept
{
	state = STATE_WHITESPACE;
	stateFlags = 0;
	expect = EXPECT_ANYTHING;
	hasKeyName = false;
	valueFound = false;
	totalProcessed = 0;
	errorPos = 0;
	bomPos = 0;
	st.clear();
	token.clear();
	keyName.clear();
}

static inline bool isWhiteSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

#define E(err) do { state = STATE_ERROR; errorPos = totalProcessed + pos; return err; } while (0)

static const uint8_t BOM[] = { 0xEF, 0xBB, 0xBF };

int JsonParser::process(const char* data, size_t size) noexcept
{
	size_t tokenStart = 0;
	size_t pos = 0;
	if (totalProcessed < 3 && bomPos >= 0)
	{
		size_t bomSize = 3 - bomPos;
		if (bomSize > size) bomSize = size;
		while (pos < bomSize)
		{
			if (BOM[bomPos] != (uint8_t) data[pos])
			{
				if (bomPos) E(ERR_INVALID_BOM);
				bomPos = -1;
				break;
			}
			pos++;
			bomPos++;
		}
	}
	while (pos < size)
	{
		char c = data[pos];
		switch (state)
		{
			case STATE_WHITESPACE:
				if (c == '"')
				{
					if (expect == EXPECT_COLON) E(ERR_COLON_EXPECTED);
					if (expect == EXPECT_ARRAY_COMMA || expect == EXPECT_OBJECT_COMMA || expect == EXPECT_NOTHING) E(ERR_UNEXPECTED_CHARACTER);
					state = STATE_STRING;
					stateFlags = 0;
					tokenStart = pos + 1;
					pos++;
					break;
				}
				if ((c >= '0' && c <= '9') || c == '-')
				{
					if (expect == EXPECT_COLON) E(ERR_COLON_EXPECTED);
					if (expect == EXPECT_KEY_NAME) E(ERR_STRING_EXPECTED);
					if (expect == EXPECT_ARRAY_COMMA || expect == EXPECT_OBJECT_COMMA || expect == EXPECT_NOTHING) E(ERR_UNEXPECTED_CHARACTER);
					state = STATE_NUMBER;
					stateFlags = c == '-' ? STATE_FLAG_WANT_DIGIT : 0;
					tokenStart = pos;
					pos++;
					break;
				}
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
				{
					if (expect == EXPECT_COLON) E(ERR_COLON_EXPECTED);
					if (expect == EXPECT_KEY_NAME && !(flags & FLAG_ALLOW_BARE_NAMES)) E(ERR_STRING_EXPECTED);
					if (expect == EXPECT_ARRAY_COMMA || expect == EXPECT_OBJECT_COMMA || expect == EXPECT_NOTHING) E(ERR_UNEXPECTED_CHARACTER);
					state = STATE_KEYWORD;
					stateFlags = 0;
					tokenStart = pos;
					pos++;
					break;
				}
				if (c == ',')
				{
					if (expect == EXPECT_ARRAY_COMMA)
						expect = EXPECT_ARRAY_ITEM;
					else if (expect == EXPECT_OBJECT_COMMA)
						expect = EXPECT_KEY_NAME;
					else
						E(ERR_UNEXPECTED_CHARACTER);
					stateFlags = STATE_FLAG_COMMA;
					pos++;
					break;
				}
				if (c == ':')
				{
					if (expect != EXPECT_COLON) E(ERR_UNEXPECTED_CHARACTER);
					expect = EXPECT_KEY_VALUE;
					pos++;
					break;
				}
				if (c == '[' || c == '{')
				{
					if (expect == EXPECT_COLON) E(ERR_COLON_EXPECTED);
					if (expect == EXPECT_KEY_NAME) E(ERR_STRING_EXPECTED);
					if (expect == EXPECT_ARRAY_COMMA || expect == EXPECT_OBJECT_COMMA || expect == EXPECT_NOTHING) E(ERR_UNEXPECTED_CHARACTER);
					int err = beginStructure(c == '[' ? TYPE_ARRAY : TYPE_OBJECT);
					if (err) E(err);
					pos++;
					break;
				}
				if (c == ']')
				{
					if (expect == EXPECT_COLON) E(ERR_COLON_EXPECTED);
					if (expect == EXPECT_KEY_VALUE || expect == EXPECT_OBJECT_COMMA || expect == EXPECT_NOTHING) E(ERR_UNEXPECTED_CHARACTER);
					if (stateFlags & STATE_FLAG_COMMA)
					{
						stateFlags ^= STATE_FLAG_COMMA;
						if (!(flags & FLAG_ALLOW_TRAILING_COMMA)) E(ERR_TRAILING_COMMA_NOT_ALLOWED);
					}
					int err = endStructure(TYPE_ARRAY);
					if (err) E(err);
					pos++;
					break;
				}
				if (c == '}')
				{
					if (expect == EXPECT_COLON) E(ERR_COLON_EXPECTED);
					if (expect == EXPECT_KEY_VALUE || expect == EXPECT_ARRAY_COMMA || expect == EXPECT_NOTHING) E(ERR_UNEXPECTED_CHARACTER);
					if (stateFlags & STATE_FLAG_COMMA)
					{
						stateFlags ^= STATE_FLAG_COMMA;
						if (!(flags & FLAG_ALLOW_TRAILING_COMMA)) E(ERR_TRAILING_COMMA_NOT_ALLOWED);
					}
					int err = endStructure(TYPE_OBJECT);
					if (err) E(err);
					pos++;
					break;
				}
				if (isWhiteSpace(c))
				{
					pos++;
					break;
				}
				if (c == '/' && (flags & FLAG_ALLOW_COMMENTS))
				{
					state = STATE_COMMENT;
					stateFlags = (stateFlags & STATE_FLAG_COMMA) | STATE_FLAG_START_COMMENT;
					tokenStart = pos + 1;
					pos++;
					break;
				}
				E(ERR_UNEXPECTED_CHARACTER);

			case STATE_STRING:
				if (c == '"')
				{
					if (stateFlags & STATE_FLAG_QUOTE)
					{
						stateFlags &= ~STATE_FLAG_QUOTE;
						pos++;
						break;
					}
					int err = handleString(data, tokenStart, pos);
					if (err) E(err);
					tokenStart = 0;
					state = STATE_WHITESPACE;
					pos++;
					break;
				}
				if ((unsigned char) c < 0x20) E(ERR_UNESCAPED_CHARACTER);
				if (c == '\\')
					stateFlags ^= STATE_FLAG_QUOTE;
				else
					stateFlags &= ~STATE_FLAG_QUOTE;
				pos++;
				break;

			case STATE_NUMBER:
				if (c == '.')
				{
					if (stateFlags & (STATE_FLAG_FRAC | STATE_FLAG_EXP | STATE_FLAG_WANT_DIGIT)) E(ERR_INVALID_NUMBER);
					stateFlags |= STATE_FLAG_FRAC | STATE_FLAG_WANT_FRAC_DIGIT | STATE_FLAG_FLOAT;
					pos++;
					break;
				}
				if (c == 'e' || c == 'E')
				{
					if (stateFlags & (STATE_FLAG_EXP | STATE_FLAG_WANT_FRAC_DIGIT)) E(ERR_INVALID_NUMBER);
					stateFlags |= STATE_FLAG_EXP | STATE_FLAG_WANT_EXP_DIGIT | STATE_FLAG_FLOAT;
					pos++;
					break;
				}
				if (c == '+' || c == '-')
				{
					if ((stateFlags & (STATE_FLAG_WANT_EXP_DIGIT | STATE_FLAG_EXP_SIGN)) != STATE_FLAG_WANT_EXP_DIGIT) E(ERR_INVALID_NUMBER);
					stateFlags |= STATE_FLAG_EXP_SIGN;
					pos++;
					break;
				}
				if (c >= '0' && c <= '9')
				{
					stateFlags &= ~(STATE_FLAG_WANT_DIGIT | STATE_FLAG_WANT_FRAC_DIGIT | STATE_FLAG_WANT_EXP_DIGIT);
					pos++;
					break;
				}
				if (isWhiteSpace(c))
				{
					int err = handleNumber(data, tokenStart, pos);
					if (err) E(err);
					tokenStart = 0;
					state = STATE_WHITESPACE;
					pos++;
					break;
				}
				if (c == ',' || c == ']' || c == '}')
				{
					int err = handleNumber(data, tokenStart, pos);
					if (err) E(err);
					tokenStart = 0;
					state = STATE_WHITESPACE;
					break;
				}
				E(ERR_UNEXPECTED_CHARACTER);

			case STATE_KEYWORD:
				if (isWhiteSpace(c))
				{
					int err = handleKeyword(data, tokenStart, pos);
					if (err) E(err);
					tokenStart = 0;
					state = STATE_WHITESPACE;
					pos++;
					break;
				}
				if (c == ',' || c == ']' || c == '}' || c == ':')
				{
					int err = handleKeyword(data, tokenStart, pos);
					if (err) E(err);
					tokenStart = 0;
					state = STATE_WHITESPACE;
					break;
				}
				pos++;
				break;

			case STATE_COMMENT:
				if (c == '*')
				{
					if (stateFlags & STATE_FLAG_START_COMMENT)
						stateFlags ^= STATE_FLAG_START_COMMENT;
					else if (!(stateFlags & STATE_FLAG_SINGLE_LINE_COMMENT))
						stateFlags |= STATE_FLAG_END_COMMENT;
				}
				else if (c == '/')
				{
					if (stateFlags & STATE_FLAG_START_COMMENT)
						stateFlags = (stateFlags & ~STATE_FLAG_START_COMMENT) | STATE_FLAG_SINGLE_LINE_COMMENT;
					else if (stateFlags & STATE_FLAG_END_COMMENT)
					{
						int err = handleComment(data, tokenStart, pos);
						if (err) E(err);
						tokenStart = 0;
						state = STATE_WHITESPACE;
						stateFlags ^= STATE_FLAG_END_COMMENT;
					}
				}
				else if (c == '\n')
				{
					if (stateFlags & STATE_FLAG_START_COMMENT) E(ERR_UNEXPECTED_CHARACTER);
					if (stateFlags & STATE_FLAG_SINGLE_LINE_COMMENT)
					{
						int err = handleComment(data, tokenStart, pos);
						if (err) E(err);
						tokenStart = 0;
						state = STATE_WHITESPACE;
						stateFlags ^= STATE_FLAG_SINGLE_LINE_COMMENT;
					}
					else
						stateFlags &= ~STATE_FLAG_END_COMMENT;
				}
				else
				{
					if (stateFlags & STATE_FLAG_START_COMMENT) E(ERR_UNEXPECTED_CHARACTER);
					stateFlags &= ~STATE_FLAG_END_COMMENT;
				}
				pos++;
				break;

			default:
				E(ERR_INVALID_STATE);
		}
	}
	totalProcessed += size;
	if (state >= STATE_STRING && tokenStart < size)
		token.append(data + tokenStart, size - tokenStart);
	return NO_ERROR;
}

int JsonParser::finish() noexcept
{
	const size_t pos = 0;
	if (state == STATE_STRING) E(ERR_STRING_NOT_TERMINATED);
	if (!st.empty()) E(ERR_COMPOUND_TYPE_NOT_CLOSED);
	if (bomPos > 0 && bomPos < 3) E(ERR_INVALID_BOM);
	if (state == STATE_NUMBER)
	{
		int err = handleNumber(nullptr, 0, 0);
		if (err) E(err);
	}
	else if (state == STATE_KEYWORD)
	{
		int err = handleKeyword(nullptr, 0, 0);
		if (err) E(err);
	}
	else if (!valueFound) E(ERR_NO_DATA);
	return NO_ERROR;
}

#undef E

static inline int getHexDigit(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static int getHexChar(const char* s) noexcept
{
	int h1 = getHexDigit(s[0]);
	if (h1 < 0) return -1;
	int h2 = getHexDigit(s[1]);
	if (h2 < 0) return -1;
	int h3 = getHexDigit(s[2]);
	if (h3 < 0) return -1;
	int h4 = getHexDigit(s[3]);
	if (h4 < 0) return -1;
	return h1 << 12 | h2 << 8 | h3 << 4 | h4;
}

int JsonParser::unescape(string& s) noexcept
{
	if (s.empty()) return 0;
	char* data = &s[0];
	size_t writePos = 0;
	size_t readPos = 0;
	size_t size = s.length();
	size_t prevSurrogatePos = 0;
	unsigned prevSurrogate = 0;
	while (readPos < size)
	{
		char* p = static_cast<char*>(memchr(data + readPos, '\\', size - readPos));
		if (!p) break;
		size_t delta = p - (data + readPos);
		if (delta)
		{
			if (readPos != writePos) memmove(data + writePos, data + readPos, delta);
			readPos += delta;
			writePos += delta;
		}
		if (readPos + 1 == size) return ERR_INVALID_ESCAPE;
		int32_t chr;
		int escapeLen = 2;
		switch (data[readPos + 1])
		{
			case '\\': chr = '\\'; break;
			case '\"': chr = '\"'; break;
			case '/':  chr = '/';  break;
			case 'b':  chr = '\b'; break;
			case 'f':  chr = '\f'; break;
			case 'n':  chr = '\n'; break;
			case 'r':  chr = '\r'; break;
			case 't':  chr = '\t'; break;
			case 'u':
			{
				if (size - readPos < 6) return ERR_INVALID_ESCAPE;
				chr = getHexChar(data + readPos + 2);
				if (chr < 0) return ERR_INVALID_ESCAPE;
				escapeLen = 6;
				break;
			}
			default: return ERR_INVALID_ESCAPE;
		}
		if (chr >= 0xD800 && chr < 0xDC00)
		{
			if (prevSurrogate) return ERR_INVALID_ESCAPE;
			prevSurrogatePos = readPos;
			prevSurrogate = chr;
			readPos += 6;
			continue;
		}
		if (chr >= 0xDC00 && chr < 0xE000)
		{
			if (!prevSurrogate || prevSurrogatePos + 6 != readPos) return ERR_INVALID_ESCAPE;
			chr = ((chr - 0xDC00) | (prevSurrogate - 0xD800) << 10) + 0x10000;
			prevSurrogatePos = 0;
			prevSurrogate = 0;
		}
		if (prevSurrogate) return ERR_INVALID_ESCAPE;
		writePos += Text::wcToUtf8(chr, data + writePos);
		readPos += escapeLen;
	}
	if (prevSurrogate) return ERR_INVALID_ESCAPE;
	if (writePos)
	{
		size_t delta = size - readPos;
		if (delta)
		{
			if (readPos != writePos) memmove(data + writePos, data + readPos, delta);
			writePos += delta;
		}
		s.resize(writePos);
	}
	return NO_ERROR;
}

int JsonParser::handleString(const char* data, size_t start, size_t pos) noexcept
{
	token.append(data + start, pos - start);
	int err = unescape(token);
	if (err)
	{
		token.clear();
		return err;
	}
	if ((flags & FLAG_VALIDATE_UTF8) && !Text::validateUtf8(token)) return ERR_INVALID_UTF8;
	if (expect == EXPECT_KEY_NAME)
	{
		expect = EXPECT_COLON;
		keyName = std::move(token);
		hasKeyName = true;
		token.clear();
		return NO_ERROR;
	}
	if (hasKeyName)
	{
		if (!onNamedValue(keyName, token, TYPE_STRING)) return ERR_PARSING_ABORTED;
		keyName.clear();
		hasKeyName = false;
	}
	else
	{
		if (!onValue(token, TYPE_STRING)) return ERR_PARSING_ABORTED;
	}
	stateFlags = 0;
	token.clear();
	updateExpect();
	valueFound = true;
	return NO_ERROR;
}

int JsonParser::handleNumber(const char* data, size_t start, size_t pos) noexcept
{
	if (stateFlags & (STATE_FLAG_WANT_DIGIT | STATE_FLAG_WANT_EXP_DIGIT | STATE_FLAG_WANT_FRAC_DIGIT)) return ERR_INVALID_NUMBER;
	token.append(data + start, pos - start);
	if ((flags & FLAG_STRICT_NUMBER_CHECKS) && token.length() >= 2)
	{
		char c0 = token[0];
		char c1 = token[1];
		char c2 = token.length() > 2 ? token[2] : 0;
		if ((c0 == '-' && c1 == '0' && !(c2 == '.' || c2 == 'e' || c2 == 'E' || c2 == 0)) ||
			(c0 == '0' && !(c1 == '.' || c1 == 'e' || c1 == 'E')))
				return ERR_INVALID_NUMBER;
	}
	if (hasKeyName)
	{
		if (!onNamedValue(keyName, token, (stateFlags & STATE_FLAG_FLOAT) ? TYPE_FLOAT : TYPE_INT)) return ERR_PARSING_ABORTED;
		keyName.clear();
		hasKeyName = false;
	}
	else
	{
		if (!onValue(token, (stateFlags & STATE_FLAG_FLOAT) ? TYPE_FLOAT : TYPE_INT)) return ERR_PARSING_ABORTED;
	}
	stateFlags = 0;
	token.clear();
	updateExpect();
	valueFound = true;
	return NO_ERROR;
}

int JsonParser::handleKeyword(const char* data, size_t start, size_t pos) noexcept
{
	token.append(data + start, pos - start);
	int kw;
	if (token == "null")
		kw = KEYWORD_NULL;
	else if (token == "false")
		kw = KEYWORD_FALSE;
	else if (token == "true")
		kw = KEYWORD_TRUE;
	else
		kw = KEYWORD_OTHER;
	if (expect == EXPECT_KEY_NAME)
	{
		if (kw != KEYWORD_OTHER) return ERR_KEYWORD_NOT_ALLOWED;
		if ((flags & FLAG_VALIDATE_UTF8) && !Text::validateUtf8(token)) return ERR_INVALID_UTF8;
		expect = EXPECT_COLON;
		keyName = std::move(token);
		hasKeyName = true;
		token.clear();
		return NO_ERROR;
	}
	if (kw == KEYWORD_OTHER) return ERR_UNKNOWN_KEYWORD;
	int type = kw == KEYWORD_NULL ? TYPE_NULL : TYPE_BOOL;
	if (hasKeyName)
	{
		if (!onNamedValue(keyName, token, type)) return ERR_PARSING_ABORTED;
		keyName.clear();
		hasKeyName = false;
	}
	else
	{
		if (!onValue(token, type)) return ERR_PARSING_ABORTED;
	}
	token.clear();
	updateExpect();
	valueFound = true;
	return NO_ERROR;
}

int JsonParser::handleComment(const char* data, size_t start, size_t pos) noexcept
{
	token.append(data + start, pos - start);
	if (!onComment(token)) return ERR_PARSING_ABORTED;
	token.clear();
	return NO_ERROR;
}

int JsonParser::beginStructure(int type) noexcept
{
	string emptyString;
	if (st.size() >= maxNesting) return ERR_MAX_NESTING_EXCEEDED;
	if (hasKeyName)
	{
		if (!onNamedValue(keyName, emptyString, type)) return ERR_PARSING_ABORTED;
		keyName.clear();
		hasKeyName = false;
	}
	else
	{
		if (!onValue(emptyString, type)) return ERR_PARSING_ABORTED;
	}
	st.push_back(type);
	expect = type == TYPE_ARRAY ? EXPECT_ARRAY_ITEM : EXPECT_KEY_NAME;
	stateFlags &= ~STATE_FLAG_COMMA;
	return NO_ERROR;
}

int JsonParser::endStructure(int type) noexcept
{
	if (st.empty() || st.back() != type) return ERR_UNEXPECTED_CHARACTER;
	if (!onEndStructure(type)) return ERR_PARSING_ABORTED;
	st.pop_back();
	updateExpect();
	valueFound = true;
	return NO_ERROR;
}

void JsonParser::updateExpect() noexcept
{
	if (st.empty())
	 	expect = EXPECT_NOTHING;
	else if (st.back() == TYPE_OBJECT)
		expect = EXPECT_OBJECT_COMMA;
	else
		expect = EXPECT_ARRAY_COMMA;
}
