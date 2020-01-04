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
#include "PGLoader.h"
#include "IpGuard.h"
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

#ifdef FLYLINKDC_USE_BLOCK_ERROR_CMD
FastCriticalSection UserConnection::g_error_cs;
std::unordered_map<string, unsigned> UserConnection::g_error_cmd_map;
#endif

#ifdef DEBUG_USER_CONNECTION
static int nextConnID;
#endif

// We only want ConnectionManager to create this...
UserConnection::UserConnection() noexcept :
#ifdef DEBUG_USER_CONNECTION
	id(++nextConnID),
#endif
	lastEncoding(Text::CHARSET_SYSTEM_DEFAULT),
	state(STATE_UNCONNECTED),
	speed(0),
	chunkSize(0),
	socket(nullptr),
	lastActivity(0),
	slotType(NOSLOT)
{
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Created, p=" +
			Util::toHexString(this), false);
#endif
}

UserConnection::~UserConnection()
{
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SYSTEM))
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

bool UserConnection::isIPGuard(ResourceManager::Strings p_id_string, bool p_is_download_connection)
{
	uint32_t l_ip4;
	if (IpGuard::is_block_ip(getRemoteIp(), l_ip4))
	{
		getUser()->setFlag(User::PG_P2PGUARD_BLOCK);
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
		return true;
	}
	bool l_is_ip_guard = false;
#ifdef FLYLINKDC_USE_IPFILTER
	l_is_ip_guard = PGLoader::check(l_ip4);
	string l_p2p_guard;
	if (BOOLSETTING(ENABLE_P2P_GUARD) && p_is_download_connection == false)
	{
		l_p2p_guard = CFlylinkDBManager::getInstance()->is_p2p_guard(l_ip4);
		if (!l_p2p_guard.empty())
		{
			l_is_ip_guard = true;
			l_p2p_guard = " [P2PGuard] " + l_p2p_guard + " [http://emule-security.org]";
			const bool l_is_manual = l_p2p_guard.find("Manual block IP") != string::npos;
			if (getUser())
			{
				l_p2p_guard += "[User = " + getUser()->getLastNick() + "] [Hub:" + getHubUrl() + "] [Nick:" + ClientManager::findMyNick(getHubUrl()) + "]";
			}
			if (!l_is_manual)
			{
				LogManager::message("(" + getRemoteIp() + ')' + l_p2p_guard);
			}
		}
	}
	if (l_is_ip_guard)
	{
		error(STRING(YOUR_IP_IS_BLOCKED) + l_p2p_guard);
		if (l_p2p_guard.empty())
			getUser()->setFlag(User::PG_IPTRUST_BLOCK);
		else
			getUser()->setFlag(User::PG_P2PGUARD_BLOCK);
		//if (!getUser()->isSet(User::PG_LOG_BLOCK_BIT))
		{
			getUser()->setFlag(User::PG_LOG_BLOCK_BIT);
			LogManager::message("IPFilter: " + ResourceManager::getString(p_id_string) + " (" + getRemoteIp() + ") " + l_p2p_guard);
		}
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
		return true;
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
				if (!(port && Util::isValidIP(ip)))
					reflectedAddress.clear();
			}
			
			int unused;
			if (g_portTest.processInfo(PortTest::PORT_TCP, PortTest::PORT_TLS, 0, reflectedAddress, param.substr(0, 39)) &&
			    g_portTest.getState(PortTest::PORT_TCP, unused, &reflectedAddress) == PortTest::STATE_SUCCESS)
			{
				Util::parseIpPort(reflectedAddress, ip, port);
				if (Util::isValidIP(ip))
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
			// [+] SSA
			if (getDownload()) // Не понятно почему падаю тут - https://drdump.com/Problem.aspx?ProblemID=96544
			{
				if (getDownload()->isSet(Download::FLAG_USER_GET_IP)) // Crash https://drdump.com/Problem.aspx?ClientID=guest&ProblemID=90376
				{
					fly_fire1(UserConnectionListener::CheckUserIP(), this);
				}
				else
				{
					fly_fire1(UserConnectionListener::FileNotAvailable(), this);
				}
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
			const string l_ip = getRemoteIp();
			uint32_t l_ip4;
			if (!IpGuard::is_block_ip(l_ip, l_ip4))
			{
				fly_fire2(UserConnectionListener::Key(), this, param);
			}
			else
			{
				dcdebug("Block IP %s", l_ip.c_str());
			}
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
			fly_fire2(UserConnectionListener::Supports(), this, StringTokenizer<string>(param, ' ').getTokensForWrite());
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

#ifdef FLYLINKDC_USE_BLOCK_ERROR_CMD
bool UserConnection::add_error_user(const string& p_ip)
{
	CFlyFastLock(g_error_cs);
	if (++g_error_cmd_map[p_ip] > 3)
	{
		return true;
	}
	return false;
}
bool UserConnection::is_error_user(const string& p_ip)
{
	CFlyFastLock(g_error_cs);
	auto i = g_error_cmd_map.find(p_ip);
	if (i != g_error_cmd_map.end())
	{
		{
			CFlyServerJSON::pushError(83, "is_error_user: " + p_ip);
			return true;
		}
	}
	return false;
}
#endif

void UserConnection::connect(const string& aServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole)
{
	dcassert(!socket);
	socket = BufferedSocket::getBufferedSocket(0, this);
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Using sock=" +
			Util::toHexString(socket), false);
#endif
	const bool allowUntrusred = BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS);
	const bool secure = isSet(FLAG_SECURE);
	socket->connect(aServer, aPort, localPort, natRole, secure, allowUntrusred, true, Socket::PROTO_DEFAULT);
}

void UserConnection::addAcceptedSocket(unique_ptr<Socket>& newSock, uint16_t port)
{
	dcassert(!socket);
	socket = BufferedSocket::getBufferedSocket(0, this);
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Accepted, using sock=" +
			Util::toHexString(socket), false);
#endif
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
	const string x = Util::toSupportsCommand(feat);
	send(x);
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
	if (len && BOOLSETTING(ENABLE_RATIO_USER_LIST))
	{
		getUser()->AddRatioDownload(getSocket()->getIp4(), len);
	}
#endif
	DownloadManager::getInstance()->fireData(this, data, len);
}

void UserConnection::onBytesSent(size_t bytes, size_t actual)
{
//	const auto l_tick =
	updateLastActivity();
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	if (actual && BOOLSETTING(ENABLE_RATIO_USER_LIST))
	{
		getUser()->AddRatioUpload(getSocket()->getIp4(), actual);
	}
#endif
	dcassert(getState() == UserConnection::STATE_RUNNING);
	getUpload()->addPos(bytes, actual);
	// getUpload()->tick(l_tick); // - данные код есть в оригинале
	//fly_fire3(UserConnectionListener::UserBytesSent(), this, bytes, actual);
}

/*
void UserConnection::on(BytesSent, size_t p_Bytes, size_t p_Actual) noexcept
{
    dcassert(0);
    fireBytesSent(p_Bytes, p_Actual);
}
*/

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

void UserConnection::setUser(const UserPtr& user)
{
	hintedUser.user = user;
	if (!socket)
		return;
		
	if (!user)
	{
		setUploadLimit(FavoriteUser::UL_NONE);
	}
	else
	{
		int limit;
		FavoriteUser::MaskType flags;
		if (FavoriteManager::getFavUserParam(user, flags, limit))
			setUploadLimit(limit);
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

void UserConnection::maxedOut(size_t queue_position)
{
	if (isSet(FLAG_NMDC))
	{
		send("$MaxedOut " + Util::toString(queue_position) + '|');
	}
	else
	{
		AdcCommand cmd(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_SLOTS_FULL, "Slots full");
		cmd.addParam("QP", Util::toString(queue_position));
		send(cmd);
	}
}


void UserConnection::fileNotAvail(const std::string& msg /*= g_FILE_NOT_AVAILABLE*/)
{
	isSet(FLAG_NMDC) ? send("$Error " + msg + '|') : send(AdcCommand(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_FILE_NOT_AVAILABLE, msg));
}

void UserConnection::setDownload(const DownloadPtr& d)
{
	dcassert(isSet(FLAG_DOWNLOAD));
	download = d;
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Download " +
			download->getPath() + " from " + download->getUser()->m_nick.c_str() +
			", p=" + Util::toHexString(this), false);
#endif
}

void UserConnection::setUpload(const UploadPtr& u)
{
	dcassert(isSet(FLAG_UPLOAD));
	upload = u;
#ifdef DEBUG_USER_CONNECTION
	if (BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Upload " +
			upload->getPath() + " to " + upload->getUser()->m_nick.c_str() +
			", p=" + Util::toHexString(this), false);
#endif
}

#ifdef DEBUG_USER_CONNECTION
void UserConnection::dumpInfo() const
{
	LogManager::message("UserConnection(" + Util::toString(id) + "): p=" +
		Util::toHexString(this) + " state=" + Util::toString(state) +
		" sock=" + Util::toHexString(socket), false);
}
#endif
