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

#ifndef DCPLUSPLUS_DCPP_BUFFEREDSOCKETLISTENER_H_
#define DCPLUSPLUS_DCPP_BUFFEREDSOCKETLISTENER_H_

#include "noexcept.h"
#include "typedefs.h"

class BufferedSocketListener
{
	public:
		virtual ~BufferedSocketListener() {}
		virtual void onConnecting() noexcept {}
		virtual void onConnected() noexcept {}
		virtual void onDataLine(const char*, size_t) noexcept {}
		virtual void onData(const uint8_t*, size_t) {}
		virtual void onBytesLoaded(size_t bytes) {}
		virtual void onBytesSent(size_t bytes) {}
		virtual void onModeChange() noexcept {}
		virtual void onTransmitDone() noexcept {}
		virtual void onFailed(const string&) noexcept {}
		virtual void onUpdated() noexcept {}
		virtual void onUpgradedToSSL() noexcept {}
};

#endif /*BUFFEREDSOCKETLISTENER_H_*/
