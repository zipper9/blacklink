/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "stdinc.h"
#include "UriUtil.h"
#include "StrUtil.h"
#include "Text.h"
#include "idna/idna.h"

static inline int getHex(char c)
{
	if (c >= '0' && c <= '9') return c-'0';
	if (c >= 'a' && c <= 'f') return c-'a'+10;
	if (c >= 'A' && c <= 'F') return c-'A'+10;
	return -1;
}

static string encodeUri(const string& str, const string& chars)
{
	static const char* hexDigits = "0123456789ABCDEF";
	string res;
	res.reserve(str.length());
	char escape[4];
	for (string::size_type i = 0; i < str.length(); ++i)
	{
		if (str[i] == ' ')
			res += '+';
		else if (str[i] <= 0x1F || str[i] >= 0x7F || chars.find(str[i]) != string::npos)
		{
			escape[0] = '%';
			escape[1] = hexDigits[(str[i] >> 4) & 0xF];
			escape[2] = hexDigits[str[i] & 0xF];
			res.append(escape, 3);
		}
		else
			res += str[i];
	}
	return res;
}

string Util::encodeUriQuery(const string& str)
{
	static const string disallowed =
		";/?:@&=+$," // reserved
		"<>#%\" "    // delimiters
		"{}|\\^[]`"; // unwise
	return encodeUri(str, disallowed);
}

string Util::encodeUriPath(const string& str)
{
	static const string disallowed =
		";?&=+$,"    // reserved
		"<>#%\" "    // delimiters
		"{}|\\^[]`"; // unwise
	return encodeUri(str, disallowed);
}

string Util::decodeUri(const string& str)
{
	string res;
	res.reserve(str.length());
	for (string::size_type i = 0; i < str.length(); ++i)
	{
		if (str[i] == '%' && i + 2 < str.length())
		{
			int h1 = getHex(str[i+1]);
			int h2 = getHex(str[i+2]);
			if (h1 != -1 && h2 != -1)
			{
				res += h1 << 4 | h2;
				i += 2;
				continue;
			}
		}
		else if (str[i] == '+')
		{
			res += ' ';
			continue;
		}
		res += str[i];
	}
	return res;
}

bool Util::getDefaultPort(const string& protocol, uint16_t& port, bool& secure)
{
	if (protocol == "dchub" || protocol == "nmdc" || protocol == "adc")
	{
		port = 411;
		secure = false;
		return true;
	}
	if (protocol == "nmdcs")
	{
		port = 411;
		secure = true;
		return true;
	}
	if (protocol == "adcs")
	{
		port = 412;
		secure = true;
		return true;
	}
	if (protocol == "http")
	{
		port = 80;
		secure = false;
		return true;
	}
	if (protocol == "https")
	{
		port = 443;
		secure = true;
		return true;
	}
	if (protocol == "ftp")
	{
		port = 21;
		secure = false;
		return true;
	}
	port = 0;
	secure = false;
	return false;
}

void Util::decodeUrl(const string& url, ParsedUrl& res, const string& defProto)
{
	auto fragmentEnd = url.size();
	auto fragmentStart = url.rfind('#');
	auto userStart = string::npos;

	res.isSecure = false;
	res.port = 0;
	res.user.clear();
	res.password.clear();

	size_t queryEnd;
	if (fragmentStart == string::npos)
	{
		queryEnd = fragmentStart = fragmentEnd;
	}
	else
	{
		queryEnd = fragmentStart;
		fragmentStart++;
	}

	auto queryStart = url.rfind('?', queryEnd);
	size_t fileEnd;

	if (queryStart == string::npos)
	{
		fileEnd = queryStart = queryEnd;
	}
	else
	{
		fileEnd = queryStart;
		queryStart++;
	}

	auto protoEnd = url.find("://");
	if (protoEnd != string::npos)
	{
		res.protocol = Text::toLower(url.substr(0, protoEnd));
		if (res.protocol.empty())
			res.protocol = defProto;
	}
	else
		res.protocol = defProto;

	auto authorityStart = protoEnd == string::npos ? 0 : protoEnd + 3;
	auto userEnd = url.find('@', authorityStart);
	if (userEnd != string::npos)
	{
		auto pos = url.find_first_of("/#?", authorityStart);
		if (pos == string::npos || pos > userEnd)
		{
			userStart = authorityStart;
			authorityStart = userEnd + 1;
		}
	}

	auto authorityEnd = url.find_first_of("/#?", authorityStart);

	size_t fileStart;
	if (authorityEnd == string::npos)
		authorityEnd = fileStart = fileEnd;
	else
		fileStart = authorityEnd;

	if (authorityEnd > authorityStart)
	{
		size_t portStart = string::npos;
		if (url[authorityStart] == '[')
		{
			// IPv6?
			auto hostEnd = url.find(']');
			if (hostEnd == string::npos)
			{
				return;
			}

			res.host = url.substr(authorityStart + 1, hostEnd - authorityStart - 1);
			if (hostEnd + 1 < url.size() && url[hostEnd + 1] == ':')
				portStart = hostEnd + 2;
		}
		else
		{
			size_t hostEnd;
			portStart = url.find(':', authorityStart);
			if (portStart != string::npos && portStart > authorityEnd)
				portStart = string::npos;

			if (portStart == string::npos)
				hostEnd = authorityEnd;
			else
			{
				hostEnd = portStart;
				portStart++;
			}
			res.host = Text::toLower(url.substr(authorityStart, hostEnd - authorityStart));
		}

		uint16_t defPort;
		getDefaultPort(res.protocol, defPort, res.isSecure);
		if (portStart != string::npos)
			res.port = static_cast<uint16_t>(Util::toInt(url.substr(portStart, authorityEnd - portStart)));
		else
			res.port = defPort;
	}

	if (userStart != string::npos)
	{
		auto pos = url.find(':', userStart);
		if (pos != string::npos && pos < userEnd)
		{
			res.user = url.substr(userStart, pos - userStart);
			res.password = url.substr(pos + 1, userEnd - (pos + 1));
		}
		else
			res.user = url.substr(userStart, userEnd - userStart);
	}

	res.path = url.substr(fileStart, fileEnd - fileStart);
	res.query = url.substr(queryStart, queryEnd - queryStart);
	res.fragment = url.substr(fragmentStart, fragmentEnd - fragmentStart);
	if (!Text::isAscii(res.host))
	{
		wstring wstr;
		Text::utf8ToWide(res.host, wstr);
		Text::toLower(wstr);
		int error;
		string converted;
		if (IDNA_convert_to_ACE(wstr, converted, error))
			res.host = std::move(converted);
	}
}

string Util::formatUrl(const Util::ParsedUrl& p, bool omitDefaultPort)
{
	string result;
	if (p.protocol.empty() || p.host.empty()) return result;
	result = p.protocol;
	result += "://";
	if (!p.user.empty())
	{
		result += p.user;
		if (!p.password.empty())
		{
			result += ':';
			result += p.password;
		}
		result += '@';
	}
	if (p.host.find(':') != string::npos)
		result += "[" + p.host + "]";
	else
		result += p.host;
	if (p.port)
	{
		if (omitDefaultPort)
		{
			uint16_t defPort;
			bool secure;
			getDefaultPort(p.protocol, defPort, secure);
			if (p.port != defPort) omitDefaultPort = false;
		}
		if (!omitDefaultPort)
		{
			result += ':';
			result += Util::toString(p.port);
		}
	}
	result += p.path;
	if (!p.query.empty())
	{
		result += '?';
		result += p.query;
	}
	if (!p.fragment.empty())
	{
		result += '#';
		result += p.fragment;
	}
	return result;
}

bool Util::parseIpPort(const string& ipPort, string& ip, uint16_t& port)
{
	bool result = true;
	string::size_type i = ipPort.rfind(':');
	if (i == string::npos)
	{
		ip = ipPort;
	}
	else
	{
		string::size_type j = ipPort[0] == '[' ? ipPort.rfind(']') : string::npos;
		if (j == string::npos || j < i)
		{
			result = false;
			ip = ipPort.substr(0, i);
			auto portLen = ipPort.length() - (i + 1);
			if (portLen <= 5 && portLen)
			{
				uint32_t val = Util::toUInt32(ipPort.c_str() + i + 1);
				if (val && val < 0x10000)
				{
					port = val;
					result = true;
				}
			}
		}
		else
			ip = ipPort;
	}
	if (ip.length() > 2 && ip[0] == '[' && ip.back() == ']')
	{
		ip.erase(0, 1);
		ip.erase(ip.length()-1);
	}
	if (ip.empty()) result = false;
	return result;
}

std::map<string, string> Util::decodeQuery(const string& query)
{
	std::map<string, string> ret;
	size_t start = 0;
	while (start < query.size())
	{
		auto eq = query.find('=', start);
		if (eq == string::npos)
			break;

		auto param = eq + 1;
		auto end = query.find('&', param);

		if (end == string::npos)
			end = query.size();

		if (eq > start && end > param)
			ret[query.substr(start, eq - start)] = query.substr(param, end - param);

		start = end + 1;
	}

	return ret;
}

string Util::getQueryParam(const string& query, const string& key)
{
	string value;
	size_t start = 0;
	while (start < query.size())
	{
		auto eq = query.find('=', start);
		if (eq == string::npos)
			break;

		auto param = eq + 1;
		auto end = query.find('&', param);

		if (end == string::npos)
			end = query.size();

		if (eq > start && end > param && query.substr(start, eq - start) == key)
		{
			value = query.substr(param, end - param);
			break;
		}

		start = end + 1;
	}

	return value;
}
