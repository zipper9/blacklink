#ifndef HASH_UTIL_H_
#define HASH_UTIL_H_

#include "MerkleTree.h"
#include <atomic>

namespace Util
{
	bool getTTH(const string& filename, bool isAbsPath, size_t bufSize, std::atomic_bool& stopFlag, TigerTree& tree, unsigned maxLevels = 0);
}

#endif // HASH_UTIL_H_
