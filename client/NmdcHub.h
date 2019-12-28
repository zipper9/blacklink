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

#include "SettingsManager.h"
#include "forward.h"
#include "User.h"
#include "Text.h"
#include "ConnectionManager.h"
#include "UploadManager.h"
#include "ZUtils.h"

class ClientManager;
typedef boost::unordered_map<string, std::pair<std::string, unsigned>>  CFlyUnknownCommand;
typedef boost::unordered_map<string, std::unordered_map<std::string, unsigned> >  CFlyUnknownCommandArray;

class NmdcHub : public Client, private Flags
{
	public:
		using Client::send;
		using Client::connect;
		
		void connect(const OnlineUser& user, const string& token, bool forcePassive);
		void disconnect(bool graceless) override;
		
		void hubMessage(const string& aMessage, bool thirdPerson = false);
		void privateMessage(const OnlineUserPtr& aUser, const string& aMessage, bool thirdPerson = false);
		void sendUserCmd(const UserCommand& command, const StringMap& params);
		void searchToken(const SearchParamToken& sp);
		void password(const string& aPass)
		{
			send("$MyPass " + fromUtf8(aPass) + '|');
		}
		void info(bool forceUpdate)
		{
			myInfo(forceUpdate);
		}
		size_t getUserCount() const
		{
			return m_users.size();
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
		static string unescape(const string& str)
		{
			return validateMessage(str, true);
		}
		void send(const AdcCommand&)
		{
			dcassert(0);
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
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		void AutodetectComplete();
		bool IsAutodetectPending() const
		{
			return m_bAutodetectionPending;
		}
		void RequestConnectionForAutodetect();
#endif
		
	private:
		friend class ClientManager;
		enum SupportFlags
		{
			SUPPORTS_USERCOMMAND = 0x01,
			SUPPORTS_NOGETINFO = 0x02,
			SUPPORTS_USERIP2 = 0x04,
#ifdef FLYLINKDC_USE_EXT_JSON
			SUPPORTS_EXTJSON2 = 0x08,
#endif
			SUPPORTS_NICKRULE = 0x10,
			SUPPORTS_SEARCH_TTHS = 0x20, // —жатый формат поиска
			SUPPORTS_SEARCHRULE = 0x40,
		};
		
		enum
		{
			WAITING_FOR_MYINFO,
			MYINFO_LIST,
			MYINFO_LIST_COMPLETED
		};
		
		typedef boost::unordered_map<string, OnlineUserPtr> NickMap;
		
		NickMap  m_users;
		string   lastMyInfo;
		string   lastExtJSONInfo;
		int64_t  lastBytesShared;
		uint64_t lastUpdate;
		uint8_t  hubSupportFlags;
		uint8_t  m_version_fly_info;
		char lastModeChar; // last Mode MyINFO
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool m_hubSupportsSlots;//[+] FlylinkDC
#endif
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		bool m_bAutodetectionPending;
		bool m_is_get_user_ip_from_hub;
		
		int m_iRequestCount;
#endif
		static CFlyUnknownCommand g_unknown_command;
		static CFlyUnknownCommandArray g_unknown_command_array;
		static FastCriticalSection g_unknown_cs;
		static uint8_t g_version_fly_info;

	public:
		static void inc_version_fly_info()
		{
			++g_version_fly_info;
		}
		static void log_all_unknown_command();
		static string get_all_unknown_command();

	private:
		void processAutodetect(bool isMyInfo);
		
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

		NmdcHub(const string& hubURL, bool secure);
		~NmdcHub();
		
		void clearUsers();
		void onLine(const string& aLine);
		static void logPM(const UserPtr& user, const string& msg, const string& hubUrl);
		
		OnlineUserPtr getUser(const string& aNick);
		OnlineUserPtr findUser(const string& aNick) const;
		void putUser(const string& aNick);
		
		string getMyNickFromUtf8() const
		{
			return fromUtf8(getMyNick());
		}
		void privateMessage(const string& nick, const string& aMessage, bool thirdPerson);
		void sendValidateNick(const string& aNick)
		{
			send("$ValidateNick " + fromUtf8(aNick) + '|');
		}
		void key(const string& aKey)
		{
			send("$Key " + aKey + '|');
		}
		void version()
		{
			send("$Version 1,0091|"); // TODO - ?
		}
		void getNickList()
		{
			send("$GetNickList|");
		}
		void connectToMe(const OnlineUser& aUser
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                 , ExpectedMap::DefinedExpectedReason reason = ExpectedMap::REASON_DEFAULT
#endif
		                );
		                
		static void sendPacket(const string& address, uint16_t port, string& sr);
		void handleSearch(const SearchParam& searchParam);
		bool handlePartialSearch(const SearchParam& searchParam);
		string calcExternalIP() const;
		void revConnectToMe(const OnlineUser& aUser);
		bool resendMyINFO(bool alwaysSend, bool forcePassive);
		void myInfo(bool alwaysSend, bool forcePassive = false);
		void myInfoParse(const string& param);
#ifdef FLYLINKDC_USE_EXT_JSON
		bool extJSONParse(const string& param, bool p_is_disable_fire = false);
// #define FLYLINKDC_USE_EXT_JSON_GUARD
#ifdef FLYLINKDC_USE_EXT_JSON_GUARD
		std::unordered_map<string, string> m_ext_json_deferred;
#endif // FLYLINKDC_USE_EXT_JSON_GUARD
#endif
		void searchParse(const string& param);
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
		void chatMessageParse(const string& aLine);
		void supports(const StringList& feat);
		void updateFromTag(Identity& id, const string& tag, bool p_is_version_change);
		
		virtual void checkNick(string& p_nick);
		
		void onConnected() noexcept override;
		void onDataLine(const string& l) noexcept override;
		void onDDoSSearchDetect(const string&) noexcept override;
		void onFailed(const string&) noexcept override;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	public:
		bool hubIsNotSupportSlot() const //[+]FlylinkDC
		{
			return m_hubSupportsSlots;
		}
#endif // IRAINMAN_ENABLE_AUTO_BAN
};

#endif // DCPLUSPLUS_DCPP_NMDC_HUB_H
