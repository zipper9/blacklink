#ifndef HASH_DATABASE_LMDB_H
#define HASH_DATABASE_LMDB_H

#include "mdb/lmdb.h"
#include "Locks.h"
#include "MerkleTree.h"

class HashDatabaseConnection
{
		friend class HashDatabaseLMDB;

	public:
		~HashDatabaseConnection() noexcept;
		HashDatabaseConnection(const HashDatabaseConnection&) = delete;
		HashDatabaseConnection& operator= (const HashDatabaseConnection&) = delete;

		bool getFileInfo(const void *tth, unsigned &flags, uint64_t *fileSize, string *path, size_t *treeSize, uint32_t *uploadCount) noexcept;
		bool getTigerTree(const void *tth, TigerTree &tree) noexcept;
		bool putFileInfo(const void *tth, unsigned flags, uint64_t fileSize, const string *path, bool incUploadCount) noexcept;
		bool putTigerTree(const TigerTree &tree) noexcept;

	private:
		bool busy = true;
		uint64_t removeTime = 0;
		MDB_txn *txnRead = nullptr;
		MDB_env *const env;
		std::vector<uint8_t> buf;

		HashDatabaseConnection(MDB_env *env) : env(env) {}
		bool createReadTxn(MDB_dbi &dbi) noexcept;
		bool createWriteTxn(MDB_dbi &dbi, MDB_txn* &txnWrite) noexcept;
		bool writeData(MDB_txn *txnWrite, MDB_dbi dbi, MDB_val &key, MDB_val &val) noexcept;
		bool resizeMap() noexcept;
};

class HashDatabaseLMDB
{
		friend class HashDatabaseConnection;

	public:
		HashDatabaseLMDB() {}
		~HashDatabaseLMDB() noexcept { close(); }
		HashDatabaseLMDB(const HashDatabaseLMDB&) = delete;
		HashDatabaseLMDB& operator= (const HashDatabaseLMDB&) = delete;

		bool open() noexcept;
		void close() noexcept;
		bool getDBInfo(size_t &dataItems, uint64_t &dbSize) noexcept;
		static string getDBPath() noexcept;
		HashDatabaseConnection *getDefaultConnection() noexcept;
		HashDatabaseConnection *getConnection() noexcept;
		void putConnection(HashDatabaseConnection *conn) noexcept;
		void closeIdleConnections(uint64_t tick) noexcept;

	private:
		MDB_env *env = nullptr;
		mutable CriticalSection cs;
		std::unique_ptr<HashDatabaseConnection> defConn;
		std::list<std::unique_ptr<HashDatabaseConnection>> conn;
		uintptr_t defThreadId = 0;

		static bool checkError(int error) noexcept;
};

#endif /* HASH_DATABASE_LMDB_H */
