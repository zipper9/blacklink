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

#ifndef MAPPER_H
#define MAPPER_H

#include "IpAddress.h"
#include <utility>

/** abstract class to represent an implementation usable by MappingManager. */
class Mapper
{
	public:
		Mapper(const string &localIp, int af);
		virtual ~Mapper() {}

		Mapper(const Mapper&) = delete;
		Mapper& operator= (const Mapper&) = delete;

		enum Protocol
		{
			PROTOCOL_TCP,
			PROTOCOL_UDP,
			PROTOCOL_LAST
		};
		static const char *protocols[PROTOCOL_LAST];

		/** begin the initialization phase.
		@return true if the initialization passed; false otherwise. */
		virtual bool init() = 0;
		/** end the initialization phase. called regardless of the return value of init(). */
		virtual void uninit() = 0;

		virtual bool addMapping(int port, Protocol protocol, const string &description) = 0;
		virtual bool removeMapping(int port, Protocol protocol) = 0;

		/** interval after which ports should be re-mapped, in seconds. 0 = no renewal. */
		virtual int renewal() const = 0;

		virtual string getDeviceName() const = 0;
		virtual IpAddress getExternalIP() = 0;
		virtual int getExternalPort() const = 0;

		/* by contract, implementations of this class should define a public user-friendly name in:
		static const string name; */

		/** user-friendly name for this implementation. */
		virtual const string &getName() const = 0;
		virtual bool supportsProtocol(int af) const = 0;

	protected:
		string localIp;
		const int af;
};

#endif // MAPPER_H
