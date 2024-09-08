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
#include "AdcCommand.h"
#include "StrUtil.h"

AdcCommand::AdcCommand(uint32_t cmd, char type /* = TYPE_CLIENT */) : cmdInt(cmd), from(0), to(0), type(type)
{
	dcassert(cmdChar[3] == 0);
	cmdChar[3] = 0;
}

AdcCommand::AdcCommand(uint32_t cmd, const uint32_t target, char type) : cmdInt(cmd), from(0), to(target), type(type)
{
	dcassert(cmdChar[3] == 0);
	cmdChar[3] = 0;
}

AdcCommand::AdcCommand(Severity sev, Error err, const string& desc, char type /* = TYPE_CLIENT */) : cmdInt(CMD_STA), from(0), to(0), type(type)
{
	addParam((sev == SEV_SUCCESS && err == SUCCESS) ? "000" : Util::toString(sev * 100 + err));
	addParam(desc);
	dcassert(cmdChar[3] == 0);
	cmdChar[3] = 0;
}

int AdcCommand::parse(const char* buf, size_t len, bool nmdc /* = false */) noexcept
{
	size_t i = 5;
	if (nmdc)
	{
		// "$ADCxxx ..."
		if (len < 7)
			return PARSE_ERROR_TOO_SHORT;
		type = TYPE_CLIENT;
		cmdChar[0] = buf[4];
		cmdChar[1] = buf[5];
		cmdChar[2] = buf[6];
		i += 3;
	}
	else
	{
		// "yxxx ..."
		if (len < 4)
			return PARSE_ERROR_TOO_SHORT;
		type = buf[0];
		cmdChar[0] = buf[1];
		cmdChar[1] = buf[2];
		cmdChar[2] = buf[3];
	}

	if (type != TYPE_BROADCAST && type != TYPE_CLIENT && type != TYPE_DIRECT && type != TYPE_ECHO && type != TYPE_FEATURE && type != TYPE_INFO && type != TYPE_HUB && type != TYPE_UDP)
		return PARSE_ERROR_INVALID_TYPE;

	if (type == TYPE_INFO)
		from = HUB_SID;

	string cur;
	cur.reserve(128);

	bool toSet = false;
	bool featureSet = false;
	bool fromSet = nmdc; // $ADCxxx never have a from CID...

	while (i < len)
	{
		switch (buf[i])
		{
			case '\\':
				++i;
				if (i == len)
					return PARSE_ERROR_ESCAPE_AT_EOL;
				if (buf[i] == 's')
					cur += ' ';
				else if (buf[i] == 'n')
					cur += '\n';
				else if (buf[i] == '\\')
					cur += '\\';
				else if (buf[i] == ' ' && nmdc) // $ADCGET escaping, leftover from old specs
					cur += ' ';
				else
					return PARSE_ERROR_ESCAPE_AT_EOL;
				break;
			case ' ': // New parameter...
			{
				if ((type == TYPE_BROADCAST || type == TYPE_DIRECT || type == TYPE_ECHO || type == TYPE_FEATURE) && !fromSet)
				{
					from = toSID(cur);
					if (!from) return PARSE_ERROR_INVALID_SID_LENGTH;
					fromSet = true;
				}
				else if ((type == TYPE_DIRECT || type == TYPE_ECHO) && !toSet)
				{
					to = toSID(cur);
					if (!to) return PARSE_ERROR_INVALID_SID_LENGTH;
					toSet = true;
				}
				else if (type == TYPE_FEATURE && !featureSet)
				{
					if (cur.length() % 5 != 0)
						return PARSE_ERROR_INVALID_FEATURE_LENGTH;
					// Skip...
					featureSet = true;
				}
				else
				{
					parameters.push_back(cur);
				}
				cur.clear();
			}
			break;
			default:
				cur += buf[i];
		}
		++i;
	}
	if (!cur.empty())
	{
		if ((type == TYPE_BROADCAST || type == TYPE_DIRECT || type == TYPE_ECHO || type == TYPE_FEATURE) && !fromSet)
		{
			from = toSID(cur);
			if (!from) return PARSE_ERROR_INVALID_SID_LENGTH;
			fromSet = true;
		}
		else if ((type == TYPE_DIRECT || type == TYPE_ECHO) && !toSet)
		{
			to = toSID(cur);
			if (!to) return PARSE_ERROR_INVALID_SID_LENGTH;
			toSet = true;
		}
		else if (type == TYPE_FEATURE && !featureSet)
		{
			if (cur.length() % 5 != 0)
				return PARSE_ERROR_INVALID_FEATURE_LENGTH;
			// Skip...
			featureSet = true;
		}
		else
		{
			parameters.push_back(cur);
		}
	}

	if ((type == TYPE_BROADCAST || type == TYPE_DIRECT || type == TYPE_ECHO || type == TYPE_FEATURE) && !fromSet)
		return PARSE_ERROR_MISSING_FROM_SID;

	if (type == TYPE_FEATURE && !featureSet)
		return PARSE_ERROR_MISSING_FEATURE;

	if ((type == TYPE_DIRECT || type == TYPE_ECHO) && !toSet)
		return PARSE_ERROR_MISSING_TO_SID;

	return PARSE_OK;
}

string AdcCommand::toString(const CID& cid, bool nmdc /* = false */) const noexcept
{
	return getHeaderString(cid) + getParamString(nmdc);
}

string AdcCommand::toString(uint32_t sid /* = 0 */, bool nmdc /* = false */) const noexcept
{
	return getHeaderString(sid, nmdc) + getParamString(nmdc);
}

string AdcCommand::escape(const string& str, bool old) noexcept
{
	string tmp = str;
	string::size_type i = 0;
	while ((i = tmp.find_first_of(" \n\\", i)) != string::npos)
	{
		if (old)
		{
			tmp.insert(i, "\\");
		}
		else
		{
			switch (tmp[i])
			{
				case ' ':
					tmp.replace(i, 1, "\\s");
					break;
				case '\n':
					tmp.replace(i, 1, "\\n");
					break;
				case '\\':
					tmp.replace(i, 1, "\\\\");
					break;
			}
		}
		i += 2;
	}
	return tmp;
}

string AdcCommand::getHeaderString(uint32_t sid, bool nmdc) const noexcept
{
	string tmp;
	if (nmdc)
		tmp = "$ADC";
	else
		tmp = getType();

	tmp += cmdChar;

	if (type == TYPE_BROADCAST || type == TYPE_DIRECT || type == TYPE_ECHO || type == TYPE_FEATURE)
	{
		tmp += ' ';
		tmp += fromSID(sid);
	}

	if (type == TYPE_DIRECT || type == TYPE_ECHO)
	{
		tmp += ' ';
		tmp += fromSID(to);
	}

	if (type == TYPE_FEATURE)
	{
		tmp += ' ';
		tmp += features;
	}
	return tmp;
}

string AdcCommand::getHeaderString(const CID& cid) const noexcept
{
	dcassert(type == TYPE_UDP);
	string tmp;
	tmp.reserve(44);
	tmp += getType();
	tmp += cmdChar;
	tmp += ' ';
	tmp += cid.toBase32();
	return tmp;
}

string AdcCommand::getParamString(bool nmdc) const noexcept
{
	string tmp;
	tmp.reserve(65);
	for (auto i = parameters.cbegin(); i != parameters.cend(); ++i)
	{
		tmp += ' ';
		tmp += escape(*i, nmdc);
	}
	if (nmdc)
		tmp += '|';
	else
		tmp += '\n';
	return tmp;
}

bool AdcCommand::getParam(uint16_t name, size_t start, string& value) const noexcept
{
	for (string::size_type i = start; i < parameters.size(); ++i)
		if (parameters[i].length() >= 2 && name == toCode(parameters[i].c_str()))
		{
			value = parameters[i].substr(2);
			return true;
		}
	return false;
}

bool AdcCommand::getParam(const char* name, size_t start, string& value) const noexcept
{
	return getParam(toCode(name), start, value);
}

bool AdcCommand::hasFlag(uint16_t name, size_t start) const noexcept
{
	for (string::size_type i = start; i < parameters.size(); ++i)
		if (parameters[i].length() == 3 && name == toCode(parameters[i].c_str()) && parameters[i][2] == '1')
			return true;
	return false;
}

bool AdcCommand::hasFlag(const char* name, size_t start) const noexcept
{
	return hasFlag(toCode(name), start);
}
