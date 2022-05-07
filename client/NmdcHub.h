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

#ifndef DCPLUSPLUS_DCPP_NMDC_HUB_H
#define DCPLUSPLUS_DCPP_NMDC_HUB_H

#include "forward.h"
#include "User.h"
#include "Text.h"
#include "Client.h"

class ClientManager;
typedef boost::unordered_map<string, std::pair<std::string, unsigned>>  CFlyUnknownCommand;
typedef boost::unordered_map<string, std::unordered_map<std::string, unsigned> >  CFlyUnknownCommandArray;

class NmdcHub : public Client, private Flags
{
	public:
		using Client::send;
		using Client::connect;

		static ClientBasePtr create(const string& hubURL, const string& address, uint16_t port, bool secure);
		void connect(const OnlineUserPtr& user, const string& token, bool forcePassive);
		void disconnect(bool graceless) override;

		int getType() const { return TYPE_NMDC; }
		void hubMessage(const string& message, bool thirdPerson = false);
		bool privateMessage(const OnlineUserPtr& user, const string& message, bool thirdPerson, bool automatic);
		void sendUserCmd(const UserCommand& command, const StringMap& params);
		void searchToken(const SearchParamToken& sp);
		void password(const string& pwd, bool setPassword);
		void info(bool forceUpdate)
		{
			myInfo(forceUpdate);
		}
		size_t getUserCount() const
		{
			READ_LOCK(*csUsers);
			return users.size();
		}
		string escape(const string& str) const noexcept
		{
			return validateMessage(str, false);
		}
		bool convertNick(string& nick, bool& suffixAppended) const noexcept override
		{
			if (!nickRule)
			{
				suffixAppended = false;
				return true;
			}
			return nickRule->convertNick(nick, suffixAppended);
		}
		void checkNick(string& nick) const noexcept;
		static string unescape(const string& str)
		{
			return validateMessage(str, true);
		}
		bool send(const AdcCommand&)
		{
			dcassert(0);
			return false;
		}
		static string validateMessage(string tmp, bool reverse) noexcept;
		void refreshUserList(bool);
		
		void getUserList(OnlineUserList& result) const;

		static string makeKeyFromLock(const string& lock);
		static const string& getLock();
		static const string& getPk();
		static bool isExtended(const string& lock)
		{
			return strncmp(lock.c_str(), "EXTENDEDPROTOCOL", 16) == 0;
		}
		
	private:
		friend class ClientManager;
		enum SupportFlags
		{
			SUPPORTS_USERCOMMAND = 0x01,
			SUPPORTS_NOGETINFO   = 0x02,
			SUPPORTS_USERIP2     = 0x04,
#ifdef FLYLINKDC_USE_EXT_JSON
			SUPPORTS_EXTJSON2    = 0x08,
#endif
			SUPPORTS_NICKRULE    = 0x10,
			SUPPORTS_SEARCH_TTHS = 0x20, // $SA and $SP
			SUPPORTS_SEARCHRULE  = 0x40,
			SUPPORTS_SALT_PASS   = 0x80
		};
		
		enum
		{
			WAITING_FOR_MYINFO,
			MYINFO_LIST,
			MYINFO_LIST_COMPLETED
		};

		NmdcHub(const string& hubURL, const string& address, uint16_t port, bool secure);
		~NmdcHub();

		typedef boost::unordered_map<string, OnlineUserPtr> NickMap;
		
		NickMap users;
		std::unique_ptr<RWLock> csUsers;

		string   lastMyInfo;
		string   lastExtJSONInfo;
		string   salt;
		uint64_t lastUpdate;
		unsigned hubSupportFlags;
		char lastModeChar; // last Mode MyINFO
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool hubSupportsSlots;
#endif

	private:
		void updateMyInfoState(bool isMyInfo);
		
		int myInfoState;
		string m_cache_hub_url_flood;

		struct NickRule
		{
			static const size_t MAX_CHARS = 32;
			static const size_t MAX_PREFIXES = 16;
			unsigned minLen = 0;
			unsigned maxLen = 0;
			vector<char> invalidChars;
			vector<string> prefixes;
			bool convertNick(string& nick, bool& suffixAppended) const noexcept;
		};
		std::unique_ptr<NickRule> nickRule;

		void clearUsers();
		void onLine(const string& aLine);

		OnlineUserPtr getUser(const string& nick);
		OnlineUserPtr findUser(const string& nick) const;
		void putUser(const string& nick);
		bool getShareGroup(const string& seeker, CID& shareGroup, bool& hideShare) const;
		
		void privateMessage(const string& nick, const string& myNick, const string& message, bool thirdPerson);
		void version()
		{
			send("$Version 1,0091|");
		}
		void getNickList()
		{
			send("$GetNickList|");
		}
		void connectToMe(const OnlineUser& user, const string& token);
		                
		static void sendUDP(const string& address, uint16_t port, string& sr);
		void handleSearch(const NmdcSearchParam& searchParam);
		bool handlePartialSearch(const NmdcSearchParam& searchParam);
		string getMyExternalIP() const;
		void getMyUDPAddr(string& ip, uint16_t& port) const;
		void revConnectToMe(const OnlineUser& aUser);
		bool resendMyINFO(bool alwaysSend, bool forcePassive);
		void myInfo(bool alwaysSend, bool forcePassive = false);
		void myInfoParse(const string& param);
#ifdef FLYLINKDC_USE_EXT_JSON
		bool extJSONParse(const string& param);
#endif
		void searchParse(const string& param, int type);
		void connectToMeParse(const string& param);
		void revConnectToMeParse(const string& param);
		void hubNameParse(const string& param);
		void supportsParse(const string& param);
		void userCommandParse(const string& param);
		void lockParse(const string& aLine);
		void helloParse(const string& param);
		void userIPParse(const string& param);
		void botListParse(const string& param);
		void nickListParse(const string& param);
		void opListParse(const string& param);
		void toParse(const string& param);
		void chatMessageParse(const string& line);
		void updateFromTag(Identity& id, const string& tag);
		static int getEncodingFromDomain(const string& domain);

		void onConnected() noexcept override;
		void onDataLine(const string& l) noexcept override;
		void onFailed(const string&) noexcept override;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	public:
		bool slotsReported() const { return hubSupportsSlots; }
#endif // IRAINMAN_ENABLE_AUTO_BAN
};

#endif // DCPLUSPLUS_DCPP_NMDC_HUB_H
