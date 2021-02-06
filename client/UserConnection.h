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

#ifndef DCPLUSPLUS_DCPP_USER_CONNECTION_H
#define DCPLUSPLUS_DCPP_USER_CONNECTION_H

#include "UserConnectionListener.h"
#include "FavoriteUser.h"
#include "BufferedSocket.h"
#include "Upload.h"
#include "Download.h"
#include "Speaker.h"
#include "OnlineUser.h"
#include <atomic>

class UserConnection :
	public Speaker<UserConnectionListener>,
	private BufferedSocketListener,
	public Flags,
	private CommandHandler<UserConnection>
{
	public:
		friend class ConnectionManager;
		
		static const string FEATURE_MINISLOTS;
		static const string FEATURE_XML_BZLIST;
		static const string FEATURE_ADCGET;
		static const string FEATURE_ZLIB_GET;
		static const string FEATURE_TTHL;
		static const string FEATURE_TTHF;
		static const string FEATURE_ADC_BAS0;
		static const string FEATURE_ADC_BASE;
		static const string FEATURE_ADC_BZIP;
		static const string FEATURE_ADC_TIGR;
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
		static const string FEATURE_BANMSG;
#endif
		
		static const string FILE_NOT_AVAILABLE;
#if defined (FLYLINKDC_USE_DOS_GUARD) && defined (IRAINMAN_DISALLOWED_BAN_MSG)
		static const string g_PLEASE_UPDATE_YOUR_CLIENT;
#endif
		
		enum KnownSupports
		{
			FLAG_SUPPORTS_MINISLOTS     = 1,
			FLAG_SUPPORTS_XML_BZLIST    = 1 << 1,
			FLAG_SUPPORTS_ADCGET        = 1 << 2,
			FLAG_SUPPORTS_ZLIB_GET      = 1 << 3,
			FLAG_SUPPORTS_TTHL          = 1 << 4,
			FLAG_SUPPORTS_TTHF          = 1 << 5,
			FLAG_SUPPORTS_BANMSG        = 1 << 6,
			FLAG_SUPPORTS_LAST = FLAG_SUPPORTS_BANMSG
		};
		
		enum Flags
		{
			FLAG_INTERNAL_FIRST = FLAG_SUPPORTS_LAST,
			FLAG_NMDC           = FLAG_INTERNAL_FIRST << 1,
			FLAG_UPLOAD         = FLAG_INTERNAL_FIRST << 2,
			FLAG_DOWNLOAD       = FLAG_INTERNAL_FIRST << 3,
			FLAG_INCOMING       = FLAG_INTERNAL_FIRST << 4,
			FLAG_ASSOCIATED     = FLAG_INTERNAL_FIRST << 5,
			FLAG_SECURE         = FLAG_INTERNAL_FIRST << 6
		};
		
		enum States
		{
			// ConnectionManager
			STATE_UNCONNECTED,
			STATE_CONNECT,
			
			// Handshake
			STATE_SUPNICK,      // ADC: SUP, Nmdc: $Nick
			STATE_INF,
			STATE_LOCK,
			STATE_DIRECTION,
			STATE_KEY,
			
			// UploadManager
			STATE_GET,          // Waiting for GET
			STATE_SEND,         // Waiting for $Send
			
			// DownloadManager
			STATE_SND,  // Waiting for SND
			STATE_IDLE, // No more downloads for the moment
			
			// Up & down
			STATE_RUNNING,      // Transmitting data

			STATE_UNUSED        // Connection should be removed by ConnectionManager
		};
		
		enum SlotTypes
		{
			NOSLOT      = 0,
			STDSLOT     = 1,
			EXTRASLOT   = 2,
			PARTIALSLOT = 3
		};
		
		int16_t getNumber() const
		{
			return (int16_t)((((size_t)this) >> 2) & 0x7fff);
		}
#ifdef DEBUG_USER_CONNECTION
		void dumpInfo() const;
#endif
		bool isIpBlocked(bool isDownload);
		// NMDC stuff
		void myNick(const string& aNick)
		{
			send("$MyNick " + Text::fromUtf8(aNick, lastEncoding) + '|');
		}
		void lock(const string& aLock, const string& aPk)
		{
			send("$Lock " + aLock + " Pk=" + aPk + '|');
		}
		void key(const string& aKey)
		{
			send("$Key " + aKey + '|');
		}
		void direction(const string& aDirection, int aNumber)
		{
			send("$Direction " + aDirection + ' ' + Util::toString(aNumber) + '|');
		}
		void fileLength(const string& aLength)
		{
			send("$FileLength " + aLength + '|');
		}
		void error(const string& aError)
		{
			send("$Error " + aError + '|');
		}
		void listLen(const string& aLength)
		{
			send("$ListLen " + aLength + '|');
		}
		
		void maxedOut(size_t queuePosition);
		void fileNotAvail(const string& msg = FILE_NOT_AVAILABLE);
		void yourIpIsBlocked();
		void supports(const StringList& feat);
		
		// ADC Stuff
		void sup(const StringList& features);
		void inf(bool withToken);
		void send(const AdcCommand& c)
		{
			send(c.toString(0, isSet(FLAG_NMDC)));
		}
		
		void setDataMode(int64_t bytes = -1)
		{
			dcassert(socket);
			if (socket)
				socket->setDataMode(bytes);
		}
		void setLineMode()
		{
			dcassert(socket);
			if (socket)
				socket->setMode(BufferedSocket::MODE_LINE);
		}
		
		void connect(const string& aServer, uint16_t aPort, uint16_t localPort, const BufferedSocket::NatRoles natRole);
		void addAcceptedSocket(unique_ptr<Socket>& newSock, uint16_t port);
		
		void updated()
		{
			dcassert(socket);
			if (socket)
				socket->updated();
		}
		
		void disconnect(bool graceless = false)
		{
			if (socket)
				socket->disconnect(graceless);
		}
		void transmitFile(InputStream* f)
		{
			dcassert(socket);
			if (socket)
				socket->transmitFile(f);
		}
		
		const string& getDirectionString() const
		{
			static const string upload   = "Upload";
			static const string download = "Download";
			dcassert(isSet(FLAG_UPLOAD) ^ isSet(FLAG_DOWNLOAD));
			return isSet(FLAG_UPLOAD) ? upload : download;
		}
		
		const UserPtr& getUser() const { return hintedUser.user; }
		const UserPtr& getUser() { return hintedUser.user; }
		const HintedUser& getHintedUser() const { return hintedUser; }
		const HintedUser& getHintedUser() { return hintedUser; }
		
		bool isSecure() const
		{
			dcassert(socket);
			return socket && socket->isSecure();
		}
		bool isTrusted() const
		{
			dcassert(socket);
			return socket && socket->isTrusted();
		}
		string getCipherName() const noexcept
		{
			dcassert(socket);
			return socket ? socket->getCipherName() : Util::emptyString;
		}
		
		vector<uint8_t> getKeyprint() const
		{
			dcassert(socket);
			return socket ? socket->getKeyprint() : Util::emptyByteVector;
		}
		
		bool verifyKeyprint(const string& expKeyp, bool allowUntrusted)  noexcept
		{
			return socket ? socket->verifyKeyprint(expKeyp, allowUntrusted) : true;
		}

		string getRemoteIpPort() const
		{
			dcassert(socket);
			return socket ? socket->getRemoteIpPort() : Util::emptyString;
		}
		
		Ip4Address getRemoteIp() const
		{
			dcassert(socket); 
			return socket ? socket->getIp4() : 0;
		}

		DownloadPtr& getDownload()
		{
			dcassert(isSet(FLAG_DOWNLOAD));
			return download;
		}

		const DownloadPtr& getDownload() const
		{
			dcassert(isSet(FLAG_DOWNLOAD));
			return download;
		}

		void setDownload(const DownloadPtr& d);
		UploadPtr& getUpload()
		{
			dcassert(isSet(FLAG_UPLOAD));
			return upload;
		}
		void setUpload(const UploadPtr& u);
		
		void handle(AdcCommand::SUP t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::INF t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::GET t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::SND t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::STA t, const AdcCommand& c);
		void handle(AdcCommand::RES t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		void handle(AdcCommand::GFI t, const AdcCommand& c)
		{
			fly_fire2(t, this, c);
		}
		
		// Ignore any other ADC commands for now
		template<typename T> void handle(T, const AdcCommand&) { }
		
		int64_t getChunkSize() const
		{
			return chunkSize;
		}
		void updateChunkSize(int64_t leafSize, int64_t lastChunk, uint64_t ticks);
		
		void setHubUrl(const string& hubUrl)
		{
#ifdef _DEBUG
			if (hubUrl != "DHT")
				dcassert(hubUrl == Text::toLower(hubUrl));
#endif
			hintedUser.hint = hubUrl;
		}
		const string& getHubUrl()
		{
			return hintedUser.hint;
		}
		
		GETSET(string, userConnectionToken, UserConnectionToken);
		GETSET(string, connectionQueueToken, ConnectionQueueToken);
		GETSET(int64_t, speed, Speed);
		
		void updateLastActivity();
		uint64_t getLastActivity() const { return lastActivity; }

	public:
		GETSET(int, lastEncoding, Encoding);
		GETSET(States, state, State);
		GETSET(SlotTypes, slotType, SlotType);
		GETSET(string, m_server_port, ServerPort); // CTM2HUB
		const BufferedSocket* getSocket() const { return socket; }
		void setLastUploadSpeed(int64_t speed) { lastUploadSpeed = speed; }
		int64_t getLastUploadSpeed() const { return lastUploadSpeed; }
		void setLastDownloadSpeed(int64_t speed) { lastDownloadSpeed = speed; }
		int64_t getLastDownloadSpeed() const { return lastDownloadSpeed; }
		string getDescription() const;

	private:
		int id;
		int64_t chunkSize;
		BufferedSocket* socket;
		uint64_t lastActivity;
		HintedUser hintedUser;
		
		DownloadPtr download;
		UploadPtr upload;

		std::atomic<int64_t> lastDownloadSpeed;
		std::atomic<int64_t> lastUploadSpeed;
		
		// We only want ConnectionManager to create this...
		UserConnection() noexcept;
		UserConnection(const UserConnection &) = delete;
		UserConnection& operator= (const UserConnection&) = delete;
		virtual ~UserConnection();

		void setUser(const UserPtr& user);
		void send(const string& str);
		void setUploadLimit(int limit);
		void setDefaultLimit();
		
		// BufferedConnectionListener
		void onConnected() noexcept override;
		void onDataLine(const string&) noexcept override;
		void onModeChange() noexcept override;
		void onTransmitDone() noexcept override;
		void onFailed(const string&) noexcept override;
		void onUpdated() noexcept override;
		void onBytesSent(size_t bytes, size_t actual);
		void onData(const uint8_t* data, size_t len);
		void onUpgradedToSSL() noexcept override;
};

class UcSupports
{
	public:
		static StringList setSupports(UserConnection* conn, const StringList& feat, uint8_t& knownUcSupports)
		{
			StringList unknownSupports;
			for (auto i = feat.cbegin(); i != feat.cend(); ++i)
			{
			
#define CHECK_FEAT(feature) if (*i == UserConnection::FEATURE_##feature) { conn->setFlag(UserConnection::FLAG_SUPPORTS_##feature); knownUcSupports |= UserConnection::FLAG_SUPPORTS_##feature; }
			
				CHECK_FEAT(MINISLOTS) else
				CHECK_FEAT(XML_BZLIST) else
				CHECK_FEAT(ADCGET) else
				CHECK_FEAT(ZLIB_GET) else
				CHECK_FEAT(TTHL) else
				CHECK_FEAT(TTHF) else
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
				CHECK_FEAT(BANMSG) else
#endif
				{
					unknownSupports.push_back(*i);
				}
										
#undef CHECK_FEAT
										
			}
			return unknownSupports;
		}
		
		static string getSupports(const Identity& id)
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
};


#endif // !defined(USER_CONNECTION_H)
