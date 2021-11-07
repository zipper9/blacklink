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
		void addListener(FavoriteManagerListener* listener)
		{
			Speaker<FavoriteManagerListener>::addListener(listener);
		}
		void removeListener(FavoriteManagerListener* listener)
		{
			Speaker<FavoriteManagerListener>::removeListener(listener);
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

		void addFavoriteUser(const UserPtr& user);
		bool isFavoriteUser(const UserPtr& user, bool& isBanned) const;
		bool getFavoriteUser(const UserPtr& user, FavoriteUser& favuser) const;
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
		FavoriteUser::Flags getFlags(const UserPtr& user) const;
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
		bool setFavoriteHubPassword(const string& server, const string& nick, const string& password, bool addIfNotFound);
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
			WRITE_LOCK(*csHubs);
			swap(favHubGroups, newFavHubGroups);
		}

		bool addFavHubGroup(const string& name, bool priv);
		bool removeFavHubGroup(const string& name);
		bool updateFavHubGroup(const string& name, const string& newName, bool priv);

		// Favorite Directories

		struct FavoriteDirectory
		{
			StringList ext;
			string dir;
			string name;
		};
		typedef vector<FavoriteDirectory> FavDirList;
		
		bool addFavoriteDir(const string& directory, const string& name, const string& ext);
		bool removeFavoriteDir(const string& name);
		bool updateFavoriteDir(const string& name, const string& newName, const string& directory, const string& ext);
		string getDownloadDirectory(const string& ext, const UserPtr& user) const;

		class LockInstanceDirs
		{
			public:
				LockInstanceDirs() : fm(FavoriteManager::getInstance())
				{
					fm->csDirs->acquireShared();
				}
				~LockInstanceDirs()
				{
					fm->csDirs->releaseShared();
				}
				const FavDirList& getFavoriteDirs() const
				{
					return fm->favoriteDirs;
				}

			private:
				FavoriteManager* const fm;
		};
		
		// Recent Hubs

		const RecentHubEntry::List& getRecentHubs() const { return recentHubs; }
		RecentHubEntry* getRecentHubEntry(const string& server);
		RecentHubEntry* addRecent(const RecentHubEntry& entry);
		void removeRecent(const RecentHubEntry* entry);
		void updateRecent(const RecentHubEntry* entry);
		void clearRecents();

		// Preview Apps

		PreviewApplication* addPreviewApp(const string& name, const string& application, const string& arguments, const string& extension);
		void removePreviewApp(const size_t index);
		const PreviewApplication* getPreviewApp(const size_t index) const;
		PreviewApplication* getPreviewApp(const size_t index);
		const PreviewApplication::List& getPreviewApps() const { return previewApplications; }

		// User Commands

		UserCommand addUserCommand(int type, int ctx, Flags::MaskType flags, const string& name, const string& command, const string& to, const string& hub);
		bool getUserCommand(int id, UserCommand& uc) const;
		int findUserCommand(const string& name, const string& hub) const;
		bool moveUserCommand(int id, int delta);
		void updateUserCommand(const UserCommand& uc);
		void removeUserCommand(int id);

		list<UserCommand> getUserCommands() const
		{
			READ_LOCK(*csUserCommand);
			return userCommands;
		}
		void getUserCommands(vector<UserCommand>& result, int ctx, const StringList& hub) const;
		
		void load();
		void saveFavorites();
		void saveRecents();
		
	private:
		FavoriteHubEntryList favoriteHubs;
		FavHubGroups favHubGroups;
		mutable std::unique_ptr<RWLock> csHubs;
		int favHubId;

		list<UserCommand> userCommands;
		mutable std::unique_ptr<RWLock> csUserCommand;
		int userCommandId;

		RecentHubEntry::List recentHubs;
		PreviewApplication::List previewApplications;
		
		FavoriteMap favoriteUsers;
		mutable std::unique_ptr<RWLock> csUsers;

		FavDirList favoriteDirs;
		std::unique_ptr<RWLock> csDirs;
		
		int dontSave; // Used during loading to prevent saving.
		bool recentsDirty;
		bool favsDirty;
		uint64_t recentsLastSave;
		uint64_t favsLastSave;

	public:
		void shutdown();
		
		void loadRecents(SimpleXML& xml);
		void loadPreview(SimpleXML& xml);
		void savePreview(SimpleXML& xml) const;
		
	private:
		friend class Singleton<FavoriteManager>;
		
		FavoriteManager();
		~FavoriteManager();

		string getFavoriteDir(const string& ext) const;
		
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
