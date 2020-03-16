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
#include "CFlylinkDBManager.h"
#include "UserConnection.h"
#include "LogManager.h"

#ifdef _DEBUG
#define DISALLOW(a, b) { uint16_t tag1 = TAG(name[0], name[1]); uint16_t tag2 = TAG(a, b); dcassert(tag1 != tag2); }
#else
#define DISALLOW(a, b)
#endif

#ifdef _DEBUG
boost::atomic_int User::g_user_counts(0);
boost::atomic_int OnlineUser::g_online_user_counts(0);
#endif

User::User(const CID& cid, const string& nick
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	, uint32_t hubId
#endif
	) : cid(cid),
	nick(nick),
	flags(0),
	bytesShared(0),
	limit(0),
	slots(0)
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	, hubId(hubId)
	, ratioPtr(nullptr)
	, messageCount(0)
#endif
{
	BOOST_STATIC_ASSERT(LAST_BIT < 32);
#ifdef _DEBUG
	++g_user_counts;
# ifdef ENABLE_DEBUG_LOG_IN_USER_CLASS
	dcdebug(" [!!!!!!]   [!!!!!!]  User::User(const CID& aCID) this = %p, g_user_counts = %d\n", this, g_user_counts);
# endif
#endif
}

User::~User()
{
	// TODO пока нельзя - вешается flushRatio();
#ifdef _DEBUG
	--g_user_counts;
# ifdef ENABLE_DEBUG_LOG_IN_USER_CLASS
	dcdebug(" [!!!!!!]   [!!!!!!]  User::~User() this = %p, g_user_counts = %d\n", this, g_user_counts);
# endif
#endif
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	delete ratioPtr;
#endif
}

string User::getLastNick() const
{
	CFlyFastLock(cs);
	return nick;
}

void User::setLastNick(const string& newNick)
{
	CFlyFastLock(cs);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	if (ratioPtr == nullptr)
	{
		nick = newNick;
	}
	else
	{
		if (nick != newNick)
		{
			if (!nick.empty() && !newNick.empty() && ratioPtr)
			{
				delete ratioPtr;
				ratioPtr = nullptr;
				flags &= ~RATIO_LOADED;
			}
			nick = newNick;
		}
	}
#else
	nick = newNick;
#endif
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
	CFlyFastLock(cs);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	if (!lastIp.set(ip))
		return;
	delete ratioPtr;
	ratioPtr = nullptr;
	flags &= ~(LAST_IP_LOADED | RATIO_LOADED);
#else
	lastIp = ip;
#endif
}

boost::asio::ip::address_v4 User::getIP() const
{
	CFlyFastLock(cs);
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	return lastIp.get();
#else
	return lastIp;
#endif
}

void User::getInfo(string& nick, boost::asio::ip::address_v4& ip, int64_t& bytesShared, int& slots) const
{
	CFlyFastLock(cs);
	nick = this->nick;
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	ip = lastIp.get();
#else
	ip = lastIp;
#endif
	bytesShared = this->bytesShared;
	slots = this->slots;
}

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
uint64_t User::getMessageCount() const
{
	CFlyFastLock(cs);
	return messageCount.get();
}

uint64_t User::getBytesUploaded() const
{
	CFlyFastLock(cs);
	return ratioPtr ? ratioPtr->get_upload() : 0;
}

uint64_t User::getBytesDownloaded() const
{
	CFlyFastLock(cs);
	return ratioPtr ? ratioPtr->get_download() : 0;
}

void User::getBytesTransfered(uint64_t out[]) const
{
	CFlyFastLock(cs);
	if (ratioPtr)
	{
		out[0] = ratioPtr->get_download();
		out[1] = ratioPtr->get_upload();
	}
	else
		out[0] = out[1] = 0;
}

void User::addBytesUploaded(boost::asio::ip::address_v4 ip, uint64_t size)
{
	CFlyFastLock(cs);
	if (ratioPtr)
		ratioPtr->addUpload(ip, size);
}

void User::addBytesDownloaded(boost::asio::ip::address_v4 ip, uint64_t size)
{
	CFlyFastLock(cs);
	if (ratioPtr)
		ratioPtr->addDownload(ip, size);
}

bool User::isDirty(bool enableMessageCounter) const
{
	CFlyFastLock(cs)
	if (enableMessageCounter && messageCount.get() && (messageCount.is_dirty() || lastIp.is_dirty()))
		return true;
	if (ratioPtr)
		return ratioPtr->is_dirty();
	return false;
}

bool User::flushRatio()
{
	bool result = false;
	string currentNick;
	uint32_t currentHubID;
	bool lastIpChanged;
	bool messageCountChanged;
	boost::asio::ip::address_v4 lastIpVal;
	uint32_t messageCountVal;
	CFlyUserRatioInfo* tempRatio = nullptr;
	MaskType userFlags;
	{
		CFlyFastLock(cs);
		if (nick.empty() || !hubId) return false;
		lastIpChanged = lastIp.is_dirty();
		messageCountChanged = messageCount.is_dirty();
		bool messageInfoChanged = (messageCountChanged || lastIpChanged) && messageCount.get();
		if (ratioPtr)
		{
			if (!(ratioPtr->is_dirty() || messageInfoChanged))
				return false;
			tempRatio = new CFlyUserRatioInfo(*ratioPtr);
		}
		else
		{
			if (!messageInfoChanged)
				return false;
		}
		currentNick = nick;
		currentHubID = hubId;
		userFlags = flags;
		lastIpVal = lastIp.get();
		messageCountVal = messageCount.get();
		lastIp.reset_dirty();
		messageCount.reset_dirty();
	}

	bool sqlNotFound = (userFlags & LAST_IP_NOT_IN_DB) != 0;
	if (tempRatio)
	{
		CFlylinkDBManager::getInstance()->store_all_ratio_and_last_ip(currentHubID, currentNick, *tempRatio,
			messageCountVal, lastIpVal, lastIpChanged, messageCountChanged, sqlNotFound);
		delete tempRatio;
	}
	else
	{
		CFlylinkDBManager::getInstance()->update_last_ip_and_message_count(currentHubID, currentNick,
			lastIpVal, messageCountVal, sqlNotFound, lastIpChanged, messageCountChanged);
	}
	if (!sqlNotFound)
	{
		CFlyFastLock(cs);
		flags &= ~LAST_IP_NOT_IN_DB;
	}
	return result;
}

bool User::loadRatio()
{
	if (!BOOLSETTING(ENABLE_RATIO_USER_LIST))
		return false;

	string currentNick;
	uint32_t currentHubID;
	{
		CFlyFastLock(cs);
		if (nick.empty() || !hubId) return false;
		if (flags & RATIO_LOADED) return true;
		currentNick = nick;
		currentHubID = hubId;
		flags |= RATIO_LOADED;
	}

	CFlyUserRatioInfo* tempRatio = new CFlyUserRatioInfo;
	CFlylinkDBManager::getInstance()->load_ratio(currentHubID, currentNick, *tempRatio);

	{
		CFlyFastLock(cs);
		std::swap(ratioPtr, tempRatio);
	}

	delete tempRatio;
	return true;
}

bool User::loadIpAndMessageCount()
{
	if (!BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER))
		return false;
	
	string currentNick;
	uint32_t currentHubID;
	{
		CFlyFastLock(cs);
		if (nick.empty() || !hubId) return false;
		if (flags & LAST_IP_LOADED) return true;
		currentNick = nick;
		currentHubID = hubId;
		flags |= LAST_IP_LOADED;
	}
	
	uint32_t messageCount;
	boost::asio::ip::address_v4 lastIp;
	if (CFlylinkDBManager::getInstance()->load_last_ip_and_user_stat(currentHubID, currentNick, messageCount, lastIp))
	{
		CFlyFastLock(cs);
		this->messageCount.set(messageCount);
		this->messageCount.reset_dirty();
		this->lastIp.set(lastIp);
		this->lastIp.reset_dirty();
	}
	return true;
}

void User::incMessageCount()
{
	loadIpAndMessageCount();
	messageCount.set(messageCount.get() + 1);
}
#endif

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

bool Identity::setExtJSON(const string& p_ExtJSON)
{
	bool l_result = true;
#ifdef FLYLINKDC_USE_CHECK_EXT_JSON
	if (m_lastExtJSON == p_ExtJSON)
	{
		l_result = false;
#ifdef _DEBUG
		LogManager::message("Duplicate ExtJSON = " + p_ExtJSON);
		dcassert(0);
#endif
	}
	else
	{
		m_lastExtJSON = p_ExtJSON;
	}
#endif
	m_is_ext_json = true;
	return l_result;
}

void Identity::getParams(StringMap& sm, const string& prefix, bool compatibility) const
{
	PROFILE_THREAD_START();
	{
#define APPEND(cmd, val) sm[prefix + cmd] = val;
#define SKIP_EMPTY(cmd, val) { if (!val.empty()) { APPEND(cmd, val); } }
	
		APPEND("NI", getNick());
		SKIP_EMPTY("SID", getSIDString());
		const auto l_cid = user->getCID().toBase32();
		APPEND("CID", l_cid);
		APPEND("SSshort", Util::formatBytes(getBytesShared()));
		SKIP_EMPTY("SU", getSupports());
// Справочные значения заберем через функцию get т.к. в мапе их нет
		SKIP_EMPTY("VE", getStringParam("VE"));
		SKIP_EMPTY("AP", getStringParam("AP"));
		if (compatibility)
		{
			if (prefix == "my")
			{
				sm["mynick"] = getNick();
				sm["mycid"] = l_cid;
			}
			else
			{
				sm["nick"] = getNick();
				sm["cid"] = l_cid;
				sm["ip"] = getIpAsString();
				sm["tag"] = getTag();
				sm["description"] = getDescription();
				sm["email"] = getEmail();
				sm["share"] = Util::toString(getBytesShared());
				const auto l_share = Util::formatBytes(getBytesShared());
				sm["shareshort"] = l_share;
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
				sm["realshareformat"] = Util::formatBytes(getRealBytesShared());
#else
				sm["realshareformat"] = l_share;
#endif
			}
		}
#undef APPEND
#undef SKIP_EMPTY
	}
	{
		CFlyFastLock(cs);
		for (auto i = stringInfo.cbegin(); i != stringInfo.cend(); ++i)
		{
			sm[prefix + string((char*)(&i->first), 2)] = i->second;
		}
	}
}

string Identity::getTag() const
{
	CFlyFastLock(cs);
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
	CFlyFastLock(cs);
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
		CFlyFastLock(cs);
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

	CFlyFastLock(cs);
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

void Identity::calcP2PGuard()
{
	if (!m_is_p2p_guard_calc)
	{
		if (getIp().to_ulong() && Util::isPrivateIp(getIp().to_ulong()) == false)
		{
			const string l_p2p_guard = CFlylinkDBManager::getInstance()->is_p2p_guard(getIp().to_ulong());
			setP2PGuard(l_p2p_guard);
			m_is_p2p_guard_calc = true;
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
		const auto loc = Util::getIpCountry(value);
		string location = Text::fromT(loc.getDescription());
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

void Identity::getReport(string& report) const
{
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
		
		appendIfValueNotEmpty(STRING(NICK), getNick());
		if (!isNmdc)
			appendIfValueNotEmpty("Nicks", Util::toString(ClientManager::getNicks(user->getCID(), Util::emptyString)));
		
		{
			CFlyFastLock(cs);
			for (auto i = stringInfo.cbegin(); i != stringInfo.cend(); ++i)
			{
				auto name = string((char*)(&i->first), 2);
				const auto& value = i->second;
				// TODO: translate known tags and format values to something more readable
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
						name = "KeyPrint";
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
						continue;
						break;
					default:
						name += " (unknown)";
				}
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
		appendIfValueNotEmpty("Known supports", getSupports());
		
		appendIfValueNotEmpty("IPv4 Address", formatIpString(getIpAsString()));
		// appendIfValueNotEmpty("IPv6 Address", formatIpString(getIp())); TODO
		
		// Справочные значения заберем через функцию get т.к. в мапе их нет
		appendIfValueNotEmpty("DC client", getStringParam("AP"));
		appendIfValueNotEmpty("Client version", getStringParam("VE"));
		
		appendIfValueNotEmpty("P2P Guard", getP2PGuard());
		appendIfValueNotEmpty("Support info", getExtJSONSupportInfo());
		appendIfValueNotEmpty("Gender", Text::fromT(getGenderTypeAsString()));
		
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
		appendIfValueNotEmpty("Country", getFlyHubCountry());
		appendIfValueNotEmpty("City", getFlyHubCity());
		appendIfValueNotEmpty("ISP", getFlyHubISP());
#endif
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
	if (!m_ip.is_unspecified())
		return m_ip.to_string();
	else
	{
		if (isUseIP6())
		{
			return getIP6();
		}
		else
		{
			if (user)
			{
				auto ip = user->getIP();
				if (!ip.is_unspecified())
					return ip.to_string();
			}
			return Util::emptyString;
		}
	}
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
		m_ip = boost::asio::ip::address_v4::from_string(temp, ec);
	}
	else
	{
		m_ip = boost::asio::ip::address_v4::from_string(ip, ec);
	}
	if (ec)
	{
		dcassert(0);
		return;
	}
	getUser()->setIP(m_ip);
	change(CHANGES_IP | CHANGES_GEO_LOCATION);
}

bool Identity::isPhantomIP() const
{
	if (m_ip.is_unspecified())
	{
		if (isUseIP6())
			return false;
		else
			return true;
	}
	return false;
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
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		if (getHubID() != 0) // Value HubID is zero for himself, do not check your user.
#endif
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
	}
	return static_cast<DefinedAutoBanFlags>(ban);
}
#endif // IRAINMAN_ENABLE_AUTO_BAN

#ifdef FLYLINKDC_USE_CHECK_CHANGE_TAG
bool OnlineUser::isTagUpdate(const string& p_tag, bool& p_is_version_change)
{
	p_is_version_change = false;
	if (p_tag != m_tag)
	{
		if (!m_tag.empty())
		{
			auto l_find_sep = p_tag.find(',');
			if (l_find_sep != string::npos)
			{
				if (m_tag.size() > l_find_sep && m_tag[l_find_sep] == ',')
				{
					// Сравним приложение и версию - если не изменились - упростим парсинг тэга позже
					if (m_tag.compare(0, l_find_sep, p_tag, 0, l_find_sep) == 0)
						p_is_version_change = false;
					else
						p_is_version_change = true;
				}
			}
		}
		else
		{
			p_is_version_change = true; // Первый раз
		}
		m_tag = p_tag;
		return true;
	}
	else
	{
#ifdef _DEBUG
		LogManager::message("OnlineUser::isTagUpdate - duplicate tag = " + p_tag + " Hub =" + getClient().getHubUrl() + " user = " + getUser()->getLastNick());
#endif
		return false;
	}
}
#endif // FLYLINKDC_USE_CHECK_CHANGE_TAG
