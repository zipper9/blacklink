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
#include "UploadManager.h"
#include "QueueManager.h"
#include "DebugManager.h"
#include "ConnectivityManager.h"
#include "IpGuard.h"
#include "IpTrust.h"
#include "LocationUtil.h"
#include "PortTest.h"
#include "NmdcHub.h"
#include "AntiFlood.h"
#include "Random.h"

#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
#include "TagCollector.h"
extern TagCollector collNmdcFeatures;
#endif

extern IpBans tcpBans;

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
const string UserConnection::FEATURE_ADC_CPMI = "CPMI";
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
const string UserConnection::FEATURE_BANMSG = "BanMsg"; // !SMT!-B
#endif

const string UserConnection::FILE_NOT_AVAILABLE = "File Not Available";

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
	lastMessageActivity(0),
	lastDownloadSpeed(0),
	lastUploadSpeed(0),
	slotType(NOSLOT),
	number(Util::rand() & 0x7FFF)
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
		socket->disconnect(true);
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
	IpAddress ip = getRemoteIp();
	if (ip.type != AF_INET) return false;

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
	if (ipTrust.isBlocked(ip.data.v4))
	{
		LogManager::message(STRING_F(IP_BLOCKED, "IPTrust" % Util::printIpAddress(ip.data.v4)));
		yourIpIsBlocked();
		getUser()->setFlag(User::PG_IPTRUST_BLOCK);
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
		return true;
	}
	if (BOOLSETTING(ENABLE_P2P_GUARD) && !isDownload)
	{
		IPInfo ipInfo;
		Util::getIpInfo(ip, ipInfo, IPInfo::FLAG_P2P_GUARD);
		if (!ipInfo.p2pGuard.empty())
		{
			LogManager::message(STRING_F(IP_BLOCKED2, "P2PGuard" % Util::printIpAddress(ip.data.v4) % ipInfo.p2pGuard));
			yourIpIsBlocked();
			getUser()->setFlag(User::PG_P2PGUARD_BLOCK);
			QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
			return true;
		}
	}
#endif
	return false;
}

void UserConnection::failed(const string& line) noexcept
{
	if (isSet(FLAG_ASSOCIATED | FLAG_UPLOAD))
		UploadManager::getInstance()->failed(this, line);
	if (isSet(FLAG_ASSOCIATED | FLAG_DOWNLOAD))
		DownloadManager::getInstance()->fail(this, line);
	ConnectionManager::getInstance()->failed(this, line, false);
}

void UserConnection::protocolError(const string& error) noexcept
{
	ConnectionManager::getInstance()->failed(this, error, true);
	if (isSet(FLAG_ASSOCIATED | FLAG_DOWNLOAD))
		DownloadManager::getInstance()->fail(this, error);
}

bool UserConnection::checkState(int state, const string& command) const noexcept
{
	if (getState() == state) return true;
	LogManager::message("UserConnection(" + Util::toString(id) + "): Command " +
		command + " not expected in state " + Util::toString(getState()), false);
	return false;
}

bool UserConnection::checkState(int state, const AdcCommand& command) const noexcept
{
	if (getState() == state) return true;
	LogManager::message("UserConnection(" + Util::toString(id) + "): Command " +
		command.getFourCC() + " not expected in state " + Util::toString(getState()), false);
	return false;
}

void UserConnection::onDataLine(const string& line) noexcept
{
	if (line.length() < 2 || ClientManager::isBeforeShutdown())
		return;
	if (CMD_DEBUG_ENABLED())
		COMMAND_DEBUG(line, DebugTask::CLIENT_IN, getRemoteIpPort());
	
	if (line[0] == 'C' && !isSet(FLAG_NMDC))
	{
		if (!Text::validateUtf8(line))
		{
			// @todo Report to user?
			return;
		}
		dispatch(line);
		return;
	}
	else if (line[0] == '$')
	{
		setFlag(FLAG_NMDC);
	}
	else
	{
		// We shouldn't be here?
		if (getUser() && line.length() < 255)
		{
			ClientManager::setUnknownCommand(getUser(), line);
		}
		
		ConnectionManager::getInstance()->failed(this, "Invalid data", true);
		return;
	}
	
	string cmd;
	string param;
	
	string::size_type x = line.find(' ');
	
	if (x == string::npos)
	{
		cmd = line.substr(1);
	}
	else
	{
		cmd = line.substr(1, x - 1);
		param = line.substr(x + 1);
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
			if (g_portTest.processInfo(PortTest::PORT_TCP, PortTest::PORT_TLS, ConnectionManager::getInstance()->getPort(), reflectedAddress, param.substr(0, 39)) &&
			    g_portTest.getState(PortTest::PORT_TCP, unused, &reflectedAddress) == PortTest::STATE_SUCCESS)
			{
				Util::parseIpPort(reflectedAddress, ip, port);
				IpAddress addr;
				if (Util::parseIpAddress(addr, ip) && addr.type == AF_INET && Util::isValidIp(addr))
					ConnectivityManager::getInstance()->setReflectedIP(addr);
			}
			ConnectivityManager::getInstance()->processPortTestResult();
		}
	} else
	if (cmd == "MyNick")
	{
		if (!checkState(STATE_SUPNICK, cmd)) return;
		if (!param.empty())
			ConnectionManager::getInstance()->processMyNick(this, param);
	}
	else if (cmd == "Direction")
	{
		if (!checkState(STATE_DIRECTION, cmd)) return;
		x = param.find(' ');
		if (x != string::npos)
		{
			string dirStr = param.substr(0, x);
			string numberStr = param.substr(x + 1);
			dcassert(isSet(FLAG_DOWNLOAD) ^ isSet(FLAG_UPLOAD));
			if (dirStr == "Upload")
			{
				// Fine, the other fellow want's to send us data...make sure we really want that...
				if (isSet(FLAG_UPLOAD))
				{
#ifdef DEBUG_USER_CONNECTION
					if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
						LogManager::message("UserConnection(" + Util::toString(id) + "): No queued downloads", false);
#endif
					ConnectionManager::getInstance()->putConnection(this);
					return;
				}
			}
			else if (isSet(FLAG_DOWNLOAD))
			{
				int number = Util::toInt(numberStr);
				// Damn, both want to download...the one with the highest number wins...
				if (getNumber() < number)
				{
					// Damn! We lost!
					unsetFlag(FLAG_DOWNLOAD);
					setFlag(FLAG_UPLOAD);
				}
				else if (getNumber() == number)
				{
					ConnectionManager::getInstance()->putConnection(this);
					return;
				}
			}
			dcassert(isSet(FLAG_DOWNLOAD) ^ isSet(FLAG_UPLOAD));
			setState(STATE_KEY);
		}
	}
	else if (cmd == "Error")
	{
		if (param.compare(0, FILE_NOT_AVAILABLE.length(), FILE_NOT_AVAILABLE) == 0 ||
		    param.rfind(/*path/file*/" no more exists") != string::npos)
		{
			const DownloadPtr& download = getDownload();
			if (download)
			{
				if (!checkState(STATE_SND, param))
				{
					disconnect();
					return;
				}
				if (download->isSet(Download::FLAG_USER_GET_IP))
					DownloadManager::getInstance()->checkUserIP(this);
				else
					DownloadManager::getInstance()->fileNotAvailable(this);
			}
		}
		else
		{
			if (param.compare(0, 7, "CTM2HUB", 7) == 0)
			{
				IpPortKey key;
				key.setIP(socket->getIp(), socket->getPort());
				tcpBans.addBan(key, GET_TICK(), "CTM2HUB");
			}
			else
				dcdebug("Unknown $Error %s\n", param.c_str());
			protocolError(param);
		}
	}
	else if (cmd == "Get")
	{
		if (!checkState(STATE_GET, cmd)) return;
		x = param.find('$');
		if (x != string::npos)
		{
			UploadManager::getInstance()->processGet(this, Text::toUtf8(param.substr(0, x), lastEncoding), Util::toInt64(param.substr(x + 1)) - 1);
		}
	}
	else if (cmd == "Key")
	{
		if (!checkState(STATE_KEY, cmd)) return;
		if (!param.empty())
		{
			IpAddress addr = getRemoteIp();
			if (addr.type == AF_INET && ipGuard.isBlocked(addr.data.v4))
			{
				LogManager::message(STRING_F(IP_BLOCKED, "IPGuard" % Util::printIpAddress(addr)));
				yourIpIsBlocked();
			}
			else
				ConnectionManager::getInstance()->processKey(this);
		}
	}
	else if (cmd == "Lock")
	{
		static const string upload   = "Upload";
		static const string download = "Download";
		if (!checkState(STATE_LOCK, cmd)) return;
		if (!param.empty())
		{
			x = param.find(' ');
			string lock = x != string::npos ? param.substr(0, x) : param;
			if (NmdcHub::isExtended(lock))
			{
				StringList features = ConnectionManager::getInstance()->getNmdcFeatures();
				supports(features);
			}
			setState(STATE_DIRECTION);
			send("$Direction " + (isSet(FLAG_UPLOAD) ? upload : download) + ' ' + Util::toString(getNumber()) + '|');
			key(NmdcHub::makeKeyFromLock(lock));
		}
	}
	else if (cmd == "Send")
	{
		if (!checkState(STATE_SEND, cmd)) return;
		UploadManager::getInstance()->processSend(this);
	}
	else if (cmd == "MaxedOut")
	{
		if (!checkState(STATE_SND, cmd))
		{
			disconnect();
			return;
		}
		DownloadManager::getInstance()->noSlots(this, param);
	}
	else if (cmd == "Supports")
	{
		if (!param.empty())
		{
			StringTokenizer<string> st(param, ' ');
			const auto& feat = st.getTokens();
			dcassert(getUser());
			if (getUser())
			{
				uint8_t knownUcSupports = 0;
#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
				string sourceName = getUser()->getLastNick();
				const string& hubUrl = getHubUrl();
				if (!hubUrl.empty())
				{
					sourceName += '\t';
					sourceName += hubUrl;
				}
				UcSupports::setSupports(this, feat, knownUcSupports, sourceName);
#else
				UcSupports::setSupports(this, feat, knownUcSupports, Util::emptyString);
#endif
				ClientManager::setSupports(getUser(), knownUcSupports);
			}
			else
				LogManager::message("Error UserConnectionListener::Supports conn->getUser() == nullptr, url = " + getHintedUser().hint);
		}
	}
	else if (cmd.compare(0, 3, "ADC", 3) == 0)
	{
		dispatch(line, true);
	}
	else if (cmd == "ListLen")
	{
		// ignored
	}
	else if (cmd == "GetListLen")
	{
		error("GetListLength not supported");
		disconnect(false);
	}
	else if (cmd == "GetBlock" || cmd == "GetZBlock" || cmd == "UGetBlock" || cmd == "UGetZBlock") 
	{
		if (!checkState(STATE_GET, cmd)) return;
		UploadManager::getInstance()->processGetBlock(this, cmd, param);
	}
	else
	{
		if (getUser() && line.length() < 255)
			ClientManager::setUnknownCommand(getUser(), line);
			
		dcdebug("UserConnection Unknown NMDC command: %.50s\n", line.c_str());
		string log = "Unknown command from ";
		if (getHintedUser().user)
			log += getHintedUser().user->getLastNick();
		else
			log += '?';
		log += '@';
		log += getHubUrl();
		log += " (";
		log += socket->getRemoteIpAsString();
		log += "): ";
		log += line;
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

void UserConnection::connect(const IpAddress& address, uint16_t port, uint16_t localPort, BufferedSocket::NatRoles natRole)
{
	dcassert(!socket);
	socket = BufferedSocket::getBufferedSocket(0, this);
	const bool secure = isSet(FLAG_SECURE);
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Using sock=" +
			Util::toHexString(socket) + ", secure=" + Util::toString((int) secure), false);
	socket->connect(Util::printIpAddress(address), port, localPort, natRole, secure, true, true, Socket::PROTO_DEFAULT);
	socket->start();
}

void UserConnection::addAcceptedSocket(unique_ptr<Socket>& newSock, uint16_t port)
{
	dcassert(!socket);
	socket = BufferedSocket::getBufferedSocket(0, this);
	if (BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Accepted, using sock=" +
			Util::toHexString(socket), false);
	socket->addAcceptedSocket(std::move(newSock), port);
	socket->start();
}

void UserConnection::handle(AdcCommand::STA t, const AdcCommand& c)
{
	if (c.getParameters().size() >= 2)
	{
		const string& code = c.getParam(0);
		if (!code.empty() && code[0] - '0' == AdcCommand::SEV_FATAL)
		{
			protocolError(c.getParam(1));
			return;
		}
	}
	if (isSet(FLAG_ASSOCIATED | FLAG_DOWNLOAD))
		DownloadManager::getInstance()->processSTA(this, c);
}

void UserConnection::handle(AdcCommand::SUP, const AdcCommand& cmd)
{
	if (!checkState(STATE_SUPNICK, cmd)) return;

	bool baseOk = false;
	for (auto i = cmd.getParameters().cbegin(); i != cmd.getParameters().cend(); ++i)
	{
		if (i->compare(0, 2, "AD", 2) == 0)
		{
			bool tigrOk = false;
			string feat = i->substr(2);
			if (feat == FEATURE_ADC_BASE || feat == FEATURE_ADC_BAS0)
			{
				baseOk = true;
				// For bas0 tiger is implicit
				if (feat == FEATURE_ADC_BAS0)
					tigrOk = true;
				// ADC clients must support all these...
				setFlag(
					FLAG_SUPPORTS_ADCGET |
					FLAG_SUPPORTS_MINISLOTS |
					FLAG_SUPPORTS_TTHF |
					FLAG_SUPPORTS_TTHL |
					FLAG_SUPPORTS_XML_BZLIST // For compatibility with older clients...
				);
			}
			else if (feat == FEATURE_ZLIB_GET)
				setFlag(FLAG_SUPPORTS_ZLIB_GET);
			else if (feat == FEATURE_ADC_BZIP)
				setFlag(FLAG_SUPPORTS_XML_BZLIST);
			else if (feat == FEATURE_ADC_TIGR)
				tigrOk = true; // Variable 'tigrOk' is assigned a value that is never used.
			else if (feat == FEATURE_ADC_CPMI)
				setFlag(FLAG_SUPPORTS_CPMI);
#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
			else if (!feat.empty())
			{
				if (!unknownFeatures.empty()) unknownFeatures += ' ';
				unknownFeatures += feat;
			}
#endif
		}
	}

	if (!baseOk)
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Invalid SUP"));
		disconnect();
		return;
	}

	if (isSet(FLAG_INCOMING))
	{
		StringList features = ConnectionManager::getInstance()->getAdcFeatures();
		sup(features);
	}
	else
	{
		inf(true);
	}
	setState(STATE_INF);
}

void UserConnection::handle(AdcCommand::INF t, const AdcCommand& cmd)
{
	if (!checkState(STATE_INF, cmd))
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Expecting INF"));
		disconnect();
		return;
	}
	ConnectionManager::getInstance()->processINF(this, cmd);
}

void UserConnection::handle(AdcCommand::GET t, const AdcCommand& cmd)
{
	if (!checkState(STATE_GET, cmd)) return;
	if (isSet(FLAG_ASSOCIATED | FLAG_UPLOAD))
		UploadManager::getInstance()->processGET(this, cmd);
}

void UserConnection::handle(AdcCommand::SND t, const AdcCommand& cmd)
{
	if (!checkState(STATE_SND, cmd)) return;
	if (isSet(FLAG_ASSOCIATED | FLAG_DOWNLOAD))
		DownloadManager::getInstance()->processSND(this, cmd);
}

void UserConnection::handle(AdcCommand::GFI t, const AdcCommand& cmd)
{
	if (!checkState(STATE_GET, cmd)) return;
	if (isSet(FLAG_ASSOCIATED | FLAG_UPLOAD))
		UploadManager::getInstance()->processGFI(this, cmd);
}

void UserConnection::handle(AdcCommand::MSG t, const AdcCommand& cmd)
{
	if (!checkState(STATE_IDLE, cmd)) return;
	ConnectionManager::getInstance()->processMSG(this, cmd);
}

void UserConnection::handle(AdcCommand::PMI t, const AdcCommand& cmd)
{
	if (!checkState(STATE_IDLE, cmd)) return;
	ConnectionManager::getInstance()->processMSG(this, cmd);
}

void UserConnection::inf(bool withToken)
{
	AdcCommand c(AdcCommand::CMD_INF);
	c.addParam("ID", ClientManager::getMyCID().toBase32());
	if (withToken)
		c.addParam("TO", getUserConnectionToken());
	if (isSet(FLAG_CCPM))
		c.addParam("PM1");
	send(c);
}

void UserConnection::sup(const StringList& features)
{
	AdcCommand c(AdcCommand::CMD_SUP);
	for (const string& feature : features)
		c.addParam("AD" + feature);
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

void UserConnection::onConnected() noexcept
{
	updateLastActivity();
#if 0
	if (isSecure() && !isTrusted() && !BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS))
	{
		ConnectionManager::getInstance()->putConnection(this);
		LogManager::message(STRING(CERTIFICATE_NOT_TRUSTED));
		return;
	}
#endif
	dcassert(getState() == STATE_CONNECT);
	if (isSet(UserConnection::FLAG_NMDC))
	{
		myNick(getUserConnectionToken());
		lock(NmdcHub::getLock(), NmdcHub::getPk() + "Ref=" + getHubUrl());
	}
	else
	{
		StringList features = ConnectionManager::getInstance()->getAdcFeatures();
		sup(features);
		send(AdcCommand(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, Util::emptyString).addParam("RF", getHubUrl()));
	}
	setState(UserConnection::STATE_SUPNICK);
}

void UserConnection::onData(const uint8_t* data, size_t len)
{
	updateLastActivity();
#ifdef BL_FEATURE_IP_DATABASE
	if (len)
	{
		getUser()->loadIPStat();
		getUser()->addBytesDownloaded(getSocket()->getIp(), len);
	}
#endif
	DownloadManager::getInstance()->onData(this, data, len);
}

void UserConnection::onBytesSent(size_t bytes)
{
	updateLastActivity();
#ifdef BL_FEATURE_IP_DATABASE
	if (bytes)
	{
		getUser()->loadIPStat();
		getUser()->addBytesUploaded(getSocket()->getIp(), bytes);
	}
#endif
	dcassert(getState() == UserConnection::STATE_RUNNING);
	UploadPtr& upload = getUpload();
	if (upload) upload->addPos(bytes, bytes);
}

void UserConnection::onModeChange() noexcept
{
	updateLastActivity();
}

void UserConnection::onTransmitDone() noexcept
{
	UploadManager::getInstance()->transmitDone(this);
}

void UserConnection::onFailed(const string& line) noexcept
{
	if (state != STATE_UNUSED)
		state = STATE_UNCONNECTED;
	failed(line);
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

void UserConnection::send(const string& str)
{
	updateLastActivity();
	if (CMD_DEBUG_ENABLED()) COMMAND_DEBUG(str, DebugTask::CLIENT_OUT, getRemoteIpPort());
	socket->write(str);
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
		if (FavoriteManager::getInstance()->getFavUserParam(user, flags, limit))
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
		string cmd = "$MaxedOut";
		if (BOOLSETTING(SEND_QP_PARAM))
			cmd += ' ' + Util::toString(queuePosition);
		cmd += '|';
		send(cmd);
	}
	else
	{
		AdcCommand cmd(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_SLOTS_FULL, "Slots full");
		if (BOOLSETTING(SEND_QP_PARAM))
			cmd.addParam("QP", Util::toString(queuePosition));
		send(cmd);
	}
}

void UserConnection::fileNotAvail(const string& msg /*= FILE_NOT_AVAILABLE*/)
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
	if (d && BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
		LogManager::message("UserConnection(" + Util::toString(id) + "): Download " +
			download->getPath() + " from " + download->getUser()->getLastNick() +
			", p=" + Util::toHexString(this), false);
}

void UserConnection::setUpload(const UploadPtr& u)
{
	dcassert(isSet(FLAG_UPLOAD));
	upload = u;
	if (u && BOOLSETTING(LOG_SOCKET_INFO) && BOOLSETTING(LOG_SYSTEM))
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

void UcSupports::setSupports(UserConnection* conn, const StringList& feat, uint8_t& knownUcSupports, const string& source)
{
	for (auto i = feat.cbegin(); i != feat.cend(); ++i)
	{
#define CHECK_FEAT(feature) if (*i == UserConnection::FEATURE_##feature) { conn->setFlag(UserConnection::FLAG_SUPPORTS_##feature); knownUcSupports |= UserConnection::FLAG_SUPPORTS_##feature; }

		CHECK_FEAT(MINISLOTS) else
		CHECK_FEAT(XML_BZLIST) else
		CHECK_FEAT(ADCGET) else
		CHECK_FEAT(ZLIB_GET) else
		CHECK_FEAT(TTHL) else
		CHECK_FEAT(TTHF)
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
		else CHECK_FEAT(BANMSG)
#endif
#ifdef BL_FEATURE_COLLECT_UNKNOWN_FEATURES
		else collNmdcFeatures.addTag(*i, source);
#endif
#undef CHECK_FEAT
	}
}

string UcSupports::getSupports(const Identity& id)
{
	string tmp;
	const auto su = id.getKnownUcSupports();
#define CHECK_FEAT(feature) if (su & UserConnection::FLAG_SUPPORTS_##feature) { tmp += UserConnection::FEATURE_##feature + ' '; }

	CHECK_FEAT(MINISLOTS);
	CHECK_FEAT(XML_BZLIST);
	CHECK_FEAT(ADCGET);
	CHECK_FEAT(ZLIB_GET);
	CHECK_FEAT(TTHL);
	CHECK_FEAT(TTHF);
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
	CHECK_FEAT(BANMSG);
#endif

#undef CHECK_FEAT
	return tmp;
}
