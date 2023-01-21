#include "stdinc.h"
#include "HashDatabaseLMDB.h"
#include "StrUtil.h"
#include "Util.h"
#include "File.h"
#include "TimerManager.h"
#include "LogManager.h"
#include "unaligned.h"

static const size_t TTH_SIZE = 24;
static const size_t BASE_ITEM_SIZE = 10;

enum
{
	ITEM_TIGER_TREE   = 1,
	ITEM_LOCAL_PATH   = 2,
	ITEM_UPLOAD_COUNT = 3
};

#if defined(_WIN64) || defined(__LP64__)
#define PLATFORM_TAG "x64"
#else
#define PLATFORM_TAG "x32"
#endif

string HashDatabaseLMDB::getDBPath() noexcept
{
	string path = Util::getConfigPath();
	path += "hash-db." PLATFORM_TAG;
	return path;
}

bool HashDatabaseLMDB::open() noexcept
{
	if (env) return false;

	int error = mdb_env_create(&env);
	if (!checkError(error)) return false;

	string path = getDBPath();
	path += PATH_SEPARATOR;
	File::ensureDirectory(path);
	error = mdb_env_open(env, path.c_str(), 0, 0664);
	if (!checkError(error))
	{
		mdb_env_close(env);
		env = nullptr;
		return false;
	}

	static const mdb_size_t minMapSize = 1024ull*1024ull*1024ull;
	MDB_envinfo info;
	error = mdb_env_info(env, &info);
	if (error || info.me_mapsize < minMapSize)
	{
		error = mdb_env_set_mapsize(env, minMapSize);
		if (!checkError(error))
		{
			mdb_env_close(env);
			env = nullptr;
			return false;
		}
	}

	defThreadId = BaseThread::getCurrentThreadId();
	return true;
}

void HashDatabaseLMDB::close() noexcept
{
	{
		LOCK(cs);
		defConn.reset();
		conn.clear();
	}
	if (env)
	{
		mdb_env_close(env);
		env = nullptr;
	}
}

bool HashDatabaseLMDB::checkError(int error) noexcept
{
	if (!error) return true;
	string errorText = "LMDB error: " + Util::toString(error);
	LogManager::message(errorText);
	return false;
}

class ItemParser
{
	public:
		ItemParser(const uint8_t *data, size_t dataSize) : data(data), dataSize(dataSize), ptr(0)
		{
		}

		bool getItem(int &itemType, const void* &itemData, size_t &itemSize, size_t &headerSize) noexcept;

	private:
		const uint8_t* const data;
		const size_t dataSize;
		size_t ptr;
};

bool ItemParser::getItem(int &itemType, const void* &itemData, size_t &itemSize, size_t &headerSize) noexcept
{
	if (ptr >= dataSize) return false;
	uint8_t type = data[ptr];
	if (type & 0x80)
	{
		type &= 0x7F;
		headerSize = 3;
		if (dataSize - ptr < 3) return false;
		itemSize = loadUnaligned16(data + ptr + 1);
	} else
	{
		headerSize = 2;
		if (dataSize - ptr < 2) return false;
		itemSize = data[ptr + 1];
	}
	if (dataSize - ptr < headerSize + itemSize) return false;
	ptr += headerSize;
	itemData = data + ptr;
	ptr += itemSize;
	itemType = type;
	return true;
}

static size_t putItem(uint8_t *ptr, int type, const void *data, size_t dataSize) noexcept
{
	size_t result = 0;
	*ptr = type;
	if (dataSize > 255)
	{
		*ptr |= 0x80;
		storeUnaligned16(ptr + 1, dataSize);
		result = 3;
	} else
	{
		ptr[1] = static_cast<uint8_t>(dataSize);
		result = 2;
	}
	if (data)
	{
		memcpy(ptr + result, data, dataSize);
		result += dataSize;
	}
	return result;
}

HashDatabaseConnection::~HashDatabaseConnection() noexcept
{
	mdb_txn_abort(txnRead);
}

bool HashDatabaseConnection::createReadTxn(MDB_dbi &dbi) noexcept
{
	int error;
	if (!txnRead)
		error = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txnRead);
	else
		error = mdb_txn_renew(txnRead);
	if (!HashDatabaseLMDB::checkError(error)) return false;
	error = mdb_dbi_open(txnRead, nullptr, 0, &dbi);
	return HashDatabaseLMDB::checkError(error);
}

bool HashDatabaseConnection::createWriteTxn(MDB_dbi &dbi, MDB_txn* &txnWrite) noexcept
{
	int error = mdb_txn_begin(env, nullptr, 0, &txnWrite);
	if (!HashDatabaseLMDB::checkError(error)) return false;
	error = mdb_dbi_open(txnWrite, nullptr, 0, &dbi);
	return HashDatabaseLMDB::checkError(error);
}

bool HashDatabaseConnection::writeData(MDB_txn *txnWrite, MDB_dbi dbi, MDB_val &key, MDB_val &val) noexcept
{
	int error = mdb_put(txnWrite, dbi, &key, &val, 0);
	if (error == MDB_MAP_FULL)
	{
		if (!resizeMap())
		{
			mdb_txn_abort(txnWrite);
			return false;
		}
		error = mdb_put(txnWrite, dbi, &key, &val, 0);
	}
	if (!HashDatabaseLMDB::checkError(error))
	{
		mdb_txn_abort(txnWrite);
		return false;
	}
	error = mdb_txn_commit(txnWrite);
	bool result = HashDatabaseLMDB::checkError(error);
	mdb_dbi_close(env, dbi);
	return result;
}

bool HashDatabaseConnection::resizeMap() noexcept
{
	MDB_envinfo info;
	int error = mdb_env_info(env, &info);
	if (!HashDatabaseLMDB::checkError(error)) return false;

	size_t newMapSize = info.me_mapsize << 1;
	error = mdb_env_set_mapsize(env, newMapSize);
	return HashDatabaseLMDB::checkError(error);
}

bool HashDatabaseConnection::getFileInfo(const void *tth, unsigned &flags, uint64_t *fileSize, string *path, size_t *treeSize, uint32_t *uploadCount) noexcept
{
	flags = 0;
	if (path) path->clear();
	if (fileSize) *fileSize = 0;
	if (treeSize) *treeSize = 0;
	if (uploadCount) *uploadCount = 0;

	MDB_dbi dbi;
	if (!createReadTxn(dbi)) return false;

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (error)
	{
		mdb_txn_reset(txnRead);
		return false;
	}

	if (val.mv_size < BASE_ITEM_SIZE)
	{
		mdb_txn_reset(txnRead);
		return false;
	}

	const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
	flags = loadUnaligned16(ptr);
	if (fileSize) *fileSize = loadUnaligned64(ptr + 2);

	ItemParser parser(ptr + BASE_ITEM_SIZE, val.mv_size - BASE_ITEM_SIZE);
	int itemType;
	size_t itemSize, headerSize;
	const void *itemData;
	while (parser.getItem(itemType, itemData, itemSize, headerSize))
	{
		if (itemType == ITEM_TIGER_TREE)
		{
			if (treeSize)
				*treeSize = itemSize;
		}
		else if (itemType == ITEM_LOCAL_PATH)
		{
			if (path)
				path->assign(static_cast<const char*>(itemData), itemSize);
		}
		else if (itemType == ITEM_UPLOAD_COUNT)
		{
			if (uploadCount)
			{
				if (itemSize == 2)
					*uploadCount = loadUnaligned16(itemData);
				else if (itemSize == 4)
					*uploadCount = loadUnaligned32(itemData);
			}
		}
	}

	mdb_txn_reset(txnRead);
	return true;
}

bool HashDatabaseConnection::getTigerTree(const void *tth, TigerTree &tree) noexcept
{
	MDB_dbi dbi;
	if (!createReadTxn(dbi)) return false;

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (error)
	{
		mdb_txn_reset(txnRead);
		return false;
	}

	if (val.mv_size < BASE_ITEM_SIZE)
	{
		mdb_txn_reset(txnRead);
		return false;
	}

	const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
	uint64_t fileSize = loadUnaligned64(ptr + 2);
	ItemParser parser(ptr + BASE_ITEM_SIZE, val.mv_size - BASE_ITEM_SIZE);
	int itemType;
	size_t itemSize, headerSize;
	const void *itemData;
	bool found = false;
	while (parser.getItem(itemType, itemData, itemSize, headerSize))
	{
		if (itemType == ITEM_TIGER_TREE)
		{
			if (itemSize < TTH_SIZE * 2 || itemSize % TTH_SIZE) // Invalid size
				return false;
			found = tree.load(fileSize, static_cast<const uint8_t*>(itemData), itemSize);
			break;
		}
	}

	mdb_txn_reset(txnRead);
	return found;
}

bool HashDatabaseConnection::putFileInfo(const void *tth, unsigned flags, uint64_t fileSize, const string *path, bool incUploadCount) noexcept
{
	unsigned setFlags = 0;
	bool updateFlags = false, updatePath = path != nullptr;
	size_t prevSize = 0;
	uint32_t uploadCount = 0;

	MDB_txn *txnWrite = nullptr;
	MDB_dbi dbi;
	if (!createWriteTxn(dbi, txnWrite)) return false;

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnWrite, dbi, &key, &val);
	if (!error && val.mv_size >= BASE_ITEM_SIZE)
	{
		const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
		setFlags = loadUnaligned16(ptr);
		prevSize = val.mv_size;
		if (updatePath || incUploadCount)
		{
			ItemParser parser(ptr + BASE_ITEM_SIZE, prevSize - BASE_ITEM_SIZE);
			int itemType;
			size_t itemSize, headerSize;
			const void *itemData;
			while (parser.getItem(itemType, itemData, itemSize, headerSize))
			{
				if (itemType == ITEM_LOCAL_PATH)
				{
					if (itemSize == path->length() && path->compare(0, itemSize, (const char *) itemData, itemSize) == 0)
						updatePath = false;
				}
				else if (itemType == ITEM_UPLOAD_COUNT)
				{
					if (itemSize == 2)
						uploadCount = loadUnaligned16(itemData);
					else if (itemSize == 4)
						uploadCount = loadUnaligned32(itemData);
				}
			}
		}
	}

	if ((setFlags | flags) != setFlags)
	{
		setFlags |= flags;
		updateFlags = true;
	}

	if (!updateFlags && !updatePath && !incUploadCount)
	{
		mdb_txn_abort(txnWrite);
		return true;
	}

	size_t bufSize = prevSize ? prevSize : BASE_ITEM_SIZE;
	if (updatePath)
	{
		bufSize += path->length() + 2;
		if (path->length() > 255) bufSize++;
	}
	if (incUploadCount)
	{
		++uploadCount;
		bufSize += uploadCount < 0x10000 ? 4 : 6;
	}

	buf.resize(bufSize);

	uint8_t *ptr = buf.data();
	storeUnaligned16(ptr, setFlags);
	ptr += 2;
	storeUnaligned64(ptr, fileSize);
	ptr += 8;

	if (prevSize)
	{
		ItemParser parser(static_cast<const uint8_t*>(val.mv_data) + BASE_ITEM_SIZE, prevSize - BASE_ITEM_SIZE);
		int itemType;
		size_t itemSize, headerSize;
		const void *itemData;
		while (parser.getItem(itemType, itemData, itemSize, headerSize))
		{
			if ((itemType == ITEM_LOCAL_PATH && updatePath) || (itemType == ITEM_UPLOAD_COUNT && incUploadCount))
				continue;
			memcpy(ptr, static_cast<const uint8_t*>(itemData) - headerSize, itemSize + headerSize);
			ptr += itemSize + headerSize;
		}
	}

	if (updatePath)
		ptr += putItem(ptr, ITEM_LOCAL_PATH, path->data(), path->length());
	if (incUploadCount)
	{
		if (uploadCount < 0x10000)
		{
			uint16_t count16 = (uint16_t) uploadCount;
			ptr += putItem(ptr, ITEM_UPLOAD_COUNT, &count16, sizeof(count16));
		}
		else
			ptr += putItem(ptr, ITEM_UPLOAD_COUNT, &uploadCount, sizeof(uploadCount));
	}

	size_t outSize = ptr - buf.data();
	dcassert(outSize <= bufSize);

	val.mv_data = buf.data();
	val.mv_size = outSize;
	return writeData(txnWrite, dbi, key, val);
}

bool HashDatabaseConnection::putTigerTree(const TigerTree &tree) noexcept
{
	if (tree.getLeaves().size() < 2) return false;

	MDB_txn *txnWrite = nullptr;
	MDB_dbi dbi;
	if (!createWriteTxn(dbi, txnWrite)) return false;

	const TigerTree::MerkleList &leaves = tree.getLeaves();
	unsigned setFlags = 0;
	bool updateTree = true;
	size_t prevSize = 0;
	size_t prevTreeSize = 0;
	size_t newTreeSize = leaves.size() * TTH_SIZE;

	MDB_val key, val;
	key.mv_data = (void *)(tree.getRoot().data);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnWrite, dbi, &key, &val);
	if (!error && val.mv_size >= BASE_ITEM_SIZE)
	{
		const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
		setFlags = loadUnaligned16(ptr);
		uint64_t fileSize = loadUnaligned64(ptr + 2);
		prevSize = val.mv_size;
		ItemParser parser(ptr + BASE_ITEM_SIZE, prevSize - BASE_ITEM_SIZE);
		int itemType;
		size_t itemSize, headerSize;
		const void *itemData;
		while (parser.getItem(itemType, itemData, itemSize, headerSize))
		{
			if (itemType == ITEM_TIGER_TREE)
			{
				prevTreeSize = itemSize + headerSize;
				if (itemSize == newTreeSize && (uint64_t) tree.getFileSize() == fileSize)
					updateTree = false;
				break;
			}
		}
	}

	if (!updateTree)
	{
		mdb_txn_abort(txnWrite);
		return true;
	}

	size_t outSize = (prevSize ? prevSize - prevTreeSize : BASE_ITEM_SIZE) + newTreeSize + 2;
	if (newTreeSize > 255) outSize++;

	buf.resize(outSize);
	uint8_t *ptr = buf.data();
	storeUnaligned16(ptr, setFlags);
	ptr += 2;
	storeUnaligned64(ptr, tree.getFileSize());
	ptr += 8;

	ptr += putItem(ptr, ITEM_TIGER_TREE, nullptr, newTreeSize);
	for (size_t i = 0; i < leaves.size(); ++i)
	{
		memcpy(ptr, leaves[i].data, TTH_SIZE);
		ptr += TTH_SIZE;
	}

	if (prevSize)
	{
		ItemParser parser(static_cast<const uint8_t*>(val.mv_data) + BASE_ITEM_SIZE, prevSize - BASE_ITEM_SIZE);
		int itemType;
		size_t itemSize, headerSize;
		const void *itemData;
		while (parser.getItem(itemType, itemData, itemSize, headerSize))
		{
			if (itemType != ITEM_TIGER_TREE)
			{
				memcpy(ptr, static_cast<const uint8_t*>(itemData) - headerSize, itemSize + headerSize);
				ptr += itemSize + headerSize;
			}
		}
	}

	val.mv_data = buf.data();
	val.mv_size = outSize;
	return writeData(txnWrite, dbi, key, val);
}

bool HashDatabaseLMDB::getDBInfo(size_t &dataItems, uint64_t &dbSize) noexcept
{
	MDB_stat stat;
	if (mdb_env_stat(env, &stat))
	{
		dataItems = 0;
		dbSize = 0;
		return false;
	}
	dataItems = stat.ms_entries;
	dbSize = uint64_t(stat.ms_branch_pages + stat.ms_leaf_pages + stat.ms_overflow_pages) * stat.ms_psize;
	return true;
}

HashDatabaseConnection *HashDatabaseLMDB::getDefaultConnection() noexcept
{
	ASSERT_MAIN_THREAD();
	LOCK(cs);
	if (!defConn) defConn.reset(new HashDatabaseConnection(env));
	return defConn.get();
}

HashDatabaseConnection *HashDatabaseLMDB::getConnection() noexcept
{
	if (BaseThread::getCurrentThreadId() == defThreadId)
		return getDefaultConnection();
	{
		LOCK(cs);
		for (auto& c : conn)
			if (!c->busy)
			{
				c->busy = true;
				return c.get();
			}
	}
	HashDatabaseConnection *newConn = new HashDatabaseConnection(env);
	std::unique_ptr<HashDatabaseConnection> c(newConn);
	LOCK(cs);
	conn.emplace_back(std::move(c));
	return newConn;
}

void HashDatabaseLMDB::putConnection(HashDatabaseConnection *conn) noexcept
{
	uint64_t removeTime = GET_TICK() + 120 * 1000;
	LOCK(cs);
	dcassert(conn->busy || conn == defConn.get());
	conn->busy = false;
	conn->removeTime = removeTime;
}

void HashDatabaseLMDB::closeIdleConnections(uint64_t tick) noexcept
{
	vector<unique_ptr<HashDatabaseConnection>> v;
	{
		LOCK(cs);
		for (auto i = conn.begin(); i != conn.end();)
		{
			unique_ptr<HashDatabaseConnection> &c = *i;
			if (!c->busy && tick > c->removeTime)
			{
				v.emplace_back(std::move(c));
				i = conn.erase(i);
			}
			else
				++i;
		}
	}
	v.clear();
}
