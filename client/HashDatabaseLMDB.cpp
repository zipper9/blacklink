#include "stdinc.h"
#include "HashDatabaseLMDB.h"
#include "StrUtil.h"
#include "AppPaths.h"
#include "PathUtil.h"
#include "Util.h" // ASSERT_MAIN_THREAD
#include "File.h"
#include "TimeUtil.h"
#include "LogManager.h"
#include "unaligned.h"

static const size_t TTH_SIZE = TigerTree::BYTES;
static const size_t BASE_ITEM_SIZE = 10;

enum
{
	ITEM_TIGER_TREE   = 1,
	ITEM_LOCAL_PATH   = 2,
	ITEM_UPLOAD_COUNT = 3
};

#if defined(_WIN64) || defined(__LP64__)
#define PLATFORM_TAG "x64"
static const mdb_size_t MIN_MAP_SIZE = 1024ull*1024ull*1024ull;
#else
#define PLATFORM_TAG "x32"
static const mdb_size_t MIN_MAP_SIZE = 400ull*1024ull*1024ull;
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
	if (!checkError(error, "mdb_env_create")) return false;

	string path = getDBPath();
	path += PATH_SEPARATOR;
	File::ensureDirectory(path);
	error = mdb_env_open(env, path.c_str(), 0, 0664);
	if (!checkError(error, "mdb_env_open"))
	{
		mdb_env_close(env);
		env = nullptr;
		return false;
	}

	MDB_envinfo info;
	error = mdb_env_info(env, &info);
	if (error || info.me_mapsize < MIN_MAP_SIZE)
	{
		error = mdb_env_set_mapsize(env, MIN_MAP_SIZE);
		if (!checkError(error, "mdb_env_set_mapsize"))
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

bool HashDatabaseLMDB::checkError(int error, const char* what, HashDatabaseConnection* conn) noexcept
{
	if (!error) return true;
	string errorText = "LMDB error: " + Util::toString(error);
	if (what)
	{
		errorText += " (";
		errorText += what;
		errorText += ")";
	}
	errorText += ", thread 0x";
	errorText += Util::toHexString(BaseThread::getCurrentThreadId());
	LogManager::message(errorText);
	if (conn) conn->error = true;
	return false;
}

void HashDatabaseLMDB::printWarning(int error, const char *what)
{
	dcassert(error);
	string errorText = "LMDB recoverable error: " + Util::toString(error);
	if (what)
	{
		errorText += " (";
		errorText += what;
		errorText += ")";
	}
	errorText += ", thread 0x";
	errorText += Util::toHexString(BaseThread::getCurrentThreadId());
	LogManager::message(errorText, false);
}

class ItemParser
{
	public:
		ItemParser(const uint8_t *data, size_t dataSize) noexcept : data(data), dataSize(dataSize), ptr(0) {}
		bool getItem(int &itemType, const void* &itemData, size_t &itemSize, size_t &headerSize) noexcept;
		void reset() noexcept { ptr = 0; }

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
	if (!parent->addTransaction()) return false;
	int error;
	const char* what;
	if (!txnRead)
	{
		what = "mdb_txn_begin";
		error = mdb_txn_begin(parent->env, nullptr, MDB_RDONLY, &txnRead);
	}
	else
	{
		error = mdb_txn_renew(txnRead);
		what = "mdb_txn_renew";
	}
	if (!HashDatabaseLMDB::checkError(error, what, this))
	{
		if (txnRead)
		{
			mdb_txn_abort(txnRead);
			txnRead = nullptr;
		}
		parent->releaseTransaction(this);
		return false;
	}
	error = mdb_dbi_open(txnRead, nullptr, 0, &dbi);
	if (!HashDatabaseLMDB::checkError(error, "mdb_dbi_open", this))
	{
		parent->releaseTransaction(this);
		return false;
	}
	return true;
}

void HashDatabaseConnection::completeReadTxn() noexcept
{
	mdb_txn_reset(txnRead);
	parent->releaseTransaction(this);
}

bool HashDatabaseConnection::createWriteTxn(MDB_dbi &dbi, MDB_txn* &txnWrite) noexcept
{
	if (!parent->addTransaction()) return false;
	int error = mdb_txn_begin(parent->env, nullptr, 0, &txnWrite);
	if (!HashDatabaseLMDB::checkError(error, "mdb_txn_begin", this))
	{
		parent->releaseTransaction(this);
		return false;
	}
	error = mdb_dbi_open(txnWrite, nullptr, 0, &dbi);
	if (!HashDatabaseLMDB::checkError(error, "mdb_dbi_open", this))
	{
		parent->releaseTransaction(this);
		return false;
	}
	return true;
}

void HashDatabaseConnection::abortWriteTxn(MDB_txn *txnWrite) noexcept
{
	mdb_txn_abort(txnWrite);
	parent->releaseTransaction(this);
}

bool HashDatabaseConnection::writeData(MDB_txn *txnWrite, MDB_dbi dbi, MDB_val &key, MDB_val &val) noexcept
{
	bool result = false;
	int retryCount = 0;
	while (retryCount < 2)
	{
		int error = mdb_put(txnWrite, dbi, &key, &val, 0);
		if (error) HashDatabaseLMDB::printWarning(error, "mdb_put");
		if (error == MDB_MAP_FULL)
		{
			abortWriteTxn(txnWrite);
			if (!resizeMap() || !createWriteTxn(dbi, txnWrite)) break;
			++retryCount;
			continue;
		}
		if (!HashDatabaseLMDB::checkError(error, "mdb_put", this))
		{
			abortWriteTxn(txnWrite);
			break;
		}
		error = mdb_txn_commit(txnWrite);
		if (error) HashDatabaseLMDB::printWarning(error, "mdb_txn_commit");
		txnWrite = nullptr; // Must not call mdb_txn_abort after mdb_txn_commit, even when an error is returned
		if (error == MDB_MAP_FULL)
		{
			parent->releaseTransaction(this);
			if (!resizeMap() || !createWriteTxn(dbi, txnWrite)) break;
			++retryCount;
			continue;
		}
		result = HashDatabaseLMDB::checkError(error, "mdb_txn_commit", this);
		break;
	}
	if (result)
	{
		mdb_dbi_close(parent->env, dbi); // dbi is always 1, so mdb_dbi_close does nothing
		parent->releaseTransaction(this);
	}
	return result;
}

bool HashDatabaseConnection::deleteData(MDB_txn *txnWrite, MDB_dbi dbi, MDB_val &key) noexcept
{
	bool result = false;
	int retryCount = 0;
	while (retryCount < 2)
	{
		int error = mdb_del(txnWrite, dbi, &key, nullptr);
		if (error == MDB_MAP_FULL)
		{
			abortWriteTxn(txnWrite);
			if (!resizeMap() || !createWriteTxn(dbi, txnWrite)) break;
			++retryCount;
			continue;
		}
		if (!HashDatabaseLMDB::checkError(error, "mdb_del", this))
		{
			abortWriteTxn(txnWrite);
			break;
		}
		error = mdb_txn_commit(txnWrite);
		txnWrite = nullptr;
		if (error == MDB_MAP_FULL)
		{
			parent->releaseTransaction(this);
			if (!resizeMap() || !createWriteTxn(dbi, txnWrite)) break;
			++retryCount;
			continue;
		}
		result = HashDatabaseLMDB::checkError(error, "mdb_txn_commit", this);
		break;
	}
	if (result)
	{
		mdb_dbi_close(parent->env, dbi); // dbi is always 1, so mdb_dbi_close does nothing
		parent->releaseTransaction(this);
	}
	return result;
}

bool HashDatabaseConnection::resizeMap() noexcept
{
	if (!parent->resizeMap()) return false;
	if (parent->getSharedState() == HashDatabaseLMDB::STATE_NORMAL) return true; // already resized by another thread

	MDB_envinfo info;
	int error = mdb_env_info(parent->env, &info);
	if (!HashDatabaseLMDB::checkError(error, "mdb_env_info", this))
	{
		parent->completeResize();
		return false;
	}

	size_t newMapSize = info.me_mapsize << 1;
	LogManager::message("Resizing LMDB map to " + Util::toString(newMapSize), false);
	error = mdb_env_set_mapsize(parent->env, newMapSize);
	bool result = HashDatabaseLMDB::checkError(error, "mdb_env_set_mapsize", this);
	parent->completeResize();
	return result;
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
		completeReadTxn();
		return false;
	}

	if (val.mv_size < BASE_ITEM_SIZE)
	{
		completeReadTxn();
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

	completeReadTxn();
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
		completeReadTxn();
		return false;
	}

	if (val.mv_size < BASE_ITEM_SIZE)
	{
		completeReadTxn();
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

	completeReadTxn();
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
					if (path && itemSize == path->length() && path->compare(0, itemSize, (const char *) itemData, itemSize) == 0)
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
		abortWriteTxn(txnWrite);
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
		abortWriteTxn(txnWrite);
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

bool HashDatabaseConnection::removeTigerTree(const void *tth) noexcept
{
	MDB_txn *txnWrite = nullptr;
	MDB_dbi dbi;
	if (!createWriteTxn(dbi, txnWrite)) return false;

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = TTH_SIZE;

	int error = mdb_get(txnWrite, dbi, &key, &val);
	if (!error && val.mv_size >= BASE_ITEM_SIZE + TTH_SIZE)
	{
		const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
		unsigned flags = loadUnaligned16(ptr);
		size_t treeSize = 0;
		ItemParser parser(ptr + BASE_ITEM_SIZE, val.mv_size - BASE_ITEM_SIZE);
		int itemType;
		size_t itemSize, headerSize;
		const void *itemData;
		while (parser.getItem(itemType, itemData, itemSize, headerSize))
		{
			if (itemType == ITEM_TIGER_TREE)
			{
				treeSize = itemSize + headerSize;
				break;
			}
		}
		if (treeSize)
		{
			parser.reset();
			buf.resize(val.mv_size - treeSize);
			uint8_t *outPtr = buf.data();
			memcpy(outPtr, ptr, BASE_ITEM_SIZE);
			outPtr += BASE_ITEM_SIZE;
			while (parser.getItem(itemType, itemData, itemSize, headerSize))
			{
				if (itemType == ITEM_TIGER_TREE) continue;
				memcpy(outPtr, static_cast<const uint8_t*>(itemData) - headerSize, itemSize + headerSize);
				outPtr += itemSize + headerSize;
			}
			size_t outSize = outPtr - buf.data();
			if (outSize == BASE_ITEM_SIZE && flags == 0)
				return deleteData(txnWrite, dbi, key);
			
			val.mv_data = buf.data();
			val.mv_size = outSize;
			return writeData(txnWrite, dbi, key, val);
		}
	}

	abortWriteTxn(txnWrite);
	return false;
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
	if (!defConn) defConn.reset(new HashDatabaseConnection(this));
	return defConn.get();
}

HashDatabaseConnection *HashDatabaseLMDB::getConnection() noexcept
{
	if (BaseThread::getCurrentThreadId() == defThreadId)
		return getDefaultConnection();
	{
		LOCK(cs);
		for (auto& c : conn)
			if (!c->busy && !c->error)
			{
				c->busy = true;
				return c.get();
			}
	}
	HashDatabaseConnection *newConn = new HashDatabaseConnection(this);
	std::unique_ptr<HashDatabaseConnection> c(newConn);
	LOCK(cs);
	conn.emplace_back(std::move(c));
	return newConn;
}

void HashDatabaseLMDB::putConnection(HashDatabaseConnection *conn) noexcept
{
	uint64_t removeTime = GET_TICK();
	LOCK(cs);
	dcassert(conn->busy || conn == defConn.get());
	conn->busy = false;
	conn->removeTime = removeTime;
	if (!conn->error) conn->removeTime += 120 * 1000;
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

bool HashDatabaseConnection::getDBInfo(DbInfo &info, int flags) noexcept
{
	info.totalKeysSize = info.totalDataSize = info.totalTreesSize = info.mapSize = -1;

	MDB_stat stat;
	if (mdb_env_stat(parent->env, &stat))
	{
		info.numPages = 0;
		info.numKeys = 0;
		return false;
	}

	info.numKeys = stat.ms_entries;
	info.numPages = stat.ms_branch_pages + stat.ms_leaf_pages + stat.ms_overflow_pages;

	if (!(flags & GET_DB_INFO_DETAILS)) return true;

	if (flags & GET_DB_INFO_MAP_SIZE)
	{
		MDB_envinfo envinfo;
		if (!mdb_env_info(parent->env, &envinfo))
			info.mapSize = envinfo.me_mapsize;
	}

	MDB_dbi dbi;
	if (!createReadTxn(dbi)) return false;

	MDB_cursor *cursor = nullptr;
	if (mdb_cursor_open(txnRead, dbi, &cursor)) return false;

	TigerHash hashKeys, hashValues;

	info.totalKeysSize = 0;
	info.totalDataSize = 0;
	info.totalTreesSize = 0;

	MDB_val key, val;
	if (mdb_cursor_get(cursor, &key, &val, MDB_FIRST) == 0)
	{
		do
		{
			info.totalKeysSize += key.mv_size;
			info.totalDataSize += val.mv_size;
			if (flags & GET_DB_INFO_DIGEST)
			{
				hashKeys.update(key.mv_data, key.mv_size);
				hashValues.update(val.mv_data, val.mv_size);
			}
			if (flags & GET_DB_INFO_TREES)
			{
				ItemParser parser(static_cast<const uint8_t*>(val.mv_data) + BASE_ITEM_SIZE, val.mv_size - BASE_ITEM_SIZE);
				int itemType;
				size_t itemSize, headerSize;
				const void *itemData;
				while (parser.getItem(itemType, itemData, itemSize, headerSize))
				{
					if (itemType == ITEM_TIGER_TREE)
						info.totalTreesSize += itemSize;
				}
			}
		} while (mdb_cursor_get(cursor, &key, &val, MDB_NEXT) == 0);
	}

	mdb_cursor_close(cursor);
	completeReadTxn();

	if (flags & GET_DB_INFO_DIGEST)
	{
		memcpy(info.keysHash, hashKeys.finalize(), TigerHash::BYTES);
		memcpy(info.dataHash, hashValues.finalize(), TigerHash::BYTES);
	}
	else
	{
		memset(info.keysHash, 0, sizeof(info.keysHash));
		memset(info.dataHash, 0, sizeof(info.dataHash));
	}
	if (!(flags & GET_DB_INFO_TREES))
		info.totalTreesSize = -1;

	return true;
}

bool HashDatabaseLMDB::addTransaction() noexcept
{
	for (;;)
	{
		csSharedState.lock();
		int state = sharedState;
		if (state == STATE_NORMAL) ++numTxn;
		csSharedState.unlock();

		if (state == STATE_NORMAL) break;
		if (state == STATE_SHUTDOWN) return false;
		dcassert(state == STATE_RESIZE || state == STATE_WANT_RESIZE);
		if (!waitResizeComplete()) return false;
	}
	return true;
}

void HashDatabaseLMDB::releaseTransaction(HashDatabaseConnection *conn) noexcept
{
	csSharedState.lock();
	dcassert(sharedState == STATE_NORMAL || sharedState == STATE_WANT_RESIZE || sharedState == STATE_SHUTDOWN);
	dcassert(numTxn > 0);
	int state = sharedState;
	int newNumTxn = --numTxn;
	csSharedState.unlock();
	if (state == STATE_WANT_RESIZE)
	{
		mdb_txn_abort(conn->txnRead);
		conn->txnRead = nullptr;
		if (newNumTxn == 0) mayResize.notify();
	}
}

bool HashDatabaseLMDB::resizeMap() noexcept
{
	int state = -1;
	csSharedState.lock();
	int prevState = sharedState;
	if (prevState != STATE_RESIZE && prevState != STATE_WANT_RESIZE)
	{
		sharedState = numTxn == 0 ? STATE_RESIZE : STATE_WANT_RESIZE;
		state = sharedState;
		if (resizeComplete.empty())
			resizeComplete.create();
		else
			resizeComplete.reset();
		if (state == STATE_WANT_RESIZE)
		{
			if (mayResize.empty())
				mayResize.create();
			else
				mayResize.reset();
		}
	}
	csSharedState.unlock();
	if (prevState == STATE_RESIZE || prevState == STATE_WANT_RESIZE) // Another writer is resizing the map
		return waitResizeComplete();
	if (state == STATE_WANT_RESIZE)
	{
		if (!waitMayResize()) return false;
	}
#ifdef _DEBUG
	csSharedState.lock();
	state = sharedState;
	csSharedState.unlock();
	dcassert(state == STATE_RESIZE);
#endif
	return true;
}

bool HashDatabaseLMDB::waitMayResize() noexcept
{
	bool result = true;
	mayResize.wait();
	csSharedState.lock();
	if (sharedState != STATE_SHUTDOWN)
	{
		dcassert(sharedState == STATE_WANT_RESIZE);
		sharedState = STATE_RESIZE;
	}
	else
		result = false;
	csSharedState.unlock();
	return result;
}

bool HashDatabaseLMDB::waitResizeComplete() noexcept
{
	resizeComplete.wait();
	csSharedState.lock();
	bool result = sharedState != STATE_SHUTDOWN;
	csSharedState.unlock();
	return result;
}

void HashDatabaseLMDB::completeResize() noexcept
{
	csSharedState.lock();
	if (sharedState != STATE_SHUTDOWN)
	{
		dcassert(sharedState == STATE_RESIZE);
		sharedState = STATE_NORMAL;
	}
	csSharedState.unlock();
	resizeComplete.notify();
}

int HashDatabaseLMDB::getSharedState() const noexcept
{
	LOCK(csSharedState);
	return sharedState;
}

void HashDatabaseLMDB::shutdown() noexcept
{
	csSharedState.lock();
	sharedState = STATE_SHUTDOWN;
	csSharedState.unlock();
	if (!resizeComplete.empty()) resizeComplete.notify();
	if (!mayResize.empty()) mayResize.notify();
}

string HashDatabaseLMDB::getConnectionInfo() const noexcept
{
	string info;
	{
		uint64_t now = GET_TICK();
		LOCK(cs);
		for (const auto& item : conn)
		{
			HashDatabaseConnection* c = item.get();
			info += "Connection ";
			info += Util::toHexString(c);
			info += ": busy=";
			info += Util::toString((int) c->busy);
			info += ", error=";
			info += Util::toString((int) c->error);
			if (!c->busy)
			{
				uint64_t expires = now < c->removeTime ? (c->removeTime - now) / 1000 : 0;
				info += ", expires=";
				info += Util::toString(expires);
			}
			info += '\n';
		}
	}
	return info;
}
