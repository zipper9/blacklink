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


#ifndef DCPLUSPLUS_DCPP_SEGMENT_H_
#define DCPLUSPLUS_DCPP_SEGMENT_H_

#include "BaseUtil.h"

class Segment
{
	public:
		Segment() : start(0), size(-1), overlapped(false) { }
		Segment(int64_t start, int64_t size, bool overlapped = false) : start(start), size(size), overlapped(overlapped)
		{
		}
		
		int64_t getStart() const { return start; }
		int64_t getSize() const { return size; }
		int64_t getEnd() const { return start + size; }
		
		void setSize(int64_t size) { this->size = size; }
		
		bool overlaps(const Segment& rhs) const
		{
			return getStart() < rhs.getEnd() && rhs.getStart() < getEnd();
		}

		bool operator<(const Segment& rhs) const
		{
			return (getStart() < rhs.getStart()) || (getStart() == rhs.getStart() && getSize() < rhs.getSize());
		}

		bool operator==(const Segment& rhs) const
		{
			return start == rhs.start && size == rhs.size;
		}

		bool contains(const Segment& rhs) const
		{
			return getStart() <= rhs.getStart() && getEnd() == rhs.getEnd(); // ???
		}

	private:
		int64_t start;
		int64_t size;
		
		GETSET(bool, overlapped, Overlapped);
};

#endif /*SEGMENT_H_*/
