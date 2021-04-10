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

#ifndef DCPLUSPLUS_DCPP_MERKLE_TREE_H
#define DCPLUSPLUS_DCPP_MERKLE_TREE_H

#include "typedefs.h"
#include "TigerHash.h"
#include "HashValue.h"

/**
 * A class that represents a Merkle Tree hash. Storing
 * only the leaves of the tree, it is rather memory efficient,
 * but can still take a significant amount of memory during / after
 * hash generation.
 * The root hash produced can be used like any
 * other hash to verify the integrity of a whole file, while
 * the leaves provide checking of smaller parts of the file.
 */

const uint64_t MIN_BLOCK_SIZE = 65536;

template < class Hasher, size_t baseBlockSize = 1024 >
class MerkleTree
{
	public:
		static const size_t BITS = Hasher::BITS;
		static const size_t BYTES = Hasher::BYTES;
		static const size_t BASE_BLOCK_SIZE = baseBlockSize;
		
		typedef HashValue<Hasher> MerkleValue;
		typedef std::vector<MerkleValue> MerkleList;
		
		MerkleTree() : fileSize(0), blockSize(BASE_BLOCK_SIZE) { }
		explicit MerkleTree(int64_t aBlockSize) : fileSize(0), blockSize(aBlockSize) { }
		
		/** Initialise a single root tree */
		MerkleTree(int64_t aFileSize, int64_t aBlockSize, const MerkleValue& aRoot) : root(aRoot), fileSize(aFileSize), blockSize(aBlockSize)
		{
			leaves.push_back(root);
		}
		
		/**
		 * Loads a set of leaf hashes, calculating the root
		 */
		bool load(int64_t fileSize, const uint8_t* data, size_t dataSize)
		{
			if (dataSize % BYTES) return false;
			size_t hashCount = dataSize / BYTES;
			leaves.resize(hashCount);
			for (size_t i = 0; i < hashCount; i++, data += BYTES)
				memcpy(leaves[i].data, data, BYTES);
			int64_t bl = 1024;
			while (static_cast<int64_t>(bl * hashCount) < fileSize)
				bl <<= 1;
			this->fileSize = fileSize;
			blockSize = bl;
			calcRoot();
			return true;
		}
		
		static uint64_t calcBlockSize(const uint64_t aFileSize, const unsigned int maxLevels)
		{
			uint64_t tmp = BASE_BLOCK_SIZE;
			const uint64_t maxHashes = ((uint64_t)1) << (maxLevels - 1);
			while ((maxHashes * tmp) < aFileSize)
			{
				tmp *= 2;
			}
			return tmp;
		}

		static size_t calcBlocks(int64_t aFileSize, int64_t aBlockSize)
		{
			dcassert(aBlockSize > 0);
			dcassert(aFileSize >= aBlockSize);
			return max((size_t)((aFileSize + aBlockSize - 1) / aBlockSize), (size_t)1);
		}

		static uint16_t calcBlocks(int64_t aFileSize)
		{
			return (uint16_t)calcBlocks(aFileSize, calcBlockSize(aFileSize, 10));
		}

		static uint64_t getMaxBlockSize(int64_t fileSize)
		{
			return max(calcBlockSize(fileSize, 10), MIN_BLOCK_SIZE);
		}
		
		uint64_t calcFullLeafCnt(uint64_t ttrBlockSize)
		{
			return ttrBlockSize / BASE_BLOCK_SIZE;
		}
		
		/**
		 * Update the merkle tree.
		 * @param len Length of data, must be a multiple of BASE_BLOCK_SIZE, unless it's
		 *            the last block.
		 */
		void update(const void* data, size_t len)
		{
			uint8_t* buf = (uint8_t*)data;
			uint8_t zero = 0;
			size_t i = 0;
			
			// Skip empty data sets if we already added at least one of them...
			if (len == 0 && !(leaves.empty() && blocks.empty()))
				return;
				
			do
			{
				size_t n = min(baseBlockSize, len - i);
				Hasher h;
				h.update(&zero, 1);
				h.update(buf + i, n);
				if ((int64_t) baseBlockSize < blockSize)
				{
					blocks.push_back(MerkleBlock(MerkleValue(h.finalize()), baseBlockSize));
					reduceBlocks();
				}
				else
				{
					leaves.push_back(MerkleValue(h.finalize()));
				}
				i += n;
			}
			while (i < len);
			fileSize += len;
		}
		
		uint8_t* finalize()
		{
			// No updates yet, make sure we have at least one leaf for 0-length files...
			if (leaves.empty() && blocks.empty())
			{
				update(nullptr, 0);
			}
			if (blocks.size() > 1)
			{
				auto size = blocks.size();
				while (true)
				{
					MerkleBlock& a = blocks[size - 2];
					MerkleBlock& b = blocks[size - 1];
					a.first = combine(a.first, b.first);
					if (size == 2)
					{
						blocks.resize(1);
						break;
					}
					--size;
				}
			}
			dcassert(blocks.empty() || blocks.size() == 1);
			if (!blocks.empty())
			{
				leaves.push_back(blocks[0].first);
				blocks.clear();
			}
			calcRoot();
			return root.data;
		}
		
		const MerkleValue& getRoot() const
		{
			return root;
		}

		// FIXME: remove non-constant getLeaves
		MerkleList& getLeaves()
		{
			return leaves;
		}

		const MerkleList& getLeaves() const
		{
			return leaves;
		}
		
		int64_t getBlockSize() const
		{
			return blockSize;
		}

		void setBlockSize(int64_t aSize)
		{
			blockSize = aSize;
		}
		
		int64_t getFileSize() const
		{
			return fileSize;
		}

		void setFileSize(int64_t aSize)
		{
			fileSize = aSize;
		}
		
		bool verifyRoot(const uint8_t* aRoot)
		{
			return memcmp(aRoot, getRoot().data(), BYTES) == 0;
		}
		
		void calcRoot()
		{
			root = getHash(0, fileSize);
		}
		
		void getLeafData(ByteVector& buf)
		{
			size_t size = leaves.size();
			if (size)
			{
				buf.resize(size * BYTES);
				auto p = &buf[0];
				for (size_t i = 0; i < size; ++i, p += BYTES)
					memcpy(p, leaves[i].data, BYTES);
				return;
			}
			buf.clear();
		}
		
		void clear()
		{
			blocks.clear();
			leaves.clear();
			fileSize = 0;
		}

	protected:
		typedef std::pair<MerkleValue, int64_t> MerkleBlock;
		typedef std::vector<MerkleBlock> MBList;
		
		MBList blocks;
		
		MerkleList leaves;
		
		/** Total size of hashed data */
		int64_t fileSize;
		/** Final block size */
		int64_t blockSize;
		
	private:
		MerkleValue root;
		
		MerkleValue getHash(int64_t start, int64_t length)
		{
			dcassert(start + length <= fileSize);
			dcassert((start % blockSize) == 0);
			if (length <= blockSize)
			{
				start /= blockSize;
				if (start < static_cast<int64_t>(leaves.size()))
					return leaves[static_cast<size_t>(start)];
				dcassert(0);
				return MerkleValue();
			}
			else
			{
				int64_t l = blockSize;
				while (true)
				{
					const int64_t l2 = l * 2;
					if (l2 < length)
						l = l2;
					else
						break;
				}
				return combine(getHash(start, l), getHash(start + l, length - l));
			}
		}
		
		MerkleValue combine(const MerkleValue& a, const MerkleValue& b)
		{
			uint8_t one = 1;
			Hasher h;
			h.update(&one, 1);
			h.update(a.data, MerkleValue::BYTES);
			h.update(b.data, MerkleValue::BYTES);
			return MerkleValue(h.finalize());
		}
		
	protected:
		void reduceBlocks()
		{
			if (blocks.size() > 1)
			{
				auto size = blocks.size();
				while (true)
				{
					MerkleBlock& a = blocks[size - 2];
					MerkleBlock& b = blocks[size - 1];
					if (a.second == b.second)
					{
						if (a.second * 2 == blockSize)
						{
							leaves.push_back(combine(a.first, b.first));
							size -= 2;
						}
						else
						{
							a.second *= 2;
							a.first = combine(a.first, b.first);
							--size;
						}
					}
					else
					{
						blocks.resize(size);
						break;
					}
					if (size < 2)
					{
						blocks.resize(size);
						break;
					}
				}
			}
		}
};

typedef MerkleTree<TigerHash> TigerTree;
typedef TigerTree::MerkleValue TTHValue;

struct TTFilter
{
		TTFilter() : tt(1024 * 1024 * 1024) { }
		void operator()(const void* data, size_t len)
		{
			tt.update(data, len);
		}
		TigerTree& getTree()
		{
			return tt;
		}
	private:
		TigerTree tt;
};

#endif // DCPLUSPLUS_DCPP_MERKLE_TREE_H
