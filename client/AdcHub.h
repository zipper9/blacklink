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

#ifndef DCPLUSPLUS_DCPP_ADC_HUB_H
#define DCPLUSPLUS_DCPP_ADC_HUB_H

#include "Client.h"
#include "AdcSupports.h"

class ClientManager;

class AdcHub : public Client, public CommandHandler<AdcHub>
{
	public:
		using Client::send;
		using Client::connect;
		
		void connect(const OnlineUser& user, const string& token, bool forcePassive);
		
		void hubMessage(const string& aMessage, bool thirdPerson = false);
		void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false);
		void sendUserCmd(const UserCommand& command, const StringMap& params);
		void searchToken(const SearchParamToken& sp);
		void password(const string& pwd);
		void info(bool forceUpdate);
		void refreshUserList(bool);
		size_t getUserCount() const
		{
			return m_adc_users.size();
		}
		void checkNick(string& nick) const noexcept;
		string escape(const string& str) const noexcept
		{
			return AdcCommand::escape(str, false);
		}
		void send(const AdcCommand& cmd);
		
		string getMySID() const
		{
			return AdcCommand::fromSID(sid);
		}
		
		static const vector<StringList>& getSearchExts();
		static StringList parseSearchExts(int flag);
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool slotsReported() const
		{
			return true;
		}
#endif // IRAINMAN_ENABLE_AUTO_BAN

	private:
		friend class ClientManager;
		friend class CommandHandler<AdcHub>;
		friend class Identity;

		enum
		{
			FEATURE_FLAG_OLD_PASSWORD        = 1,
			FEATURE_FLAG_ALLOW_NAT_TRAVERSAL = 2,
			FEATURE_FLAG_USER_COMMANDS       = 4,
			FEATURE_FLAG_SEND_BLOOM          = 8
		};
		
		AdcHub(const string& hubURL, bool secure);
		~AdcHub();
		
		/** Map session id to OnlineUser */
		typedef boost::unordered_map<uint32_t, OnlineUserPtr> SIDMap;
		
		void connectUser(const OnlineUser& user, const string& token, bool secure);
		void getUserList(OnlineUserList& list) const;
		bool resendMyINFO(bool alwaysSend, bool forcePassive);
		
		unsigned featureFlags;
		int lastErrorCode;
		SIDMap m_adc_users;
		StringMap m_lastInfoMap;
		FastCriticalSection m_info_cs;
		void addParam(AdcCommand& p_c, const string& p_var, const string& p_value);
		
		string salt;
		uint32_t sid;
		
		boost::unordered_set<uint32_t> forbiddenCommands;
		
		static const vector<StringList> m_searchExts;
		
		OnlineUserPtr getUser(const uint32_t aSID, const CID& aCID, const string& nick);
		OnlineUserPtr findUser(const uint32_t sid) const;
		OnlineUserPtr findUser(const CID& cid) const;
		
		// just a workaround
		OnlineUserPtr findUser(const string& aNick) const;
		
		void putUser(const uint32_t sid, bool disconnect);
		
		void clearUsers();
		
		void handle(AdcCommand::SUP, const AdcCommand& c) noexcept;
		void handle(AdcCommand::SID, const AdcCommand& c) noexcept;
		void handle(AdcCommand::MSG, const AdcCommand& c) noexcept;
		void handle(AdcCommand::INF, const AdcCommand& c) noexcept;
		void handle(AdcCommand::GPA, const AdcCommand& c) noexcept;
		void handle(AdcCommand::QUI, const AdcCommand& c) noexcept;
		void handle(AdcCommand::CTM, const AdcCommand& c) noexcept;
		void handle(AdcCommand::RCM, const AdcCommand& c) noexcept;
		void handle(AdcCommand::STA, const AdcCommand& c) noexcept;
		void handle(AdcCommand::SCH, const AdcCommand& c) noexcept;
		void handle(AdcCommand::CMD, const AdcCommand& c) noexcept;
		void handle(AdcCommand::RES, const AdcCommand& c) noexcept;
		void handle(AdcCommand::GET, const AdcCommand& c) noexcept;
		void handle(AdcCommand::NAT, const AdcCommand& c) noexcept;
		void handle(AdcCommand::RNT, const AdcCommand& c) noexcept;
		void handle(AdcCommand::PSR, const AdcCommand& c) noexcept;
		void handle(AdcCommand::ZON, const AdcCommand& c) noexcept;
		void handle(AdcCommand::ZOF, const AdcCommand& c) noexcept;
		
		
		template<typename T> void handle(T, const AdcCommand&) { }
		
		void sendSearch(AdcCommand& c);
		void sendUDP(const AdcCommand& cmd) noexcept;
		void unknownProtocol(uint32_t target, const string& protocol, const string& p_token);
		
		void onConnected() noexcept override;
		void onDataLine(const string& aLine) noexcept override;
		void onFailed(const string& aLine) noexcept override;
};

#endif // !defined(ADC_HUB_H)
