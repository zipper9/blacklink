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

#ifndef CONNECTION_MANAGER_LISTENER_H
#define CONNECTION_MANAGER_LISTENER_H

class ConnectionManagerListener
{
	public:
		virtual ~ConnectionManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Added;
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
		typedef X<1> Connected;
#endif
		typedef X<2> Removed;
		typedef X<3> FailedDownload;
		typedef X<4> FailedUpload;
		typedef X<5> ConnectionStatusChanged;
		typedef X<6> UserUpdated;
#ifdef FLYLINKDC_USE_FORCE_CONNECTION
		typedef X<7> Forced;
#endif
		typedef X<8> ListenerStarted;
		typedef X<9> ListenerFailed;
		typedef X<10> RemoveToken;
		
		virtual void on(Added, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept { }
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
		virtual void on(Connected, const ConnectionQueueItemPtr&) noexcept { }
#endif
		virtual void on(RemoveToken, const string& token) noexcept { }
		virtual void on(Removed, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept { }
		virtual void on(FailedDownload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept { }
		virtual void on(FailedUpload, const HintedUser& hintedUser, const string& reason, const string& token) noexcept { }
		virtual void on(ConnectionStatusChanged, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept { }
		virtual void on(UserUpdated, const HintedUser& hintedUser, bool isDownload, const string& token) noexcept { }
#ifdef FLYLINKDC_USE_FORCE_CONNECTION
		virtual void on(Forced, const ConnectionQueueItemPtr&) noexcept { }
#endif
		virtual void on(ListenerStarted) noexcept { }
		virtual void on(ListenerFailed, const char* type, int errorCode) noexcept { }
};

#endif // !defined(CONNECTION_MANAGER_LISTENER_H)
