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
#include "AdcHub.h"
#include "Client.h"
#include "ClientManager.h"
#include "UserCommand.h"
#include "LocationUtil.h"
#include "DatabaseManager.h"
#include "UserConnection.h"
#include "LogManager.h"

#ifdef _DEBUG
#define DISALLOW(a, b) { uint16_t tag1 = TAG(name[0], name[1]); uint16_t tag2 = TAG(a, b); dcassert(tag1 != tag2); }
#else
#define DISALLOW(a, b)
#endif

#ifdef _DEBUG
std::atomic_int User::g_user_counts(0);
std::atomic_int OnlineUser::g_online_user_counts(0);
#endif

User::User(const CID& cid, const string& nick) : cid(cid),
	nick(nick),
	flags(0),
	bytesShared(0),
	limit(0),
	slots(0),
	uploadCount(0)
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	, ipStat(nullptr)
#endif
{
	BOOST_STATIC_ASSERT(LAST_BIT < 32);
#ifdef _DEBUG
	++g_user_counts;
#endif
}

User::~User()
{
	// TODO пока нельзя - вешается flushRatio();
#ifdef _DEBUG
	--g_user_counts;
#endif
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	delete ipStat;
#endif
}

string User::getLastNick() const
{
	LOCK(cs);
	return nick;
}

bool User::hasNick() const
{
	LOCK(cs);
	return !nick.empty();
}

void User::setLastNick(const string& newNick)
{
	dcassert(!newNick.empty());
	LOCK(cs);
	nick = newNick;
}

void User::updateNick(const string& newNick)
{
	dcassert(!newNick.empty());
	LOCK(cs);
	if (nick.empty())
		nick = newNick;
}

void User::setIP(const string& ipStr)
{
	boost::system::error_code ec;
	const auto ip = boost::asio::ip::address_v4::from_string(ipStr, ec);
	dcassert(!ec);
	if (!ec && !ip.is_unspecified())
		setIP(ip);
}

void User::setIP(boost::asio::ip::address_v4 ip)
{
	if (ip.is_unspecified())
		return;
	LOCK(cs);
	if (lastIp == ip)
		return;
	lastIp = ip;
	flags |= LAST_IP_CHANGED;
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	userStat.setIP(ip.to_string());
	if ((userStat.flags & (UserStatItem::FLAG_LOADED | UserStatItem::FLAG_CHANGED)) == (UserStatItem::FLAG_LOADED | UserStatItem::FLAG_CHANGED))
	{
		dcassert(flags & USER_STAT_LOADED);
		flags |= SAVE_USER_STAT;
	}
#endif
}

boost::asio::ip::address_v4 User::getIP() const
{
	LOCK(cs);
	return lastIp;
}

void User::getInfo(string& nick, boost::asio::ip::address_v4& ip, int64_t& bytesShared, int& slots) const
{
	LOCK(cs);
	nick = this->nick;
	ip = lastIp;
	bytesShared = this->bytesShared;
	slots = this->slots;
}

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
unsigned User::getMessageCount() const
{
	LOCK(cs);
	return userStat.messageCount;
}

uint64_t User::getBytesUploaded() const
{
	LOCK(cs);
	return ipStat ? ipStat->totalUploaded : 0;
}

uint64_t User::getBytesDownloaded() const
{
	LOCK(cs);
	return ipStat ? ipStat->totalDownloaded : 0;
}

void User::getBytesTransfered(uint64_t out[]) const
{
	LOCK(cs);
	if (ipStat)
	{
		out[0] = ipStat->totalDownloaded;
		out[1] = ipStat->totalUploaded;
	}
	else
		out[0] = out[1] = 0;
}

void User::addBytesUploaded(boost::asio::ip::address_v4 ip, uint64_t size)
{
	LOCK(cs);
	if (flags & IP_STAT_LOADED)
	{
		if (!ipStat) ipStat = new IPStatMap;
		IPStatItem& item = ipStat->data[ip.to_string()];
		item.upload += size;
		item.flags |= IPStatItem::FLAG_CHANGED;
		ipStat->totalUploaded += size;
		flags |= IP_STAT_CHANGED;
	}
}

void User::addBytesDownloaded(boost::asio::ip::address_v4 ip, uint64_t size)
{
	LOCK(cs);
	if (flags & IP_STAT_LOADED)
	{
		if (!ipStat) ipStat = new IPStatMap;
		IPStatItem& item = ipStat->data[ip.to_string()];
		item.download += size;
		item.flags |= IPStatItem::FLAG_CHANGED;
		ipStat->totalDownloaded += size;
		flags |= IP_STAT_CHANGED;
	}
}

void User::loadIPStatFromDB()
{
	if (!BOOLSETTING(ENABLE_RATIO_USER_LIST))
		return;

	IPStatMap* dbStat = DatabaseManager::getInstance()->loadIPStat(getCID());
	LOCK(cs);
	flags |= IP_STAT_LOADED;
	delete ipStat;
	ipStat = dbStat;
}

void User::loadIPStat()
{
	if (!(getFlags() & IP_STAT_LOADED))
		loadIPStatFromDB();	
}

void User::loadUserStatFromDB()
{
	if (!BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER))
		return;
	
	UserStatItem dbStat;
	DatabaseManager::getInstance()->loadUserStat(getCID(), dbStat);
	LOCK(cs);
	flags |= USER_STAT_LOADED;
	for (const auto& nick : userStat.nickList)
		dbStat.addNick(nick);
	if (!userStat.lastIp.empty())
		dbStat.setIP(userStat.lastIp);
	userStat = std::move(dbStat);
	if (lastIp.is_unspecified())
	{
		boost::system::error_code ec;
		const auto ip = boost::asio::ip::address_v4::from_string(userStat.lastIp, ec);
		if (!ec && !ip.is_unspecified())
		{		
			lastIp = ip;
			flags |= LAST_IP_CHANGED;
		}
	}
}

void User::loadUserStat()
{
	if (!(getFlags() & USER_STAT_LOADED))
		loadUserStatFromDB();
}

void User::saveUserStat()
{
	cs.lock();
	if (!(flags & SAVE_USER_STAT))
	{
		cs.unlock();
		return;
	}
	flags &= ~SAVE_USER_STAT;
	dcassert(flags & USER_STAT_LOADED);
	if (userStat.nickList.empty())
	{
		cs.unlock();
		dcassert(0);
		return;
	}
	userStat.flags &= ~UserStatItem::FLAG_CHANGED;
	UserStatItem dbStat = userStat;
	userStat.flags |= UserStatItem::FLAG_LOADED;
	cs.unlock();
	DatabaseManager::getInstance()->saveUserStat(getCID(), dbStat);
}

void User::saveIPStat()
{
	vector<IPStatVecItem> items;
	bool loadUserStat = false;
	cs.lock();
	if ((flags & (IP_STAT_LOADED | IP_STAT_CHANGED)) != (IP_STAT_LOADED | IP_STAT_CHANGED) || !ipStat)
	{
		cs.unlock();
		return;
	}
	for (auto& i : ipStat->data)
	{
		IPStatItem& item = i.second;
		if (item.flags & IPStatItem::FLAG_CHANGED)
		{
			item.flags &= ~IPStatItem::FLAG_CHANGED;
			items.emplace_back(IPStatVecItem{i.first, item});
			item.flags |= IPStatItem::FLAG_LOADED;
		}
	}
	flags &= ~IP_STAT_CHANGED;
	if (!items.empty() && !(userStat.flags & UserStatItem::FLAG_LOADED))
	{
		flags |= SAVE_USER_STAT;
		if (!(flags & USER_STAT_LOADED)) loadUserStat = true;
	}
	cs.unlock();
	DatabaseManager::getInstance()->saveIPStat(getCID(), items);
	if (loadUserStat) loadUserStatFromDB();
}

void User::incMessageCount()
{
	cs.lock();
	if (!(flags & USER_STAT_LOADED))
	{
		cs.unlock();
		loadUserStatFromDB();
		cs.lock();
	}
	userStat.messageCount++;
	userStat.flags |= UserStatItem::FLAG_CHANGED;
	flags |= SAVE_USER_STAT;
	cs.unlock();
}

void User::saveStats(bool ipStat, bool userStat)
{
	if (ipStat) saveIPStat();
	if (userStat) saveUserStat();
}

bool User::statsChanged() const
{
	LOCK(cs);
	if (flags & SAVE_USER_STAT) return true;
	if ((flags & (IP_STAT_LOADED | IP_STAT_CHANGED)) == (IP_STAT_LOADED | IP_STAT_CHANGED) && ipStat) return true;
	return false;
}

bool User::getLastNickAndHub(string& nick, string& hub) const
{
	LOCK(cs);
	if (userStat.nickList.empty()) return false;
	const string& val = userStat.nickList.back();
	auto pos = val.find('\t');
	if (pos == string::npos) return false;
	nick = val.substr(0, pos);
	hub = val.substr(pos + 1);
	return true;
}
#endif

void User::addNick(const string& nick, const string& hub)
{
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	if (nick.empty() || hub.empty()) return;
	LOCK(cs);
	userStat.addNick(nick, hub);
	if (!(flags & USER_STAT_LOADED))
		userStat.flags &= ~UserStatItem::FLAG_CHANGED;
	else if (userStat.flags & UserStatItem::FLAG_CHANGED)
		flags |= SAVE_USER_STAT;
#endif
}

bool Identity::isTcpActive() const
{
	auto flags = user->getFlags();
	if (flags & User::NMDC)
		return !(flags & User::NMDC_FILES_PASSIVE);
	else
		return !getIp().is_unspecified() && (flags & User::TCP4) != 0;
}

bool Identity::isUdpActive() const
{
	if (getIp().is_unspecified() || !getUdpPort())
		return false;
	else
		return (user->getFlags() & User::UDP4) != 0;
}

void Identity::setExtJSON()
{
	hasExtJson = true;
}

void Identity::getParams(StringMap& sm, const string& prefix, bool compatibility, bool dht) const
{
	{
#define APPEND(cmd, val) sm[prefix + cmd] = val;
#define SKIP_EMPTY(cmd, val) { if (!val.empty()) { APPEND(cmd, val); } }
	
		string cid;
		SKIP_EMPTY("NI", getNick());
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

		SKIP_EMPTY("VE", getStringParam("VE"));
		SKIP_EMPTY("AP", getStringParam("AP"));
		if (compatibility)
		{
			if (cid.empty()) cid = user->getCID().toBase32();
			if (prefix == "my")
			{
				sm["mynick"] = getNick();
				sm["mycid"] = cid;
			}
			else
			{
				sm["nick"] = getNick();
				sm["cid"] = cid;
				sm["ip"] = getIpAsString();
				sm["tag"] = getTag();
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
#undef APPEND
#undef SKIP_EMPTY
	}
	{
		LOCK(cs);
		for (auto i = stringInfo.cbegin(); i != stringInfo.cend(); ++i)
		{
			sm[prefix + string((char*)(&i->first), 2)] = i->second;
		}
	}
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
		_snprintf(tagItem, sizeof(tagItem), ",M:%c,H:%u/%u/%u,S:%u>",
			isTcpActive() ? 'A' : 'P', getHubsNormal(), getHubsRegistered(), getHubsOperator(), getSlots());
		result += tagItem;
		return result;
	}
	return Util::emptyString;
}

string Identity::getApplication() const
{
	LOCK(cs);
	const string& application = getStringParamL("AP");
	const string& version = getStringParamL("VE");
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

const string& Identity::getStringParamL(const char* name) const
{
	CHECK_GET_SET_COMMAND();
	
	uint16_t tag = *reinterpret_cast<const uint16_t*>(name);
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
	CHECK_GET_SET_COMMAND();
	
	uint16_t tag = *reinterpret_cast<const uint16_t*>(name);
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
	CHECK_GET_SET_COMMAND();
	
	uint16_t tag = *reinterpret_cast<const uint16_t*>(name);
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
		auto addr = getIp().to_ulong();
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
	return bytes ? Util::formatBytes(bytes) + " (" + Text::fromT(Util::formatExactSize(bytes)) + ")" : Util::emptyString;
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
		auto appendBoolValue = [&](const string & name, const bool value, const string & iftrue, const string & iffalse)
		{
			report += "\t" + name + ": " + (value ? iftrue : iffalse) + "\r\n";
		};
		
		auto appendStringIfSetBool = [&](const string & str, const bool value)
		{
			if (value)
				report += str + ' ';
		};
		
		auto appendIfValueNotEmpty = [&](const string & name, const string & value)
		{
			if (!value.empty())
				report += "\t" + name + ": " + value + "\r\n";
		};
		
		auto appendIfValueSetInt = [&](const string & name, const unsigned int value)
		{
			if (value)
				appendIfValueNotEmpty(name, Util::toString(value));
		};
		
		auto appendIfValueSetSpeedLimit = [&](const string & name, const unsigned int value)
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
			_snprintf(buf, sizeof(buf), "%u (%u/%u/%u)", countHubs, countNormal, countReg, countOp);
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
		appendIfValueSetInt("Session ID", getSID());
		
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
		
		// Справочные значения заберем через функцию get т.к. в мапе их нет
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

string Identity::getSupports() const // [+] IRainman fix.
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
	if (!ip.is_unspecified())
		return ip.to_string();
	if (isUseIP6())
		return getIP6();
	if (user)
	{
		auto ip = user->getIP();
		if (!ip.is_unspecified())
			return ip.to_string();
	}
	return Util::emptyString;
}

void Identity::setIp(const string& ip) // "I4"
{
	if (ip.empty())
		return;

	boost::system::error_code ec;
	if (ip[0] == ' ' || ip.back() == ' ')
	{
		string temp = ip;
		boost::algorithm::trim(temp);
		this->ip = boost::asio::ip::address_v4::from_string(temp, ec);
	}
	else
	{
		this->ip = boost::asio::ip::address_v4::from_string(ip, ec);
	}
	if (ec)
	{
		dcassert(0);
		return;
	}
	getUser()->setIP(this->ip);
	change(1<<COLUMN_IP);
}

void Identity::setIp(boost::asio::ip::address_v4 ip)
{
	this->ip = ip;
	getUser()->setIP(ip);
	change(1<<COLUMN_IP);
}

bool Identity::isPhantomIP() const
{
	if (!ip.is_unspecified())
		return false;
	if (isUseIP6())
		return false;
	return true;
}

#ifdef IRAINMAN_ENABLE_AUTO_BAN
User::DefinedAutoBanFlags User::hasAutoBan(const Client *client, bool isFavorite)
{
	// Check exclusion first
	bool forceAllow = BOOLSETTING(DONT_BAN_FAVS) && isFavorite;
	if (!forceAllow)
	{
		const string nick = getLastNick();
		forceAllow = !nick.empty() && !UserManager::getInstance()->isInProtectedUserList(nick);
	}
	int ban = BAN_NONE;
	if (!forceAllow)
	{
		const int limit = getLimit();
		const int slots = getSlots();
			
		const int settingBanSlotMax = SETTING(AUTOBAN_SLOTS_MAX);
		const int settingBanSlotMin = SETTING(AUTOBAN_SLOTS_MIN);
		const int settingLimit = SETTING(AUTOBAN_LIMIT);
		const int settingShare = SETTING(AUTOBAN_SHARE);
			
		if (settingBanSlotMin && slots < settingBanSlotMin)
		{
			bool slotsReported = client ? client->slotsReported() : !(getFlags() & NMDC);
			if (slots || slotsReported)
				ban |= BAN_BY_MIN_SLOT;
		}
				
		if (settingBanSlotMax && slots > settingBanSlotMax)
			ban |= BAN_BY_MAX_SLOT;
				
		if (settingShare && static_cast<int>(getBytesShared() / uint64_t(1024 * 1024 * 1024)) < settingShare)
			ban |= BAN_BY_SHARE;
				
		// Skip users with limitation turned off
		if (settingLimit && limit && limit < settingLimit)
			ban |= BAN_BY_LIMIT;
	}
	return static_cast<DefinedAutoBanFlags>(ban);
}
#endif // IRAINMAN_ENABLE_AUTO_BAN
