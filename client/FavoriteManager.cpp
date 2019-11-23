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
#include "UserCommand.h"
#include "BZUtils.h"
#include "FilteredFile.h"
#include "ConnectionManager.h"
#include "LogManager.h"
#include <boost/algorithm/string.hpp>

bool FavoriteManager::g_recent_dirty = false;
std::unordered_set<std::string> FavoriteManager::g_AllHubUrls;
std::string FavoriteManager::g_DefaultHubUrl;
FavoriteManager::FavoriteMap FavoriteManager::g_fav_users_map;
StringSet FavoriteManager::g_fav_users;
UserCommand::List FavoriteManager::g_userCommands;
FavoriteManager::FavDirList FavoriteManager::g_favoriteDirs;
FavHubGroups FavoriteManager::g_favHubGroups;
RecentHubEntry::List FavoriteManager::g_recentHubs;
FavoriteHubEntryList FavoriteManager::g_favoriteHubs;
PreviewApplication::List FavoriteManager::g_previewApplications;
uint16_t FavoriteManager::g_dontSave = 0;
int FavoriteManager::g_lastId = 0;
std::unique_ptr<webrtc::RWLockWrapper> FavoriteManager::g_csFavUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> FavoriteManager::g_csHubs = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> FavoriteManager::g_csDirs = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> FavoriteManager::g_csUserCommand = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
StringSet FavoriteManager::g_redirect_hubs;

FavoriteManager::FavoriteManager()
{
	ClientManager::getInstance()->addListener(this);
	
	File::ensureDirectory(Util::getHubListsPath());
}

FavoriteManager::~FavoriteManager()
{
	ClientManager::getInstance()->removeListener(this);
	shutdown();
	
	for_each(g_favoriteHubs.begin(), g_favoriteHubs.end(), [](auto p) { delete p; });
	for_each(g_recentHubs.begin(), g_recentHubs.end(), [](auto p) { delete p; });
	for_each(g_previewApplications.begin(), g_previewApplications.end(), [](auto p) { delete p; });
}

size_t FavoriteManager::getCountFavsUsers()
{
	CFlyReadLock(*g_csFavUsers);
	return g_fav_users_map.size();
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

UserCommand FavoriteManager::addUserCommand(int type, int ctx, Flags::MaskType flags, const string& name, const string& command, const string& to, const string& hub)
{
	const UserCommand uc(g_lastId++, type, ctx, flags, name, command, to, hub);
	{
		// No dupes, add it...
		CFlyWriteLock(*g_csUserCommand);
		g_userCommands.push_back(uc);
	}
	if (!uc.isSet(UserCommand::FLAG_NOSAVE))
		saveFavorites();
	
	return uc;
}

bool FavoriteManager::getUserCommand(int cid, UserCommand& uc)
{
	CFlyReadLock(*g_csUserCommand);
	for (auto i = g_userCommands.cbegin(); i != g_userCommands.cend(); ++i)
	{
		if (i->getId() == cid)
		{
			uc = *i;
			return true;
		}
	}
	return false;
}

bool FavoriteManager::moveUserCommand(int cid, int delta)
{
	dcassert(delta == -1 || delta == 1);
	CFlyWriteLock(*g_csUserCommand);
	if (delta == -1)
	{
		UserCommand::List::iterator prev = g_userCommands.end();
		for (auto i = g_userCommands.begin(); i != g_userCommands.end(); ++i)
		{
			if (i->getId() == cid)
			{
				if (prev == g_userCommands.end()) return false;
				std::swap(*i, *prev);
				return true;
			}
			prev = i;
		}
	}
	else if (delta == 1)
	{
		for (auto i = g_userCommands.begin(); i != g_userCommands.end(); ++i)
		{
			if (i->getId() == cid)
			{
				UserCommand::List::iterator next = i;
				if (++next == g_userCommands.end()) return false;
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
		CFlyWriteLock(*g_csUserCommand);
		for (auto i = g_userCommands.begin(); i != g_userCommands.end(); ++i)
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

int FavoriteManager::findUserCommand(const string& name, const string& hub)
{
	CFlyReadLock(*g_csUserCommand);
	for (auto i = g_userCommands.cbegin(); i != g_userCommands.cend(); ++i)
		if (i->getName() == name && i->getHub() == hub)
			return i->getId();
	return -1;
}

void FavoriteManager::removeUserCommandCID(int cid)
{
	{
		CFlyWriteLock(*g_csUserCommand);
		for (auto i = g_userCommands.cbegin(); i != g_userCommands.cend(); ++i)
		{
			if (i->getId() == cid)
			{
				bool nosave = i->isSet(UserCommand::FLAG_NOSAVE);
				g_userCommands.erase(i);
				if (nosave) return;
				break;
			}
		}
	}
	saveFavorites();
}

void FavoriteManager::shutdown()
{
	saveRecents();
}

void FavoriteManager::prepareClose()
{
	CFlyWriteLock(*g_csUserCommand);
}

#ifdef _DEBUG
size_t FavoriteManager::countHubUserCommands(const string& hub)
{
	size_t count = 0;
	{
		CFlyReadLock(*g_csUserCommand);
		for (auto i = g_userCommands.cbegin(); i != g_userCommands.cend(); ++i)
		{
			if (i->isSet(UserCommand::FLAG_NOSAVE) && i->getHub() == hub)
				count++;
		}
	}
	return count;
}
#endif

void FavoriteManager::removeHubUserCommands(int ctx, const string& hub)
{
	CFlyWriteLock(*g_csUserCommand);
	{
		for (auto i = g_userCommands.cbegin(); i != g_userCommands.cend();)
		{
			if (i->isSet(UserCommand::FLAG_NOSAVE) && i->getHub() == hub && (i->getCtx() & ctx))
				i = g_userCommands.erase(i);
			else
				++i;
		}
	}
}

void FavoriteManager::updateEmptyStateL()
{
	g_fav_users.clear();
	getFavoriteUsersNamesL(g_fav_users, false);
	//getFavoriteUsersNamesL(m_ban_users,true);
}

// [+] SSA addUser (Unified)
bool FavoriteManager::addUserL(const UserPtr& aUser, FavoriteMap::iterator& iUser, bool create /*= true*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	// [!] always use external lock for this function.
	iUser = g_fav_users_map.find(aUser->getCID());
	if (iUser == g_fav_users_map.end() && create)
	{
		StringList hubs = ClientManager::getHubs(aUser->getCID(), Util::emptyString);
		StringList nicks = ClientManager::getNicks(aUser->getCID(), Util::emptyString);
		
		/// @todo make this an error probably...
		if (hubs.empty())
			hubs.push_back(Util::emptyString);
		if (nicks.empty())
			nicks.push_back(Util::emptyString);
			
		iUser = g_fav_users_map.insert(make_pair(aUser->getCID(), FavoriteUser(aUser, nicks[0], hubs[0]))).first;
		updateEmptyStateL();
		return true;
	}
	return false;
}

bool FavoriteManager::getFavUserParam(const UserPtr& aUser, FavoriteUser::MaskType& flags, int& uploadLimit)
{
	CFlyReadLock(*g_csFavUsers);
	const auto user = g_fav_users_map.find(aUser->getCID());
	if (user != g_fav_users_map.end())
	{
		flags = user->second.getFlags();
		uploadLimit = user->second.uploadLimit;
		return true;
	}
	return false;
}

bool FavoriteManager::isNoFavUserOrUserIgnorePrivate(const UserPtr& aUser)
{
	CFlyReadLock(*g_csFavUsers);
	const auto user = g_fav_users_map.find(aUser->getCID());
	return user == g_fav_users_map.end() || user->second.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE);
}

bool FavoriteManager::isNoFavUserOrUserBanUpload(const UserPtr& aUser)
{
	bool isBanned;
	const bool isFav = isFavoriteUser(aUser, isBanned);
	return !isFav || isBanned;
}

bool FavoriteManager::getFavoriteUser(const UserPtr& aUser, FavoriteUser& favuser)
{
	CFlyReadLock(*g_csFavUsers);
	const auto user = g_fav_users_map.find(aUser->getCID());
	if (user != g_fav_users_map.end())
	{
		favuser = user->second;
		return true;
	}
	return false;
}

bool FavoriteManager::isFavoriteUser(const UserPtr& aUser, bool& isBanned)
{
	CFlyReadLock(*g_csFavUsers);
	bool result;
	const auto user = g_fav_users_map.find(aUser->getCID());
	if (user != g_fav_users_map.end())
	{
		result = true;
		isBanned = user->second.uploadLimit == FavoriteUser::UL_BAN;
	}
	else
	{
		isBanned = false;
		result = false;
	}
	return result;
}

void FavoriteManager::getFavoriteUsersNamesL(StringSet& p_users, bool p_is_ban) // TODO оптимизировать упаковку в уникальные ники отложенно в момент измени€ базовой мапы users.
{
	for (auto i = g_fav_users_map.cbegin(); i != g_fav_users_map.cend(); ++i)
	{
		const string& nick = i->second.nick;
		dcassert(!nick.empty());
		if (!nick.empty()) // Ќики могут дублировать и могут быть пустыми.
		{
			const bool l_is_ban = i->second.uploadLimit == FavoriteUser::UL_BAN;
			if ((!p_is_ban && !l_is_ban) || (p_is_ban && l_is_ban))
				p_users.insert(nick);
		}
	}
}

void FavoriteManager::addFavoriteUser(const UserPtr& aUser)
{
	// [!] SSA see _addUserIfnotExist function
	{
		FavoriteMap::iterator i;
		FavoriteUser l_fav_user;
		{
			CFlyWriteLock(*g_csFavUsers);
			if (!addUserL(aUser, i))
				return;
			l_fav_user = i->second;
		}
		fly_fire1(FavoriteManagerListener::UserAdded(), l_fav_user);
	}
	saveFavorites();
}

void FavoriteManager::removeFavoriteUser(const UserPtr& aUser)
{
	{
		FavoriteUser l_fav_user;
		{
			CFlyWriteLock(*g_csFavUsers);
			const auto i = g_fav_users_map.find(aUser->getCID());
			if (i == g_fav_users_map.end())
				return;
			l_fav_user = i->second;
			g_fav_users_map.erase(i);
			updateEmptyStateL();
		}
		fly_fire1(FavoriteManagerListener::UserRemoved(), l_fav_user);
	}
	saveFavorites();
}

string FavoriteManager::getUserUrl(const UserPtr& aUser)
{
	CFlyReadLock(*g_csFavUsers);
	const auto user = g_fav_users_map.find(aUser->getCID());
	if (user != g_fav_users_map.end())
		return user->second.url;
	return string();
}

FavoriteHubEntry* FavoriteManager::addFavorite(const FavoriteHubEntry& entry, const AutoStartType autostart/* = NOT_CHANGE*/)
{
	FavoriteHubEntry* fhe = getFavoriteHubEntry(entry.getServer());
	if (fhe)
	{
		if (autostart != NOT_CHANGE)
		{
			fhe->setConnect(autostart == ADD);
			fly_fire1(FavoriteManagerListener::FavoriteAdded(), nullptr); // rebuild fav hubs list
		}
		return fhe;
	}
	fhe = new FavoriteHubEntry(entry);
	fhe->setConnect(autostart == ADD);
	{
		CFlyWriteLock(*g_csHubs);
		g_favoriteHubs.push_back(fhe);
	}
	fly_fire1(FavoriteManagerListener::FavoriteAdded(), fhe);
	saveFavorites();
	return fhe;
}

void FavoriteManager::removeFavorite(const FavoriteHubEntry* entry)
{
	{
		CFlyWriteLock(*g_csHubs);
		auto i = find(g_favoriteHubs.begin(), g_favoriteHubs.end(), entry);
		if (i == g_favoriteHubs.end())
			return;
			
		g_favoriteHubs.erase(i);
	}
	fly_fire1(FavoriteManagerListener::FavoriteRemoved(), entry);
	delete entry;
	saveFavorites();
}

bool FavoriteManager::addFavoriteDir(string aDirectory, const string& aName, const string& aExt)
{
	AppendPathSeparator(aDirectory); //[+]PPA
	
	{
		CFlyWriteLock(*g_csDirs);
		for (auto i = g_favoriteDirs.cbegin(); i != g_favoriteDirs.cend(); ++i)
		{
			if ((strnicmp(aDirectory, i->dir, i->dir.length()) == 0) && (strnicmp(aDirectory, i->dir, aDirectory.length()) == 0))
			{
				return false;
			}
			if (stricmp(aName, i->name) == 0)
			{
				return false;
			}
			if (!aExt.empty() && stricmp(aExt, Util::toSettingString(i->ext)) == 0) // [!] IRainman opt.
			{
				return false;
			}
		}
		FavoriteDirectory favDir = { Util::splitSettingAndReplaceSpace(aExt), aDirectory, aName };
		g_favoriteDirs.push_back(favDir);
	}
	saveFavorites();
	return true;
}

bool FavoriteManager::removeFavoriteDir(const string& aName)
{
	string d(aName);
	
	AppendPathSeparator(d); //[+]PPA
	
	bool upd = false;
	{
		CFlyWriteLock(*g_csDirs);
		for (auto j = g_favoriteDirs.cbegin(); j != g_favoriteDirs.cend(); ++j)
		{
			if (stricmp(j->dir.c_str(), d.c_str()) == 0)
			{
				g_favoriteDirs.erase(j);
				upd = true;
				break;
			}
		}
	}
	if (upd)
		saveFavorites();
	return upd;
}

bool FavoriteManager::renameFavoriteDir(const string& aName, const string& anotherName)
{
	bool upd = false;
	{
		CFlyWriteLock(*g_csDirs);
		for (auto j = g_favoriteDirs.begin(); j != g_favoriteDirs.end(); ++j)
		{
			if (stricmp(j->name.c_str(), aName.c_str()) == 0)
			{
				j->name = anotherName;
				upd = true;
				break;
			}
		}
	}
	if (upd)
		saveFavorites();
	return upd;
}

bool FavoriteManager::updateFavoriteDir(const string& aName, const string& dir, const string& ext) // [!] IRainman opt.
{
	bool upd = false;
	{
		CFlyWriteLock(*g_csDirs);
		for (auto j = g_favoriteDirs.begin(); j != g_favoriteDirs.end(); ++j)
		{
			if (stricmp(j->name.c_str(), aName.c_str()) == 0)
			{
				j->dir = dir;
				j->ext = Util::splitSettingAndReplaceSpace(ext);
				j->name = aName;
				upd = true;
				break;
			}
		}
	}
	if (upd)
		saveFavorites();
	return upd;
}

string FavoriteManager::getDownloadDirectory(const string& ext)
{
	if (ext.size() > 1)
	{
		CFlyReadLock(*g_csDirs);
		for (auto i = g_favoriteDirs.cbegin(); i != g_favoriteDirs.cend(); ++i)
		{
			for (auto j = i->ext.cbegin(); j != i->ext.cend(); ++j) // [!] IRainman opt.
			{
				if (stricmp(ext.substr(1).c_str(), j->c_str()) == 0)
					return i->dir;
			}
		}
	}
	return SETTING(DOWNLOAD_DIRECTORY);
}

RecentHubEntry* FavoriteManager::addRecent(const RecentHubEntry& aEntry)
{
	if (aEntry.getRedirect())
	{
		g_redirect_hubs.insert(aEntry.getServer());
	}
	auto i = getRecentHub(aEntry.getServer());
	if (i != g_recentHubs.end())
	{
		return *i;
	}
	RecentHubEntry* f = new RecentHubEntry(aEntry);
	g_recentHubs.push_back(f);
	g_recent_dirty = true;
	fly_fire1(FavoriteManagerListener::RecentAdded(), f);
	return f;
}

void FavoriteManager::removeRecent(const RecentHubEntry* entry)
{
	const auto& i = find(g_recentHubs.begin(), g_recentHubs.end(), entry);
	if (i == g_recentHubs.end())
	{
		return;
	}
	fly_fire1(FavoriteManagerListener::RecentRemoved(), entry);
	g_recentHubs.erase(i);
	g_recent_dirty = true;
	delete entry;
}

void FavoriteManager::updateRecent(const RecentHubEntry* entry)
{
	g_recent_dirty = true;
	if (!ClientManager::isBeforeShutdown())
	{
		const auto i = find(g_recentHubs.begin(), g_recentHubs.end(), entry);
		if (i == g_recentHubs.end())
		{
			dcassert(0);
			return;
		}
		fly_fire1(FavoriteManagerListener::RecentUpdated(), entry);
	}
}

void FavoriteManager::saveFavorites()
{
	if (g_dontSave)
		return;
	CFlySafeGuard<uint16_t> l_satrt(g_dontSave);
	try
	{
		SimpleXML xml;
		
		xml.addTag("Favorites");
		xml.stepIn();
		
		//xml.addChildAttrib("ConfigVersion", string(A_REVISION_NUM_STR));// [+] IRainman fav options
		
		xml.addTag("Hubs");
		xml.stepIn();
		
		{
			CFlyReadLock(*g_csHubs);
			for (auto i = g_favHubGroups.cbegin(), iend = g_favHubGroups.cend(); i != iend; ++i)
			{
				xml.addTag("Group");
				xml.addChildAttrib("Name", i->first);
				xml.addChildAttrib("Private", i->second.priv);
			}
			for (auto i = g_favoriteHubs.cbegin(); i != g_favoriteHubs.cend(); ++i)
			{
				xml.addTag("Hub");
				xml.addChildAttrib("Name", (*i)->getName());
				xml.addChildAttrib("Connect", (*i)->getConnect());
				xml.addChildAttrib("Description", (*i)->getDescription());
				xml.addChildAttribIfNotEmpty("Nick", (*i)->getNick(false));
				xml.addChildAttribIfNotEmpty("Password", (*i)->getPassword());
				xml.addChildAttrib("Server", (*i)->getServer());
				xml.addChildAttribIfNotEmpty("UserDescription", (*i)->getUserDescription());
				if (!Util::isAdcHub((*i)->getServer()))
				{
					xml.addChildAttrib("Encoding", (*i)->getEncoding());
				}
				xml.addChildAttribIfNotEmpty("AwayMsg", (*i)->getAwayMsg());
				xml.addChildAttribIfNotEmpty("Email", (*i)->getEmail());
				xml.addChildAttrib("WindowPosX", (*i)->getWindowPosX());
				xml.addChildAttrib("WindowPosY", (*i)->getWindowPosY());
				xml.addChildAttrib("WindowSizeX", (*i)->getWindowSizeX());
				xml.addChildAttrib("WindowSizeY", (*i)->getWindowSizeY());
				xml.addChildAttrib("WindowType", (*i)->getWindowType());
				xml.addChildAttrib("ChatUserSplitSize", (*i)->getChatUserSplit());
#ifdef SCALOLAZ_HUB_SWITCH_BTN
				xml.addChildAttrib("ChatUserSplitState", (*i)->getChatUserSplitState());
#endif
				xml.addChildAttrib("HideShare", (*i)->getHideShare()); // Save paramethers always IRAINMAN_INCLUDE_HIDE_SHARE_MOD
				xml.addChildAttrib("ShowJoins", (*i)->getShowJoins()); // Show joins
				xml.addChildAttrib("ExclChecks", (*i)->getExclChecks()); // Excl. from client checking
				xml.addChildAttrib("ExclusiveHub", (*i)->getExclusiveHub()); // Exclusive Hub
				xml.addChildAttrib("SuppressChatAndPM", (*i)->getSuppressChatAndPM());
				xml.addChildAttrib("UserListState", (*i)->getUserListState());
				xml.addChildAttrib("HeaderOrder", (*i)->getHeaderOrder());
				xml.addChildAttrib("HeaderWidths", (*i)->getHeaderWidths());
				xml.addChildAttrib("HeaderVisible", (*i)->getHeaderVisible());
				xml.addChildAttrib("HeaderSort", (*i)->getHeaderSort());
				xml.addChildAttrib("HeaderSortAsc", (*i)->getHeaderSortAsc());
				xml.addChildAttribIfNotEmpty("RawOne", (*i)->getRawOne());
				xml.addChildAttribIfNotEmpty("RawTwo", (*i)->getRawTwo());
				xml.addChildAttribIfNotEmpty("RawThree", (*i)->getRawThree());
				xml.addChildAttribIfNotEmpty("RawFour", (*i)->getRawFour());
				xml.addChildAttribIfNotEmpty("RawFive", (*i)->getRawFive());
				xml.addChildAttrib("Mode", Util::toString((*i)->getMode()));
				xml.addChildAttribIfNotEmpty("IP", (*i)->getIP());
				xml.addChildAttribIfNotEmpty("OpChat", (*i)->getOpChat());
				xml.addChildAttrib("SearchInterval", Util::toString((*i)->getSearchInterval()));
				xml.addChildAttrib("SearchIntervalPassive", Util::toString((*i)->getSearchIntervalPassive()));
				xml.addChildAttribIfNotEmpty("ClientName", (*i)->getClientName());
				xml.addChildAttribIfNotEmpty("ClientVersion", (*i)->getClientVersion());
				xml.addChildAttrib("OverrideId", Util::toString((*i)->getOverrideId())); // !SMT!-S
				xml.addChildAttribIfNotEmpty("Group", (*i)->getGroup());
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
				xml.addChildAttrib("Status", (*i)->getConnectionStatus().getStatus());
				xml.addChildAttrib("LastAttempts", (*i)->getConnectionStatus().getLastAttempts());
				xml.addChildAttrib("LastSucces", (*i)->getConnectionStatus().getLastSucces());
#endif
			}
		}
		xml.stepOut();
		xml.addTag("Users");
		xml.stepIn();
		{
			CFlyReadLock(*g_csFavUsers);
			for (auto i = g_fav_users_map.cbegin(), iend = g_fav_users_map.cend(); i != iend; ++i)
			{
				const auto &u = i->second;
				xml.addTag("User");
				xml.addChildAttrib("Nick", u.nick);
				xml.addChildAttrib("URL", u.url);
				if (u.lastSeen)
					xml.addChildAttrib("LastSeen", u.lastSeen);
				if (u.isSet(FavoriteUser::FLAG_GRANT_SLOT))
					xml.addChildAttrib("GrantSlot", true);
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
			CFlyReadLock(*g_csUserCommand);
			for (auto i = g_userCommands.cbegin(); i != g_userCommands.cend(); ++i)
			{
				if (!i->isSet(UserCommand::FLAG_NOSAVE))
				{
					xml.addTag("UserCommand");
					xml.addChildAttrib("Type", i->getType());
					xml.addChildAttrib("Context", i->getCtx());
					xml.addChildAttrib("Name", i->getName());
					xml.addChildAttrib("Command", i->getCommand());
					xml.addChildAttrib("Hub", i->getHub());
				}
			}
		}
		xml.stepOut();
		
		//Favorite download to dirs
		xml.addTag("FavoriteDirs");
		xml.stepIn();
		{
			CFlyReadLock(*g_csDirs);
			for (auto i = g_favoriteDirs.cbegin(), iend = g_favoriteDirs.cend(); i != iend; ++i)
			{
				xml.addTag("Directory", i->dir);
				xml.addChildAttrib("Name", i->name);
				xml.addChildAttrib("Extensions", Util::toSettingString(i->ext));
			}
		}
		xml.stepOut();
		
		xml.stepOut();
		
		const string fname = getConfigFavoriteFile();
		
		const string l_tmp_file = fname + ".tmp";
		{
			File f(l_tmp_file, File::WRITE, File::CREATE | File::TRUNCATE);
			f.write(SimpleXML::utf8Header);
			f.write(xml.toXML());
		}
		// ѕроверим валидность XML
		try
		{
			SimpleXML xml_check;
			xml_check.fromXML(File(l_tmp_file, File::READ, File::OPEN).read());
			File::deleteFile(fname);
			File::renameFile(l_tmp_file, fname);
		}
		catch (SimpleXMLException& e)
		{
			dcassert(0);
			LogManager::message("FavoriteManager::save error parse tmp file: " + l_tmp_file + " error = " + e.getError());
			File::deleteFile(l_tmp_file);
		}
	}
	catch (const Exception& e)
	{
		dcassert(0);
		dcdebug("FavoriteManager::save: %s\n", e.getError().c_str());
		LogManager::message("FavoriteManager::save error = " + e.getError());
	}
}

void FavoriteManager::saveRecents()
{
	if (g_recent_dirty)
	{
		CFlyRegistryMap values;
		for (auto i = g_recentHubs.cbegin(); i != g_recentHubs.cend(); ++i)
		{		
			string recentHubsStr;
			recentHubsStr += (*i)->getDescription();
			recentHubsStr += '\n';
			recentHubsStr += (*i)->getUsers();
			recentHubsStr += '\n';
			recentHubsStr += (*i)->getShared();
			recentHubsStr += '\n';
			recentHubsStr += (*i)->getServer();
			recentHubsStr += '\n';
			recentHubsStr += (*i)->getLastSeen();
			recentHubsStr += '\n';
			if ((*i)->getRedirect() == false)
				recentHubsStr += (*i)->getOpenTab();
			else
				recentHubsStr += "-";
			recentHubsStr += '\n';
			values[(*i)->getName()] = recentHubsStr;
		}
		CFlylinkDBManager::getInstance()->save_registry(values, e_RecentHub, true);
		g_recent_dirty = false;
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
	
	try
	{
		SimpleXML xml;
		Util::migrate(getConfigFavoriteFile());
		//LogManager::message("FavoriteManager::load File = " + getConfigFavoriteFile());
		xml.fromXML(File(getConfigFavoriteFile(), File::READ, File::OPEN).read());
		
		if (xml.findChild("Favorites"))
		{
			xml.stepIn();
			load(xml);
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		LogManager::message("[Error] FavoriteManager::load " + e.getError() + " File = " + getConfigFavoriteFile());
		dcdebug("FavoriteManager::load: %s\n", e.getError().c_str());
	}
	
	const bool oldConfigExist = !g_recentHubs.empty(); // [+] IRainman fix: FlylinkDC stores recents hubs in the sqlite database, so you need to keep the values in the database after loading the file.
	
	CFlyRegistryMap l_values;
	CFlylinkDBManager::getInstance()->load_registry(l_values, e_RecentHub);
	for (auto k = l_values.cbegin(); k != l_values.cend(); ++k)
	{
		const StringTokenizer<string> tok(k->second.m_val_str, '\n');
		if (tok.getTokens().size() > 3)
		{
			RecentHubEntry* e = new RecentHubEntry();
			e->setName(k->first);
			e->setDescription(tok.getTokens()[0]);
			e->setUsers(tok.getTokens()[1]);
			e->setShared(tok.getTokens()[2]);
			const string l_server = Util::formatDchubUrl(tok.getTokens()[3]);
			e->setServer(l_server);
			if (tok.getTokens().size() > 4) //-V112
			{
				e->setLastSeen(tok.getTokens()[4]);
				if (tok.getTokens().size() > 5) //-V112
				{
					e->setOpenTab(tok.getTokens()[5]);
				}
			}
			g_recentHubs.push_back(e);
			g_recent_dirty = true;
		}
	}
	// [+] IRainman fix: FlylinkDC stores recents hubs in the sqlite database, so you need to keep the values in the database after loading the file.
	if (oldConfigExist)
	{
		g_recent_dirty = true;
	}
}

void FavoriteManager::load(SimpleXML& aXml)
{
	static bool g_is_ISPDisableFlylinkDCSupportHub = false;
	{
		CFlySafeGuard<uint16_t> l_satrt(g_dontSave);
		//const int l_configVersion = Util::toInt(aXml.getChildAttrib("ConfigVersion"));// [+] IRainman fav options
		aXml.resetCurrentChild();
		if (aXml.findChild("Hubs"))
		{
			aXml.stepIn();
			{
				CFlyWriteLock(*g_csHubs);
				while (aXml.findChild("Group"))
				{
					const string& name = aXml.getChildAttrib("Name");
					if (name.empty())
						continue;
					const FavHubGroupProperties props = { aXml.getBoolChildAttrib("Private") };
					g_favHubGroups[name] = props;
				}
			}
			aXml.resetCurrentChild();
			g_AllHubUrls.clear();
			while (aXml.findChild("Hub"))
			{
				const bool isConnect = aXml.getBoolChildAttrib("Connect");
				const string currentServerUrl = Text::toLower(Util::formatDchubUrl(aXml.getChildAttrib("Server")));
#ifdef _DEBUG
				LogManager::message("Load favorites item: " + currentServerUrl);
#endif
#if 0 // Hardcoded hosts removed
				if (
				    currentServerUrl.find(".kurskhub.ru") != string::npos || // http://dchublist.ru/forum/viewtopic.php?p=24102#p24102
				    currentServerUrl.find(".dchub.net") != string::npos ||
				    currentServerUrl.find(".dchublist.biz") != string::npos ||
				    currentServerUrl.find(".dchublists.com") != string::npos)
				{
					LogManager::message("Block hub: " + currentServerUrl);
					continue;
				}
#endif
				g_AllHubUrls.insert(currentServerUrl);
					
				FavoriteHubEntry* e = new FavoriteHubEntry();
				const string& name = aXml.getChildAttrib("Name");
				e->setName(name);
				
				e->setConnect(isConnect);
				const string& description = aXml.getChildAttrib("Description");
				const string& group = aXml.getChildAttrib("Group");
				e->setDescription(description);
				e->setServer(currentServerUrl);
				const unsigned searchInterval = Util::toUInt32(aXml.getChildAttrib("SearchInterval"));
				e->setSearchInterval(searchInterval);
				const bool userListState = aXml.getBoolChildAttrib("UserListState");
				e->setUserListState(userListState);
				const bool suppressChatAndPM = aXml.getBoolChildAttrib("SuppressChatAndPM");
				e->setSuppressChatAndPM(suppressChatAndPM);
				
				const bool isOverrideId = Util::toInt(aXml.getChildAttrib("OverrideId")) != 0;
				string clientName = aXml.getChildAttrib("ClientName");
				string clientVersion = aXml.getChildAttrib("ClientVersion");
				
				if (Util::isAdcHub(currentServerUrl))
					e->setEncoding(Text::g_utf8);
				else
					e->setEncoding(aXml.getChildAttrib("Encoding"));

				string nick = aXml.getChildAttrib("Nick");
				const string& password = aXml.getChildAttrib("Password");
				const auto posRndMarker = nick.rfind("_RND_");
				if (password.empty() && posRndMarker != string::npos && atoi(nick.c_str() + posRndMarker + 5) > 1000)
					nick.clear();
				e->setNick(nick);
				e->setPassword(password);
				e->setUserDescription(aXml.getChildAttrib("UserDescription"));
				e->setAwayMsg(aXml.getChildAttrib("AwayMsg"));
				e->setEmail(aXml.getChildAttrib("Email"));
				bool valueOutOfBounds = false;
				e->setWindowPosX(aXml.getIntChildAttrib("WindowPosX", 0, 10, valueOutOfBounds));
				e->setWindowPosY(aXml.getIntChildAttrib("WindowPosY", 0, 100, valueOutOfBounds));
				e->setWindowSizeX(aXml.getIntChildAttrib("WindowSizeX", 50, 1600, valueOutOfBounds));
				e->setWindowSizeY(aXml.getIntChildAttrib("WindowSizeY", 50, 1600, valueOutOfBounds));
				if (!valueOutOfBounds)
					e->setWindowType(aXml.getIntChildAttrib("WindowType", "3")); // SW_MAXIMIZE if missing
				else
					e->setWindowType(3); // SW_MAXIMIZE
				e->setChatUserSplit(aXml.getIntChildAttrib("ChatUserSplitSize"));
#ifdef SCALOLAZ_HUB_SWITCH_BTN
				e->setChatUserSplitState(aXml.getBoolChildAttrib("ChatUserSplitState"));
#endif
				e->setHideShare(aXml.getBoolChildAttrib("HideShare")); // Hide Share Mod
				e->setShowJoins(aXml.getBoolChildAttrib("ShowJoins")); // Show joins
				e->setExclChecks(aXml.getBoolChildAttrib("ExclChecks")); // Excl. from client checking
				e->setExclusiveHub(aXml.getBoolChildAttrib("ExclusiveHub")); // Exclusive Hub Mod
				e->setHeaderOrder(aXml.getChildAttrib("HeaderOrder", SETTING(HUB_FRAME_ORDER)));
				e->setHeaderWidths(aXml.getChildAttrib("HeaderWidths", SETTING(HUB_FRAME_WIDTHS)));
				e->setHeaderVisible(aXml.getChildAttrib("HeaderVisible", SETTING(HUB_FRAME_VISIBLE)));
				e->setHeaderSort(aXml.getIntChildAttrib("HeaderSort", "-1"));
				e->setHeaderSortAsc(aXml.getBoolChildAttrib("HeaderSortAsc"));
				e->setRawOne(aXml.getChildAttrib("RawOne"));
				e->setRawTwo(aXml.getChildAttrib("RawTwo"));
				e->setRawThree(aXml.getChildAttrib("RawThree"));
				e->setRawFour(aXml.getChildAttrib("RawFour"));
				e->setRawFive(aXml.getChildAttrib("RawFive"));
				e->setMode(Util::toInt(aXml.getChildAttrib("Mode")));
				e->setIP(aXml.getChildAttribTrim("IP"));
				e->setOpChat(aXml.getChildAttrib("OpChat"));
					
				if (clientName.empty())
				{
					const string& clientID = aXml.getChildAttrib("ClientId");
					if (!clientID.empty())
						splitClientId(clientID, clientName, clientVersion);
				}
				e->setClientName(clientName);
				e->setClientVersion(clientVersion);
				e->setOverrideId(isOverrideId);

				e->setGroup(group);
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
				e->setSavedConnectionStatus(Util::toInt(aXml.getChildAttrib("Status")),
				                            Util::toInt64(aXml.getChildAttrib("LastAttempts")),
				                            Util::toInt64(aXml.getChildAttrib("LastSucces")));
#endif
				{
					CFlyWriteLock(*g_csHubs);
					g_favoriteHubs.push_back(e);
				}
			}
			aXml.stepOut();
		}

		aXml.resetCurrentChild();
		{
			if (aXml.findChild("Users"))
			{
				aXml.stepIn();
				while (aXml.findChild("User"))
				{
					UserPtr u;
					// [!] FlylinkDC
					const string& nick = aXml.getChildAttrib("Nick");
					
					const string hubUrl = Util::formatDchubUrl(aXml.getChildAttrib("URL")); // [!] IRainman fix: toLower already called in formatDchubUrl ( decodeUrl )
					
					const string cid = Util::isAdcHub(hubUrl) ? aXml.getChildAttrib("CID") : ClientManager::makeCid(nick, hubUrl).toBase32();
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
					const uint32_t l_hub_id = CFlylinkDBManager::getInstance()->get_dic_hub_id(hubUrl);
#else
					const uint32_t l_hub_id = 0;
#endif
					// [~] FlylinkDC
					
					if (cid.length() != 39)
					{
						if (nick.empty() || hubUrl.empty())
							continue;
						u = ClientManager::getUser(nick, hubUrl, l_hub_id);
					}
					else
					{
						u = ClientManager::createUser(CID(cid), nick, l_hub_id);
					}
					
					CFlyWriteLock(*g_csFavUsers);
					auto i = g_fav_users_map.insert(make_pair(u->getCID(), FavoriteUser(u, nick, hubUrl))).first;
					auto &user = i->second;
					
					if (aXml.getBoolChildAttrib("IgnorePrivate")) // !SMT!-S
						user.setFlag(FavoriteUser::FLAG_IGNORE_PRIVATE);
					if (aXml.getBoolChildAttrib("FreeAccessPM")) // !SMT!-PSW
						user.setFlag(FavoriteUser::FLAG_FREE_PM_ACCESS);
						
					if (aXml.getBoolChildAttrib("SuperUser"))
						user.uploadLimit = FavoriteUser::UL_SU;
					else
						user.uploadLimit = FavoriteUser::UPLOAD_LIMIT(aXml.getIntChildAttrib("UploadLimit"));
						
					if (aXml.getBoolChildAttrib("GrantSlot"))
						user.setFlag(FavoriteUser::FLAG_GRANT_SLOT);
						
					user.lastSeen = aXml.getInt64ChildAttrib("LastSeen");					
					user.description = aXml.getChildAttrib("UserDescription");
				}
				updateEmptyStateL();
				aXml.stepOut();
			}
			aXml.resetCurrentChild();
			if (aXml.findChild("UserCommands"))
			{
				aXml.stepIn();
				while (aXml.findChild("UserCommand"))
				{
					addUserCommand(aXml.getIntChildAttrib("Type"), aXml.getIntChildAttrib("Context"), 0, aXml.getChildAttrib("Name"),
					               aXml.getChildAttrib("Command"), aXml.getChildAttrib("To"), aXml.getChildAttrib("Hub"));
				}
				aXml.stepOut();
			}
			//Favorite download to dirs
			aXml.resetCurrentChild();
			if (aXml.findChild("FavoriteDirs"))
			{
				aXml.stepIn();
				while (aXml.findChild("Directory"))
				{
					const auto& virt = aXml.getChildAttrib("Name");
					const auto& ext = aXml.getChildAttrib("Extensions");
					const auto& d = aXml.getChildData();
					addFavoriteDir(d, virt, ext);
				}
				aXml.stepOut();
			}
		}
	}
}

void FavoriteManager::userUpdated(const OnlineUser& info)
{
	if (!ClientManager::isBeforeShutdown())
	{
		CFlyReadLock(*g_csFavUsers);
		auto i = g_fav_users_map.find(info.getUser()->getCID());
		if (i == g_fav_users_map.end())
			return;
		i->second.update(info);
	}
}

FavoriteHubEntry* FavoriteManager::getFavoriteHubEntry(const string& aServer)
{
	CFlyReadLock(*g_csHubs); // [+] IRainman fix.
#ifdef _DEBUG
	static int g_count;
	static string g_last_url;
	if (g_last_url != aServer)
	{
		g_last_url = aServer;
	}
	else
	{
		// LogManager::message("FavoriteManager::getFavoriteHubEntry DUP call! srv = " + aServer +
		//                                    " favoriteHubs.size() = " + Util::toString(favoriteHubs.size()) +
		//                                    " g_count = " + Util::toString(++g_count));
	}
#endif
	for (auto i = g_favoriteHubs.cbegin(); i != g_favoriteHubs.cend(); ++i)
	{
		if ((*i)->getServer() == aServer)
		{
			return (*i);
		}
	}
	return nullptr;
}

FavoriteHubEntryList FavoriteManager::getFavoriteHubs(const string& group)
{
	FavoriteHubEntryList ret;
	CFlyReadLock(*g_csHubs);
	for (auto i = g_favoriteHubs.cbegin(), iend = g_favoriteHubs.cend(); i != iend; ++i)
	{
		if ((*i)->getGroup() == group)
		{
			ret.push_back(*i);
		}
	}
	return ret;
}

bool FavoriteManager::isPrivate(const string& p_url)
{
//	dcassert(!p_url.empty());
	if (!p_url.empty())
	{
		// TODO «оветс€ без полного лока
		FavoriteHubEntry* fav = getFavoriteHubEntry(p_url); // https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=23193
		if (fav)
		{
			const string& name = fav->getGroup();
			if (!name.empty())
			{
				CFlyReadLock(*g_csHubs); // [+] IRainman fix.
				const auto group = g_favHubGroups.find(name);
				if (group != g_favHubGroups.end())
				{
					return group->second.priv;
				}
			}
		}
	}
	return false;
}

void FavoriteManager::setUploadLimit(const UserPtr& aUser, int lim, bool createUser/* = true*/)
{
	ConnectionManager::setUploadLimit(aUser, lim);
	FavoriteMap::iterator i;
	FavoriteUser l_fav_user;
	bool l_is_added = false;
	{
		CFlyWriteLock(*g_csFavUsers);
		l_is_added = addUserL(aUser, i, createUser);
		if (i == g_fav_users_map.end())
			return;
		i->second.uploadLimit = FavoriteUser::UPLOAD_LIMIT(lim);
		l_fav_user = i->second;
	}
	speakUserUpdate(l_is_added, l_fav_user);
	saveFavorites();
}

bool FavoriteManager::getFlag(const UserPtr& aUser, FavoriteUser::Flags f)
{
	if (!ClientManager::isBeforeShutdown())
	{
		CFlyReadLock(*g_csFavUsers);
		const auto i = g_fav_users_map.find(aUser->getCID());
		if (i != g_fav_users_map.end())
			return i->second.isSet(f);
	}
	return false;
}

void FavoriteManager::setFlag(const UserPtr& aUser, FavoriteUser::Flags f, bool value, bool createUser /*= true*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		FavoriteMap::iterator i;
		FavoriteUser l_fav_user;
		bool l_is_added = false;
		{
			CFlyWriteLock(*g_csFavUsers);
			l_is_added = addUserL(aUser, i, createUser);
			if (i == g_fav_users_map.end())
				return;
			if (value)
				i->second.setFlag(f);
			else
				i->second.unsetFlag(f);
			l_fav_user = i->second;
		}
		
		speakUserUpdate(l_is_added, l_fav_user);
	}
	saveFavorites();
}

void FavoriteManager::setUserDescription(const UserPtr& aUser, const string& aDescription)
{
	{
		CFlyReadLock(*g_csFavUsers);
		auto i = g_fav_users_map.find(aUser->getCID());
		if (i == g_fav_users_map.end())
			return;
		i->second.description = aDescription;
	}
	saveFavorites();
}

void FavoriteManager::loadRecents(SimpleXML& aXml)
{
	aXml.resetCurrentChild();
	if (aXml.findChild("Hubs"))
	{
		aXml.stepIn();
		while (aXml.findChild("Hub"))
		{
			RecentHubEntry* e = new RecentHubEntry();
			e->setName(aXml.getChildAttrib("Name"));
			e->setDescription(aXml.getChildAttrib("Description"));
			e->setUsers(aXml.getChildAttrib("Users"));
			e->setShared(aXml.getChildAttrib("Shared"));
			e->setServer(Util::formatDchubUrl(aXml.getChildAttrib("Server")));
			e->setLastSeen(aXml.getChildAttrib("DateTime"));
			g_recentHubs.push_back(e);
			g_recent_dirty = true;
		}
		aXml.stepOut();
	}
}

RecentHubEntry::Iter FavoriteManager::getRecentHub(const string& aServer)
{
	for (auto i = g_recentHubs.cbegin(); i != g_recentHubs.cend(); ++i)
	{
		if ((*i)->getServer() == aServer)
			return i;
	}
	return g_recentHubs.end();
}

RecentHubEntry* FavoriteManager::getRecentHubEntry(const string& aServer)
{
	// TODO Lock
	for (auto i = g_recentHubs.cbegin(); i != g_recentHubs.cend(); ++i)
	{
		RecentHubEntry* r = *i;
		if (stricmp(r->getServer(), aServer) == 0)
		{
			return r;
		}
	}
	return nullptr;
}

void FavoriteManager::getUserCommands(vector<UserCommand>& result, int ctx, const StringList& hubs) const
{
	result.clear();
	vector<bool> isOp(hubs.size());
	
	for (size_t i = 0; i < hubs.size(); ++i)
		isOp[i] = ClientManager::isOp(ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks(), hubs[i]);
	
	{
		CFlyReadLock(*g_csUserCommand);
		for (auto i = g_userCommands.cbegin(); i != g_userCommands.cend(); ++i)
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
}

void FavoriteManager::on(UserUpdated, const OnlineUserPtr& user) noexcept
{
	userUpdated(*user);
}

void FavoriteManager::on(UserDisconnected, const UserPtr& aUser) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		{
			CFlyReadLock(*g_csFavUsers);
			auto i = g_fav_users_map.find(aUser->getCID());
			if (i == g_fav_users_map.end())
				return;
			i->second.lastSeen = GET_TIME(); // TODO: if ClientManager::isBeforeShutdown() returns true, it will not be updated
		}
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire1(FavoriteManagerListener::StatusChanged(), aUser);
		}
	}
}

void FavoriteManager::on(UserConnected, const UserPtr& aUser) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		{
			CFlyReadLock(*g_csFavUsers);
			if (!isUserExistL(aUser))
				return;
		}
		if (!ClientManager::isBeforeShutdown())
		{
			fly_fire1(FavoriteManagerListener::StatusChanged(), aUser);
		}
	}
}

void FavoriteManager::loadPreview(SimpleXML& aXml)
{
	aXml.resetCurrentChild();
	if (aXml.findChild("PreviewApps"))
	{
		aXml.stepIn();
		while (aXml.findChild("Application"))
		{
			addPreviewApp(aXml.getChildAttrib("Name"), aXml.getChildAttrib("Application"),
			              aXml.getChildAttrib("Arguments"), aXml.getChildAttrib("Extension"));
		}
		aXml.stepOut();
	}
}

void FavoriteManager::savePreview(SimpleXML& aXml)
{
	aXml.addTag("PreviewApps");
	aXml.stepIn();
	for (auto i = g_previewApplications.cbegin(); i != g_previewApplications.cend(); ++i)
	{
		aXml.addTag("Application");
		aXml.addChildAttrib("Name", (*i)->name);
		aXml.addChildAttrib("Application", (*i)->application);
		aXml.addChildAttrib("Arguments", (*i)->arguments);
		aXml.addChildAttrib("Extension", (*i)->extension);
	}
	aXml.stepOut();
}

#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
void FavoriteManager::changeConnectionStatus(const string& hubUrl, ConnectionStatus::Status status)
{
	FavoriteHubEntry* hub = getFavoriteHubEntry(hubUrl);
	if (hub)
	{
		hub->setConnectionStatus(status);
#ifdef UPDATE_CON_STATUS_ON_FAV_HUBS_IN_REALTIME
		fly_fire1(FavoriteManagerListener::FavoriteStatusChanged(), hub);
#endif
	}
}
#endif

void FavoriteManager::speakUserUpdate(const bool added, const FavoriteUser& p_fav_user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		{
			if (added)
			{
				fly_fire1(FavoriteManagerListener::UserAdded(), p_fav_user);
			}
			else
			{
				fly_fire1(FavoriteManagerListener::StatusChanged(), p_fav_user.user);
			}
		}
	}
}

PreviewApplication* FavoriteManager::addPreviewApp(const string& name, const string& application, const string& arguments, string p_extension) // [!] PVS V813 Decreased performance. The 'name', 'application', 'arguments', 'extension' arguments should probably be rendered as constant references. favoritemanager.h 366
{
	boost::replace_all(p_extension, " ", "");
	boost::replace_all(p_extension, ",", ";");
	PreviewApplication* pa = new PreviewApplication(name, application, arguments, p_extension);
	g_previewApplications.push_back(pa);
	return pa;
}

void FavoriteManager::removePreviewApp(const size_t index)
{
	if (g_previewApplications.size() > index)
	{
		auto i = g_previewApplications.begin() + index;
		delete *i;
		g_previewApplications.erase(i);
	}
}

PreviewApplication* FavoriteManager::getPreviewApp(const size_t index)
{
	if (g_previewApplications.size() > index)
	{
		return g_previewApplications[index];
	}
	return nullptr;
}

void FavoriteManager::removeallRecent()
{
	g_recentHubs.clear();
	g_recent_dirty = true;
}
