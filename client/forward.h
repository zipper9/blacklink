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

#ifndef DCPLUSPLUS_DCPP_FORWARD_H_
#define DCPLUSPLUS_DCPP_FORWARD_H_

/** @file
 * This file contains forward declarations for the various DC++ classes
 */

#include <memory>
#include <vector>
#include <list>
#include <deque>
#include <unordered_set>

class FavoriteHubEntry;
typedef std::vector<FavoriteHubEntry*> FavoriteHubEntryList;

class FinishedItem;
typedef std::shared_ptr<FinishedItem> FinishedItemPtr;
typedef std::deque<FinishedItemPtr> FinishedItemList;

class HubEntry;

class ClientBase;
typedef std::shared_ptr<ClientBase> ClientBasePtr;

class OnlineUser;
typedef std::shared_ptr<OnlineUser> OnlineUserPtr;
typedef std::vector<OnlineUserPtr> OnlineUserList;

class QueueItem;
typedef std::shared_ptr<QueueItem> QueueItemPtr;
typedef std::list<QueueItemPtr> QueueItemList;

class UploadQueueFile;
typedef std::shared_ptr<UploadQueueFile> UploadQueueFilePtr;

class User;
typedef std::shared_ptr<User> UserPtr;
typedef std::vector<UserPtr> UserList;
typedef std::unordered_set<UserPtr> UserSet;

class UserConnection;
typedef std::shared_ptr<UserConnection> UserConnectionPtr;

#endif /*DCPLUSPLUS_CLIENT_FORWARD_H_*/
