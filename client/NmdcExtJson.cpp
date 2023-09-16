#include "stdinc.h"
#include "NmdcExtJson.h"
#include "JsonParser.h"
#include "JsonFormatter.h"

class NmdcExtJsonParser : public JsonParser
{
	public:
		NmdcExtJsonParser(NmdcExtJson::Data& data) : data(data) {}

	protected:
		bool onNamedValue(const string& key, string& value, int type) noexcept
		{
			if (getNestingLevel() != 1) return true;
			int index = NmdcExtJson::getAttribByName(key);
			if (index != -1) data.attr[index] = std::move(value);
			return true;
		}

	private:
		NmdcExtJson::Data& data;
};

bool NmdcExtJson::Data::parse(const string& json) noexcept
{
	NmdcExtJsonParser p(*this);
	p.setFlags(JsonParser::FLAG_VALIDATE_UTF8 | JsonParser::FLAG_STRICT_NUMBER_CHECKS);
	return p.process(json.data(), json.length()) == JsonParser::NO_ERROR && p.finish() == JsonParser::NO_ERROR;
}

void NmdcExtJson::appendStringAttrib(JsonFormatter& jf, int index, const string& value) noexcept
{
	jf.appendKey(getAttribText(index));
	jf.appendStringValue(value);
}

void NmdcExtJson::appendIntAttrib(JsonFormatter& jf, int index, int value) noexcept
{
	jf.appendKey(getAttribText(index));
	jf.appendIntValue(value);
}

void NmdcExtJson::appendInt64Attrib(JsonFormatter& jf, int index, int64_t value) noexcept
{
	jf.appendKey(getAttribText(index));
	jf.appendInt64Value(value);
}
