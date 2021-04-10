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

#ifndef DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H
#define DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H

#include <vector>

template<class T, class T2 = std::vector<T>>
class StringTokenizer
{
	private:
		T2 tokens;
		
		template<class T3>
		void slice(const T& str, const T3& tok, const size_t tokLen, size_t reserve) noexcept
		{
			if (reserve) tokens.reserve(reserve);
			typename T::size_type pos = 0;
			while (true)
			{
				const typename T::size_type next = str.find(tok, pos);
				if (next != T::npos)
				{
					tokens.push_back(str.substr(pos, next - pos));
					pos = next + tokLen;
				}
				else
				{
					if (pos < str.size()) tokens.push_back(str.substr(pos));
					break;
				}
			}
		}

	public:
		StringTokenizer() {}
		
		StringTokenizer(const T& str, typename T::value_type tok, size_t reserve = 0) noexcept
		{
			slice(str, tok, 1, reserve);
		}

		StringTokenizer(const T& str, const typename T::value_type* tok, size_t reserve = 0) noexcept
		{
			const T tmp(tok);
			slice(str, tmp, tmp.size(), reserve);
		}

		const T2& getTokens() const
		{
			return tokens;
		}

		T2& getWritableTokens()
		{
			return tokens;
		}
};

#endif // !DCPLUSPLUS_CLIENT_STRING_TOKENIZER_H
