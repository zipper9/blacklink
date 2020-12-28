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

#ifndef DCPLUSPLUS_DCPP_FAVORITE_MANAGER_H
#define DCPLUSPLUS_DCPP_FAVORITE_MANAGER_H

#include <boost/unordered/unordered_map.hpp>

#include "UserCommand.h"
#include "FavoriteUser.h"
#include "ClientManagerListener.h"
#include "FavoriteManagerListener.h"
#include "HubEntry.h"
#include "FavHubGroup.h"
#include "TimerManager.h"
#include "RWLock.h"

class PreviewApplication
{
	public:
		typedef vector<PreviewApplication*> List;
		
		PreviewApplication() noexcept {}
		PreviewApplication(const string& n, const string& a, const string& r, const string& e) : name(n), application(a), arguments(r), extension(Text::toLower(e))
		{
		}
		~PreviewApplication() noexcept { }
		
		PreviewApplication(const PreviewApplication &) = delete;
		PreviewApplication& operator= (const PreviewApplication &) = delete;
		
		string name;
		string application;
		string arguments;
		string extension;
};

class SimpleXML;

class FavoriteManager : private Speaker<FavoriteManagerListener>,
	public Singleton<FavoriteManager>,
	private ClientManagerListener,
	private TimerManagerListener
{
	public:
		void addListener(FavoriteManagerListener* aListener)
		{
			Speaker<FavoriteManagerListener>::addListener(aListener);
		}
		void removeListener(FavoriteManagerListener* aListener)
		{
			Speaker<FavoriteManagerListener>::removeListener(aListener);
		}

		static void splitClientId(const string& id, string& name, string& version);
		
		// Favorite Users

		typedef boost::unordered_map<CID, FavoriteUser> FavoriteMap;
		class LockInstanceUsers
		{
			public:
				LockInstanceUsers() : fm(FavoriteManager::getInstance())
				{
					fm->csUsers->acquireShared();
				}
				~LockInstanceUsers()
				{
					fm->csUsers->releaseShared();
				}
				const FavoriteMap& getFavoriteUsersL() const
				{
					return fm->favoriteUsers;
				}

			private:
				FavoriteManager* const fm;
		};
		static const PreviewApplication::List& getPreviewApps()
		{
			return g_previewApplications;
		}
		
		void addFavoriteUser(const UserPtr& user);
		bool isFavoriteUser(const UserPtr& user, bool& isBanned) const;
		bool getFavoriteUser(const UserPtr& user, FavoriteUser& favuser) const;
		bool isFavUserAndNotBanned(const UserPtr& user) const;
		bool getFavUserParam(const UserPtr& user, FavoriteUser::MaskType& flags, int& uploadLimit) const;
		bool getFavUserParam(const UserPtr& user, FavoriteUser::MaskType& flags, int& uploadLimit, CID& shareGroup) const;

		void removeFavoriteUser(const UserPtr& user);

		void setUserAttributes(const UserPtr& user, FavoriteUser::Flags flags, int uploadLimit, const CID& shareGroup, const string& description);
		bool hasAutoGrantSlot(const UserPtr& user) const
		{
			return getFlag(user, FavoriteUser::FLAG_GRANT_SLOT);
		}
		void setAutoGrantSlot(const UserPtr& user, bool grant)
		{
			setFlag(user, FavoriteUser::FLAG_GRANT_SLOT, grant);
		}
		void userUpdated(const OnlineUser& info);
		string getUserUrl(const UserPtr& user) const;
		
		void setUploadLimit(const UserPtr& user, int lim, bool createUser = true);

		bool getFlag(const UserPtr& user, FavoriteUser::Flags) const;
		void setFlag(const UserPtr& user, FavoriteUser::Flags, bool flag, bool createUser = true);
		void setFlags(const UserPtr& user, FavoriteUser::Flags flags, FavoriteUser::Flags mask, bool createUser = true);
		
		bool hasIgnorePM(const UserPtr& user) const
		{
			return getFlag(user, FavoriteUser::FLAG_IGNORE_PRIVATE);
		}
		bool hasFreePM(const UserPtr& user) const
		{
			return getFlag(user, FavoriteUser::FLAG_FREE_PM_ACCESS);
		}

		// Favorite Hubs
		struct WindowInfo
		{
			int windowPosX;
			int windowPosY;
			int windowSizeX;
			int windowSizeY;
			int windowType;
			bool hideUserList;
			string headerOrder;
			string headerWidths;
			string headerVisible;
			int headerSort;
			bool headerSortAsc;
			int chatUserSplit;
			bool swapPanels;
		};

		bool addFavoriteHub(FavoriteHubEntry& entry, bool save = true);
		bool removeFavoriteHub(const string& server, bool save = true);
		bool removeFavoriteHub(int id, bool save = true);
		bool setFavoriteHub(const FavoriteHubEntry& entry);
		bool getFavoriteHub(const string& server, FavoriteHubEntry& entry) const;
		bool getFavoriteHub(int id, FavoriteHubEntry& entry) const;
		bool setFavoriteHubWindowInfo(const string& server, const WindowInfo& wi);
		bool getFavoriteHubWindowInfo(const string& server, WindowInfo& wi) const;
		bool setFavoriteHubPassword(const string& server, const string& password, bool addIfNotFound);
		bool setFavoriteHubAutoConnect(const string& server, bool autoConnect);
		bool setFavoriteHubAutoConnect(int id, bool autoConnect);
		bool isFavoriteHub(const string& server, int excludeID = 0) const;
		bool isPrivateHub(const string& server) const;
		const FavoriteHubEntry* getFavoriteHubEntryPtr(const string& server) const noexcept;
		const FavoriteHubEntry* getFavoriteHubEntryPtr(int id) const noexcept;
		void releaseFavoriteHubEntryPtr(const FavoriteHubEntry* fhe) const noexcept;
		void changeConnectionStatus(const string& hubUrl, ConnectionStatus::Status status);

		// Favorite hub groups

		class LockInstanceHubs
		{
				const bool unique;
				FavoriteManager* const fm;

			public:
				explicit LockInstanceHubs(FavoriteManager* fm, bool unique) : fm(fm), unique(unique)
				{
					if (unique)
						fm->csHubs->acquireExclusive();
					else
						fm->csHubs->acquireShared();
				}
				~LockInstanceHubs()
				{
					if (unique)
						fm->csHubs->releaseExclusive();
					else
						fm->csHubs->releaseShared();
				}
				const FavHubGroups& getFavHubGroups() const
				{
					return fm->favHubGroups;
				}
				FavoriteHubEntryList& getFavoriteHubs() const
				{
					return fm->favoriteHubs;
				}
		};

		void setFavHubGroups(FavHubGroups& newFavHubGroups)
		{
			CFlyWriteLock(*csHubs);
			swap(favHubGroups, newFavHubGroups);
		}
		
		// Favorite Directories

		struct FavoriteDirectory
		{
			StringList ext;
			string dir;
			string name;
		};
		typedef vector<FavoriteDirectory> FavDirList;
		
		static bool addFavoriteDir(const string& directory, const string& name, const string& ext);
		static bool removeFavoriteDir(const string& name);
		static bool updateFavoriteDir(const string& name, const string& newName, const string& directory, const string& ext);
		static string getDownloadDirectory(const string& ext);
		static size_t getFavoriteDirsCount()
		{
			g_csDirs->acquireShared();
			size_t result = g_favoriteDirs.size();
			g_csDirs->releaseShared();
			return result;
		}
		class LockInstanceDirs
		{
			public:
				LockInstanceDirs()
				{
					FavoriteManager::g_csDirs->acquireShared();
				}
				~LockInstanceDirs()
				{
					FavoriteManager::g_csDirs->releaseShared();
				}
				static const FavDirList& getFavoriteDirsL()
				{
					return FavoriteManager::g_favoriteDirs;
				}
		};
		
		// Recent Hubs

		static const RecentHubEntry::List& getRecentHubs()
		{
			return g_recentHubs;
		}
		
		RecentHubEntry* addRecent(const RecentHubEntry& aEntry);
		void removeRecent(const RecentHubEntry* entry);
		void updateRecent(const RecentHubEntry* entry);
		
		static RecentHubEntry* getRecentHubEntry(const string& aServer);
		static PreviewApplication* addPreviewApp(const string& name, const string& application, const string& arguments, string p_extension);
		static void removePreviewApp(const size_t index);
		static PreviewApplication* getPreviewApp(const size_t index);
		static void clearRecents();
		
		// User Commands

		UserCommand addUserCommand(int type, int ctx, Flags::MaskType flags, const string& name, const string& command, const string& to, const string& hub);
		bool getUserCommand(int cid, UserCommand& uc) const;
		int findUserCommand(const string& name, const string& hub) const;
		bool moveUserCommand(int cid, int delta);
		void updateUserCommand(const UserCommand& uc);
		void removeUserCommandCID(int cid);
		void removeHubUserCommands(int ctx, const string& hub);
#ifdef _DEBUG
		size_t countHubUserCommands(const string& hub) const;
#endif
		
		UserCommand::List getUserCommands() const
		{
			CFlyReadLock(*csUserCommand);
			return userCommands;
		}
		void getUserCommands(vector<UserCommand>& result, int ctx, const StringList& hub) const;
		
		void load();
		void saveFavorites();
		static void saveRecents();
		static bool isRedirectHub(const string& p_server)
		{
			auto i = g_redirect_hubs.find(p_server);
			if (i == g_redirect_hubs.end())
				return false;
			else
				return true;
		}
		
	private:
		FavoriteHubEntryList favoriteHubs;
		FavHubGroups favHubGroups;
		mutable std::unique_ptr<RWLock> csHubs;
		int favHubId;

		UserCommand::List userCommands;
		mutable std::unique_ptr<RWLock> csUserCommand;
		int userCommandId;

		static StringSet g_redirect_hubs;
		static RecentHubEntry::List g_recentHubs;
		static PreviewApplication::List g_previewApplications;
		
		FavoriteMap favoriteUsers;
		mutable std::unique_ptr<RWLock> csUsers;

		static FavDirList g_favoriteDirs;
		static std::unique_ptr<RWLock> g_csDirs;
		
		static int dontSave; // Used during loading to prevent saving.
		static bool recentsDirty;
		static bool favsDirty;
		static uint64_t recentsLastSave;
		static uint64_t favsLastSave;

	public:
		void shutdown();
		
		static void loadRecents(SimpleXML& aXml);
		static void loadPreview(SimpleXML& aXml);
		static void savePreview(SimpleXML& aXml);
		
	private:
		friend class Singleton<FavoriteManager>;
		
		FavoriteManager();
		~FavoriteManager();
		
		static RecentHubEntry::Iter getRecentHub(const string& aServer);
		
		// ClientManagerListener
		void on(UserUpdated, const OnlineUserPtr& user) noexcept override;
		void on(UserConnected, const UserPtr& user) noexcept override;
		void on(UserDisconnected, const UserPtr& user) noexcept override;

		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
		
		void load(SimpleXML& xml);
		                
		static string getFavoritesFile()
		{
			return Util::getConfigPath() + "Favorites.xml";
		}
		
		bool addUserL(const UserPtr& user, FavoriteMap::iterator& iUser, bool create = true);		
		void speakUserUpdate(const bool added, const FavoriteUser& user);
};

#endif // !defined(FAVORITE_MANAGER_H)
