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

#ifndef HUB_ENTRY_H_
#define HUB_ENTRY_H_

#include "BaseUtil.h"
#include "CID.h"
#include "ConnectionStatus.h"
#include "Text.h"

class HubEntry
{
	public:
		typedef vector<HubEntry> List;

		static bool checkKeyPrintFormat(const string& kp);

		HubEntry() : reliability(0), shared(0), minShare(0), users(0), minSlots(0), maxHubs(0), maxUsers(0)
		{
		}

		HubEntry(const string& name, const string& server, const string& description, const string& users, const string& country,
		         const string& shared, const string& minShare, const string& minSlots, const string& maxHubs, const string& maxUsers,
		         const string& reliability, const string& rating, const string& encoding,
		         const string& secureUrl, const string& website, const string& email,
		         const string& software, const string& network);

		GETSET(string, name, Name);
		GETSET(string, server, Server);
		GETSET(string, keyPrint, KeyPrint);
		GETSET(string, description, Description);
		GETSET(string, country, Country);
		GETSET(string, rating, Rating);

		GETSET(float, reliability, Reliability);
		GETSET(int64_t, shared, Shared);
		GETSET(int64_t, minShare, MinShare);
		GETSET(int, users, Users);
		GETSET(int, minSlots, MinSlots);
		GETSET(int, maxHubs, MaxHubs);
		GETSET(int, maxUsers, MaxUsers);

		GETSET(string, encoding, Encoding);
		GETSET(string, secureUrl, SecureUrl);
		GETSET(string, website, Website);
		GETSET(string, email, Email);
		GETSET(string, software, Software);
		GETSET(string, network, Network);
};

class FavoriteHubEntry
{
	public:
		typedef vector<FavoriteHubEntry*> List;
		
		FavoriteHubEntry() noexcept :
			id(0),
			autoConnect(false), encoding(Text::CHARSET_SYSTEM_DEFAULT), preferIP6(false),
			windowposx(0), windowposy(0), windowsizex(0),
			windowsizey(0), windowtype(0), chatUserSplit(0),
			hideUserList(false),
			swapPanels(false),
			hideShare(false),
			fakeFileCount(-1),
			fakeSlots(-1),
			fakeClientStatus(0),
			exclusiveHub(false), showJoins(false), exclChecks(false), mode(0),
			searchInterval(0),
			searchIntervalPassive(0),
			overrideId(0),
			headerSort(-1), headerSortAsc(true), suppressChatAndPM(false)
		{
		}

		string getNick(bool useDefault = true) const;
		void setNick(const string& newNick) { nick = newNick; }

		GETSET(string, userDescription, UserDescription);
		GETSET(string, awayMsg, AwayMsg);
		GETSET(string, email, Email);
		GETSET(string, name, Name);
		GETSET(string, server, Server);
		GETSET(string, keyPrint, KeyPrint);
		GETSET(string, description, Description);
		GETSET(string, password, Password);
		GETSET(string, headerOrder, HeaderOrder);
		GETSET(string, headerWidths, HeaderWidths);
		GETSET(string, headerVisible, HeaderVisible);
		GETSET(int, headerSort, HeaderSort);
		GETSET(bool, headerSortAsc, HeaderSortAsc);
		GETSET(bool, autoConnect, AutoConnect);
		GETSET(int, windowposx, WindowPosX);
		GETSET(int, windowposy, WindowPosY);
		GETSET(int, windowsizex, WindowSizeX);
		GETSET(int, windowsizey, WindowSizeY);
		GETSET(int, windowtype, WindowType);
		GETSET(int, chatUserSplit, ChatUserSplit);
		GETSET(bool, hideUserList, HideUserList);
		GETSET(bool, swapPanels, SwapPanels);
		GETSET(bool, hideShare, HideShare);
		GETSET(bool, showJoins, ShowJoins);
		GETSET(bool, exclChecks, ExclChecks); // Excl. from client checking
		GETSET(bool, exclusiveHub, ExclusiveHub); // Fake hub count
		GETSET(string, fakeShare, FakeShare);
		GETSET(int64_t, fakeFileCount, FakeFileCount);
		GETSET(int, fakeSlots, FakeSlots);
		GETSET(int, fakeClientStatus, FakeClientStatus);
		GETSET(bool, suppressChatAndPM, SuppressChatAndPM);
		GETSET(int, mode, Mode); // 0 = default, 1 = active, 2 = passive
		GETSET(string, ip, IP);
		GETSET(string, opChat, OpChat);
		GETSET(string, clientName, ClientName);
		GETSET(string, clientVersion, ClientVersion);
		GETSET(bool, overrideId, OverrideId);
		GETSET(CID, shareGroup, ShareGroup);
		GETSET(uint32_t, searchInterval, SearchInterval);
		GETSET(uint32_t, searchIntervalPassive, SearchIntervalPassive);
		GETSET(int, encoding, Encoding);
		GETSET(bool, preferIP6, PreferIP6);
		GETSET(string, group, Group);

		int getID() const { return id; }

		const ConnectionStatus& getConnectionStatus() const { return connectionStatus; }
		ConnectionStatus& getConnectionStatus() { return connectionStatus; }

		static double parseSizeString(const string& s, double* origSize = nullptr, int* unit = nullptr);

	private:
		int id;
		string nick;
		ConnectionStatus connectionStatus;

		friend class FavoriteManager;
};

class RecentHubEntry
{
	public:
		typedef vector<RecentHubEntry*> List;

		explicit RecentHubEntry() : name("*"),
			description("*"),
			users("*"),
			shared("*"),
			lastseen("*"),
			opentab("-"),
			autoopen(false),
			redirect(false) {}

		GETSET(string, name, Name);
		GETSET(string, server, Server);
		GETSET(string, description, Description);
		GETSET(string, users, Users);
		GETSET(string, shared, Shared);
		GETSET(string, lastseen, LastSeen);
		GETSET(string, opentab, OpenTab);
		GETSET(bool, autoopen, AutoOpen);
		GETSET(bool, redirect, Redirect);
};

#endif // HUB_ENTRY_H_
