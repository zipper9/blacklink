#include "stdinc.h"
#include "HashDatabaseLMDB.h"
#include "Text.h"
#include "Util.h"
#include "File.h"

static const size_t TTH_SIZE = 24;
static const size_t BASE_ITEM_SIZE = 10;

enum
{
	ITEM_TIGER_TREE = 1,
	ITEM_LOCAL_PATH = 2
};

HashDatabaseLMDB::HashDatabaseLMDB()
{
	env = nullptr;
	dbi = 0;
	txnRead = nullptr;
	txnReadReset = false;
}

#ifdef _WIN64
#define PLATFORM_TAG "x64"
#else
#define PLATFORM_TAG "x32"
#endif

bool HashDatabaseLMDB::open()
{
	if (env) return false;

	int error = mdb_env_create(&env);
	if (!checkError(error)) return false;

	error = mdb_env_set_mapsize(env, 1024ull*1024ull*1024ull);
	if (!checkError(error)) return false;

	string path = Text::fromUtf8(Util::getConfigPath());
	path += "hash-db." PLATFORM_TAG;
	path += PATH_SEPARATOR;
	File::ensureDirectory(path);
	error = mdb_env_open(env, path.c_str(), 0, 0664);
	if (!checkError(error))
	{
		mdb_env_close(env);
		env = nullptr;
		return false;
	}

	error = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txnRead);
	if (!checkError(error))
	{
		mdb_env_close(env);
		env = nullptr;
		return false;
	}

	error = mdb_dbi_open(txnRead, nullptr, 0, &dbi);
	if (!checkError(error))
	{
		mdb_env_close(env);
		env = nullptr;
		return false;
	}

	return true;
}

void HashDatabaseLMDB::close()
{
	if (txnRead)
	{
		mdb_txn_abort(txnRead);
		txnRead = nullptr;
	}
	if (env)
	{
		mdb_env_close(env);
		env = nullptr;
	}
}

bool HashDatabaseLMDB::checkError(int error)
{
	if (!error) return true;
	// TODO: log error
	//string errorText = mdb_strerror(error);
	return false;
}

class ItemParser
{
	public:
		ItemParser(const uint8_t *data, size_t dataSize) : data(data), dataSize(dataSize), ptr(0)
		{
		}

		bool getItem(int &itemType, const void* &itemData, size_t &itemSize, size_t &headerSize);
	
	private:
		const uint8_t* const data;
		const size_t dataSize;
		size_t ptr;
};

bool ItemParser::getItem(int &itemType, const void* &itemData, size_t &itemSize, size_t &headerSize)
{
	if (ptr >= dataSize) return false;
	uint8_t type = data[ptr];
	if (type & 0x80)
	{
		type &= 0x7F;
		headerSize = 3;
		if (dataSize - ptr < 3) return false;
		itemSize = *(const uint16_t *) (data + ptr + 1);
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

static size_t putItem(uint8_t *ptr, int type, const void *data, size_t dataSize)
{
	size_t result = 0;
	*ptr = type;
	if (dataSize > 255)
	{
		*ptr |= 0x80;
		*(uint16_t *)(ptr + 1) = static_cast<uint16_t>(dataSize);
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

bool HashDatabaseLMDB::getFileInfo(const void *tth, unsigned &flags, string &path)
{	
	flags = 0;
	path.clear();

	CFlyLock(cs);
	if (!txnRead) return false;

	if (txnReadReset)
	{
		mdb_txn_renew(txnRead);
		txnReadReset = false;
	}

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (error)
	{		
		resetReadTrans();
		return false;
	}

	if (val.mv_size < BASE_ITEM_SIZE)
	{
		resetReadTrans();
		return false;
	}
	
	const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
	flags = *(const uint16_t *) ptr;

	ItemParser parser(ptr + BASE_ITEM_SIZE, val.mv_size - BASE_ITEM_SIZE);
	int itemType;	
	size_t itemSize, headerSize;
	const void *itemData;
	while (parser.getItem(itemType, itemData, itemSize, headerSize))
	{
		if (itemType == ITEM_LOCAL_PATH)
		{
			path.assign(static_cast<const char*>(itemData), itemSize);
			break;
		}
	}
	
	resetReadTrans();
	return true;
}

bool HashDatabaseLMDB::getTigerTree(const void *tth, TigerTree &tree)
{
	CFlyLock(cs);
	if (!txnRead) return false;

	if (txnReadReset)
	{
		mdb_txn_renew(txnRead);
		txnReadReset = false;
	}

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (error)
	{		
		resetReadTrans();
		return false;
	}

	if (val.mv_size < BASE_ITEM_SIZE)
	{
		resetReadTrans();
		return false;
	}
	
	const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
	uint64_t fileSize = *(const uint64_t *) (ptr + 2);
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
	
	resetReadTrans();
	return found;
}

bool HashDatabaseLMDB::putFileInfo(const void *tth, unsigned flags, uint64_t fileSize, const string *path)
{
	CFlyLock(cs);
	if (!txnRead) return false;

	if (txnReadReset)
	{
		mdb_txn_renew(txnRead);
		txnReadReset = false;
	}

	unsigned setFlags = 0;
	bool updateFlags = false, updatePath = false;
	size_t prevSize = 0;

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (!error && val.mv_size >= BASE_ITEM_SIZE)
	{		
		const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
		setFlags = *(const uint16_t *) ptr;
		prevSize = val.mv_size;
		if (path)
		{
			updatePath = true;
			ItemParser parser(ptr + BASE_ITEM_SIZE, prevSize - BASE_ITEM_SIZE);
			int itemType;
			size_t itemSize, headerSize;
			const void *itemData;
			while (parser.getItem(itemType, itemData, itemSize, headerSize))
			{
				if (itemType == ITEM_LOCAL_PATH && itemSize == path->length() && path->compare(0, itemSize, (const char *) itemData, itemSize) == 0)
				{
					updatePath = false;
					break;
				}
			}
		}
	} else
	{
		if (path) updatePath = true;
	}

	resetReadTrans();

	if ((setFlags | flags) != setFlags)
	{
		setFlags |= flags;
		updateFlags = true;
	}

	if (!updateFlags && !updatePath) return true;

	size_t outSize = prevSize ? prevSize : BASE_ITEM_SIZE;
	if (updatePath)
	{		
		outSize += path->length() + 2;
		if (path->length() > 255) outSize++;
	}
	
	buf.resize(outSize);
	if (prevSize)
		memcpy(buf.data(), val.mv_data, prevSize);

	uint8_t *ptr = buf.data();
	*(uint16_t *) ptr = setFlags;
	ptr += 2;
	*(uint64_t *) ptr = fileSize;
	ptr += 8;

	if (updatePath)
	{
		if (prevSize)
			ptr += prevSize - BASE_ITEM_SIZE;
		putItem(ptr, ITEM_LOCAL_PATH, path->data(), path->length());
	}

	val.mv_data = buf.data();
	val.mv_size = outSize;

	MDB_txn *txnWrite = nullptr;
	error = mdb_txn_begin(env, nullptr, 0, &txnWrite);
	if (!checkError(error)) return false;

	error = mdb_put(txnWrite, dbi, &key, &val, 0);
	bool result = checkError(error);
	mdb_txn_commit(txnWrite);

	return result;
}

bool HashDatabaseLMDB::putTigerTree(const TigerTree &tree)
{
	if (tree.getLeaves().size() < 2) return false;
	
	CFlyLock(cs);
	if (!txnRead) return false;

	if (txnReadReset)
	{
		mdb_txn_renew(txnRead);
		txnReadReset = false;
	}

	const TigerTree::MerkleList &leaves = tree.getLeaves();
	unsigned setFlags = 0;
	bool updateTree = true;
	size_t prevSize = 0;
	size_t prevTreeSize = 0;
	size_t newTreeSize = leaves.size() * TTH_SIZE;

	MDB_val key, val;
	key.mv_data = (void *)(tree.getRoot().data);
	key.mv_size = TTH_SIZE;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (!error && val.mv_size >= BASE_ITEM_SIZE)
	{		
		const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
		setFlags = *(const uint16_t *) ptr;
		uint64_t fileSize = *(const uint64_t *) (ptr + 2);
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

	resetReadTrans();

	if (!updateTree) return true;

	size_t outSize = (prevSize ? prevSize - prevTreeSize : BASE_ITEM_SIZE) + newTreeSize + 2;
	if (newTreeSize > 255) outSize++;

	buf.resize(outSize);
	uint8_t *ptr = buf.data();
	*(uint16_t *) ptr = setFlags;
	ptr += 2;
	*(uint64_t *) ptr = tree.getFileSize();
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

	MDB_txn *txnWrite = nullptr;
	error = mdb_txn_begin(env, nullptr, 0, &txnWrite);
	if (!checkError(error)) return false;

	error = mdb_put(txnWrite, dbi, &key, &val, 0);
	bool result = checkError(error);
	mdb_txn_commit(txnWrite);

	return result;
}

void HashDatabaseLMDB::resetReadTrans()
{
	if (!txnReadReset)
	{
		mdb_txn_reset(txnRead);
		txnReadReset = true;
	}
}
