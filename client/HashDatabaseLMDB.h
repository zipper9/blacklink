#ifndef HASH_DATABASE_LMDB_H
#define HASH_DATABASE_LMDB_H

#include "mdb/lmdb.h"
#include "CFlyThread.h"
#include "MerkleTree.h"

class HashDatabaseLMDB
{
	public:
		HashDatabaseLMDB();
		~HashDatabaseLMDB() { close(); }
		HashDatabaseLMDB(const HashDatabaseLMDB&) = delete;
		HashDatabaseLMDB& operator= (const HashDatabaseLMDB&) = delete;

		bool open();
		void close();
		bool getFileInfo(const void *tth, unsigned &flags, string &path);
		bool getTigerTree(const void *tth, TigerTree &tree);
		bool putFileInfo(const void *tth, unsigned flags, uint64_t fileSize, const string *path);
		bool putTigerTree(const TigerTree &tree);

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
