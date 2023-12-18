#include "stdinc.h"
#include "JsonFormatter.h"
#include "StrUtil.h"

static const char INDENT_CHAR = '\t';

void JsonFormatter::open(char c) noexcept
{
	if (wantComma) s += ',';
	if (decorate)
	{
		if (!s.empty()) s += '\n';
		s.append(indent, INDENT_CHAR);
		s += c;
		indent++;
	}
	else
		s += c;
	wantComma = expectValue = false;
}

void JsonFormatter::close(char c) noexcept
{
	if (decorate)
	{
		s += '\n';
		s.append(--indent, INDENT_CHAR);
	}
	s += c;
	wantComma = true;
}

void JsonFormatter::appendKey(const char* key) noexcept
{
	if (decorate)
	{
		if (wantComma) s += ',';
		s += '\n';
		s.append(indent, INDENT_CHAR);
		s += '"';
		s += key;
		s += "\" : ";
	}
	else
	{
		if (wantComma) s += ',';
		s += '"';
		s += key;
		s += "\":";
	}
	wantComma = false;
	expectValue = true;
}

void JsonFormatter::appendKey(const string& key) noexcept
{
	if (decorate)
	{
		if (wantComma) s += ',';
		s += '\n';
		s.append(indent, INDENT_CHAR);
		s += '"';
		s += key;
		s += "\" : ";
	}
	else
	{
		if (wantComma) s += ',';
		s += '"';
		s += key;
		s += "\":";
	}
	wantComma = false;
	expectValue = true;
}

void JsonFormatter::appendStringValue(const string& val, bool escape) noexcept
{
	appendValue();
	s += '"';
	size_t lastAppended = 0;
	size_t len = val.length();
	if (escape)
	{
		const char* data = val.data();
		for (size_t i = 0; i < len; ++i)
			if ((unsigned char) data[i] < ' ' || data[i] == '\\' || data[i] == '"')
			{
				if (lastAppended < i)
					s.append(val, lastAppended, i - lastAppended);
				s += '\\';
				switch (data[i])
				{
					case '\\':
					case '"':
						s += data[i];
						break;
					case '\b':
						s += 'b';
						break;
					case '\f':
						s += 'f';
						break;
					case '\n':
						s += 'n';
						break;
					case '\r':
						s += 'r';
						break;
					case '\t':
						s += 't';
						break;
					default:
					{
						char tmp[8];
						sprintf(tmp, "u%04x", (unsigned char) data[i]);
						s.append(tmp, 5);
					}
				}
				lastAppended = i + 1;
			}
	}
	if (lastAppended < len)
		s.append(val, lastAppended, len - lastAppended);
	s += '"';
}

void JsonFormatter::appendIntValue(int val) noexcept
{
	appendValue();
	s += Util::toString(val);
}

void JsonFormatter::appendInt64Value(int64_t val) noexcept
{
	appendValue();
	s += Util::toString(val);
}

void JsonFormatter::appendBoolValue(bool val) noexcept
{
	appendValue();
	s += val ? "true" : "false";
}

void JsonFormatter::appendValue()
{
	if (wantComma)
	{
		if (decorate) s += ", "; else s += ',';
	}
	else if (decorate && !expectValue)
	{
		s += '\n';
		s.append(indent, INDENT_CHAR);
	}
	wantComma = true;
	expectValue = false;
}
