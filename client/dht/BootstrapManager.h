/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
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

#ifndef _BOOTSTRAPMANAGER_H
#define _BOOTSTRAPMANAGER_H

#include "Constants.h"
#include "KBucket.h"

#include "../CID.h"
#include "../HttpClientListener.h"
#include "../Singleton.h"

namespace dht
{

	class BootstrapManager : public Singleton<BootstrapManager>, private HttpClientListener
	{
	public:
		BootstrapManager();
		~BootstrapManager();

		void bootstrap();
		void process();
		void addBootstrapNode(const string& ip, uint16_t udpPort, const CID& targetCID, const UDPKey& udpKey);
		void cleanup();
		bool hasBootstrapNodes() const;

	private:
		mutable CriticalSection csNodes;
		CriticalSection csState;
		uint64_t downloadRequest;
		bool hasListener;

		struct BootstrapNode
		{
			string		ip;
			uint16_t	udpPort;
			CID			cid;
			UDPKey		udpKey;
		};

		/** List of bootstrap nodes */
		deque<BootstrapNode> bootstrapNodes;

		void on(Completed, uint64_t id, const Http::Response& resp, const Result& data) noexcept override;
		void on(Failed, uint64_t id, const string& error) noexcept override;
		void on(Redirected, uint64_t id, const string& redirUrl) noexcept override {}
	};

}

#endif	// _BOOTSTRAPMANAGER_H
