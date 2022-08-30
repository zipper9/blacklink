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
			init();
		}

		~StringSearch() noexcept
		{
			::operator delete(delta1);
		}

		StringSearch(const StringSearch& rhs) noexcept :
			pattern(rhs.pattern), ignoreCase(rhs.ignoreCase), smallTable(rhs.smallTable)
		{
			if (rhs.delta1)
			{
				size_t size = rhs.getTableSize();
				delta1 = ::operator new(size);
				memcpy(delta1, rhs.delta1, size);
			}
			else
				delta1 = nullptr;
		}

		StringSearch(StringSearch&& rhs) noexcept :
			pattern(std::move(rhs.pattern)), ignoreCase(rhs.ignoreCase), smallTable(rhs.smallTable)
		{
			delta1 = rhs.delta1;
			rhs.delta1 = nullptr;
			rhs.pattern.clear();
		}

		StringSearch& operator=(const StringSearch& rhs) noexcept
		{
			::operator delete(delta1);
			if (rhs.delta1)
			{
				size_t size = rhs.getTableSize();
				delta1 = ::operator new(size);
				memcpy(delta1, rhs.delta1, size);
			}
			else
				delta1 = nullptr;
			pattern = rhs.pattern;
			ignoreCase = rhs.ignoreCase;
			smallTable = rhs.smallTable;
			return *this;
		}

		StringSearch& operator=(StringSearch&& rhs) noexcept
		{
			::operator delete(delta1);
			delta1 = rhs.delta1;
			pattern = std::move(rhs.pattern);
			ignoreCase = rhs.ignoreCase;
			smallTable = rhs.smallTable;
			rhs.delta1 = nullptr;
			rhs.pattern.clear();
			return *this;
		}

		bool operator==(const StringSearch& rhs) const noexcept
		{
			return pattern == rhs.pattern && ignoreCase == rhs.ignoreCase;
		}

		const string& getPattern() const noexcept
		{
			return pattern;
		}

		bool getIgnoreCase() const noexcept
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
			if (!delta1) return false;
			const string::size_type plen = pattern.length();
			const string::size_type tlen = text.length();
			if (tlen < plen) return false;
			const uint8_t* tx = (const uint8_t*) text.data();
			const uint8_t* px = (const uint8_t*) pattern.data();
			return smallTable ?
				doMatch<uint8_t>(tx, px, tlen, plen) :
				doMatch<uint16_t>(tx, px, tlen, plen);
		}

	private:
		void* delta1;
		string pattern;
		bool ignoreCase;
		bool smallTable;

		template<typename T>
		void initDelta1() noexcept
		{
			T x = (T) (pattern.length() + 1);
			delta1 = ::operator new(256 * sizeof(T));
			T* d = static_cast<T*>(delta1);
			for (unsigned i = 0; i < 256; ++i)
				d[i] = x;
			x--;
			const uint8_t* p = (const uint8_t*) pattern.data();
			for (unsigned i = 0; i < x; ++i)
				d[p[i]] = (T) (x - i);
		}

		template<typename T>
		bool doMatch(const uint8_t* tx, const uint8_t* px, size_t tlen, size_t plen) const noexcept
		{
			const T* d = static_cast<const T*>(delta1);
			const uint8_t* end = tx + tlen - plen + 1;
			while (tx < end)
			{
				size_t i = 0;
				while (i < plen && px[i] == tx[i]) ++i;
				if (i == plen) return true;
				tx += d[tx[plen]];
			}
			return false;
		}

		void init() noexcept
		{
			if (pattern.length() < 255)
			{
				smallTable = true;
				initDelta1<uint8_t>();
			}
			else
			{
				smallTable = false;
				initDelta1<uint16_t>();
			}
		}

		size_t getTableSize() const noexcept
		{
			return smallTable ? 256 : 512;
		}
};

#endif // DCPLUSPLUS_DCPP_STRING_SEARCH_H
