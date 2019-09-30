#include "stdinc.h"
#include "HashDatabaseLMDB.h"
#include "Text.h"
#include "Util.h"
#include "File.h"

HashDatabaseLMDB::HashDatabaseLMDB()
{
	env = nullptr;	
	dbi = 0;
	txnRead = nullptr;
	txnReadReset = false;
}

bool HashDatabaseLMDB::open()
{
	if (env) return false;

	int error = mdb_env_create(&env);
	if (!checkError(error)) return false;

	error = mdb_env_set_mapsize(env, 1024ull*1024ull*1024ull);
	if (!checkError(error)) return false;

	string path = Text::fromUtf8(Util::getConfigPath());
	path += "hash-db";
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
	key.mv_size = 24;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (error)
	{		
		resetReadTrans();
		return false;
	}

	if (val.mv_size < 3)
	{
		resetReadTrans();
		return false;
	}
	const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
	flags = *(const uint16_t *) ptr;
	ptr += 2;
	unsigned pathCount = *ptr++;
	if (pathCount)
	{
		const char *end = static_cast<const char*>(memchr(ptr, 0, val.mv_size - 3));
		if (end)
		{
			const char *start = reinterpret_cast<const char*>(ptr);
			path.assign(start, end-start);
		}
	}
	
	resetReadTrans();
	return true;
}

bool HashDatabaseLMDB::putFileInfo(const void *tth, unsigned flags, const string *path)
{
	CFlyLock(cs);
	if (!txnRead) return false;

	if (txnReadReset)
	{
		mdb_txn_renew(txnRead);
		txnReadReset = false;
	}

	unsigned setFlags = 0;	
	size_t prevPathSize = 0;
	unsigned prevPathCount = 0;
	bool updateFlags = false, updatePath = false;

	MDB_val key, val;
	key.mv_data = const_cast<void*>(tth);
	key.mv_size = 24;
	int error = mdb_get(txnRead, dbi, &key, &val);
	if (!error && val.mv_size >= 3)
	{		
		const uint8_t *ptr = static_cast<const uint8_t*>(val.mv_data);
		setFlags = *(const uint16_t *) ptr;
		ptr += 2;
		prevPathCount = *ptr++;
		prevPathSize = val.mv_size - 3;
		if (path && prevPathCount < 255)
		{
			updatePath = true;
			unsigned pathCount = prevPathCount;
			size_t pathSize = prevPathSize;
			while (pathCount)
			{
				const char *end = static_cast<const char*>(memchr(ptr, 0, pathSize));
				if (!end) break;
				const char *start = reinterpret_cast<const char*>(ptr);
				size_t strSize = end-start;
				if (!path->compare(start))
				{
					updatePath = false;
					break;
				}
				ptr += strSize + 1;
				pathSize -= strSize + 1;
				pathCount--;
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

	size_t outSize = 3 + prevPathSize;
	if (updatePath)
	{
		outSize += path->length() + 1;
		prevPathCount++;
	}
	
	buf.resize(outSize);
	uint8_t *ptr = &buf[0];
	*(uint16_t *) ptr = setFlags;
	ptr += 2;
	*ptr++ = prevPathCount;
	if (updatePath)
	{
		memcpy(ptr, path->c_str(), path->length() + 1);
		ptr += path->length() + 1;
	}
	if (prevPathSize)
		memcpy(ptr, static_cast<const uint8_t*>(val.mv_data) + 3, prevPathSize);

	val.mv_data = &buf[0];
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
