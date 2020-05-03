#include "stdinc.h"
#include "IpList.h"
#include "Util.h"

bool IpList::addRange(uint32_t start, uint32_t end, uint64_t payload, int& error)
{
	Range range;
	range.end = end;
	range.payload = payload;

	auto i = m.find(start);
	if (i != m.end())
	{
		auto& ranges = i->second;
		for (const auto& r : ranges)
			if (r.end == end)
			{
				error = ERR_ALREADY_EXISTS;
				return false;
			}
		ranges.push_back(range);
	}
	else
	{
		RangeList rangeList = { range };
		m.insert(std::make_pair(start, rangeList));
	}
	error = 0;
	return true;
}

bool IpList::find(uint32_t addr, uint64_t& payload) const
{
	auto i = m.lower_bound(addr);
	if ((i == m.cend() || i->first != addr) && i != m.cbegin()) --i;
	bool result = false;
	while (i != m.cend())
	{
		uint32_t start = i->first;
		if (addr < start) break;
		uint32_t minRangeLen = 0xFFFFFFFF;
		const auto& ranges = i->second;
		for (const auto& range : ranges)
			if (addr <= range.end)
			{
				uint32_t rangeLen = range.end - start;
				if (rangeLen <= minRangeLen)
				{
					result = true;
					minRangeLen = rangeLen;
					payload = range.payload;
				}
			}
		if (result) break;
		--i;
	}
	return result;
}

static void skipWhiteSpace(const string& s, string::size_type& i)
{
	while (i < s.length() && (s[i] == ' ' || s[i] == '\t')) ++i;
}

static void skipIpAddress(const string& s, string::size_type& i)
{
	while (i < s.length() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.')) ++i;
}

static void skipNumber(const string& s, string::size_type& i)
{
	while (i < s.length() && s[i] >= '0' && s[i] <= '9') ++i;
}

static bool isValidMask(uint32_t x)
{
	if (!x) return false;
	x = ~x;
	return (x & (x+1)) == 0;
}

int IpList::parseLine(const std::string& s, IpList::ParseLineResult& res, const IpList::ParseLineOptions* options, string::size_type startPos)
{
	string::size_type i = startPos;
	res.start = res.end = startPos;
	res.pos = startPos;
	res.specialChar = 0;
	skipWhiteSpace(s, i);
	if (i == s.length())
		return ERR_LINE_SKIPPED;
	if (s[i] == '#')
		return ERR_LINE_SKIPPED;
	if (i + 1 < s.length() && s[i+1] == s[i] && (s[i] == '/' || s[i] == '-'))
		return ERR_LINE_SKIPPED;
	if (options && options->specialCharCount)
	{
		for (int j = 0; j < options->specialCharCount; ++j)
			if (options->specialChars[j] == s[i])
			{
				res.specialChar = s[i];
				break;
			}
		if (res.specialChar && ++i == s.length())
			return ERR_BAD_FORMAT;
	}
	string::size_type j = i;
	skipIpAddress(s, j);
	if (!Util::parseIpAddress(res.start, s, i, j))
		return ERR_BAD_FORMAT;
	i = j;
	skipWhiteSpace(s, i);
	if (i == s.length())
	{
		res.end = res.start;
		res.pos = i;
		return 0;
	}

	char sep = s[i];
	++i;
	skipWhiteSpace(s, i);
	if (i == s.length())
		return ERR_BAD_FORMAT;
	if (sep == '/')
	{
		j = i;
		skipNumber(s, j);
		uint32_t mask;
		if (j < s.length() && s[j] == '.')
		{
			j = i;
			skipIpAddress(s, j);
			if (!Util::parseIpAddress(mask, s, i, j) || !isValidMask(mask))
				return ERR_BAD_NETMASK;
		}
		else
		{
			int bits = atoi(s.c_str() + i);
			if (bits <= 0 || bits > 32)
				return ERR_BAD_NETMASK;
			mask = 0xFFFFFFFF << (32-bits);
		}
		res.end = res.start | ~mask;
	}
	else
	if (sep == '-')
	{
		j = i;
		skipIpAddress(s, j);
		if (!Util::parseIpAddress(res.end, s, i, j))
			return ERR_BAD_FORMAT;
	}
	else
	{
		res.end = res.start;
	}
	res.pos = j;
	if (res.end < res.start) return ERR_BAD_RANGE;
	return 0;
}

string IpList::getErrorText(int error)
{
	switch (error)
	{
		case 0:
			return "No error";
		case ERR_BAD_RANGE:
			return "Invalid range";
		case ERR_ALREADY_EXISTS:
			return "Entry already exists";
		case ERR_BAD_FORMAT:
			return "Invalid format";
		case ERR_BAD_NETMASK:
			return "Invalid network mask";
	}
	char buf[64];
	sprintf(buf, "Error %d", error);
	return buf;
}
