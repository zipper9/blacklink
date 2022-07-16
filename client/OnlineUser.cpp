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
#include "ConnectivityManager.h"
#include "Resolver.h"
#include "Tag16.h"

#ifdef _DEBUG
std::atomic_int OnlineUser::onlineUserCount(0);
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
	if (flags & User::TCP4)
	{
		if (Util::isValidIp4(getIP4()))
			return true;
	}
	if (flags & User::TCP6)
	{
		if (Util::isValidIp6(getIP6()))
			return true;
	}
	return false;
}

bool Identity::isUdpActive() const
{
	auto flags = user->getFlags();
	if (flags & User::UDP4)
	{
		if (getUdp4Port() && Util::isValidIp4(getIP4()))
			return true;
	}
	if (flags & User::UDP6) // TODO: check if IPv6 is enabled
	{
		if (getUdp6Port() && Util::isValidIp6(getIP6()))
			return true;
	}
	return false;
}

bool Identity::getUdpAddress(IpAddress& ip, uint16_t& port) const
{
	auto flags = user->getFlags();
	if (flags & User::UDP4)
	{
		port = getUdp4Port();
		ip.type = AF_INET;
		ip.data.v4 = getIP4();
		if (port != 0 && Util::isValidIp4(ip.data.v4))
			return true;
	}
	if ((flags & User::UDP6) && ConnectivityManager::hasIP6())
	{
		port = getUdp6Port();
		ip.type = AF_INET6;
		ip.data.v6 = getIP6();
		if (port != 0 && Util::isValidIp6(ip.data.v6))
			return true;
	}
	ip.type = 0;
	port = 0;
	return false;
}

#if 0 // Prefer IPv6
IpAddress Identity::getConnectIP() const
{
	IpAddress ip;
	ip.data.v6 = getIP6();
	if (ConnectivityManager::hasIP6() && Util::isValidIp6(ip.data.v6))
	{
		ip.type = AF_INET6;
		return ip;
	}
	ip.data.v4 = getIP4();
	if (Util::isValidIp4(ip.data.v4))
	{
		ip.type = AF_INET;
		return ip;
	}
	ip.type = 0;
	return ip;
}
#else // Prefer IPv4
IpAddress Identity::getConnectIP() const
{
	IpAddress ip;
	ip.data.v4 = getIP4();
	if (Util::isValidIp4(ip.data.v4))
	{
		ip.type = AF_INET;
		return ip;
	}
	ip.data.v6 = getIP6();
	if (ConnectivityManager::hasIP6() && Util::isValidIp6(ip.data.v6))
	{
		ip.type = AF_INET6;
		return ip;
	}
	ip.type = 0;
	return ip;
}
#endif

bool Identity::isIPCached(int af) const
{
	switch (af)
	{
		case AF_INET:
			if (!Util::isValidIp4(ip4)) return true;
			break;
		case AF_INET6:
			if (!Util::isValidIp6(ip6)) return true;
			break;
	}
	return false;
}

void Identity::setExtJSON()
{
	hasExtJson = true;
}

string Identity::getSIDString(uint32_t sid)
{
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
		sm["I4"] = Util::printIpAddress(getIP4());
		sm["U4"] = Util::toString(getUdp4Port());
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
			sm["ip"] = Util::printIpAddress(getConnectIP());
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
	cs.lock();
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
		cs.unlock();
		snprintf(tagItem, sizeof(tagItem), ",M:%c,H:%u/%u/%u,S:%u>",
			isTcpActive() ? 'A' : 'P', getHubsNormal(), getHubsRegistered(), getHubsOperator(), getSlots());
		result += tagItem;
		return result;
	}
	cs.unlock();
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
	dcassert(!info.getIdentity().getNick().empty() || info.getClientBase()->getHubUrl().empty());
	nick = info.getIdentity().getNick();
	url = info.getClientBase()->getHubUrl();
}

void Identity::loadP2PGuard()
{
	if (!p2pGuardInfoKnown)
	{
		Ip4Address addr = getIP4();
		if (addr)
		{
			IpAddress ip;
			ip.data.v4 = addr;
			ip.type = AF_INET;
			IPInfo ipInfo;
			Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_P2P_GUARD);
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

string Identity::formatIpString(const IpAddress& ip)
{
	if (Util::isValidIp(ip))
	{
		string desc;
		string hostname = Resolver::getHostName(ip);
		IPInfo loc;
		Util::getIpInfo(ip, loc, IPInfo::FLAG_COUNTRY | IPInfo::FLAG_LOCATION);
		const string& location = Util::getDescription(loc);
		if (!hostname.empty() && !location.empty())
			desc = hostname + " - " + location;
		else if (!hostname.empty())
			desc = std::move(hostname);
		else if (!location.empty())
			desc = std::move(location);
		if (desc.empty()) return Util::printIpAddress(ip);
		return Util::printIpAddress(ip) + " (" + desc + ')';
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

		string keyPrint;
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

		auto clientType = getClientType();
		report += "\tClient type: ";
		appendStringIfSetBool("Hub", (clientType & CT_HUB) != 0);
		appendStringIfSetBool("Bot", (clientType & CT_BOT) != 0);
		appendStringIfSetBool("Registered", (clientType & CT_REGGED) != 0);
		appendStringIfSetBool("Operator", (clientType & CT_OP) != 0);
		appendStringIfSetBool("Superuser", (clientType & CT_SU) != 0);
		appendStringIfSetBool("Owner", (clientType & CT_OWNER) != 0);
		report += '(' + Util::toString(clientType) + ")\r\n";

		auto statusFlags = getStatus();
		report += "\tStatus: ";
		appendStringIfSetBool("Away", (statusFlags & SF_AWAY) != 0);
		appendStringIfSetBool("Server", (statusFlags & SF_SERVER) != 0);
		appendStringIfSetBool("Fireball", (statusFlags & SF_FIREBALL) != 0);
		appendStringIfSetBool("TLS", (flags & User::TLS) != 0);
		appendStringIfSetBool("NAT-T", (flags & User::NAT0) != 0);
		report += '(' + Util::toString(statusFlags) + ")\r\n";

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
			appendBoolValue("Files mode", (flags & User::NMDC_FILES_PASSIVE) != 0, "Passive", "Active");
			appendBoolValue("Search mode", (flags & User::NMDC_SEARCH_PASSIVE) != 0, "Passive", "Active");
		}
		if (flags & User::DHT)
			appendBoolValue("DHT mode", (flags & User::PASSIVE) != 0, "Passive", "Active");
		appendIfValueNotEmpty("Known supports", getSupports());

		IpAddress ip;
		ip.data.v4 = getIP4();
		if (ip.data.v4)
		{
			ip.type = AF_INET;
			appendIfValueNotEmpty("IPv4 address", formatIpString(ip));
		}
		ip.data.v6 = getIP6();
		if (!Util::isEmpty(ip.data.v6))
		{
			ip.type = AF_INET6;
			appendIfValueNotEmpty("IPv6 address", formatIpString(ip));
		}

		// "AP" and "VE" are not stored in stringInfo
		appendIfValueNotEmpty("DC client", getStringParam("AP"));
		appendIfValueNotEmpty("Client version", getStringParam("VE"));

		appendIfValueNotEmpty("Certificate fingerprint", keyPrint);

		appendIfValueNotEmpty("P2P Guard", getP2PGuard());
		appendIfValueNotEmpty("Support info", getExtJSONSupportInfo());
		appendIfValueNotEmpty("Gender", Text::fromT(getGenderTypeAsString()));

		appendIfValueNotEmpty("Count files", getExtJSONCountFilesAsText());
		appendIfValueNotEmpty("Last share", getExtJSONLastSharedDateAsText());
		appendIfValueNotEmpty("SQLite DB size", getExtJSONSQLiteDBSizeAsText());
		appendIfValueNotEmpty("Queue info", getExtJSONQueueFilesText());
		appendIfValueNotEmpty("Start/stop core", getExtJSONTimesStartCoreText());
#ifdef BL_FEATURE_IP_DATABASE
		uint64_t bytes[2];
		user->getBytesTransfered(bytes);
		if (bytes[0] + bytes[1])
		{
			appendIfValueNotEmpty("Downloaded", formatShareBytes(bytes[0]));
			appendIfValueNotEmpty("Uploaded", formatShareBytes(bytes[1]));
		}
		appendIfValueNotEmpty("Message count", Util::toString(user->getMessageCount()));
#endif
		appendBoolValue("Favorite", (flags & User::FAVORITE) != 0, "yes", "no");
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

void Identity::setIP4(Ip4Address ip) noexcept
{
	this->ip4 = ip;
	getUser()->setIP4(ip);
	change(1<<COLUMN_IP);
}

Ip4Address Identity::getIP4() const noexcept
{
	if (ip4) return ip4;
	return getUser()->getIP4();
}

void Identity::setIP6(const Ip6Address& ip) noexcept
{
	this->ip6 = ip;
	getUser()->setIP6(ip);
	change(1<<COLUMN_IP);
}

Ip6Address Identity::getIP6() const noexcept
{
	{
		LOCK(cs);
		if (!Util::isEmpty(ip6)) return ip6;
	}
	return getUser()->getIP6();
}

tstring Identity::getGenderTypeAsString(int index) const
{
	switch (index)
	{
		case 1:
			return TSTRING(FLY_GENDER_NONE);
		case 2:
			return TSTRING(FLY_GENDER_MALE);
		case 3:
			return TSTRING(FLY_GENDER_FEMALE);
		case 4:
			return TSTRING(FLY_GENDER_ASEXUAL);
	}
	return Util::emptyStringT;
}

string Identity::getExtJSONHubRamAsText() const
{
	string result;
	if (hasExtJson)
	{
		if (getExtJSONRAMWorkingSet())
			result = Util::formatBytes(int64_t(getExtJSONRAMWorkingSet()) << 20);
		if (getExtJSONRAMPeakWorkingSet() != getExtJSONRAMWorkingSet())
			result += " [Max: " + Util::formatBytes(int64_t(getExtJSONRAMPeakWorkingSet()) << 20) + "]";
		if (getExtJSONRAMFree())
			result += " [Free: " + Util::formatBytes(int64_t(getExtJSONRAMFree()) >> 20) + "]";
	}
	return result;
}

string Identity::getExtJSONCountFilesAsText() const
{
	if (hasExtJson && getExtJSONCountFiles())
		return Util::toString(getExtJSONCountFiles());
	return Util::emptyString;
}

string Identity::getExtJSONLastSharedDateAsText() const
{
	if (hasExtJson && getExtJSONLastSharedDate())
		return Util::formatTime(getExtJSONLastSharedDate());
	return Util::emptyString;
}

string Identity::getExtJSONSQLiteDBSizeAsText() const
{
	string result;
	if (hasExtJson)
	{
		if (getExtJSONSQLiteDBSize())
			result = Util::formatBytes(int64_t(getExtJSONSQLiteDBSize()) << 20);
		if (getExtJSONSQLiteDBSizeFree())
			result += " [Free: " + Util::formatBytes(int64_t(getExtJSONSQLiteDBSizeFree()) << 20) + "]";
		if (getExtJSONlevelDBHistSize())
			result += " [LevelDB: " + Util::formatBytes(int64_t(getExtJSONlevelDBHistSize()) << 20) + "]";
	}
	return result;
}

string Identity::getExtJSONQueueFilesText() const
{
	string result;
	if (hasExtJson)
	{
		if (getExtJSONQueueFiles())
			result = "[Files: " + Util::toString(getExtJSONQueueFiles()) + "]";
		if (getExtJSONQueueSrc())
			result += " [Sources: " + Util::toString(getExtJSONQueueSrc()) + "]";
	}
	return result;
}

string Identity::getExtJSONTimesStartCoreText() const
{
	string result;
	if (hasExtJson)
	{
		if (getExtJSONTimesStartCore())
			result = "[Start core: " + Util::toString(getExtJSONTimesStartCore()) + "]";
		if (getExtJSONTimesStartGUI())
			result += " [Start GUI: " + Util::toString(getExtJSONTimesStartGUI()) + "]";
	}
	return result;
}
