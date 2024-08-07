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

#ifndef DCPLUSPLUS_DCPP_MERKLE_CHECK_OUTPUT_STREAM_H
#define DCPLUSPLUS_DCPP_MERKLE_CHECK_OUTPUT_STREAM_H

#include "Streams.h"
#include "MerkleTree.h"

template<class TreeType, bool managed>
class MerkleCheckOutputStream : public OutputStream
{
	public:
		MerkleCheckOutputStream(const TreeType& tree, OutputStream* os, int64_t start) : s(os), real(tree), cur(tree.getBlockSize()), verified(0), bufPos(0)
		{
			//dcdebug("[==========================] MerkleCheckOutputStream() start = %d s = %d this = %d\r\n\r\n", start, s, this);
			// Only start at block boundaries
			const auto blocksize = tree.getBlockSize();
			dcassert(start % blocksize == 0);
			cur.setFileSize(start);
			
			const size_t nBlocks = static_cast<size_t>(start / blocksize);
			if (nBlocks > tree.getLeaves().size())
			{
				dcdebug("Invalid tree / parameters");
				return;
			}
			cur.getLeaves().insert(cur.getLeaves().begin(), tree.getLeaves().begin(), tree.getLeaves().begin() + nBlocks);
		}
		
		~MerkleCheckOutputStream()
		{
			//dcdebug("[==========================] ~MerkleCheckOutputStream() bufPos = %d s = %d this = %d\r\n\r\n", bufPos, s,  this);
			if (managed)
			{
				delete s;
			}
		}
		
		size_t flushBuffers(bool force) override
		{
			if (bufPos != 0)
				cur.update(buf, bufPos);
			bufPos = 0;
			
			cur.finalize();
			if (cur.getLeaves().size() == real.getLeaves().size())
			{
				if (cur.getRoot() != real.getRoot())
					throw FileException(STRING(TTH_INCONSISTENCY));
			}
			else
			{
				checkTrees();
			}
			return s->flushBuffers(force);
		}
		
		void commitBytes(const void* b, size_t len)
		{
			uint8_t* xb = (uint8_t*)b;
			size_t pos = 0;
			
			if (bufPos != 0)
			{
				size_t bytes = min(TreeType::BASE_BLOCK_SIZE - bufPos, len);
				memcpy(buf + bufPos, xb, bytes);
				pos = bytes;
				bufPos += bytes;
				
				if (bufPos == TreeType::BASE_BLOCK_SIZE)
				{
					cur.update(buf, TreeType::BASE_BLOCK_SIZE);
					bufPos = 0;
				}
			}
			
			if (pos < len)
			{
				dcassert(bufPos == 0);
				size_t left = len - pos;
				size_t part = left - (left % TreeType::BASE_BLOCK_SIZE);
				if (part)
				{
					cur.update(xb + pos, part);
					pos += part;
				}
				left = len - pos;
				memcpy(buf, xb + pos, left);
				bufPos = left;
			}
		}
		
		size_t write(const void* b, size_t len) override
		{
			commitBytes(b, len);
			checkTrees();
			return s->write(b, len);
		}
		
		int64_t verifiedBytes() const
		{
			return min(real.getFileSize(), (int64_t)(cur.getBlockSize() * cur.getLeaves().size()));
		}

	private:
		OutputStream* s;
		TreeType real;
		TreeType cur;
		size_t verified;
		
		uint8_t buf[TreeType::BASE_BLOCK_SIZE];
		size_t bufPos;
		
		void checkTrees()
		{
			while (cur.getLeaves().size() > verified)
			{
				if (cur.getLeaves().size() > real.getLeaves().size() ||
				    !(cur.getLeaves()[verified] == real.getLeaves()[verified]))
				{
					throw FileException(STRING(TTH_INCONSISTENCY));
				}
				verified++;
			}
		}
};

#endif // !defined(MERKLE_CHECK_OUTPUT_STREAM_H)
