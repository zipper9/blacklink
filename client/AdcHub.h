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

		static ClientBasePtr create(const string& hubURL, const string& address, uint16_t port, bool secure);
		void connect(const OnlineUserPtr& user, const string& token, bool forcePassive) override;

		int getType() const override { return TYPE_ADC; }
		void hubMessage(const string& message, bool thirdPerson = false) override;
		bool privateMessage(const OnlineUserPtr& user, const string& message, int flags) override;
		void sendUserCmd(const UserCommand& command, const StringMap& params) override;
		void password(const string& pwd, bool setPassword) override;
		void info(bool forceUpdate) override;
		void refreshUserList(bool) override;
		size_t getUserCount() const override
		{
			READ_LOCK(*csUsers);
			return users.size();
		}
		void checkNick(string& nick) const noexcept override;
		string escape(const string& str) const noexcept override
		{
			return AdcCommand::escape(str, false);
		}
		bool send(const AdcCommand& cmd) override;
		bool isMcPmSupported() const override { return false; }
		void processCCPMMessage(const AdcCommand& cmd, const OnlineUserPtr& ou) noexcept;

		string getMySID() const { return AdcCommand::fromSID(sid); }
		static const vector<StringList>& getSearchExts() { return searchExts; }
		static StringList parseSearchExts(int flag);

	protected:
		void searchToken(const SearchParam& sp) override;
		void getUsersToCheck(UserList& res, int64_t tick, int timeDiff) const noexcept override;

	private:
		friend class ClientManager;
		friend class CommandHandler<AdcHub>;
		friend class Identity;

		AdcHub(const string& hubURL, const string& address, uint16_t port, bool secure);
		~AdcHub();

		enum
		{
			FEATURE_FLAG_OLD_PASSWORD        = 1,
			FEATURE_FLAG_ALLOW_NAT_TRAVERSAL = 2,
			FEATURE_FLAG_USER_COMMANDS       = 4,
			FEATURE_FLAG_SEND_BLOOM          = 8
		};

		/** Map session id to OnlineUser */
		typedef boost::unordered_map<uint32_t, OnlineUserPtr> SIDMap;
		
		void connectUser(const OnlineUser& user, const string& token, bool secure, bool revConnect);
		void getUserList(OnlineUserList& list) const override;
		bool resendMyINFO(bool alwaysSend, bool forcePassive) override;

		unsigned featureFlags;
		int lastErrorCode;
		SIDMap users;
		std::unique_ptr<RWLock> csUsers;
		boost::unordered_map<uint16_t, string> lastInfoMap;

		void addInfoParam(AdcCommand& c, uint16_t param, const string& value);

		bool isFeatureSupported(unsigned feature) const
		{
			LOCK(csState);
			return (featureFlags & feature) != 0;
		}

		string salt;
		uint32_t sid;

		boost::unordered_set<uint32_t> forbiddenCommands;

		static const vector<StringList> searchExts;

		OnlineUserPtr getUser(uint32_t sid, const CID& cid, const string& nick);
		OnlineUserPtr addUser(uint32_t sid, const CID& cid, const string& nick);
		OnlineUserPtr findUser(uint32_t sid) const;
		OnlineUserPtr findUser(const CID& cid) const;

		// just a workaround
		OnlineUserPtr findUser(const string& nick) const override;

		void putUser(uint32_t sid, bool disconnect);

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
		
		void sendSearch(AdcCommand& c, SearchParamBase::SearchMode searchMode);
		void sendUDP(const AdcCommand& cmd) noexcept;
		void unknownProtocol(uint32_t target, const string& protocol, const string& p_token);
		
		void onConnected() noexcept override;
		void onDataLine(const string& line) noexcept override;
		void onFailed(const string& line) noexcept override;
};

#endif // !defined(ADC_HUB_H)
