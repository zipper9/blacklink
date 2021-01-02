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

#ifndef DCPLUSPLUS_DCPP_CID_H
#define DCPLUSPLUS_DCPP_CID_H

#include "Encoder.h"
#include "debug.h"
#include <boost/functional/hash.hpp>

class CID
{
	public:
		enum { SIZE = 192 / 8 };
		
		CID()
		{
			init();
		}
		explicit CID(const uint8_t* data)
		{
			memcpy(cid.b, data, SIZE);
		}
		explicit CID(const string& base32)
		{
			fromBase32(base32);
		}
		void init()
		{
			memset(cid.b, 0, SIZE);
		}
		
		bool operator==(const CID& rhs) const
		{
			return memcmp(cid.b, rhs.cid.b, SIZE) == 0;
		}
		bool operator!=(const CID& rhs) const
		{
			return memcmp(cid.b, rhs.cid.b, SIZE) != 0;
		}
		bool operator<(const CID& rhs) const
		{
			return memcmp(cid.b, rhs.cid.b, SIZE) < 0;
		}
		string toBase32() const
		{
			return Encoder::toBase32(cid.b, SIZE);
		}
		string& toBase32(string& tmp) const
		{
			return Encoder::toBase32(cid.b, SIZE, tmp);
		}

		void fromBase32(const string& base32)
		{
			if (base32.length() == 39)
				Encoder::fromBase32(base32.c_str(), cid.b, SIZE);
			else
			{
				string tmp = base32;
				tmp.resize(39);
				Encoder::fromBase32(tmp.c_str(), cid.b, SIZE);
			}
		}
		
		size_t toHash() const
		{
			return cid.w[0];
		}
		
		const uint8_t* data() const { return cid.b; }
		uint8_t* writableData() { return cid.b; }
		
		const size_t* dataW() const { return cid.w; }
		size_t* writableDataW() { return cid.w; }

		bool isZero() const
		{
			for (int i = 0; i < SIZE/sizeof(size_t); i++)
				if (cid.w[i]) return false;
			return true;
		}
		
		static CID generate();
		static void generate(uint8_t *cid);

		void regenerate()
		{
			generate(cid.b);
		}

	private:
		union
		{
			uint8_t b[SIZE];
			size_t  w[SIZE/sizeof(size_t)];
		} cid;
};

namespace std
{
	template<> struct hash<CID>
	{
		size_t operator()(const CID& rhs) const
		{
			return rhs.toHash();
		}
	};
}

namespace boost
{
	template<> struct hash<CID>
	{
		size_t operator()(const CID& rhs) const
		{
			return rhs.toHash();
		}
	};
}

#endif // !defined(CID_H)
