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
#include "ClientManager.h"
#include "StringTokenizer.h"
#include "Download.h"
#include "LogManager.h"
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "QueueManager.h"
#include "DebugManager.h"
#include "ConnectivityManager.h"
#include "IpGuard.h"
#include "IpTrust.h"
#include "LocationUtil.h"
#include "PortTest.h"

const string UserConnection::FEATURE_MINISLOTS = "MiniSlots";
const string UserConnection::FEATURE_XML_BZLIST = "XmlBZList";
const string UserConnection::FEATURE_ADCGET = "ADCGet";
const string UserConnection::FEATURE_ZLIB_GET = "ZLIG";
const string UserConnection::FEATURE_TTHL = "TTHL";
const string UserConnection::FEATURE_TTHF = "TTHF";
const string UserConnection::FEATURE_ADC_BAS0 = "BAS0";
const string UserConnection::FEATURE_ADC_BASE = "BASE";
const string UserConnection::FEATURE_ADC_BZIP = "BZIP";
const string UserConnection::FEATURE_ADC_TIGR = "TIGR";
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
const string UserConnection::FEATURE_BANMSG = "BanMsg"; // !SMT!-B
#endif

const string UserConnection::g_FILE_NOT_AVAILABLE = "File Not Available";
#if defined (FLYLINKDC_USE_DOS_GUARD) && defined (IRAINMAN_DISALLOWED_BAN_MSG)
const string UserConnection::g_PLEASE_UPDATE_YOUR_CLIENT = "Please update your DC++ http://flylinkdc.com";
#endif

static int nextConnID;

// We only want ConnectionManager to create this...
UserConnection::UserConnection() noexcept :
	id(++nextConnID),
	lastEncoding(Text::CHARSET_SYSTEM_DEFAULT),
	state(STATE_UNCONNECTED),
	speed(0),
	chunkSize(0),
	socket(nullptr),
	lastActivity(0),
	lastDownloadSpeed(0),
	lastUploadSpeed(0),
	slotType(NOSLOT)
{
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Created, p=" +
			Util::toHexString(this), false);
#endif
}

UserConnection::~UserConnection()
{
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Deleted, p=" +
			Util::toHexString(this) + " sock=" + Util::toHexString(socket), false);
#endif
	if (socket)
	{
		socket->shutdown();
		socket->joinThread();
		BufferedSocket::destroyBufferedSocket(socket);
	}
}

void UserConnection::updateLastActivity()
{
	lastActivity = GET_TICK();
}

bool UserConnection::isIpBlocked(bool isDownload)
{
	string remoteIp = getRemoteIp();
	boost::system::error_code ec;
	boost::asio::ip::address_v4 addr = boost::asio::ip::make_address_v4(remoteIp, ec);
	if (ec || addr.is_unspecified()) return false;

/*
	if (ipGuard.isBlocked(addr.to_ulong()))
	{
		LogManager::message(STRING_F(IP_BLOCKED, "IPGuard" % remoteIp));
		getUser()->setFlag(User::PG_P2PGUARD_BLOCK);
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
		return true;
	}
*/

#ifdef FLYLINKDC_USE_IPFILTER
	if (ipTrust.isBlocked(addr.to_ulong()))
	{
		LogManager::message(STRING_F(IP_BLOCKED, "IPTrust" % remoteIp));
		yourIpIsBlocked();
		getUser()->setFlag(User::PG_IPTRUST_BLOCK);
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
		return true;
	}
	if (BOOLSETTING(ENABLE_P2P_GUARD) && !isDownload)
	{
		IPInfo ipInfo;
		Util::getIpInfo(addr.to_ulong(), ipInfo, IPInfo::FLAG_P2P_GUARD);
		if (!ipInfo.p2pGuard.empty())
		{
			LogManager::message(STRING_F(IP_BLOCKED2, "P2PGuard" % remoteIp % ipInfo.p2pGuard));
			yourIpIsBlocked();
			getUser()->setFlag(User::PG_P2PGUARD_BLOCK);
			QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
			return true;
		}
	}
#endif
	return false;
}

void UserConnection::onDataLine(const string& aLine) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown())
	if (aLine.length() < 2 || ClientManager::isBeforeShutdown())
		return;
	if (CMD_DEBUG_ENABLED())
		COMMAND_DEBUG(aLine, DebugTask::CLIENT_IN, getRemoteIpPort());
	
	if (aLine[0] == 'C' && !isSet(FLAG_NMDC))
	{
		if (!Text::validateUtf8(aLine))
		{
			// @todo Report to user?
			return;
		}
		dispatch(aLine);
		return;
	}
	else if (aLine[0] == '$')
	{
		setFlag(FLAG_NMDC);
	}
	else
	{
		// We shouldn't be here?
		if (getUser() && aLine.length() < 255)
		{
			ClientManager::setUnknownCommand(getUser(), aLine);
		}
		
		fly_fire2(UserConnectionListener::ProtocolError(), this, "Invalid data");
		return;
	}
	
	string cmd;
	string param;
	
	string::size_type x = aLine.find(' ');
	
	if (x == string::npos)
	{
		cmd = aLine.substr(1);
	}
	else
	{
		cmd = aLine.substr(1, x - 1);
		param = aLine.substr(x + 1);
	}
	if (cmd == "FLY-TEST-PORT")
	{
		if (param.length() > 39)
		{
			string reflectedAddress = param.substr(39);
			string ip;
			uint16_t port = 0;
			if (reflectedAddress.back() == '|')
			{
				reflectedAddress.erase(reflectedAddress.length()-1);
				Util::parseIpPort(reflectedAddress, ip, port);
				if (!(port && Util::isValidIp4(ip)))
					reflectedAddress.clear();
			}
			
			int unused;
			if (g_portTest.processInfo(PortTest::PORT_TCP, PortTest::PORT_TLS, 0, reflectedAddress, param.substr(0, 39)) &&
			    g_portTest.getState(PortTest::PORT_TCP, unused, &reflectedAddress) == PortTest::STATE_SUCCESS)
			{
				Util::parseIpPort(reflectedAddress, ip, port);
				if (Util::isValidIp4(ip))
					ConnectivityManager::getInstance()->setReflectedIP(ip);
			}
			ConnectivityManager::getInstance()->processPortTestResult();
		}
	} else
	if (cmd == "MyNick")
	{
		if (!param.empty())
		{
			fly_fire2(UserConnectionListener::MyNick(), this, param);
		}
	}
	else if (cmd == "Direction")
	{
		x = param.find(' ');
		if (x != string::npos)
		{
			fly_fire3(UserConnectionListener::Direction(), this, param.substr(0, x), param.substr(x + 1));
		}
	}
	else if (cmd == "Error")
	{
		if (param.compare(0, g_FILE_NOT_AVAILABLE.size(), g_FILE_NOT_AVAILABLE) == 0 ||
		    param.rfind(/*path/file*/" no more exists") != string::npos)
		{
			const DownloadPtr& download = getDownload();
			if (download)
			{
				if (download->isSet(Download::FLAG_USER_GET_IP))
					fly_fire1(UserConnectionListener::CheckUserIP(), this);
				else
					fly_fire1(UserConnectionListener::FileNotAvailable(), this);
			}
		}
		else
		{
			if (param.compare(0, 7, "CTM2HUB", 7) == 0)
			{
				// https://github.com/Verlihub/verlihub-1.0.0/blob/4f5ad13b5aa6d5a3c2ec94262f7b7bf1b90fc567/src/cdcproto.cpp#L2358
				ConnectionManager::addCTM2HUB(getServerPort(), getHintedUser());
			}
			dcdebug("Unknown $Error %s\n", param.c_str());
			fly_fire2(UserConnectionListener::ProtocolError(), this, param);
		}
	}
	else if (cmd == "Get")
	{
		x = param.find('$');
		if (x != string::npos)
		{
			fly_fire3(UserConnectionListener::Get(), this, Text::toUtf8(param.substr(0, x), lastEncoding), Util::toInt64(param.substr(x + 1)) - (int64_t)1);
		}
	}
	else if (cmd == "Key")
	{
		if (!param.empty())
		{
			string remoteIp = getRemoteIp();
			boost::system::error_code ec;
			boost::asio::ip::address_v4 addr = boost::asio::ip::make_address_v4(remoteIp, ec);
			if (!ec && ipGuard.isBlocked(addr.to_ulong()))
			{
				LogManager::message(STRING_F(IP_BLOCKED, "IPGuard" % remoteIp));
				yourIpIsBlocked();
			}
			else
				fly_fire2(UserConnectionListener::Key(), this, param);
		}
	}
	else if (cmd == "Lock")
	{
		if (!param.empty())
		{
			x = param.find(' ');
			fly_fire2(UserConnectionListener::CLock(), this, (x != string::npos) ? param.substr(0, x) : param);
		}
	}
	else if (cmd == "Send")
	{
		fly_fire1(UserConnectionListener::Send(), this);
	}
	else if (cmd == "MaxedOut")
	{
		fly_fire2(UserConnectionListener::MaxedOut(), this, param);
	}
	else if (cmd == "Supports")
	{
		if (!param.empty())
		{
			fly_fire2(UserConnectionListener::Supports(), this, StringTokenizer<string>(param, ' ').getWritableTokens());
		}
	}
	else if (cmd.compare(0, 3, "ADC", 3) == 0)
	{
		dispatch(aLine, true);
	}
	else if (cmd == "ListLen")
	{
		if (!param.empty())
		{
			fly_fire2(UserConnectionListener::ListLength(), this, param);
		}
	}
	else if (cmd == "GetListLen")
	{
		fly_fire1(UserConnectionListener::GetListLength(), this);
	}
	else if (cmd == "UGetBlock" || cmd == "GetBlock" || cmd == "UGetZBlock") 
	{
	// https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1684
	/*
	DC supported the feature XMLBZList and the commands $GetBlock, $UGetBlock and $UGetZBlock in versions 0.307 to 0.695. 
	DC dropped support for the commands in version 0.696, whilst not removing the feature announcement. I.e., 
	DC++ signals in the $Supports XMLBzList while it does not support the actual commands.
	*/
	}
	else
	{
		if (getUser() && aLine.length() < 255)
			ClientManager::setUnknownCommand(getUser(), aLine);
			
		dcdebug("UserConnection Unknown NMDC command: %.50s\n", aLine.c_str());
		string log = "UserConnection:: Unknown NMDC command: = " + aLine + " hub = " + getHubUrl() + " remote IP = " + getRemoteIpPort();
		if (getHintedUser().user)
		{
			log += " Nick = " + getHintedUser().user->getLastNick();
		}
		LogManager::message(log, false);
		unsetFlag(FLAG_NMDC);
		disconnect(true); // https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1684
	}
}

void UserConnection::onUpgradedToSSL() noexcept
{
	dcassert(!isSet(FLAG_SECURE));
	setFlag(FLAG_SECURE);
}

void UserConnection::connect(const string& aServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole)
{
	dcassert(!socket);
	socket = BufferedSocket::getBufferedSocket(0, this);
	const bool allowUntrusred = BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS);
	const bool secure = isSet(FLAG_SECURE);
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Using sock=" +
			Util::toHexString(socket) + ", secure=" + Util::toString((int) secure), false);
	socket->connect(aServer, aPort, localPort, natRole, secure, allowUntrusred, true, Socket::PROTO_DEFAULT);
}

void UserConnection::addAcceptedSocket(unique_ptr<Socket>& newSock, uint16_t port)
{
	dcassert(!socket);
	socket = BufferedSocket::getBufferedSocket(0, this);
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Accepted, using sock=" +
			Util::toHexString(socket), false);
	socket->addAcceptedSocket(std::move(newSock), port);
}

void UserConnection::inf(bool withToken)
{
	AdcCommand c(AdcCommand::CMD_INF);
	c.addParam("ID", ClientManager::getMyCID().toBase32());
	if (withToken)
	{
		c.addParam("TO", getUserConnectionToken());
	}
	send(c);
}

void UserConnection::sup(const StringList& features)
{
	AdcCommand c(AdcCommand::CMD_SUP);
	for (auto i = features.cbegin(); i != features.cend(); ++i)
	{
		c.addParam(*i);
	}
	send(c);
}

void UserConnection::supports(const StringList& feat)
{
	string cmd = "$Supports";
	for (const string& feature : feat)
	{
		cmd += ' ';
		cmd += feature;
	}
	cmd += '|';
	send(cmd);
}

void UserConnection::handle(AdcCommand::STA t, const AdcCommand& c)
{
	if (c.getParameters().size() >= 2)
	{
		const string& code = c.getParam(0);
		if (!code.empty() && code[0] - '0' == AdcCommand::SEV_FATAL)
		{
			fly_fire2(UserConnectionListener::ProtocolError(), this, c.getParam(1));
			return;
		}
	}
	
	fly_fire2(t, this, c);
}

void UserConnection::onConnected() noexcept
{
	updateLastActivity();
	fly_fire1(UserConnectionListener::Connected(), this);
}

void UserConnection::onData(const uint8_t* data, size_t len)
{
	updateLastActivity();
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	if (len)
	{
		getUser()->loadIPStat();
		getUser()->addBytesDownloaded(getSocket()->getIp4(), len);
	}
#endif
	DownloadManager::getInstance()->onData(this, data, len);
}

void UserConnection::onBytesSent(size_t bytes, size_t actual)
{
	updateLastActivity();
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	if (actual)
	{
		getUser()->loadIPStat();
		getUser()->addBytesUploaded(getSocket()->getIp4(), actual);
	}
#endif
	dcassert(getState() == UserConnection::STATE_RUNNING);
	getUpload()->addPos(bytes, actual);
}

void UserConnection::onModeChange() noexcept
{
	updateLastActivity();
	fly_fire1(UserConnectionListener::ModeChange(), this);
}

void UserConnection::onTransmitDone() noexcept
{
	fly_fire1(UserConnectionListener::TransmitDone(), this);
}

void UserConnection::onUpdated() noexcept
{
	fly_fire1(UserConnectionListener::Updated(), this);
}

void UserConnection::onFailed(const string& line) noexcept
{
	if (state != STATE_UNUSED)
		state = STATE_UNCONNECTED;
	fly_fire2(UserConnectionListener::Failed(), this, line);
}

// # ms we should aim for per segment
static const int64_t SEGMENT_TIME = 120 * 1000;
static const int64_t MIN_CHUNK_SIZE = 64 * 1024;

void UserConnection::updateChunkSize(int64_t leafSize, int64_t lastChunk, uint64_t ticks)
{
	if (chunkSize == 0)
	{
		chunkSize = std::max((int64_t)64 * 1024, std::min(lastChunk, (int64_t)1024 * 1024));
		return;
	}
	
	if (ticks <= 10)
	{
		// Can't rely on such fast transfers - double
		chunkSize *= 2;
		return;
	}
	
	const double lastSpeed = (1000. * lastChunk) / ticks;
	
	int64_t targetSize = chunkSize;
	
	// How long current chunk size would take with the last speed...
	const double msecs = 1000 * double(targetSize) / lastSpeed;
	
	if (msecs < SEGMENT_TIME / 4)
	{
		targetSize *= 2;
	}
	else if (msecs < SEGMENT_TIME / 1.25)
	{
		targetSize += leafSize;
	}
	else if (msecs < SEGMENT_TIME * 1.25)
	{
		// We're close to our target size - don't change it
	}
	else if (msecs < SEGMENT_TIME * 4)
	{
		targetSize = std::max(MIN_CHUNK_SIZE, targetSize - chunkSize);
	}
	else
	{
		targetSize = std::max(MIN_CHUNK_SIZE, targetSize / 2);
	}
	
	chunkSize = targetSize;
}

void UserConnection::send(const string& aString)
{
	updateLastActivity();
	if (CMD_DEBUG_ENABLED()) COMMAND_DEBUG(aString, DebugTask::CLIENT_OUT, getRemoteIpPort());
	socket->write(aString);
}

void UserConnection::setDefaultLimit()
{
	int defaultLimit = SETTING(PER_USER_UPLOAD_SPEED_LIMIT);
	setUploadLimit(defaultLimit ? defaultLimit : FavoriteUser::UL_NONE);
}

void UserConnection::setUser(const UserPtr& user)
{
	hintedUser.user = user;
	if (!socket)
		return;
		
	if (!user)
	{
		setDefaultLimit();
	}
	else
	{
		int limit;
		FavoriteUser::MaskType flags;
		if (FavoriteManager::getFavUserParam(user, flags, limit))
			setUploadLimit(limit);
		else
			setDefaultLimit();
	}
}

void UserConnection::setUploadLimit(int lim)
{
	switch (lim)
	{
		case FavoriteUser::UL_BAN:
			disconnect(true);
			break;
		case FavoriteUser::UL_SU:
			socket->setMaxSpeed(-1);
			break;
		case FavoriteUser::UL_NONE:
			socket->setMaxSpeed(0);
			break;
		default:
			socket->setMaxSpeed(lim * 1024);
	}
}

void UserConnection::maxedOut(size_t queuePosition)
{
	if (isSet(FLAG_NMDC))
	{
		send("$MaxedOut " + Util::toString(queuePosition) + '|');
	}
	else
	{
		AdcCommand cmd(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_SLOTS_FULL, "Slots full");
		cmd.addParam("QP", Util::toString(queuePosition));
		send(cmd);
	}
}


void UserConnection::fileNotAvail(const std::string& msg /*= g_FILE_NOT_AVAILABLE*/)
{
	isSet(FLAG_NMDC) ? send("$Error " + msg + '|') : send(AdcCommand(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_FILE_NOT_AVAILABLE, msg));
}

void UserConnection::yourIpIsBlocked()
{
	static const string msg("Your IP is blocked");
	isSet(FLAG_NMDC) ? send("$Error " + msg + '|') : send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_BANNED_GENERIC, msg));
}

void UserConnection::setDownload(const DownloadPtr& d)
{
	dcassert(isSet(FLAG_DOWNLOAD));
	download = d;
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Download " +
			download->getPath() + " from " + download->getUser()->getLastNick() +
			", p=" + Util::toHexString(this), false);
}

void UserConnection::setUpload(const UploadPtr& u)
{
	dcassert(isSet(FLAG_UPLOAD));
	upload = u;
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Upload " +
			upload->getPath() + " to " + upload->getUser()->getLastNick() +
			", p=" + Util::toHexString(this), false);
}

string UserConnection::getDescription() const
{
	string d = "UserConnection(" + Util::toString(id) + "): p=" +
		Util::toHexString(this) + " state=" + Util::toString(state) +
		" sock=" + Util::toHexString(socket);
	if (!connectionQueueToken.empty())
		d += " queueToken=" + connectionQueueToken;
	if (!userConnectionToken.empty())
		d += " userToken=" + userConnectionToken;
	if (hintedUser.user)
	{
		d += " user=" + hintedUser.user->getLastNick() + '/' + hintedUser.user->getCID().toBase32();
		if (!hintedUser.hint.empty())
			d += " (" + hintedUser.hint + ')';
	}
	d += " slotType=" + Util::toString((int) slotType);
	return d;
}

#ifdef DEBUG_USER_CONNECTION
void UserConnection::dumpInfo() const
{
	LogManager::message(getDescription(), false);
}
#endif
