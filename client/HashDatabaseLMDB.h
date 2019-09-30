#ifndef HASH_DATABASE_LMDB_H
#define HASH_DATABASE_LMDB_H

#include "mdb/lmdb.h"
#include "CFlyThread.h"
#include <vector>

class HashDatabaseLMDB
{
	public:
		HashDatabaseLMDB();
		~HashDatabaseLMDB() { close(); }
		HashDatabaseLMDB(const HashDatabaseLMDB&) = delete;
		HashDatabaseLMDB& operator= (const HashDatabaseLMDB&) = delete;

		bool open();
		void close();
		bool getFileInfo(const void *data, unsigned &flags, string &path);
		bool putFileInfo(const void *tth, unsigned flags, const string *path);

	private:
		MDB_env *env;
		MDB_dbi dbi;
		MDB_txn *txnRead;
		bool txnReadReset;
		std::vector<uint8_t> buf;
		CriticalSection cs;

		bool checkError(int error);
		void resetReadTrans();
};

#endif /* HASH_DATABASE_LMDB_H */
