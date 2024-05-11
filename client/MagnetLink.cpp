#include "stdinc.h"
#include "MagnetLink.h"
#include "Text.h"
#include "BaseUtil.h"
#include "StrUtil.h"
#include "UriUtil.h"
#include "Base32.h"
#include "SimpleStringTokenizer.h"

void MagnetLink::clear()
{
	displayName.clear();
	exactTopic.clear();
	exactSource.clear();
	acceptableSource.clear();
	keywordTopic.clear();
	exactLength = -1;
	dirSize = -1;
}

bool MagnetLink::isValid() const
{
	return (!displayName.empty() || !keywordTopic.empty()) &&
	       (!exactTopic.empty() || !exactSource.empty() || !acceptableSource.empty());
}

bool MagnetLink::parse(const string& url)
{
	clear();
	if (url.length() < 8 || !Text::isAsciiPrefix2(url.c_str(), "magnet:?", 8)) return false;
	string param, value;
	SimpleStringTokenizer<char> st(url, '&', 8);
	while (st.getNextNonEmptyToken(param))
	{
		string::size_type pos = param.find('=');
		if (pos != string::npos)
		{
			value = Util::decodeUri(param.substr(pos + 1));
			param.erase(pos);
		}
		else
			value.clear();
		param = Util::decodeUri(param);
		if (param.length() == 2)
		{
			Text::asciiMakeLower(param);
			if (Text::isAsciiPrefix2(param.c_str(), "xt", 2))
				exactTopic.push_back(value);
			else
			if (Text::isAsciiPrefix2(param.c_str(), "dn", 2))
				displayName = std::move(value);
			else
			if (Text::isAsciiPrefix2(param.c_str(), "xl", 2))
				exactLength = Util::toInt64(value);
			else
			if (Text::isAsciiPrefix2(param.c_str(), "dl", 2))
				dirSize = Util::toInt64(value);
			else
			if (Text::isAsciiPrefix2(param.c_str(), "xs", 2))
				exactSource = std::move(value);
			else
			if (Text::isAsciiPrefix2(param.c_str(), "as", 2))
				acceptableSource = std::move(value);
			else
			if (Text::isAsciiPrefix2(param.c_str(), "kt", 2))
				keywordTopic = std::move(value);
		}
	}
	return isValid();
}

const string& MagnetLink::getFileName() const
{
	if (!displayName.empty()) return displayName;
	if (!keywordTopic.empty()) return keywordTopic;
	return Util::emptyString;
}

static bool isHex(const char* c, int size)
{
	while (size)
	{
		if (!((*c >= 'a' && *c <= 'f') || (*c >= 'A' && *c <= 'F') || (*c >= '0' && *c <= '9'))) return false;
		++c;
		size--;
	}
	return true;
}

static const char* getTTHFromParam(const string& param)
{
	size_t len = param.length();
	if (len < 4) return nullptr;
	const char* c = param.c_str();
	if (Text::isAsciiPrefix2(c, "urn:", 4))
	{
		len -= 4;
		c += 4;
		if (len < 4) return nullptr;
	}
	if (len == 32 + 1 + 39 + 9 && Text::isAsciiPrefix2(c, "bitprint:", 9)) // SHA1 "." TTH
		c += 42;
	else
	if (len == 39 + 11 && Text::isAsciiPrefix2(c, "tree:tiger:", 11))
		c += 11;
	else
	if (len == 39 + 10 && Text::isAsciiPrefix2(c, "tigertree:", 10))
		c += 10;
	else
	if (len == 39 + 12 && Text::isAsciiPrefix2(c, "tree:tiger/:", 12))
		c += 12;
	else
	if (len == 39 + 16 && Text::isAsciiPrefix2(c, "tree:tiger/1024:", 16))
		c += 16;
	else
		return nullptr;
	return Util::isBase32(c, 39) ? c : nullptr;
}

static const char* getSHA1FromParam(const string& param, int& hashSize)
{
	size_t len = param.length();
	if (len < 4) return nullptr;
	const char* c = param.c_str();
	if (Text::isAsciiPrefix2(c, "urn:", 4))
	{
		len -= 4;
		c += 4;
		if (len < 4) return nullptr;
	}
	if (len == 32 + 1 + 39 + 9 && Text::isAsciiPrefix2(c, "bitprint:", 9))
	{
		c += 9;
		hashSize = 32;
	}
	else
	if ((len == 40 + 5 || len == 32 + 5) && Text::isAsciiPrefix2(c, "btih:", 5))
	{
		c += 5;
		hashSize = len - 5;
	}
	else
		return nullptr;
	if (hashSize == 40) return isHex(c, 40) ? c : nullptr;
	return Util::isBase32(c, hashSize) ? c : nullptr;
}

static const char* getSHA256FromParam(const string& param, int& hashSize)
{
	size_t len = param.length();
	if (len < 4) return nullptr;
	const char* c = param.c_str();
	if (Text::isAsciiPrefix2(c, "urn:", 4))
	{
		len -= 4;
		c += 4;
		if (len < 4) return nullptr;
	}
	if (len == 64 + 9 && Text::isAsciiPrefix2(c, "btmh:1220", 9))
	{
		c += 9;
		hashSize = 64;
	}
	else
		return nullptr;
	return isHex(c, 64) ? c : nullptr;
}

const char* MagnetLink::getTTH() const
{
	for (const string& s : exactTopic)
	{
		const char* hash = getTTHFromParam(s);
		if (hash) return hash;
	}
	const char* hash = getTTHFromParam(exactSource);
	if (hash) return hash;
	return getTTHFromParam(acceptableSource);
}

const char* MagnetLink::getSHA1(int *hashSize) const
{
	int tmp;
	if (!hashSize) hashSize = &tmp;
	for (const string& s : exactTopic)
	{
		const char* hash = getSHA1FromParam(s, *hashSize);
		if (hash) return hash;
	}
	const char* hash = getSHA1FromParam(exactSource, *hashSize);
	if (hash) return hash;
	return getSHA1FromParam(acceptableSource, *hashSize);
}

const char* MagnetLink::getSHA256() const
{
	int tmp;
	for (const string& s : exactTopic)
	{
		const char* hash = getSHA256FromParam(s, tmp);
		if (hash) return hash;
	}
	const char* hash = getSHA256FromParam(exactSource, tmp);
	if (hash) return hash;
	return getSHA1FromParam(acceptableSource, tmp);
}
