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
#include "FavoriteManager.h"
#include "ClientManager.h"
#include "StringTokenizer.h"
#include "SimpleXML.h"
#include "ConnectionManager.h"
#include "LogManager.h"
#include "DatabaseManager.h"
#include <boost/algorithm/string.hpp>

static const unsigned SAVE_RECENTS_TIME = 3*60000;
static const unsigned SAVE_FAVORITES_TIME = 60000;

FavoriteManager::FavoriteManager()
{
	dontSave = 0;
	userCommandId = 0;
	recentsDirty = false;
	favsDirty = false;
	recentsLastSave = 0;
	favsLastSave = 0;
	favHubId = 0;
	csHubs = std::unique_ptr<RWLock>(RWLock::create());
	csUsers = std::unique_ptr<RWLock>(RWLock::create());
	csUserCommand = std::unique_ptr<RWLock>(RWLock::create());
	csDirs = std::unique_ptr<RWLock>(RWLock::create());
	ClientManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);

	File::ensureDirectory(Util::getHubListsPath());
}

FavoriteManager::~FavoriteManager()
{
	ClientManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);

	shutdown();

	for_each(hubs.begin(), hubs.end(), [](auto p) { delete p; });
	for_each(recentHubs.begin(), recentHubs.end(), [](auto p) { delete p; });
	PreviewApplication::clearList(previewApplications);
}

void FavoriteManager::shutdown()
{
	if (recentsDirty)
		saveRecents();
	if (favsDirty)
		saveFavorites();
}

void FavoriteManager::splitClientId(const string& id, string& name, string& version)
{
	const auto space = id.find(' ');
	if (space == string::npos)
	{
		name = id;
		version.clear();
		return;
	}
	name = id.substr(0, space);
	version = id.substr(space + 1);
	const auto marker = version.find("V:");
	if (marker != string::npos)
		version = version.substr(marker + 2);
}

// User commands

UserCommand FavoriteManager::addUserCommand(int type, int ctx, Flags::MaskType flags, const string& name, const string& command, const string& to, const string& hub)
{
	const UserCommand uc(userCommandId++, type, ctx, flags, name, command, to, hub);
	{
		// No dupes, add it...
		WRITE_LOCK(*csUserCommand);
		userCommands.push_back(uc);
	}
	if (!uc.isSet(UserCommand::FLAG_NOSAVE))
		saveFavorites();

	return uc;
}

bool FavoriteManager::getUserCommand(int id, UserCommand& uc) const
{
	READ_LOCK(*csUserCommand);
	for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
	{
		if (i->getId() == id)
		{
			uc = *i;
			return true;
		}
	}
	return false;
}

bool FavoriteManager::moveUserCommand(int id, int delta)
{
	dcassert(delta == -1 || delta == 1);
	WRITE_LOCK(*csUserCommand);
	if (delta == -1)
	{
		auto prev = userCommands.end();
		for (auto i = userCommands.begin(); i != userCommands.end(); ++i)
		{
			if (i->getId() == id)
			{
				if (prev == userCommands.end()) return false;
				std::swap(*i, *prev);
				return true;
			}
			prev = i;
		}
	}
	else if (delta == 1)
	{
		for (auto i = userCommands.begin(); i != userCommands.end(); ++i)
		{
			if (i->getId() == id)
			{
				auto next = i;
				if (++next == userCommands.end()) return false;
				std::swap(*i, *next);
				return true;
			}
		}
	}
	return false;
}

void FavoriteManager::updateUserCommand(const UserCommand& uc)
{
	{
		WRITE_LOCK(*csUserCommand);
		for (auto i = userCommands.begin(); i != userCommands.end(); ++i)
		{
			if (i->getId() == uc.getId())
			{
				*i = uc;
				if (uc.isSet(UserCommand::FLAG_NOSAVE))
					return;
				else
					break;
			}
		}
	}
	saveFavorites();
}

int FavoriteManager::findUserCommand(const string& name, const string& hub) const
{
	READ_LOCK(*csUserCommand);
	for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
		if (i->getName() == name && i->getHub() == hub)
			return i->getId();
	return -1;
}

void FavoriteManager::removeUserCommand(int id)
{
	{
		WRITE_LOCK(*csUserCommand);
		for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
		{
			if (i->getId() == id)
			{
				bool nosave = i->isSet(UserCommand::FLAG_NOSAVE);
				userCommands.erase(i);
				if (nosave) return;
				break;
			}
		}
	}
	saveFavorites();
}

// Users

bool FavoriteManager::addUserL(const UserPtr& user, FavoriteMap::iterator& iUser, bool create /*= true*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	iUser = favoriteUsers.find(user->getCID());
	if (iUser == favoriteUsers.end() && create)
	{
		FavoriteUser favUser(user);
		StringList hubs = ClientManager::getHubs(user->getCID(), Util::emptyString);
		if (!hubs.empty())
			favUser.url = std::move(hubs[0]);
		StringList nicks = ClientManager::getNicks(user->getCID(), favUser.url, !favUser.url.empty(), true);
		if (!nicks.empty())
			favUser.nick = std::move(nicks[0]);
		else
			favUser.nick = user->getLastNick();
		if (favUser.nick.empty())
			return false;
		favUser.lastSeen = GET_TIME();
		iUser = favoriteUsers.insert(make_pair(user->getCID(), favUser)).first;
		return true;
	}
	return false;
}

bool FavoriteManager::getFavUserParam(const UserPtr& user, FavoriteUser::MaskType& flags, int& uploadLimit) const
{
	READ_LOCK(*csUsers);
	auto i = favoriteUsers.find(user->getCID());
	if (i != favoriteUsers.end())
	{
		const FavoriteUser& favUser = i->second;
		flags = favUser.getFlags();
		uploadLimit = favUser.uploadLimit;
		return true;
	}
	return false;
}

bool FavoriteManager::getFavUserParam(const UserPtr& user, FavoriteUser::MaskType& flags, int& uploadLimit, CID& shareGroup) const
{
	READ_LOCK(*csUsers);
	auto i = favoriteUsers.find(user->getCID());
	if (i != favoriteUsers.end())
	{
		const FavoriteUser& favUser = i->second;
		flags = favUser.getFlags();
		uploadLimit = favUser.uploadLimit;
		shareGroup = favUser.shareGroup;
		return true;
	}
	return false;
}

bool FavoriteManager::getFavoriteUser(const UserPtr& user, FavoriteUser& favuser) const
{
	READ_LOCK(*csUsers);
	auto i = favoriteUsers.find(user->getCID());
	if (i != favoriteUsers.end())
	{
		favuser = i->second;
		return true;
	}
	return false;
}

bool FavoriteManager::isFavoriteUser(const UserPtr& user, bool& isBanned) const
{
	READ_LOCK(*csUsers);
	bool result;
	auto i = favoriteUsers.find(user->getCID());
	if (i != favoriteUsers.end())
	{
		result = true;
		isBanned = i->second.uploadLimit == FavoriteUser::UL_BAN;
	}
	else
	{
		isBanned = false;
		result = false;
	}
	return result;
}

bool FavoriteManager::addFavoriteUser(const UserPtr& user)
{
	FavoriteMap::iterator i;
	FavoriteUser favUser;
	{
		WRITE_LOCK(*csUsers);
		if (!addUserL(user, i))
			return false;
		favUser = i->second;
		user->setFlag(User::FAVORITE);
	}
	fire(FavoriteManagerListener::UserAdded(), favUser);
	favsDirty = true;
	return true;
}

bool FavoriteManager::removeFavoriteUser(const UserPtr& user)
{
	FavoriteUser favUser;
	{
		WRITE_LOCK(*csUsers);
		user->unsetFlag(User::FAVORITE | User::BANNED);
		auto i = favoriteUsers.find(user->getCID());
		if (i == favoriteUsers.end())
			return false;
		favUser = i->second;
		favoriteUsers.erase(i);
	}
	fire(FavoriteManagerListener::UserRemoved(), favUser);
	favsDirty = true;
	return true;
}

string FavoriteManager::getUserUrl(const UserPtr& user) const
{
	READ_LOCK(*csUsers);
	const auto i = favoriteUsers.find(user->getCID());
	if (i != favoriteUsers.end())
		return i->second.url;
	return string();
}

// Hubs

bool FavoriteManager::isFavoriteHub(const string& server, int excludeID) const
{
	READ_LOCK(*csHubs);
	if (!excludeID)
		return getFavoriteHubByUrlL(server) != nullptr;
	for (auto i = hubs.cbegin(); i != hubs.cend(); ++i)
		if ((*i)->getID() != excludeID && (*i)->getServer() == server)
			return true;
	return false;
}

bool FavoriteManager::addFavoriteHub(FavoriteHubEntry& entry, bool save)
{
	{
		WRITE_LOCK(*csHubs);
		if (getFavoriteHubByUrlL(entry.getServer()))
			return false;
		FavoriteHubEntry* fhe = new FavoriteHubEntry(entry);
		entry.id = fhe->id = ++favHubId;
		hubs.push_back(fhe);
		hubsByUrl[entry.getServer()] = fhe;
	}
	fire(FavoriteManagerListener::FavoriteAdded(), &entry);
	if (save)
		saveFavorites();
	return true;
}

bool FavoriteManager::removeFavoriteHub(const string& server, bool save)
{
	FavoriteHubEntry* entry = nullptr;
	{
		WRITE_LOCK(*csHubs);
		auto j = hubsByUrl.find(server);
		if (j == hubsByUrl.end()) return false;
		auto fhe = j->second;
		hubsByUrl.erase(j);
		for (auto i = hubs.cbegin(); i != hubs.cend(); ++i)
			if (*i == fhe)
			{
				entry = fhe;
				hubs.erase(i);
				break;
			}
	}
	if (!entry)
		return false;
	fire(FavoriteManagerListener::FavoriteRemoved(), entry);
	delete entry;
	if (save)
		saveFavorites();
	return true;
}

bool FavoriteManager::removeFavoriteHub(int id, bool save)
{
	FavoriteHubEntry* entry = nullptr;
	{
		WRITE_LOCK(*csHubs);
		for (auto i = hubs.cbegin(); i != hubs.cend(); ++i)
			if ((*i)->getID() == id)
			{
				entry = *i;
				hubs.erase(i);
				break;
			}
	}
	if (!entry)
		return false;
	fire(FavoriteManagerListener::FavoriteRemoved(), entry);
	delete entry;
	if (save)
		saveFavorites();
	return true;
}

bool FavoriteManager::setFavoriteHub(const FavoriteHubEntry& entry)
{
	bool result = false;
	{
		FavoriteHubEntry* fhe = nullptr;
		string prevUrl;
		WRITE_LOCK(*csHubs);
		for (auto i = hubs.begin(); i != hubs.end(); ++i)
		{
			fhe = *i;
			if (fhe->getID() == entry.getID())
			{
				prevUrl = fhe->getServer();
				*fhe = entry;
				result = true;
				break;
			}
		}
		if (result && prevUrl != entry.getServer())
		{
			auto j = hubsByUrl.find(prevUrl);
			if (j != hubsByUrl.end()) hubsByUrl.erase(j);
			hubsByUrl[fhe->getServer()] = fhe;
		}
	}
	if (result)
	{
		saveFavorites();
		fire(FavoriteManagerListener::FavoriteChanged(), &entry);
	}
	return result;
}

bool FavoriteManager::getFavoriteHub(const string& server, FavoriteHubEntry& entry) const
{
	READ_LOCK(*csHubs);
	const FavoriteHubEntry* fhe = getFavoriteHubByUrlL(server);
	if (!fhe) return false;
	entry = *fhe;
	return true;
}

bool FavoriteManager::getFavoriteHub(int id, FavoriteHubEntry& entry) const
{
	READ_LOCK(*csHubs);
	for (auto i = hubs.cbegin(); i != hubs.cend(); ++i)
	{
		FavoriteHubEntry* fhe = *i;
		if (fhe->getID() == id)
		{
			entry = *fhe;
			return true;
		}
	}
	return false;
}

bool FavoriteManager::setFavoriteHubWindowInfo(const string& server, const WindowInfo& wi)
{
	WRITE_LOCK(*csHubs);
	FavoriteHubEntry* fhe = getFavoriteHubByUrlL(server);
	if (!fhe) return false;
	if (wi.windowSizeX != -1 && wi.windowSizeY != -1)
	{
		fhe->setWindowPosX(wi.windowPosX);
		fhe->setWindowPosY(wi.windowPosY);
		fhe->setWindowSizeX(wi.windowSizeX);
		fhe->setWindowSizeY(wi.windowSizeY);
	}
	fhe->setWindowType(wi.windowType);
	fhe->setChatUserSplit(wi.chatUserSplit);
	fhe->setSwapPanels(wi.swapPanels);
	fhe->setHideUserList(wi.hideUserList);
	fhe->setHeaderOrder(wi.headerOrder);
	fhe->setHeaderWidths(wi.headerWidths);
	fhe->setHeaderVisible(wi.headerVisible);
	fhe->setHeaderSort(wi.headerSort);
	fhe->setHeaderSortAsc(wi.headerSortAsc);
	favsDirty = true;
	return true;
}

bool FavoriteManager::getFavoriteHubWindowInfo(const string& server, WindowInfo& wi) const
{
	READ_LOCK(*csHubs);
	const FavoriteHubEntry* fhe = getFavoriteHubByUrlL(server);
	if (!fhe) return false;
	wi.windowPosX = fhe->getWindowPosX();
	wi.windowPosY = fhe->getWindowPosY();
	wi.windowSizeX = fhe->getWindowSizeX();
	wi.windowSizeY = fhe->getWindowSizeY();
	wi.windowType = fhe->getWindowType();
	wi.chatUserSplit = fhe->getChatUserSplit();
	wi.swapPanels = fhe->getSwapPanels();
	wi.hideUserList = fhe->getHideUserList();
	wi.headerOrder = fhe->getHeaderOrder();
	wi.headerWidths = fhe->getHeaderWidths();
	wi.headerVisible = fhe->getHeaderVisible();
	wi.headerSort = fhe->getHeaderSort();
	wi.headerSortAsc = fhe->getHeaderSortAsc();
	return true;
}

bool FavoriteManager::setFavoriteHubPassword(const string& server, const string& nick, const string& password, bool addIfNotFound)
{
	bool result = false;
	FavoriteHubEntry* hubAdded = nullptr;
	FavoriteHubEntry* hubChanged = nullptr;
	{
		WRITE_LOCK(*csHubs);
		FavoriteHubEntry* fhe = getFavoriteHubByUrlL(server);
		if (fhe)
		{
			if (fhe->getNick() != nick || fhe->getPassword() != password)
			{
				fhe->setNick(nick);
				fhe->setPassword(password);
				hubChanged = new FavoriteHubEntry(*fhe);
			}
			result = true;
		}
		if (!result && addIfNotFound)
		{
			hubAdded = new FavoriteHubEntry;
			hubAdded->id = ++favHubId;
			hubAdded->setName(server);
			hubAdded->setServer(server);
			hubAdded->setNick(nick);
			hubAdded->setPassword(password);
			FavoriteHubEntry* fhe = new FavoriteHubEntry(*hubAdded);
			hubs.push_back(fhe);
			hubsByUrl[fhe->getServer()] = fhe;
			result = true;
		}
	}
	if (hubAdded || hubChanged)
		saveFavorites();
	if (hubAdded)
	{
		fire(FavoriteManagerListener::FavoriteAdded(), hubAdded);
		delete hubAdded;
	}
	if (hubChanged)
	{
		fire(FavoriteManagerListener::FavoriteChanged(), hubChanged);
		delete hubChanged;
	}
	return result;
}

bool FavoriteManager::setFavoriteHubAutoConnect(const string& server, bool autoConnect)
{
	FavoriteHubEntry* fhe;
	FavoriteHubEntry* hubChanged = nullptr;
	{
		WRITE_LOCK(*csHubs);
		fhe = getFavoriteHubByUrlL(server);
		if (fhe)
		{
			if (fhe->getAutoConnect() != autoConnect)
			{
				fhe->setAutoConnect(autoConnect);
				hubChanged = new FavoriteHubEntry(*fhe);
			}
		}
	}
	if (hubChanged)
	{
		saveFavorites();
		fire(FavoriteManagerListener::FavoriteChanged(), hubChanged);
		delete hubChanged;
	}
	return fhe != nullptr;
}

bool FavoriteManager::setFavoriteHubAutoConnect(int id, bool autoConnect)
{
	bool result = false;
	FavoriteHubEntry* hubChanged = nullptr;
	{
		WRITE_LOCK(*csHubs);
		for (auto i = hubs.begin(); i != hubs.end(); ++i)
		{
			FavoriteHubEntry* fhe = *i;
			if (fhe->getID() == id)
			{
				if (fhe->getAutoConnect() != autoConnect)
				{
					fhe->setAutoConnect(autoConnect);
					hubChanged = new FavoriteHubEntry(*fhe);
				}
				result = true;
				break;
			}
		}
	}
	if (hubChanged)
	{
		saveFavorites();
		fire(FavoriteManagerListener::FavoriteChanged(), hubChanged);
		delete hubChanged;
	}
	return result;
}

const FavoriteHubEntry* FavoriteManager::getFavoriteHubEntryPtr(const string& server) const noexcept
{
	csHubs->acquireShared();
	const FavoriteHubEntry* fhe = getFavoriteHubByUrlL(server);
	if (!fhe) csHubs->releaseShared();
	return fhe;
}

const FavoriteHubEntry* FavoriteManager::getFavoriteHubEntryPtr(int id) const noexcept
{
	csHubs->acquireShared();
	for (auto i = hubs.cbegin(); i != hubs.cend(); ++i)
	{
		if ((*i)->getID() == id)
			return (*i);
	}
	csHubs->releaseShared();
	return nullptr;
}

void FavoriteManager::releaseFavoriteHubEntryPtr(const FavoriteHubEntry* fhe) const noexcept
{
	if (fhe)
		csHubs->releaseShared();
}

bool FavoriteManager::isPrivateHub(const string& url) const
{
	if (url.empty()) return false;
	READ_LOCK(*csHubs);
	const FavoriteHubEntry* fhe = getFavoriteHubByUrlL(url);
	if (fhe)
	{
		const string& name = fhe->getGroup();
		if (!name.empty())
		{
			auto group = favHubGroups.find(name);
			if (group != favHubGroups.end())
				return group->second.priv;
		}
	}
	return false;
}

void FavoriteManager::updateConnectionStatus(FavoriteHubEntry* fhe, ConnectionStatus::Status status, time_t now)
{
	auto& cs = fhe->getConnectionStatus();
	switch (status)
	{
		case ConnectionStatus::CONNECTING:
			cs.lastAttempt = now;
			break;
		case ConnectionStatus::SUCCESS:
			cs.lastSuccess = now;
		case ConnectionStatus::FAILURE:
			cs.status = status;
	}
}

void FavoriteManager::changeConnectionStatus(int id, ConnectionStatus::Status status)
{
	time_t now = GET_TIME();
	WRITE_LOCK(*csHubs);
	for (auto i = hubs.cbegin(); i != hubs.cend(); ++i)
	{
		FavoriteHubEntry* fhe = *i;
		if (fhe->getID() == id)
		{
			updateConnectionStatus(fhe, status, now);
			break;
		}
	}
}

void FavoriteManager::changeConnectionStatus(const string& hubUrl, ConnectionStatus::Status status)
{
	time_t now = GET_TIME();
	WRITE_LOCK(*csHubs);
	FavoriteHubEntry* fhe = getFavoriteHubByUrlL(hubUrl);
	if (fhe) updateConnectionStatus(fhe, status, now);
}

const FavoriteHubEntry* FavoriteManager::getFavoriteHubByUrlL(const string& url) const
{
	auto i = hubsByUrl.find(url);
	return i == hubsByUrl.cend() ? nullptr : i->second;
}

FavoriteHubEntry* FavoriteManager::getFavoriteHubByUrlL(const string& url)
{
	auto i = hubsByUrl.find(url);
	return i == hubsByUrl.end() ? nullptr : i->second;
}

bool FavoriteManager::addFavHubGroup(const string& name, bool priv)
{
	WRITE_LOCK(*csHubs);
	return favHubGroups.insert(make_pair(name, FavHubGroupProperties{priv})).second;
}

bool FavoriteManager::removeFavHubGroup(const string& name)
{
	WRITE_LOCK(*csHubs);
	auto i = favHubGroups.find(name);
	if (i == favHubGroups.end()) return false;
	favHubGroups.erase(i);
	return true;
}

bool FavoriteManager::updateFavHubGroup(const string& name, const string& newName, bool priv)
{
	WRITE_LOCK(*csHubs);
	auto i = favHubGroups.find(name);
	if (i == favHubGroups.end()) return false;
	if (name == newName)
	{
		i->second.priv = priv;
		return true;
	}
	if (!favHubGroups.insert(make_pair(newName, FavHubGroupProperties{priv})).second) return false;
	favHubGroups.erase(i);
	return true;
}

// Directories

static bool hasExtension(const StringList& l, const string& ext)
{
	for (const auto& x : l)
		if (x.length() == ext.length() && stricmp(x, ext) == 0) return true;
	return false;
}

bool FavoriteManager::addFavoriteDir(const string& directory, const string& name, const string& ext)
{
	if (directory.empty() || directory.back() != PATH_SEPARATOR || name.empty())
		return false;
	auto extList = Util::splitSettingAndLower(ext, true);
	{
		WRITE_LOCK(*csDirs);
		for (const auto& d : favoriteDirs)
		{
			if (d.dir.length() == directory.length() && stricmp(d.dir, directory) == 0)
				return false;
			if (d.name.length() == name.length() && stricmp(d.name, name) == 0)
				return false;
			for (const auto& x : extList)
				if (hasExtension(d.ext, x))
					return false;
		}
		favoriteDirs.push_back(FavoriteDirectory{std::move(extList), directory, name});
	}
	saveFavorites();
	return true;
}

bool FavoriteManager::removeFavoriteDir(const string& name)
{
	bool upd = false;
	{
		WRITE_LOCK(*csDirs);
		for (auto j = favoriteDirs.cbegin(); j != favoriteDirs.cend(); ++j)
		{
			if (j->name.length() == name.length() && stricmp(j->name, name) == 0)
			{
				favoriteDirs.erase(j);
				upd = true;
				break;
			}
		}
	}
	if (upd) saveFavorites();
	return upd;
}

bool FavoriteManager::updateFavoriteDir(const string& name, const string& newName, const string& directory, const string& ext)
{
	if (directory.empty() || directory.back() != PATH_SEPARATOR || newName.empty())
		return false;
	auto extList = Util::splitSettingAndLower(ext, true);
	bool upd = false;
	{
		WRITE_LOCK(*csDirs);
		auto i = favoriteDirs.end();
		for (auto j = favoriteDirs.begin(); j != favoriteDirs.end(); ++j)
		{
			if (j->name.length() == name.length() && stricmp(j->name, name) == 0)
				i = j;
			else
			{
				if (j->name.length() == newName.length() && stricmp(j->name, newName) == 0)
					return false;
				if (j->dir.length() == directory.length() && stricmp(j->dir, directory) == 0)
					return false;
				for (const auto& x : extList)
					if (hasExtension(j->ext, x))
						return false;
			}
		}
		if (i != favoriteDirs.end())
		{
			i->dir = directory;
			i->name = newName;
			i->ext = std::move(extList);
			upd = true;
		}
	}
	if (upd) saveFavorites();
	return upd;
}

string FavoriteManager::getFavoriteDir(const string& ext) const
{
	dcassert(ext.length() > 1);
	size_t len = ext.length() - 1;
	READ_LOCK(*csDirs);
	for (const auto& d : favoriteDirs)
	{
		if (d.ext.empty()) continue;
		for (const auto& favExt : d.ext)
		{
			if (favExt.length() == len && stricmp(ext.c_str() + 1, favExt.c_str()) == 0)
				return d.dir;
		}
	}
	return Util::emptyString;
}

string FavoriteManager::getDownloadDirectory(const string& ext, const UserPtr& user) const
{
	string dir;
	if (ext.length() > 1) dir = getFavoriteDir(ext);
	if (dir.empty()) dir = SETTING(DOWNLOAD_DIRECTORY);
	if (dir.find('%') == string::npos) return dir;
	return Util::expandDownloadDir(dir, user);
}

// Recents

RecentHubEntry* FavoriteManager::addRecent(const RecentHubEntry& entry)
{
	ASSERT_MAIN_THREAD();
	RecentHubEntry* recent = getRecentHubEntry(entry.getServer());
	if (recent)
	{
		if (!entry.getRedirect())
		{
			recent->setOpenTab(entry.getOpenTab());
			recentsDirty = true;
		}
		return recent;
	}
	recent = new RecentHubEntry(entry);
	recentHubs.push_back(recent);
	recentsDirty = true;
	fire(FavoriteManagerListener::RecentAdded(), recent);
	return recent;
}

void FavoriteManager::removeRecent(const RecentHubEntry* entry)
{
	ASSERT_MAIN_THREAD();
	const auto& i = find(recentHubs.begin(), recentHubs.end(), entry);
	if (i == recentHubs.end())
		return;
	fire(FavoriteManagerListener::RecentRemoved(), entry);
	recentHubs.erase(i);
	recentsDirty = true;
	delete entry;
}

void FavoriteManager::updateRecent(const RecentHubEntry* entry)
{
	ASSERT_MAIN_THREAD();
	const auto i = find(recentHubs.begin(), recentHubs.end(), entry);
	if (i == recentHubs.end())
		return;
	recentsDirty = true;
	if (!ClientManager::isBeforeShutdown())
		fire(FavoriteManagerListener::RecentUpdated(), entry);
}

void FavoriteManager::saveFavorites()
{
	if (dontSave) return;
	favsLastSave = GET_TICK();
	try
	{
		SimpleXML xml;

		xml.addTag("Favorites");
		xml.stepIn();

		xml.addTag("Hubs");
		xml.stepIn();

		{
			READ_LOCK(*csHubs);
			for (auto i = favHubGroups.cbegin(), iend = favHubGroups.cend(); i != iend; ++i)
			{
				xml.addTag("Group");
				xml.addChildAttrib("Name", i->first);
				xml.addChildAttrib("Private", i->second.priv);
			}
			for (const FavoriteHubEntry* e : hubs)
			{
				string url = e->getServer();
				const string& kp = e->getKeyPrint();
				if (!kp.empty())
					url += "?kp=" + kp;
				xml.addTag("Hub");
				xml.addChildAttrib("Name", e->getName());
				xml.addChildAttrib("Connect", e->getAutoConnect());
				xml.addChildAttrib("Description", e->getDescription());
				xml.addChildAttribIfNotEmpty("Nick", e->getNick(false));
				xml.addChildAttribIfNotEmpty("Password", e->getPassword());
				xml.addChildAttrib("Server", url);
				xml.addChildAttribIfNotEmpty("UserDescription", e->getUserDescription());
				if (!Util::isAdcHub(e->getServer()))
				{
					string encoding = Text::charsetToString(e->getEncoding());
					if (!encoding.empty())
						xml.addChildAttrib("Encoding", encoding);
				}
				if (e->getPreferIP6())
					xml.addChildAttrib("PreferIP6", true);
				const CID& shareGroup = e->getShareGroup();
				if (e->getHideShare())
					xml.addChildAttrib("HideShare", true);
				else if (!shareGroup.isZero())
					xml.addChildAttrib("ShareGroup", shareGroup.toBase32());
				xml.addChildAttribIfNotEmpty("AwayMsg", e->getAwayMsg());
				xml.addChildAttribIfNotEmpty("Email", e->getEmail());
				xml.addChildAttrib("WindowPosX", e->getWindowPosX());
				xml.addChildAttrib("WindowPosY", e->getWindowPosY());
				xml.addChildAttrib("WindowSizeX", e->getWindowSizeX());
				xml.addChildAttrib("WindowSizeY", e->getWindowSizeY());
				xml.addChildAttrib("WindowType", e->getWindowType());
				xml.addChildAttrib("ChatUserSplitSize", e->getChatUserSplit());
				if (e->getShowJoins())
					xml.addChildAttrib("ShowJoins", true);
				if (e->getExclChecks())
					xml.addChildAttrib("ExclChecks", true);
				if (e->getExclusiveHub())
					xml.addChildAttrib("ExclusiveHub", true);
				if (e->getSuppressChatAndPM())
					xml.addChildAttrib("SuppressChatAndPM", true);
				if (e->getHideUserList())
					xml.addChildAttrib("HideUserList", true);
				if (e->getSwapPanels())
					xml.addChildAttrib("SwapPanels", true);
				xml.addChildAttrib("HeaderOrder", e->getHeaderOrder());
				xml.addChildAttrib("HeaderWidths", e->getHeaderWidths());
				xml.addChildAttrib("HeaderVisible", e->getHeaderVisible());
				xml.addChildAttrib("HeaderSort", e->getHeaderSort());
				xml.addChildAttrib("HeaderSortAsc", e->getHeaderSortAsc());
				const string* rawCommands = e->getRawCommands();
				xml.addChildAttribIfNotEmpty("RawOne", rawCommands[0]);
				xml.addChildAttribIfNotEmpty("RawTwo", rawCommands[1]);
				xml.addChildAttribIfNotEmpty("RawThree", rawCommands[2]);
				xml.addChildAttribIfNotEmpty("RawFour", rawCommands[3]);
				xml.addChildAttribIfNotEmpty("RawFive", rawCommands[4]);
				int mode = e->getMode();
				if (mode)
					xml.addChildAttrib("Mode", mode);
				xml.addChildAttribIfNotEmpty("IP", e->getIP());
				xml.addChildAttribIfNotEmpty("OpChat", e->getOpChat());
				uint32_t searchInterval = e->getSearchInterval();
				if (searchInterval)
					xml.addChildAttrib("SearchInterval", searchInterval);
				searchInterval = e->getSearchIntervalPassive();
				if (searchInterval)
					xml.addChildAttrib("SearchIntervalPassive", searchInterval);
				xml.addChildAttribIfNotEmpty("ClientName", e->getClientName());
				xml.addChildAttribIfNotEmpty("ClientVersion", e->getClientVersion());
				if (e->getOverrideId())
					xml.addChildAttrib("OverrideId", true);
				xml.addChildAttribIfNotEmpty("FakeShare", e->getFakeShare());
				int64_t fakeFileCount = e->getFakeFileCount();
				if (fakeFileCount > 0)
					xml.addChildAttrib("FakeFiles", fakeFileCount);
				int fakeSlots = e->getFakeSlots();
				if (fakeSlots >= 0)
					xml.addChildAttrib("FakeSlots", fakeSlots);
				int fakeClientStatus = e->getFakeClientStatus();
				if (fakeClientStatus)
					xml.addChildAttrib("FakeClientStatus", fakeClientStatus);
				xml.addChildAttribIfNotEmpty("Group", e->getGroup());
				const auto& cs = e->getConnectionStatus();
				if (cs.status != ConnectionStatus::UNKNOWN)
				{
					xml.addChildAttrib("Status", (int) cs.status);
					if (cs.lastAttempt)
						xml.addChildAttrib("LastAttempt", (int64_t) cs.lastAttempt);
					if (cs.lastSuccess)
						xml.addChildAttrib("LastSuccess", (int64_t) cs.lastSuccess);
				}
			}
		}
		xml.stepOut();
		xml.addTag("Users");
		xml.stepIn();
		{
			READ_LOCK(*csUsers);
			for (auto i = favoriteUsers.cbegin(), iend = favoriteUsers.cend(); i != iend; ++i)
			{
				const auto &u = i->second;
				xml.addTag("User");
				xml.addChildAttrib("Nick", u.nick);
				xml.addChildAttrib("URL", u.url);
				if (u.lastSeen)
					xml.addChildAttrib("LastSeen", u.lastSeen);
				if (u.isSet(FavoriteUser::FLAG_GRANT_SLOT))
					xml.addChildAttrib("GrantSlot", true);
				if (u.isSet(FavoriteUser::FLAG_HIDE_SHARE))
					xml.addChildAttrib("HideShare", true);
				else if (!u.shareGroup.isZero())
					xml.addChildAttrib("ShareGroup", u.shareGroup.toBase32());
				if (u.uploadLimit == FavoriteUser::UL_SU)
					xml.addChildAttrib("SuperUser", true);
				if (u.uploadLimit)
					xml.addChildAttrib("UploadLimit", u.uploadLimit);
				if (u.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
					xml.addChildAttrib("IgnorePrivate", true);
				if (u.isSet(FavoriteUser::FLAG_FREE_PM_ACCESS))
					xml.addChildAttrib("FreeAccessPM", true);
				if (!u.description.empty())
					xml.addChildAttrib("UserDescription", u.description);
				if (Util::isAdcHub(u.url))
					xml.addChildAttrib("CID", i->first.toBase32());
			}
		}
		xml.stepOut();

		xml.addTag("UserCommands");
		xml.stepIn();
		{
			READ_LOCK(*csUserCommand);
			for (const auto& item : userCommands)
			{
				if (!item.isSet(UserCommand::FLAG_NOSAVE))
				{
					xml.addTag("UserCommand");
					xml.addChildAttrib("Type", item.getType());
					xml.addChildAttrib("Context", item.getCtx());
					xml.addChildAttrib("Name", item.getName());
					xml.addChildAttrib("Command", item.getCommand());
					xml.addChildAttrib("To", item.getTo());
					xml.addChildAttrib("Hub", item.getHub());
				}
			}
		}
		xml.stepOut();

		//Favorite download to dirs
		xml.addTag("FavoriteDirs");
		xml.stepIn();
		{
			READ_LOCK(*csDirs);
			for (const auto& item : favoriteDirs)
			{
				xml.addTag("Directory", item.dir);
				xml.addChildAttrib("Name", item.name);
				xml.addChildAttrib("Extensions", Util::toString(';', item.ext));
			}
		}
		xml.stepOut();
		xml.stepOut();

		const string fname = getFavoritesFile();
		const string tempFile = fname + ".tmp";
		{
			File f(tempFile, File::WRITE, File::CREATE | File::TRUNCATE);
			f.write(SimpleXML::utf8Header);
			f.write(xml.toXML());
		}
		File::renameFile(tempFile, fname);
		favsDirty = false;
	}
	catch (const Exception& e)
	{
		dcdebug("FavoriteManager::save: %s\n", e.getError().c_str());
		LogManager::message("FavoriteManager::save error = " + e.getError());
	}
}

void FavoriteManager::saveRecents()
{
	ASSERT_MAIN_THREAD();
	if (recentsDirty)
	{
		recentsLastSave = GET_TICK();
		DBRegistryMap values;
		for (auto i = recentHubs.cbegin(); i != recentHubs.cend(); ++i)
		{
			const auto recent = *i;
			string recentHubsStr;
			recentHubsStr += recent->getDescription();
			recentHubsStr += '\n';
			recentHubsStr += recent->getUsers();
			recentHubsStr += '\n';
			recentHubsStr += recent->getShared();
			recentHubsStr += '\n';
			recentHubsStr += recent->getServer();
			recentHubsStr += '\n';
			recentHubsStr += recent->getLastSeen();
			recentHubsStr += '\n';
			recentHubsStr += recent->getOpenTab();
			recentHubsStr += '\n';
			values[recent->getName()] = recentHubsStr;
		}
		auto conn = DatabaseManager::getInstance()->getDefaultConnection();
		if (conn)
		{
			conn->saveRegistry(values, e_RecentHub, true);
			recentsDirty = false;
		}
	}
}

void FavoriteManager::load()
{
	// Add NMDC standard op commands
	static const char g_kickstr[] = /*"$Kick %[userNI]|";*/
	    "$To: %[userNI] From: %[myNI] $<%[myNI]> You are being kicked because: %[kickline:Reason]|<%[myNI]> is kicking %[userNI] because: %[kickline:Reason]|$Kick %[userNI]|";
	addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_USER | UserCommand::CONTEXT_SEARCH, UserCommand::FLAG_NOSAVE,
	               STRING(KICK_USER), g_kickstr, "", "op");
	static const char g_kickfilestr[] =
	    "$To: %[userNI] From: %[myNI] $<%[myNI]> You are being kicked because: %[kickline:Reason] %[fileFN]|<%[myNI]> is kicking %[userNI] because: %[kickline:Reason] %[fileFN]|$Kick %[userNI]|";
	addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_SEARCH, UserCommand::FLAG_NOSAVE,
	               STRING(KICK_USER_FILE), g_kickfilestr, "", "op");
	static const char g_redirstr[] =
	    "$OpForceMove $Who:%[userNI]$Where:%[line:Target Server]$Msg:%[line:Message]|";
	addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_USER | UserCommand::CONTEXT_SEARCH, UserCommand::FLAG_NOSAVE,
	               STRING(REDIRECT_USER), g_redirstr, "", "op");
	               
	addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_USER, UserCommand::FLAG_NOSAVE, STRING(MUTE_USER), "<%[myNI]> !gag %[userNI]|", "", "op");
	// Reserved. Work only with Ptokax
	//addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_USER, UserCommand::FLAG_NOSAVE, "Drop and Ban User", "<%[myNI]> !drop %[userNI]|", "", "op");
	
	dontSave++;
	try
	{
		SimpleXML xml;
		Util::migrate(getFavoritesFile());
		//LogManager::message("FavoriteManager::load File = " + getFavoritesFile());
		xml.fromXML(File(getFavoritesFile(), File::READ, File::OPEN).read());
		
		if (xml.findChild("Favorites"))
		{
			xml.stepIn();
			load(xml);
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		LogManager::message("[Error] FavoriteManager::load " + e.getError() + " File = " + getFavoritesFile());
		dcdebug("FavoriteManager::load: %s\n", e.getError().c_str());
	}
	dontSave--;
	
	const bool oldConfigExist = !recentHubs.empty();
	
	DBRegistryMap values;
	auto conn = DatabaseManager::getInstance()->getDefaultConnection();
	if (conn) conn->loadRegistry(values, e_RecentHub);
	for (auto k = values.cbegin(); k != values.cend(); ++k)
	{
		const StringTokenizer<string> tok(k->second.sval, '\n');
		if (tok.getTokens().size() > 3)
		{
			RecentHubEntry* e = new RecentHubEntry();
			e->setName(k->first);
			e->setDescription(tok.getTokens()[0]);
			e->setUsers(tok.getTokens()[1]);
			e->setShared(tok.getTokens()[2]);
			const string server = Util::formatDchubUrl(tok.getTokens()[3]);
			e->setServer(server);
			if (tok.getTokens().size() > 4)
			{
				e->setLastSeen(tok.getTokens()[4]);
				if (tok.getTokens().size() > 5) 
				{
					e->setOpenTab(tok.getTokens()[5]);
				}
			}
			recentHubs.push_back(e);
			recentsDirty = true;
		}
	}
	if (oldConfigExist)
		recentsDirty = true;
}

static int clampValue(int val, int minVal, int maxVal, bool& outOfBounds)
{
	if (val < minVal)
	{
		outOfBounds = true;
		return minVal;
	}
	if (val > maxVal)
	{
		outOfBounds = true;
		return maxVal;
	}
	return val;
}

void FavoriteManager::load(SimpleXML& xml)
{
	xml.resetCurrentChild();
	if (xml.findChild("Hubs"))
	{
		xml.stepIn();
		{
			WRITE_LOCK(*csHubs);
			while (xml.findChild("Group"))
			{
				const string& name = xml.getChildAttrib("Name");
				if (name.empty())
					continue;
				const FavHubGroupProperties props = { xml.getBoolChildAttrib("Private") };
				favHubGroups[name] = props;
			}
		}
		xml.resetCurrentChild();
		string query;
		Util::ParsedUrl url;
		while (xml.findChild("Hub"))
		{
			string currentServerUrl = xml.getChildAttrib("Server");
#ifdef _DEBUG
			LogManager::message("Load favorites item: " + currentServerUrl);
#endif
			Util::decodeUrl(currentServerUrl, url);
			if (url.host.empty()) continue;
			query = std::move(url.query);
			currentServerUrl = Text::toLower(Util::formatDchubUrl(url));

			FavoriteHubEntry* e = new FavoriteHubEntry();
			e->id = ++favHubId;
			const string& name = xml.getChildAttrib("Name");
			e->setName(name);

			e->setAutoConnect(xml.getBoolChildAttrib("Connect"));
			const string& description = xml.getChildAttrib("Description");
			const string& group = xml.getChildAttrib("Group");
			e->setDescription(description);
			e->setServer(currentServerUrl);
			e->setPreferIP6(xml.getBoolChildAttrib("PreferIP6"));
			e->setKeyPrint(Util::getQueryParam(query, "kp"));
			e->setSearchInterval(Util::toUInt32(xml.getChildAttrib("SearchInterval")));
			e->setSearchIntervalPassive(Util::toUInt32(xml.getChildAttrib("SearchIntervalPassive")));
			e->setHideUserList(xml.getBoolChildAttrib("HideUserList"));
			e->setSuppressChatAndPM(xml.getBoolChildAttrib("SuppressChatAndPM"));
				
			const bool isOverrideId = Util::toInt(xml.getChildAttrib("OverrideId")) != 0;
			string clientName = xml.getChildAttrib("ClientName");
			string clientVersion = xml.getChildAttrib("ClientVersion");

			if (Util::isAdcHub(currentServerUrl))
				e->setEncoding(Text::CHARSET_UTF8);
			else
				e->setEncoding(Text::charsetFromString(xml.getChildAttrib("Encoding")));

			string nick = xml.getChildAttrib("Nick");
			const string& password = xml.getChildAttrib("Password");
			const auto posRndMarker = nick.rfind("_RND_");
			if (password.empty() && posRndMarker != string::npos && atoi(nick.c_str() + posRndMarker + 5) > 1000)
				nick.clear();
			e->setNick(nick);
			e->setPassword(password);
			e->setUserDescription(xml.getChildAttrib("UserDescription"));
			e->setAwayMsg(xml.getChildAttrib("AwayMsg"));
			e->setEmail(xml.getChildAttrib("Email"));
			const string& shareGroup = xml.getChildAttrib("ShareGroup");
			if (shareGroup.length() == 39)
			{
				CID id(shareGroup);
				e->setShareGroup(id);
			}
			bool valueOutOfBounds = false;
			e->setWindowPosX(clampValue(xml.getIntChildAttrib("WindowPosX"), 0, 10, valueOutOfBounds));
			e->setWindowPosY(clampValue(xml.getIntChildAttrib("WindowPosY"), 0, 100, valueOutOfBounds));
			e->setWindowSizeX(clampValue(xml.getIntChildAttrib("WindowSizeX"), 50, 1600, valueOutOfBounds));
			e->setWindowSizeY(clampValue(xml.getIntChildAttrib("WindowSizeY"), 50, 1600, valueOutOfBounds));
			if (!valueOutOfBounds)
				e->setWindowType(xml.getIntChildAttrib("WindowType", "3")); // SW_MAXIMIZE if missing
			else
				e->setWindowType(3); // SW_MAXIMIZE
			e->setChatUserSplit(xml.getIntChildAttrib("ChatUserSplitSize"));
			e->setSwapPanels(xml.getBoolChildAttrib("SwapPanels"));
			e->setHideShare(xml.getBoolChildAttrib("HideShare"));
			e->setShowJoins(xml.getBoolChildAttrib("ShowJoins"));
			e->setExclChecks(xml.getBoolChildAttrib("ExclChecks"));
			e->setExclusiveHub(xml.getBoolChildAttrib("ExclusiveHub"));
			e->setHeaderOrder(xml.getChildAttrib("HeaderOrder", SETTING(HUB_FRAME_ORDER)));
			e->setHeaderWidths(xml.getChildAttrib("HeaderWidths", SETTING(HUB_FRAME_WIDTHS)));
			e->setHeaderVisible(xml.getChildAttrib("HeaderVisible", SETTING(HUB_FRAME_VISIBLE)));
			e->setHeaderSort(xml.getIntChildAttrib("HeaderSort", "-1"));
			e->setHeaderSortAsc(xml.getBoolChildAttrib("HeaderSortAsc"));
			e->setRawCommand(xml.getChildAttrib("RawOne"), 0);
			e->setRawCommand(xml.getChildAttrib("RawTwo"), 1);
			e->setRawCommand(xml.getChildAttrib("RawThree"), 2);
			e->setRawCommand(xml.getChildAttrib("RawFour"), 3);
			e->setRawCommand(xml.getChildAttrib("RawFive"), 4);
			e->setMode(Util::toInt(xml.getChildAttrib("Mode")));
			e->setIP(xml.getChildAttribTrim("IP"));
			e->setOpChat(xml.getChildAttrib("OpChat"));
					
			if (clientName.empty())
			{
				const string& clientID = xml.getChildAttrib("ClientId");
				if (!clientID.empty())
					splitClientId(clientID, clientName, clientVersion);
			}
			e->setClientName(clientName);
			e->setClientVersion(clientVersion);
			e->setOverrideId(isOverrideId);
			e->setFakeShare(xml.getChildAttrib("FakeShare"));
			e->setFakeFileCount(xml.getInt64ChildAttrib("FakeFiles"));
			e->setFakeSlots(xml.getIntChildAttrib("FakeSlots", "-1"));
			e->setFakeClientStatus(xml.getIntChildAttrib("FakeClientStatus"));

			e->setGroup(group);
			const string& connStatusAttr = xml.getChildAttrib("Status");
			if (!connStatusAttr.empty())
			{
				auto status = (ConnectionStatus::Status) Util::toInt(connStatusAttr);
				if (status == ConnectionStatus::SUCCESS || status == ConnectionStatus::FAILURE)
				{
					const string& lastAttempt = xml.getChildAttrib("LastAttempt");
					if (!lastAttempt.empty())
					{
						auto& cs = e->getConnectionStatus();
						cs.status = status;
						cs.lastAttempt = Util::toInt64(lastAttempt);
						cs.lastSuccess = Util::toInt64(xml.getChildAttrib("LastSuccess"));
					}
				}
			}
			WRITE_LOCK(*csHubs);
			hubs.push_back(e);
			hubsByUrl[e->getServer()] = e;
		}
		xml.stepOut();
	}

	xml.resetCurrentChild();
	if (xml.findChild("Users"))
	{
		xml.stepIn();
		while (xml.findChild("User"))
		{
			UserPtr u;
			const string& nick = xml.getChildAttrib("Nick");
			const string hubUrl = Util::formatDchubUrl(xml.getChildAttrib("URL")); // [!] IRainman fix: toLower already called in formatDchubUrl ( decodeUrl )
			const string cid = Util::isAdcHub(hubUrl) ? xml.getChildAttrib("CID") : ClientManager::makeCid(nick, hubUrl).toBase32();
			if (cid.length() != 39)
			{
				if (nick.empty() || hubUrl.empty())
					continue;
				u = ClientManager::getUser(nick, hubUrl);
			}
			else
			{
				u = ClientManager::createUser(CID(cid), nick, hubUrl);
			}
			u->setFlag(User::FAVORITE);

			WRITE_LOCK(*csUsers);
			auto i = favoriteUsers.insert(make_pair(u->getCID(), FavoriteUser(u, nick, hubUrl))).first;
			auto &user = i->second;

			if (xml.getBoolChildAttrib("IgnorePrivate"))
				user.setFlag(FavoriteUser::FLAG_IGNORE_PRIVATE);
			if (xml.getBoolChildAttrib("FreeAccessPM"))
				user.setFlag(FavoriteUser::FLAG_FREE_PM_ACCESS);
						
			if (xml.getBoolChildAttrib("SuperUser"))
				user.uploadLimit = FavoriteUser::UL_SU;
			else
			{
				user.uploadLimit = xml.getIntChildAttrib("UploadLimit");
				if (user.uploadLimit < FavoriteUser::UL_SU) user.uploadLimit = 0;
			}
			if (user.uploadLimit == FavoriteUser::UL_BAN)
				u->setFlag(User::BANNED);

			if (xml.getBoolChildAttrib("GrantSlot"))
				user.setFlag(FavoriteUser::FLAG_GRANT_SLOT);

			if (!xml.getBoolChildAttrib("HideShare"))
			{
				const string& shareGroup = xml.getChildAttrib("ShareGroup");
				if (shareGroup.length() == 39)
					user.shareGroup.fromBase32(shareGroup);
			}
			else
				user.setFlag(FavoriteUser::FLAG_HIDE_SHARE);
			
			user.lastSeen = xml.getInt64ChildAttrib("LastSeen");
			user.description = xml.getChildAttrib("UserDescription");
		}
		xml.stepOut();
	}
	xml.resetCurrentChild();
	if (xml.findChild("UserCommands"))
	{
		xml.stepIn();
		while (xml.findChild("UserCommand"))
		{
			int type = xml.getIntChildAttrib("Type");
			if (type == UserCommand::TYPE_SEPARATOR ||
			    type == UserCommand::TYPE_RAW || type == UserCommand::TYPE_RAW_ONCE ||
			    type == UserCommand::TYPE_CHAT || type == UserCommand::TYPE_CHAT_ONCE)
				addUserCommand(type, xml.getIntChildAttrib("Context"), 0, xml.getChildAttrib("Name"),
				               xml.getChildAttrib("Command"), xml.getChildAttrib("To"), xml.getChildAttrib("Hub"));
		}
		xml.stepOut();
	}
	//Favorite download to dirs
	xml.resetCurrentChild();
	if (xml.findChild("FavoriteDirs"))
	{
		xml.stepIn();
		while (xml.findChild("Directory"))
		{
			const auto& virt = xml.getChildAttrib("Name");
			const auto& ext = xml.getChildAttrib("Extensions");
			string dir = xml.getChildData();
			Util::appendPathSeparator(dir);
			addFavoriteDir(dir, virt, ext);
		}
		xml.stepOut();
	}
}

void FavoriteManager::userUpdated(const OnlineUser& info)
{
	if (!ClientManager::isBeforeShutdown())
	{
		READ_LOCK(*csUsers);
		auto i = favoriteUsers.find(info.getUser()->getCID());
		if (i == favoriteUsers.end())
			return;
		i->second.update(info);
	}
}

void FavoriteManager::setUploadLimit(const UserPtr& user, int lim, bool createUser/* = true*/)
{
	ConnectionManager::getInstance()->setUploadLimit(user, lim);
	FavoriteMap::iterator i;
	FavoriteUser favUser;
	bool added = false;
	{
		WRITE_LOCK(*csUsers);
		added = addUserL(user, i, createUser);
		if (i == favoriteUsers.end())
			return;
		i->second.uploadLimit = lim;
		favUser = i->second;
		if (lim == FavoriteUser::UL_BAN)
			user->changeFlags(User::FAVORITE | User::BANNED, 0);
		else
			user->changeFlags(User::FAVORITE, User::BANNED);
	}
	speakUserUpdate(added, favUser);
	favsDirty = true;
}

bool FavoriteManager::getFlag(const UserPtr& user, FavoriteUser::Flags f) const
{
	READ_LOCK(*csUsers);
	const auto i = favoriteUsers.find(user->getCID());
	if (i != favoriteUsers.end())
		return i->second.isSet(f);
	return false;
}

FavoriteUser::Flags FavoriteManager::getFlags(const UserPtr& user) const
{
	READ_LOCK(*csUsers);
	const auto i = favoriteUsers.find(user->getCID());
	if (i != favoriteUsers.end())
		return static_cast<FavoriteUser::Flags>(i->second.getFlags());
	return FavoriteUser::Flags(0);
}

void FavoriteManager::setFlag(const UserPtr& user, FavoriteUser::Flags f, bool value, bool createUser /*= true*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	FavoriteMap::iterator i;
	FavoriteUser favUser;
	bool added = false;
	bool changed = false;
	{
		WRITE_LOCK(*csUsers);
		changed = added = addUserL(user, i, createUser);
		if (i == favoriteUsers.end())
			return;
		Flags::MaskType oldFlags = i->second.getFlags();
		Flags::MaskType newFlags = oldFlags;
		if (value)
			newFlags |= f;
		else
			newFlags &= ~f;
		if (newFlags != oldFlags)
		{
			i->second.setFlags(newFlags);
			changed = true;
		}
		favUser = i->second;
		user->setFlag(User::FAVORITE);
	}
	speakUserUpdate(added, favUser);
	if (changed) favsDirty = true;
}

void FavoriteManager::setFlags(const UserPtr& user, FavoriteUser::Flags flags, FavoriteUser::Flags mask, bool createUser /*= true*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	FavoriteMap::iterator i;
	FavoriteUser favUser;
	bool added = false;
	bool changed = false;
	{
		WRITE_LOCK(*csUsers);
		changed = added = addUserL(user, i, createUser);
		if (i == favoriteUsers.end())
			return;
		Flags::MaskType oldFlags = i->second.getFlags();
		Flags::MaskType newFlags = (oldFlags & ~mask) | flags;
		if (newFlags != oldFlags)
		{
			i->second.setFlags(newFlags);
			changed = true;
		}
		favUser = i->second;
		user->setFlag(User::FAVORITE);
	}
	speakUserUpdate(added, favUser);
	if (changed) favsDirty = true;
}

void FavoriteManager::setUserAttributes(const UserPtr& user, FavoriteUser::Flags flags, int uploadLimit, const CID& shareGroup, const string& description)
{
	FavoriteUser favUser;
	{
		WRITE_LOCK(*csUsers);
		auto i = favoriteUsers.find(user->getCID());
		if (i == favoriteUsers.end())
			return;
		FavoriteUser& tmp = i->second;
		tmp.uploadLimit = uploadLimit;
		tmp.shareGroup = shareGroup;
		tmp.setFlags(flags);
		tmp.description = description;
		favUser = tmp;
		if (uploadLimit == FavoriteUser::UL_BAN)
			user->setFlag(User::BANNED);
		else
			user->unsetFlag(User::BANNED);
	}
	speakUserUpdate(false, favUser);
	favsDirty = true;
}

void FavoriteManager::loadRecents(SimpleXML& xml)
{
	ASSERT_MAIN_THREAD();
	xml.resetCurrentChild();
	if (xml.findChild("Hubs"))
	{
		xml.stepIn();
		while (xml.findChild("Hub"))
		{
			RecentHubEntry* e = new RecentHubEntry();
			e->setName(xml.getChildAttrib("Name"));
			e->setDescription(xml.getChildAttrib("Description"));
			e->setUsers(xml.getChildAttrib("Users"));
			e->setShared(xml.getChildAttrib("Shared"));
			e->setServer(Util::formatDchubUrl(xml.getChildAttrib("Server")));
			e->setLastSeen(xml.getChildAttrib("DateTime"));
			recentHubs.push_back(e);
			recentsDirty = true;
		}
		xml.stepOut();
	}
}

RecentHubEntry* FavoriteManager::getRecentHubEntry(const string& server)
{
	ASSERT_MAIN_THREAD();
	for (RecentHubEntry* r : recentHubs)
		if (stricmp(r->getServer(), server) == 0)
			return r;
	return nullptr;
}

void FavoriteManager::getUserCommands(vector<UserCommand>& result, int ctx, const StringList& hubs) const
{
	result.clear();
	vector<bool> isOp(hubs.size());
	
	for (size_t i = 0; i < hubs.size(); ++i)
		isOp[i] = ClientManager::isOp(hubs[i]);
	
	{
		READ_LOCK(*csUserCommand);
		for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
		{
			const UserCommand& uc = *i;
			if (!(uc.getCtx() & ctx))
				continue;
			
			for (size_t j = 0; j < hubs.size(); ++j)
			{
				const string& hub = hubs[j];
				const bool hubAdc = Util::isAdcHub(hub);
				const bool commandAdc = Util::isAdcHub(uc.getHub());
				if (hubAdc && commandAdc)
				{
					if ((uc.getHub() == "adc://" || uc.getHub() == "adcs://") ||
					    ((uc.getHub() == "adc://op" || uc.getHub() == "adcs://op") && isOp[j]) ||
					    uc.getHub() == hub)
					{
						result.push_back(*i);
						break;
					}
				}
				else if ((!hubAdc && !commandAdc) || uc.isChat())
				{
					if (uc.getHub().empty() ||
					    (uc.getHub() == "op" && isOp[j]) ||
					    uc.getHub() == hub)
					{
						result.push_back(*i);
						break;
					}
				}
			}
		}
	}
	for (const string& hubUrl : hubs)
		ClientManager::getHubUserCommands(hubUrl, result);
}

void FavoriteManager::on(UserUpdated, const OnlineUserPtr& user) noexcept
{
	userUpdated(*user);
}

void FavoriteManager::on(UserDisconnected, const UserPtr& user) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		{
			READ_LOCK(*csUsers);
			auto i = favoriteUsers.find(user->getCID());
			if (i == favoriteUsers.end())
				return;
			i->second.lastSeen = GET_TIME(); // TODO: if ClientManager::isBeforeShutdown() returns true, it will not be updated
			favsDirty = true;
		}
		if (!ClientManager::isBeforeShutdown())
		{
			fire(FavoriteManagerListener::UserStatusChanged(), user);
		}
	}
}

void FavoriteManager::on(UserConnected, const UserPtr& user) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		{
			READ_LOCK(*csUsers);
			auto i = favoriteUsers.find(user->getCID());
			if (i == favoriteUsers.end())
				return;
			i->second.lastSeen = GET_TIME();
			favsDirty = true;
		}
		if (!ClientManager::isBeforeShutdown())
		{
			fire(FavoriteManagerListener::UserStatusChanged(), user);
		}
	}
}

void FavoriteManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if (recentsDirty && tick - recentsLastSave > SAVE_RECENTS_TIME)
	{
		recentsLastSave = tick;
		fire(FavoriteManagerListener::SaveRecents());
	}
	if (favsDirty && tick - favsLastSave > SAVE_FAVORITES_TIME)
		saveFavorites();
}

void FavoriteManager::loadPreview(SimpleXML& xml)
{
	ASSERT_MAIN_THREAD();
	xml.resetCurrentChild();
	if (xml.findChild("PreviewApps"))
	{
		xml.stepIn();
		while (xml.findChild("Application"))
		{
			addPreviewApp(xml.getChildAttrib("Name"), xml.getChildAttrib("Application"),
			              xml.getChildAttrib("Arguments"), xml.getChildAttrib("Extension"));
		}
		xml.stepOut();
	}
}

void FavoriteManager::savePreview(SimpleXML& xml) const
{
	ASSERT_MAIN_THREAD();
	xml.addTag("PreviewApps");
	xml.stepIn();
	for (const auto& item : previewApplications)
	{
		xml.addTag("Application");
		xml.addChildAttrib("Name", item->name);
		xml.addChildAttrib("Application", item->application);
		xml.addChildAttrib("Arguments", item->arguments);
		xml.addChildAttrib("Extension", item->extension);
	}
	xml.stepOut();
}

void FavoriteManager::speakUserUpdate(const bool added, const FavoriteUser& user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (added)
		{
			fire(FavoriteManagerListener::UserAdded(), user);
		}
		else
		{
			fire(FavoriteManagerListener::UserStatusChanged(), user.user);
		}
	}
}

PreviewApplication* FavoriteManager::addPreviewApp(const string& name, const string& application, const string& arguments, const string& extension)
{
	ASSERT_MAIN_THREAD();
	PreviewApplication* pa;
	if (extension.find(' ') != string::npos || extension.find(',') != string::npos)
	{
		string tmp = extension;
		boost::replace_all(tmp, " ", "");
		boost::replace_all(tmp, ",", ";");
		pa = new PreviewApplication(name, application, arguments, tmp);
	}
	else
		pa = new PreviewApplication(name, application, arguments, extension);
	previewApplications.push_back(pa);
	return pa;
}

void FavoriteManager::addPreviewApps(PreviewApplication::List& apps, bool force)
{
	ASSERT_MAIN_THREAD();
	for (PreviewApplication* app : apps)
	{
		bool found = false;
		for (size_t i = 0; i < previewApplications.size(); ++i)
			if (stricmp(previewApplications[i]->name, app->name) == 0)
			{
				if (force)
				{
					delete previewApplications[i];
					previewApplications[i] = app;
				}
				else
					delete app;
				found = true;
				break;
			}
		if (!found)
			previewApplications.push_back(app);
	}
	apps.clear();
}

void FavoriteManager::removePreviewApp(const size_t index)
{
	ASSERT_MAIN_THREAD();
	if (index < previewApplications.size())
	{
		auto i = previewApplications.begin() + index;
		delete *i;
		previewApplications.erase(i);
	}
}

const PreviewApplication* FavoriteManager::getPreviewApp(const size_t index) const
{
	ASSERT_MAIN_THREAD();
	return index < previewApplications.size() ? previewApplications[index] : nullptr;
}

PreviewApplication* FavoriteManager::getPreviewApp(const size_t index)
{
	ASSERT_MAIN_THREAD();
	return index < previewApplications.size() ? previewApplications[index] : nullptr;
}

void FavoriteManager::clearRecents()
{
	ASSERT_MAIN_THREAD();
	recentHubs.clear();
	recentsDirty = true;
}
