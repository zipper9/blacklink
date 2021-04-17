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
#include "OnlineUser.h"
#include "Client.h"
#include "ClientManager.h"
#include "LocationUtil.h"
#include "UserConnection.h"

#ifdef _DEBUG
std::atomic_int OnlineUser::g_online_user_counts(0);
#endif

#ifdef _DEBUG
#define DISALLOW(a, b) { uint16_t tag2 = TAG(a, b); dcassert(tag != tag2); }
#else
#define DISALLOW(a, b)
#endif

bool Identity::isTcpActive() const
{
	auto flags = user->getFlags();
	if (flags & User::NMDC)
		return !(flags & User::NMDC_FILES_PASSIVE);
	else
		return Util::isValidIp4(getIp()) && (flags & User::TCP4) != 0;
}

bool Identity::isUdpActive() const
{
	if (!Util::isValidIp4(getIp()) || !getUdpPort())
		return false;
	else
		return (user->getFlags() & User::UDP4) != 0;
}

void Identity::setExtJSON()
{
	hasExtJson = true;
}

string Identity::getSIDString() const
{
	uint32_t sid = getSID();
	union
	{
		uint32_t val;
		char str[4];
	} u;
	u.val = sid;
	bool ok = true;
	for (int i = 0; i < 4; ++i)
	{
		char c = u.str[i];
		if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')))
		{
			ok = false;
			break;
		}
	}
	return ok ? string(u.str, 4) : Util::toString(sid);
}

void Identity::getParams(StringMap& sm, const string& prefix, bool compatibility, bool dht) const
{
#define APPEND(cmd, val) sm[prefix + cmd] = val;
#define SKIP_EMPTY(cmd, val) { if (!val.empty()) { APPEND(cmd, val); } }

	string cid;
	string nick = getNick();
	SKIP_EMPTY("NI", nick);
	if (!dht)
	{
		cid = user->getCID().toBase32();
		SKIP_EMPTY("SID", getSIDString());
		APPEND("CID", cid);
		APPEND("SSshort", Util::formatBytes(getBytesShared()));
		SKIP_EMPTY("SU", getSupports());
	}
	else
	{
		sm["I4"] = getIpAsString();
		sm["U4"] = Util::toString(getUdpPort());
	}

	string tag = getTag();
	SKIP_EMPTY("TAG", tag);
	if (compatibility)
	{
		if (cid.empty()) cid = user->getCID().toBase32();
		if (prefix == "my")
		{
			sm["mynick"] = nick;
			sm["mycid"] = cid;
		}
		else
		{
			sm["nick"] = nick;
			sm["cid"] = cid;
			sm["ip"] = getIpAsString();
			sm["tag"] = tag;
			sm["description"] = getDescription();
			sm["email"] = getEmail();
			sm["share"] = Util::toString(getBytesShared());
			const auto share = Util::formatBytes(getBytesShared());
			sm["shareshort"] = share;
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
			sm["realshareformat"] = Util::formatBytes(getRealBytesShared());
#else
			sm["realshareformat"] = share;
#endif
		}
	}
	{
		LOCK(cs);
		SKIP_EMPTY("VE", getStringParamL(TAG('V', 'E')));
		SKIP_EMPTY("AP", getStringParamL(TAG('A', 'P')));
		for (auto i = stringInfo.cbegin(); i != stringInfo.cend(); ++i)
		{
			sm[prefix + string((char*)(&i->first), 2)] = i->second;
		}
	}
#undef APPEND
#undef SKIP_EMPTY
}

string Identity::getTag() const
{
	LOCK(cs);
	auto itAP = stringInfo.find(TAG('A', 'P'));
	auto itVE = stringInfo.find(TAG('V', 'E'));
	if (itAP != stringInfo.cend() || itVE != stringInfo.cend())
	{
		string result;
		char tagItem[128];
		if (itAP != stringInfo.cend())
		{
			result = '<' + itAP->second + " V:";
			// TODO: check if "V:" followed by empty string is OK
			if (itVE != stringInfo.cend()) result += itVE->second;
		}
		else
		{
			result = '<' + itVE->second;
		}
		snprintf(tagItem, sizeof(tagItem), ",M:%c,H:%u/%u/%u,S:%u>",
			isTcpActive() ? 'A' : 'P', getHubsNormal(), getHubsRegistered(), getHubsOperator(), getSlots());
		result += tagItem;
		return result;
	}
	return Util::emptyString;
}

string Identity::getApplication() const
{
	LOCK(cs);
	const string& application = getStringParamL(TAG('A', 'P'));
	const string& version = getStringParamL(TAG('V', 'E'));
	if (version.empty())
	{
		return application;
	}
	if (application.empty())
	{
		// AP is an extension, so we can't guarantee that the other party supports it, so default to VE.
		return version;
	}
	return application + ' ' + version;
}

#define ENABLE_CHECK_GET_SET_IN_IDENTITY
#ifdef ENABLE_CHECK_GET_SET_IN_IDENTITY
# define CHECK_GET_SET_COMMAND()\
	DISALLOW('T','A');\
	DISALLOW('C','L');\
	DISALLOW('N','I');\
	DISALLOW('A','W');\
	DISALLOW('B','O');\
	DISALLOW('S','U');\
	DISALLOW('U','4');\
	DISALLOW('U','6');\
	DISALLOW('I','4');\
	DISALLOW('S','S');\
	DISALLOW('C','T');\
	DISALLOW('S','I');\
	DISALLOW('U','S');\
	DISALLOW('S','L');\
	DISALLOW('H','N');\
	DISALLOW('H','R');\
	DISALLOW('H','O');\
	DISALLOW('F','C');\
	DISALLOW('S','T');\
	DISALLOW('R','S');\
	DISALLOW('S','S');\
	DISALLOW('F','D');\
	DISALLOW('F','S');\
	DISALLOW('S','F');\
	DISALLOW('R','S');\
	DISALLOW('C','M');\
	DISALLOW('D','S');\
	DISALLOW('I','D');\
	DISALLOW('L','O');\
	DISALLOW('P','K');\
	DISALLOW('O','P');\
	DISALLOW('U','4');\
	DISALLOW('U','6');\
	DISALLOW('C','O');\
	DISALLOW('W','O');

#else
# define CHECK_GET_SET_COMMAND()
#endif // ENABLE_CHECK_GET_SET_IN_IDENTITY

const string& Identity::getStringParamL(uint16_t tag) const
{
	CHECK_GET_SET_COMMAND();

	switch (tag)
	{
		case TAG('E', 'M'):
		{
			if (!getNotEmptyStringBit(EM))
				return Util::emptyString;
			break;
		}
		case TAG('D', 'E'):
		{
			if (!getNotEmptyStringBit(DE))
				return Util::emptyString;
			break;
		}
	}

	const auto i = stringInfo.find(tag);
	if (i != stringInfo.end())
		return i->second;
	return Util::emptyString;
}

string Identity::getStringParam(const char* name) const
{
	uint16_t tag = *reinterpret_cast<const uint16_t*>(name);
	CHECK_GET_SET_COMMAND();

	switch (tag)
	{
		case TAG('E', 'M'):
		{
			if (!getNotEmptyStringBit(EM))
				return Util::emptyString;
			break;
		}
		case TAG('D', 'E'):
		{
			if (!getNotEmptyStringBit(DE))
				return Util::emptyString;
			break;
		}
	}
	{
		LOCK(cs);
		const auto i = stringInfo.find(tag);
		if (i != stringInfo.end())
			return i->second;
	}
	return Util::emptyString;
}

void Identity::setStringParam(const char* name, const string& val)
{
	uint16_t tag = *reinterpret_cast<const uint16_t*>(name);
	CHECK_GET_SET_COMMAND();

	switch (tag)
	{
		case TAG('E', 'M'):
		{
			setNotEmptyStringBit(EM, !val.empty());
			break;
		}
		case TAG('D', 'E'):
		{
			setNotEmptyStringBit(DE, !val.empty());
			break;
		}
	}

	LOCK(cs);
	if (val.empty())
		stringInfo.erase(tag);
	else
		stringInfo[tag] = val;
}

void FavoriteUser::update(const OnlineUser& info)
{
	dcassert(!info.getIdentity().getNick().empty() || info.getClient().getHubUrl().empty());
	nick = info.getIdentity().getNick();
	url = info.getClient().getHubUrl();
}

void Identity::loadP2PGuard()
{
	if (!p2pGuardInfoKnown)
	{
		auto addr = getIp();
		if (addr)
		{
			IPInfo ipInfo;
			Util::getIpInfo(addr, ipInfo, IPInfo::FLAG_P2P_GUARD);
			setP2PGuard(ipInfo.p2pGuard);
			p2pGuardInfoKnown = true;
		}
	}
}

#ifdef FLYLINKDC_USE_DETECT_CHEATING
string Identity::setCheat(const ClientBase& c, const string& aCheatDescription, bool aBadClient)
{
	if (!c.isOp() || isOp())
	{
		return Util::emptyString;
	}

	PLAY_SOUND(SOUND_FAKERFILE);

	StringMap ucParams;
	getParams(ucParams, "user", true);
	string cheat = Util::formatParams(aCheatDescription, ucParams, false);

	setStringParam("CS", cheat);
	setFakeCardBit(BAD_CLIENT, aBadClient);

	string report = "*** " + STRING(USER) + ' ' + getNick() + " - " + cheat;
	return report;
}
#endif

string Identity::formatShareBytes(uint64_t bytes)
{
	return bytes ? Util::formatBytes(bytes) + " (" + Util::formatExactSize(bytes) + ")" : Util::emptyString;
}

string Identity::formatIpString(const string& value)
{
	if (!value.empty())
	{
		string desc;
		string hostname = Socket::getRemoteHost(value);
		IPInfo loc;
		Util::getIpInfo(value, loc, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
		const string& location = Util::getDescription(loc);
		if (!hostname.empty() && !location.empty())
			desc = hostname + " - " + location;
		else if (!hostname.empty())
			desc = std::move(hostname);
		else if (!location.empty())
			desc = std::move(location);
		if (desc.empty()) return value;
		return value + " (" + desc + ')';
	}
	return Util::emptyString;
};

string Identity::formatSpeedLimit(const uint32_t limit)
{
	return limit ? Util::formatBytes(limit) + '/' + STRING(S) : Util::emptyString;
}

void Identity::getReport(string& report)
{
	user->loadIPStat();
	user->loadUserStat();
	report = " *** User info ***\r\n";
	const string sid = getSIDString();
	{
		auto appendBoolValue = [&](const string& name, bool value, const string& iftrue, const string& iffalse)
		{
			report += "\t" + name + ": " + (value ? iftrue : iffalse) + "\r\n";
		};

		auto appendStringIfSetBool = [&](const string& str, bool value)
		{
			if (value)
				report += str + ' ';
		};

		auto appendIfValueNotEmpty = [&](const string& name, const string& value)
		{
			if (!value.empty())
				report += "\t" + name + ": " + value + "\r\n";
		};

		auto appendIfValueSetInt = [&](const string& name, unsigned int value)
		{
			if (value)
				appendIfValueNotEmpty(name, Util::toString(value));
		};

		auto appendIfValueSetSpeedLimit = [&](const string& name, unsigned int value)
		{
			if (value)
				appendIfValueNotEmpty(name, formatSpeedLimit(value));
		};

		// TODO: translate
		auto flags = user->getFlags();
		bool isNmdc = (flags & User::NMDC) != 0;

		string nick = getNick();
		appendIfValueNotEmpty(STRING(NICK), nick);
		if (!isNmdc)
		{
			auto nicks = ClientManager::getNicks(user->getCID(), Util::emptyString);
			string otherNicks;
			for (const string& otherNick : nicks)
				if (otherNick != nick)
				{
					if (!otherNicks.empty()) otherNicks += ", ";
					otherNicks += otherNick;
				}
			appendIfValueNotEmpty("Other nicks", otherNicks);
		}

		string ipv6, keyPrint;
		{
			LOCK(cs);
			for (auto i = stringInfo.cbegin(); i != stringInfo.cend(); ++i)
			{
				auto name = string((char*)(&i->first), 2);
				const auto& value = i->second;
				// TODO: translate known tags and format values to something more readable
				bool append = true;
				switch (i->first)
				{
					case TAG('C', 'S'):
						name = "Cheat description";
						break;
					case TAG('D', 'E'):
						name = STRING(DESCRIPTION);
						break;
					case TAG('E', 'M'):
						name = STRING(EMAIL);
						break;
					case TAG('K', 'P'):
						keyPrint = value;
						append = false;
						break;
					case TAG('L', 'C'):
						name = STRING(LOCALE);
						break;
					case TAG('I', '6'):
						ipv6 = value;
						append = false;
						break;
					case TAG('V', 'E'):
					case TAG('A', 'P'):
					case TAG('F', '1'):
					case TAG('F', '2'):
					case TAG('F', '3'):
					case TAG('F', '4'):
					case TAG('F', '5'):
						append = false;
						break;
					default:
						name += " (unknown)";
				}
				if (append)
					appendIfValueNotEmpty(name, value);
			}
		}

		unsigned countNormal = getHubsNormal();
		unsigned countReg = getHubsRegistered();
		unsigned countOp = getHubsOperator();
		unsigned countHubs = countNormal + countReg + countOp;	
		if (countHubs)
		{
			char buf[64];
			snprintf(buf, sizeof(buf), "%u (%u/%u/%u)", countHubs, countNormal, countReg, countOp);
			appendIfValueNotEmpty(STRING(HUBS), buf);
		}
		if (!isNmdc)
		{
			appendIfValueNotEmpty("Hub names", Util::toString(ClientManager::getHubNames(user->getCID(), Util::emptyString)));
			appendIfValueNotEmpty("Hub addresses", Util::toString(ClientManager::getHubs(user->getCID(), Util::emptyString)));
		}

		report += "\tClient type: ";
		appendStringIfSetBool("Hub", isHub());
		appendStringIfSetBool("Bot", isBot());
		appendStringIfSetBool(STRING(OPERATOR), isOp());
		report += '(' + Util::toString(getClientType()) + ") ";
		appendStringIfSetBool(STRING(AWAY), (flags & User::AWAY) != 0);
		appendStringIfSetBool(STRING(SERVER), (flags & User::SERVER) != 0);
		appendStringIfSetBool(STRING(FIREBALL), (flags & User::FIREBALL) != 0);
		report += "\r\n";

		appendIfValueNotEmpty("Client ID", user->getCID().toBase32());
		if (getSID()) appendIfValueNotEmpty("Session ID", getSIDString());

		appendIfValueSetInt(STRING(SLOTS), getSlots());
		appendIfValueSetSpeedLimit(STRING(AVERAGE_UPLOAD), getLimit());
		appendIfValueSetSpeedLimit(isNmdc ? STRING(CONNECTION) : "Download speed", getDownloadSpeed());

		appendIfValueSetInt(STRING(SHARED_FILES), getSharedFiles());
		appendIfValueNotEmpty(STRING(SHARED) + " - reported", formatShareBytes(getBytesShared()));
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		appendIfValueNotEmpty(STRING(SHARED) + " - real", formatShareBytes(getRealBytesShared()));
#endif
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		appendIfValueSetInt("Fake check card", getFakeCard());
#endif
		appendIfValueSetInt("Connection Timeouts", getConnectionTimeouts());
		appendIfValueSetInt("Filelist disconnects", getFileListDisconnects());

		if (isNmdc)
		{
			appendIfValueNotEmpty("NMDC status", NmdcSupports::getStatus(*this));
			appendBoolValue("Files mode", (flags & User::NMDC_FILES_PASSIVE) != 0, "Passive", "Active");
			appendBoolValue("Search mode", (flags & User::NMDC_SEARCH_PASSIVE) != 0, "Passive", "Active");
		}
		if (flags & User::DHT)
			appendBoolValue("DHT mode", (flags & User::PASSIVE) != 0, "Passive", "Active");
		appendIfValueNotEmpty("Known supports", getSupports());

		appendIfValueNotEmpty("IPv4 address", formatIpString(getIpAsString()));
		appendIfValueNotEmpty("IPv6 address", ipv6);

		// "AP" and "VE" are not stored in stringInfo
		appendIfValueNotEmpty("DC client", getStringParam("AP"));
		appendIfValueNotEmpty("Client version", getStringParam("VE"));

		appendIfValueNotEmpty("Public key fingerprint", keyPrint);

		appendIfValueNotEmpty("P2P Guard", getP2PGuard());
		appendIfValueNotEmpty("Support info", getExtJSONSupportInfo());
		appendIfValueNotEmpty("Gender", Text::fromT(getGenderTypeAsString()));

		appendIfValueNotEmpty("Count files", getExtJSONCountFilesAsText());
		appendIfValueNotEmpty("Last share", getExtJSONLastSharedDateAsText());
		appendIfValueNotEmpty("SQLite DB size", getExtJSONSQLiteDBSizeAsText());
		appendIfValueNotEmpty("Queue info", getExtJSONQueueFilesText());
		appendIfValueNotEmpty("Start/stop core", getExtJSONTimesStartCoreText());
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		uint64_t bytes[2];
		user->getBytesTransfered(bytes);
		if (bytes[0] + bytes[1])
		{
			appendIfValueNotEmpty("Downloaded", formatShareBytes(bytes[0]));
			appendIfValueNotEmpty("Uploaded", formatShareBytes(bytes[1]));
		}
		appendIfValueNotEmpty("Message count", Util::toString(user->getMessageCount()));
#endif
	}
}

string Identity::getSupports() const
{
	string tmp = UcSupports::getSupports(*this);
	/*
	if (getUser()->isNMDC())
	{
	    tmp += NmdcSupports::getSupports(*this);
	}
	else
	*/
	{
		tmp += AdcSupports::getSupports(*this);
	}
	return tmp;
}

string Identity::getIpAsString() const
{
	if (Util::isValidIp4(ip))
		return Util::printIpAddress(ip);
	if (isUseIP6())
		return getIP6();
	if (user)
	{
		auto ip = user->getIP();
		if (Util::isValidIp4(ip))
			return Util::printIpAddress(ip);
	}
	return Util::emptyString;
}

void Identity::setIp(const string& ip) // "I4"
{
	if (ip.empty())
		return;

	Ip4Address addr;
	if (ip[0] == ' ' || ip.back() == ' ')
	{
		string temp = ip;
		boost::algorithm::trim(temp);
		if (!(Util::parseIpAddress(addr, temp) && Util::isValidIp4(addr))) return;
	}
	else
	{
		if (!(Util::parseIpAddress(addr, ip) && Util::isValidIp4(addr))) return;
	}
	this->ip = addr;
	getUser()->setIP(addr);
	change(1<<COLUMN_IP);
}

void Identity::setIp(Ip4Address ip)
{
	this->ip = ip;
	getUser()->setIP(ip);
	change(1<<COLUMN_IP);
}

bool Identity::isPhantomIP() const
{
	if (Util::isValidIp4(ip))
		return false;
	if (isUseIP6())
		return false;
	return true;
}
