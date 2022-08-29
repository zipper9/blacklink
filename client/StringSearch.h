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

#ifndef DCPLUSPLUS_DCPP_STRING_SEARCH_H
#define DCPLUSPLUS_DCPP_STRING_SEARCH_H

#include "Text.h"

#include "noexcept.h"

/**
 * A class that implements a fast substring search algo suited for matching
 * one pattern against many strings (currently Quick Search, a variant of
 * Boyer-Moore. Code based on "A very fast substring search algorithm" by
 * D. Sunday).
 * @todo Perhaps find an algo suitable for matching multiple substrings.
 */
class StringSearch
{
	public:
		typedef vector<StringSearch> List;
		
		explicit StringSearch(const string& pattern, bool ignoreCase = true) noexcept :
			pattern(pattern), ignoreCase(ignoreCase)
		{
			if (ignoreCase) Text::makeLower(this->pattern);
			initDelta1();
		}

		StringSearch(const StringSearch& rhs) noexcept :
			pattern(rhs.pattern), ignoreCase(rhs.ignoreCase)
		{
			memcpy(delta1, rhs.delta1, sizeof(delta1));
		}
		const StringSearch& operator=(const StringSearch& rhs)
		{
			memcpy(delta1, rhs.delta1, sizeof(delta1));
			pattern = rhs.pattern;
			ignoreCase = rhs.ignoreCase;
			return *this;
		}

		bool operator==(const StringSearch& rhs) const
		{
			return pattern == rhs.pattern && ignoreCase == rhs.ignoreCase;
		}
		
		const string& getPattern() const
		{
			return pattern;
		}

		bool getIgnoreCase() const
		{
			return ignoreCase;
		}

		bool match(const string& text) const noexcept
		{
			if (ignoreCase)
				return matchKeepCase(Text::toLower(text));
			return matchKeepCase(text);
		}

		bool matchKeepCase(const string& text) const noexcept
		{
			const string::size_type plen = pattern.length();
			const string::size_type tlen = text.length();
			if (tlen < plen) return false;
			const uint8_t* tx = (const uint8_t*) text.data();
			const uint8_t* px = (const uint8_t*) pattern.data();
			const uint8_t *end = tx + tlen - plen + 1;
			while (tx < end)
			{
				size_t i = 0;
				while (i < plen && px[i] == tx[i]) ++i;
				if (i == plen) return true;
				tx += delta1[tx[plen]];
			}
			
			return false;
		}

	private:
		uint16_t delta1[256];
		string pattern;
		bool ignoreCase;

		void initDelta1()
		{
			uint16_t x = (uint16_t)(pattern.length() + 1);
			for (uint16_t i = 0; i < 256; ++i)
				delta1[i] = x;
			x--;
			const uint8_t* p = (const uint8_t*) pattern.data();
			for (uint16_t i = 0; i < x; ++i)
				delta1[p[i]] = (uint16_t)(x - i);
		}
};

#endif // DCPLUSPLUS_DCPP_STRING_SEARCH_H
