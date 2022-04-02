/*
 * Copyright (C) 2001-2019 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_HTTP_CONNECTION_LISTENER_H
#define DCPLUSPLUS_DCPP_HTTP_CONNECTION_LISTENER_H

class HttpConnection;

class HttpConnectionListener
{
public:
	virtual ~HttpConnectionListener() { }
	template<int I>	struct X { enum { TYPE = I }; };

	typedef X<0> Data;
	typedef X<1> Failed;
	typedef X<2> Completed;
	typedef X<3> Disconnected;

	virtual void on(Data, HttpConnection* conn, const uint8_t* data, size_t size) noexcept = 0;
	virtual void on(Failed, HttpConnection* conn, const string& error) noexcept = 0;
	virtual void on(Completed, HttpConnection* conn, const string& requestUrl) noexcept = 0;
	virtual void on(Disconnected, HttpConnection* conn) noexcept { }
};

#endif // !defined(DCPLUSPLUS_DCPP_HTTP_CONNECTION_LISTENER_H)
